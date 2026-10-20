import serial
import threading
import time
from tkinter import *
from matplotlib.figure import Figure
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

device = 'COM5'
mySerial = serial.Serial(device, 9600)

recibiendo = False
datos_temperatura = []
muestras = []
contador = 0
ultimo_tiempo_recepcion = time.time()


def AClick ():        #boton iniciar
    global recibiendo, ultimo_tiempo_recepcion

    if not recibiendo:
        recibiendo = True
        ultimo_tiempo_recepcion = time.time()
        threadRecepcion = threading.Thread(target=recepcion)
        threadRecepcion.daemon = True
        threadRecepcion.start()
        verificar_timeout()
    



def BClick ():        #boton parar
    mySerial.write("Parar\n".encode('utf-8'))
    print("Comando: Parar")


def CClick ():        #boton reanudar
    mySerial.write("Reanudar\n".encode('utf-8'))
    print("Comando: Reanudar")


def verificar_timeout():
    global ultimo_tiempo_recepcion, recibiendo

    if recibiendo:
        tiempo_actual = time.time()
        if tiempo_actual - ultimo_tiempo_recepcion > 5.0:
            mySerial.write("Alarma\n".encode('utf-8'))
            print("Comando: Alarma")
            ultimo_tiempo_recepcion = tiempo_actual

        window.after(1000, verificar_timeout)




def recepcion():
    global contador, recibiendo, ultimo_tiempo_recepcion

    while recibiendo:
        if mySerial.in_waiting > 0:
            linea = mySerial.readline().decode('utf-8').rstrip()
            print("Entrada:", linea)
            
            ultimo_tiempo_recepcion = time.time()
            
            if linea.startswith("T:"):
                trozos = linea.split(':')
                temp_num = float(trozos[1])
            
                contador += 1
                muestras.append(contador)
                datos_temperatura.append(temp_num)
            
                ax.clear()
                ax.plot(muestras, datos_temperatura, 'r')
                ax.set_title("Evolución de la Temperatura (ºC)")
                ax.set_xlabel("Muestra Nº")
                ax.set_ylabel("Temperatura (ºC)")
                ax.grid(False)
                canvas.draw()




window = Tk()
window.title("v1")
window.geometry("800x400")
window.columnconfigure(0, weight=1)
window.columnconfigure(1, weight=3)
window.rowconfigure(0, weight=1)
window.rowconfigure(1, weight=1)
window.rowconfigure(2, weight=1)


fig = Figure(figsize=(5, 4), dpi=100)
ax = fig.add_subplot(111)
ax.set_title("Registro de Temperatura (ºC)")
ax.grid(False)

canvas = FigureCanvasTkAgg(fig, master=window)
canvas.get_tk_widget().grid(row=0, column=1, rowspan=3, sticky=N+S+E+W, padx=10, pady=10)


AButton = Button(window, text="Iniciar", bg='red', fg="white",command=AClick)
AButton.grid(row=0, column=0, padx=5, pady=5, sticky=N + S + E + W)

BButton = Button(window, text="Parar", bg='yellow', fg="black",command=BClick)
BButton.grid(row=1, column=0, padx=5, pady=5, sticky=N + S + E + W)

CButton = Button(window, text="Reanudar", bg='blue', fg="white",command=CClick)
CButton.grid(row=2, column=0, padx=5, pady=5, sticky=N + S + E + W)


window.mainloop()
