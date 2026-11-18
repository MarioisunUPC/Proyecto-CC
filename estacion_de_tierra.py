"""
ESTACIÓN DE TIERRA - Versión 2

Recibe por el puerto serie los datos que el satélite envía a través del
Arduino de tierra y los muestra en dos gráficas:
  - Temperatura, humedad y media de las 10 últimas temperaturas.
  - Radar de 360 grados con las distancias de los dos sensores.
Desde los botones se envían órdenes al satélite (ver protocolos.py).
"""

import time
import tkinter as tk
from tkinter import messagebox

import numpy as np
import serial
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure

import protocolos as P
from cola_circular import CircularQueue

# ---------------- Configuración ----------------
PUERTO = 'COM7'
BAUDIOS = 9600
MAX_PUNTOS = 60        # Puntos que se ven en la gráfica de temperatura
N_MEDIA = 10           # Temperaturas que se usan para la media
DIST_MAX = 70          # Radio máximo del radar (cm)
MEMORIA_RADAR = 5      # Segundos que un punto se queda en el radar
SALTO_MAX = 0         # Si dos puntos están a más grados, no se unen con línea

# ---------------- Variables globales ----------------
tiempos, temps, hums, medias = [], [], [], []
cola_temps = CircularQueue(N_MEDIA)
radar = np.full(360, np.nan)   # Distancia medida en cada grado (NaN = nada)
radar_t = np.zeros(360)        # Momento en que se midió cada grado
angulo = 90                    # Ángulo actual del servo
t0 = time.time()
ultimo_dato_th = time.time()

th_activo = True
dist_activo = True
media_en_satelite = False
periodo_th = 3.0
umbral = 30.0
medias_altas = 0               # Medias seguidas por encima del umbral


# ================= Funciones de comunicación =================

def enviar(comando):
    """Envía un comando al satélite. Se manda dos veces (la segunda 150 ms
    después) porque el satélite puede perderlo si llega mientras transmite.
    Parámetros: comando (str). Resultado: ninguno."""
    try:
        ser.write((comando + '\n').encode())
        ventana.after(150, lambda: ser.write((comando + '\n').encode()))
    except serial.SerialException:
        poner_alarma(lbl_alarma_com, True, "Comunicación")


def leer_puerto():
    """Lee todas las líneas que hayan llegado, las procesa y redibuja.
    Se vuelve a llamar a sí misma cada 100 ms.
    Parámetros: ninguno. Resultado: ninguno."""
    try:
        while ser.in_waiting > 0:
            linea = ser.readline().decode('utf-8', errors='ignore').strip()
            if linea:
                procesar_linea(linea)
    except serial.SerialException:
        poner_alarma(lbl_alarma_com, True, "Comunicación")

    # Si hace rato que no llegan datos de T/H, dejamos un hueco en la gráfica
    if th_activo and time.time() - ultimo_dato_th > max(5, 1.5 * periodo_th):
        anadir_hueco()

    dibujar()
    ventana.after(100, leer_puerto)


def procesar_linea(linea):
    """Interpreta una trama recibida según su código (ver protocolos.py).
    Parámetros: linea (str). Resultado: ninguno."""
    global angulo, ultimo_dato_th
    codigo, campos = P.parsear_trama(linea)
    try:
        if codigo == '1':                       # Temperatura y humedad
            t, h = P.parsear_th(campos)
            cola_temps.push(t)
            if media_en_satelite:
                media = np.nan                  # Llegará después en la trama 6
            else:
                media = cola_temps.mean()
                comprobar_umbral(media)
            anadir_punto(t, h, media)
            ultimo_dato_th = time.time()
            lbl_temp.config(text=f"Temperatura: {t:.1f} °C")
            lbl_hum.config(text=f"Humedad: {h:.1f} %")
            poner_alarma(lbl_alarma_th, False, "Sensor T/H")

        elif codigo == '2':                     # Distancias del radar
            d_front, d_tras, angulo, modo = P.parsear_distancia(campos)
            lbl_angulo.config(text=f"Ángulo: {angulo:.0f}°")
            lbl_modo.config(text="Modo: " + ["RASTREO", "MANUAL", "FIJO"][modo])
            if d_front == P.DIST_PARADA:
                lbl_dist.config(text="Distancia: parada")
            else:
                guardar_distancia(d_front, angulo)
                guardar_distancia(d_tras, P.angulo_trasero(angulo))
                lbl_dist.config(text=f"Distancia: {d_front:.0f} / {d_tras:.0f} cm")
                if d_front >= 0 or d_tras >= 0:
                    poner_alarma(lbl_alarma_dist, False, "Sensor distancia")

        elif codigo == '3':                     # Fallo del DHT11
            poner_alarma(lbl_alarma_th, True, "Sensor T/H")
            anadir_hueco()

        elif codigo == '5':                     # Fallo sensor de distancia
            poner_alarma(lbl_alarma_dist, True, "Sensor distancia")

        elif codigo == '6':                     # Media calculada en el satélite
            if media_en_satelite and medias and np.isnan(medias[-1]):
                media = float(campos[0])
                medias[-1] = media
                comprobar_umbral(media)

        elif codigo == '9':                     # Estado del enlace (Arduino tierra)
            poner_alarma(lbl_alarma_com, campos[0] == '1', "Comunicación")

    except (ValueError, IndexError):
        pass                                    # Trama mal formada: se ignora


# ================= Funciones de datos =================

def anadir_punto(t, h, media):
    """Añade un punto a la gráfica de temperatura (máximo MAX_PUNTOS).
    Parámetros: t, h, media (float). Resultado: ninguno."""
    tiempos.append(time.time() - t0)
    temps.append(t)
    hums.append(h)
    medias.append(media)
    if len(tiempos) > MAX_PUNTOS:
        del tiempos[0], temps[0], hums[0], medias[0]


def anadir_hueco():
    """Añade un punto vacío (NaN) para que la gráfica muestre un hueco.
    Parámetros: ninguno. Resultado: ninguno."""
    if temps and not np.isnan(temps[-1]):
        anadir_punto(np.nan, np.nan, np.nan)
        lbl_temp.config(text="Temperatura: --")
        lbl_hum.config(text="Humedad: --")


def guardar_distancia(d, direccion):
    """Guarda una distancia en el radar (si no hay eco, borra el punto).
    Parámetros: d (cm, negativo = sin eco), direccion (grados). Resultado: ninguno."""
    i = int(round(direccion)) % 360
    radar[i] = d if d >= 0 else np.nan
    radar_t[i] = time.time()


def comprobar_umbral(media):
    """Activa la alarma si hay 3 medias seguidas por encima del umbral.
    Parámetros: media (float). Resultado: ninguno."""
    global medias_altas
    if media > umbral:
        medias_altas += 1
    else:
        medias_altas = 0
    poner_alarma(lbl_alarma_temp, medias_altas >= 3, "Temperatura alta")


def poner_alarma(etiqueta, activa, nombre):
    """Pinta una alarma en rojo (activa) o verde (OK).
    Parámetros: etiqueta (tk.Label), activa (bool), nombre (str). Resultado: ninguno."""
    if activa:
        etiqueta.config(text=nombre + ": FALLO", bg='red')
    else:
        etiqueta.config(text=nombre + ": OK", bg='lightgreen')


def dibujar():
    """Actualiza las dos gráficas con los datos actuales.
    Parámetros: ninguno. Resultado: ninguno."""
    linea_t.set_data(tiempos, temps)
    linea_h.set_data(tiempos, hums)
    linea_m.set_data(tiempos, medias)
    ax_t.relim()
    ax_t.autoscale_view()
    canvas_t.draw_idle()

    rad = np.radians(angulo)
    puntero.set_data([rad, rad], [0, DIST_MAX])
    puntero_tras.set_data([rad + np.pi, rad + np.pi], [0, DIST_MAX])

    # Solo se dibujan los puntos medidos hace menos de MEMORIA_RADAR segundos
    recientes = ~np.isnan(radar) & (time.time() - radar_t < MEMORIA_RADAR)
    grados = np.where(recientes)[0]          # Ordenados de 0 a 359
    xs, ys = [], []
    for i in range(len(grados)):
        # Si hay un hueco grande entre dos puntos metemos un NaN para cortar la línea
        if i > 0 and grados[i] - grados[i - 1] > SALTO_MAX:
            xs.append(np.nan)
            ys.append(np.nan)
        xs.append(np.radians(grados[i]))
        ys.append(radar[grados[i]])
    linea_radar.set_data(xs, ys)
    canvas_r.draw_idle()


# ================= Funciones de los botones =================

def boton_th():
    """Para o reanuda el envío de temperatura/humedad.
    Parámetros: ninguno. Resultado: ninguno."""
    global th_activo, ultimo_dato_th
    th_activo = not th_activo
    enviar(P.cmd_toggle_th(th_activo))
    if th_activo:
        btn_th.config(text="Parar T/H")
        ultimo_dato_th = time.time()
    else:
        btn_th.config(text="Reanudar T/H")
        anadir_hueco()


def boton_dist():
    """Para o reanuda el envío de distancias.
    Parámetros: ninguno. Resultado: ninguno."""
    global dist_activo
    dist_activo = not dist_activo
    enviar(P.cmd_toggle_dist(dist_activo))
    btn_dist.config(text="Parar distancia" if dist_activo else "Reanudar distancia")


def boton_media():
    """Cambia dónde se calcula la media (tierra o satélite).
    Parámetros: ninguno. Resultado: ninguno."""
    global media_en_satelite
    media_en_satelite = not media_en_satelite
    enviar(P.cmd_origen_media(media_en_satelite))
    btn_media.config(text="Media en: SATÉLITE" if media_en_satelite else "Media en: TIERRA")


def boton_periodos():
    """Envía los nuevos periodos de T/H y distancia (en segundos).
    Parámetros: ninguno. Resultado: ninguno."""
    global periodo_th
    try:
        th = float(ent_per_th.get())
        dist = float(ent_per_dist.get())
    except ValueError:
        messagebox.showerror("Error", "Los periodos deben ser números")
        return
    if not (1 <= th <= 60 and 0.05 <= dist <= 5):
        messagebox.showerror("Error", "T/H: entre 1 y 60 s\nDistancia: entre 0.05 y 5 s")
        return
    periodo_th = th
    enviar(P.cmd_periodo_th(th * 1000))
    ventana.after(300, lambda: enviar(P.cmd_periodo_dist(dist * 1000)))


def boton_angulo():
    """Orienta el radar al ángulo escrito (0-359).
    Parámetros: ninguno. Resultado: ninguno."""
    try:
        ang = int(ent_angulo.get())
    except ValueError:
        messagebox.showerror("Error", "El ángulo debe ser un número entero")
        return
    if not 0 <= ang <= 359:
        messagebox.showerror("Error", "El ángulo debe estar entre 0 y 359")
        return
    enviar(P.cmd_angulo_fijo(ang))


def boton_rastreo():
    """Vuelve al modo rastreo (barrido automático).
    Parámetros: ninguno. Resultado: ninguno."""
    enviar(P.cmd_modo_auto())


def boton_umbral():
    """Cambia el umbral de la alarma de temperatura.
    Parámetros: ninguno. Resultado: ninguno."""
    global umbral, medias_altas
    try:
        umbral = float(ent_umbral.get())
    except ValueError:
        messagebox.showerror("Error", "El umbral debe ser un número")
        return
    medias_altas = 0
    poner_alarma(lbl_alarma_temp, False, "Temperatura alta")


def cerrar():
    """Cierra el puerto serie y la ventana.
    Parámetros: ninguno. Resultado: ninguno."""
    ser.close()
    ventana.destroy()


# ================= Programa principal =================

try:
    ser = serial.Serial(PUERTO, BAUDIOS, timeout=0.1)
except serial.SerialException:
    messagebox.showerror("Error", f"No se puede abrir el puerto {PUERTO}")
    raise SystemExit

ventana = tk.Tk()
ventana.title("Estación de tierra")
ventana.protocol("WM_DELETE_WINDOW", cerrar)

# --- Gráfica de temperatura, humedad y media ---
fig_t = Figure(figsize=(5, 3.5))
ax_t = fig_t.add_subplot(111)
ax_t.set_title("Temperatura y humedad")
ax_t.set_xlabel("Tiempo (s)")
linea_t, = ax_t.plot([], [], 'r-', label="Temperatura (°C)")
linea_h, = ax_t.plot([], [], 'b-', label="Humedad (%)")
linea_m, = ax_t.plot([], [], 'k--', label=f"Media últimas {N_MEDIA} (°C)")
ax_t.legend(loc='upper left')
canvas_t = FigureCanvasTkAgg(fig_t, master=ventana)
canvas_t.get_tk_widget().grid(row=0, column=0)

# --- Radar ---
fig_r = Figure(figsize=(4, 3.5))
ax_r = fig_r.add_subplot(111, projection='polar')
ax_r.set_title("Radar (cm)")
ax_r.set_ylim(0, DIST_MAX)
puntero, = ax_r.plot([], [], 'g-', lw=2)          # Sensor frontal
puntero_tras, = ax_r.plot([], [], 'g--', lw=1)    # Sensor trasero
linea_radar, = ax_r.plot([], [], 'o-', color='r', markersize=3)  # Objetos detectados
fig_r.tight_layout()
canvas_r = FigureCanvasTkAgg(fig_r, master=ventana)
canvas_r.get_tk_widget().grid(row=0, column=1)

# --- Datos actuales ---
f_datos = tk.LabelFrame(ventana, text="Datos")
f_datos.grid(row=1, column=0, columnspan=2, sticky='ew', padx=5)
lbl_temp = tk.Label(f_datos, text="Temperatura: --", width=20)
lbl_hum = tk.Label(f_datos, text="Humedad: --", width=16)
lbl_dist = tk.Label(f_datos, text="Distancia: --", width=24)
lbl_angulo = tk.Label(f_datos, text="Ángulo: --", width=12)
lbl_modo = tk.Label(f_datos, text="Modo: --", width=16)
for i, lbl in enumerate([lbl_temp, lbl_hum, lbl_dist, lbl_angulo, lbl_modo]):
    lbl.grid(row=0, column=i, padx=5, pady=3)

# --- Alarmas ---
f_alarmas = tk.LabelFrame(ventana, text="Alarmas")
f_alarmas.grid(row=2, column=0, columnspan=2, sticky='ew', padx=5)
lbl_alarma_th = tk.Label(f_alarmas, width=22)
lbl_alarma_dist = tk.Label(f_alarmas, width=22)
lbl_alarma_com = tk.Label(f_alarmas, width=22)
lbl_alarma_temp = tk.Label(f_alarmas, width=22)
for i, (lbl, nombre) in enumerate([(lbl_alarma_th, "Sensor T/H"),
                                   (lbl_alarma_dist, "Sensor distancia"),
                                   (lbl_alarma_com, "Comunicación"),
                                   (lbl_alarma_temp, "Temperatura alta")]):
    lbl.grid(row=0, column=i, padx=5, pady=3)
    poner_alarma(lbl, False, nombre)

# --- Controles ---
f_ctrl = tk.LabelFrame(ventana, text="Controles")
f_ctrl.grid(row=3, column=0, columnspan=2, sticky='ew', padx=5, pady=5)

btn_th = tk.Button(f_ctrl, text="Parar T/H", width=16, command=boton_th)
btn_th.grid(row=0, column=0, padx=5, pady=3)
btn_dist = tk.Button(f_ctrl, text="Parar distancia", width=16, command=boton_dist)
btn_dist.grid(row=0, column=1, padx=5)
btn_media = tk.Button(f_ctrl, text="Media en: TIERRA", width=16, command=boton_media)
btn_media.grid(row=0, column=2, padx=5)
tk.Button(f_ctrl, text="Modo rastreo", width=16, command=boton_rastreo).grid(row=0, column=3, padx=5)

tk.Label(f_ctrl, text="Periodo T/H (s):").grid(row=1, column=0, sticky='e')
ent_per_th = tk.Entry(f_ctrl, width=6)
ent_per_th.insert(0, "3")
ent_per_th.grid(row=1, column=1, sticky='w')
tk.Label(f_ctrl, text="Periodo distancia (s):").grid(row=1, column=2, sticky='e')
ent_per_dist = tk.Entry(f_ctrl, width=6)
ent_per_dist.insert(0, "0.1")
ent_per_dist.grid(row=1, column=3, sticky='w')
tk.Button(f_ctrl, text="Aplicar periodos", command=boton_periodos).grid(row=1, column=4, padx=5)

tk.Label(f_ctrl, text="Ángulo fijo (0-359):").grid(row=2, column=0, sticky='e')
ent_angulo = tk.Entry(f_ctrl, width=6)
ent_angulo.insert(0, "90")
ent_angulo.grid(row=2, column=1, sticky='w')
tk.Button(f_ctrl, text="Fijar ángulo", command=boton_angulo).grid(row=2, column=2, sticky='w')
tk.Label(f_ctrl, text="Umbral temperatura (°C):").grid(row=2, column=3, sticky='e')
ent_umbral = tk.Entry(f_ctrl, width=6)
ent_umbral.insert(0, "30")
ent_umbral.grid(row=2, column=4, sticky='w')
tk.Button(f_ctrl, text="Aplicar umbral", command=boton_umbral).grid(row=2, column=5, padx=5)


ventana.after(2000, leer_puerto)   # Esperamos a que el Arduino de tierra arranque
ventana.mainloop()