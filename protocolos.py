"""
Protocolo de aplicación entre el satélite y la estación de tierra.

Cada mensaje es una línea de texto terminada en '\\n'. Los campos se separan
con ':' y el primero es siempre el código que indica el tipo de mensaje.

Tramas Satélite -> Tierra:
  1:<temp>:<hum>                  -> Temperatura (°C) y humedad (%)
  2:<distF>:<distT>:<ang>:<modo>  -> Distancias (cm) del sensor frontal y trasero.
                                     -1 = sin eco, -2 = envío de distancia parado.
                                     ang = ángulo del servo (0-180); el sensor
                                     trasero apunta a ang + 180.
                                     modo: 0 = rastreo, 1 = manual, 2 = fijo
  3:                              -> Alarma: fallo del sensor DHT11
  4:                              -> Latido (el satélite sigue vivo)
  5:                              -> Alarma: fallo de un sensor de distancia
  6:<media>                       -> Media de las 10 últimas temperaturas (calculada
                                     en el satélite; se envía justo después del 1:)
  9:<0|1>                         -> Estado del enlace (lo genera el Arduino de tierra):
                                     1 = fallo de comunicación, 0 = recuperado

Tramas Tierra -> Satélite:
  1:<ms>    -> Periodo de envío de T/H en milisegundos (1000-60000)
  2:<dir>   -> Joystick: -1, 0, 1 (lo envía el Arduino de tierra)
  3:        -> Volver a modo rastreo (barrido automático)
  4:<0|1>   -> Parar (0) o reanudar (1) el envío de T/H
  5:<0|1>   -> Parar (0) o reanudar (1) el envío de distancia
  6:<ms>    -> Periodo de envío de distancia en milisegundos (50-5000)
  7:<ang>   -> Orientar el radar hacia un ángulo fijo (0-359)
  8:<0|1>   -> Dónde se calcula la media: 0 = tierra, 1 = satélite

Todos los comandos fijan un estado (no son "toggles"): enviarlos dos veces
no cambia el resultado, lo que permite repetirlos por si se pierde alguno.
"""

# Códigos de las tramas Satélite -> Tierra
TH = '1'
DISTANCIA = '2'
FALLO_TH = '3'
LATIDO = '4'
FALLO_DIST = '5'
MEDIA_SAT = '6'
ESTADO_ENLACE = '9'

# Valores especiales de distancia
SIN_ECO = -1
DIST_PARADA = -2

# Modos del radar
MODO_RASTREO, MODO_MANUAL, MODO_FIJO = 0, 1, 2


# ----------------------------- Tierra -> Satélite -----------------------------

def cmd_periodo_th(periodo_ms):
    """Genera el comando para cambiar el periodo de T/H.
    Parámetros: periodo_ms (int) en milisegundos. Resultado: str."""
    return f"1:{int(periodo_ms)}"


def cmd_joystick(direccion):
    """Genera el comando de movimiento manual (-1, 0, 1).
    Parámetros: direccion (int). Resultado: str."""
    return f"2:{int(direccion)}"


def cmd_modo_auto():
    """Genera el comando para activar el barrido automático.
    Parámetros: ninguno. Resultado: str."""
    return "3:"


def cmd_toggle_th(activo):
    """Genera el comando para parar o reanudar el envío de T/H.
    Parámetros: activo (bool). Resultado: str."""
    return f"4:{1 if activo else 0}"


def cmd_toggle_dist(activo):
    """Genera el comando para parar o reanudar el envío de distancia.
    Parámetros: activo (bool). Resultado: str."""
    return f"5:{1 if activo else 0}"


def cmd_periodo_dist(periodo_ms):
    """Genera el comando para cambiar el periodo de distancia.
    Parámetros: periodo_ms (int) en milisegundos. Resultado: str."""
    return f"6:{int(periodo_ms)}"


def cmd_angulo_fijo(angulo):
    """Genera el comando para orientar el radar a un ángulo fijo (0-359).
    Parámetros: angulo (int/float, se normaliza a 0-359). Resultado: str."""
    return f"7:{int(round(angulo)) % 360}"


def cmd_origen_media(en_satelite):
    """Genera el comando para elegir dónde se calcula la media de temperatura.
    Parámetros: en_satelite (bool). Resultado: str."""
    return f"8:{1 if en_satelite else 0}"


# ----------------------------- Satélite -> Tierra -----------------------------

def parsear_trama(linea):
    """Trocea una línea recibida por el puerto serie.
    Parámetros: linea (str).
    Resultado: tupla (codigo_str, lista_campos_str). ("", []) si está vacía."""
    partes = linea.strip().split(':')
    if not partes or not partes[0]:
        return "", []
    return partes[0], partes[1:]


def parsear_th(campos):
    """Interpreta los campos de una trama 1:<temp>:<hum>.
    Parámetros: campos (lista de str).
    Resultado: tupla (temperatura, humedad) en float.
    Lanza ValueError/IndexError si la trama está mal formada."""
    return float(campos[0]), float(campos[1])


def parsear_distancia(campos):
    """Interpreta los campos de una trama 2:<distF>:<distT>:<ang>:<modo>.
    Parámetros: campos (lista de str).
    Resultado: tupla (dist_frontal, dist_trasera, angulo_servo, modo).
    Lanza ValueError/IndexError si la trama está mal formada."""
    if len(campos) != 4:
        raise ValueError("La trama de distancia debe tener 4 campos")
    modo = int(campos[3])
    if modo not in (MODO_RASTREO, MODO_MANUAL, MODO_FIJO):
        raise ValueError(f"Modo desconocido: {modo}")
    return float(campos[0]), float(campos[1]), float(campos[2]), modo


def angulo_trasero(angulo_servo):
    """Dirección a la que apunta el sensor trasero.
    Parámetros: angulo_servo (grados 0-180). Resultado: grados 0-359."""
    return (angulo_servo + 180) % 360
