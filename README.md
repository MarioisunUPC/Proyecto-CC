# Misión Satélite \- Versión 1

Repositorio correspondiente a la **Versión 1** del proyecto de sistemas satelitales desarrollado en grupo.

## Demostración del Sistema

[Vídeo Versión 1 Grupo 12](https://youtu.be/vMR-2T-P4Nw)

## Funcionalidades Implementadas

El sistema cumple con todos los requisitos de la Versión 1:

1. **Captura de datos:** El Arduino Satélite captura los datos de humedad y temperatura del sensor DHT11.  
2. **Recepción y Graficado:** La estación de tierra recibe los datos empaquetados (`T:xx:H:xx`) y la interfaz en Python los muestra en una gráfica dinámica generada con Matplotlib.  
3. **Interfaz incrustada:** La gráfica está incrustada dentro de la interfaz gráfica principal usando `FigureCanvasTkAgg`.  
4. **Control de concurrencia:** El usuario puede enviar órdenes de parar y reanudar sin bloquear la interfaz, gracias al uso de `threading` en Python.  
5. **Alarma de Hardware:** El satélite incluye un sistema de timeout (5s) que detecta desconexiones del sensor DHT11 y avisa a tierra.  
6. **Alarma de Enlace:** La estación de tierra detecta la caída de comunicación (5s sin datos) y alerta mediante un LED rojo.  
7. **Diseño de Interfaz:** Interfaz estructurada en matriz con Tkinter.  
8. **Estructuración asíncrona:** El código no utiliza `delay()` en el bucle principal, basándose en la función `millis()`.

## Estructura del repositorio

- `/satelite`: Código para el Arduino Satélite (Mac).  
- `/tierra`: Código para el Arduino de la Estación de Tierra (Windows).  
- `/interfaz`: Código fuente en Python para la interfaz de usuario.  
