# Versión 2 – Grupo 12

Proyecto de un satélite con Arduino: un Arduino **satélite** con sensores, un Arduino de **tierra** que hace de puente por radio/serie y una **interfaz en Python** para el usuario.

## Versiones

Cada versión entregada está marcada con un *tag*. Para ver el código exacto de una versión, elegidla en el desplegable de ramas/tags de GitHub o en la pestaña **Releases**.

| Versión | Tag | Vídeo | Novedades |
| --- | --- | --- | --- |
| Versión 2 (actual) | [`v2`](https://github.com/MarioisunUPC/Proyecto-CC/tree/v2) | [Vídeo Versión 2]([video2](https://youtu.be/rhLB5lbT1cw)) | Radar de 360º con dos HC-SR04 y servo, media de las 10 últimas temperaturas (en tierra o en el satélite), alarma de temperatura alta, cambio de periodos, joystick y alarma de fallo del sensor de distancia |
| Versión 1 | [`v1`](https://github.com/MarioisunUPC/Proyecto-CC/tree/v1) | [Vídeo Versión 1](https://youtu.be/vMR-2T-P4Nw) | Temperatura y humedad con DHT11, gráfica dinámica, parar/reanudar, alarmas de sensor y de comunicación |

## Estructura del repositorio

```
satelite/satelite.ino            Código del Arduino satélite
tierra/tierra.ino                Código del Arduino de tierra
interfaz/estacion_de_tierra.py   Interfaz gráfica (programa principal)
interfaz/protocolos.py           Protocolo de aplicación (formato de todos los mensajes)
interfaz/cola_circular.py        Cola circular para la media de temperaturas
tests/                           Tests unitarios
```

## Cómo ponerlo en marcha

1. Cargar `satelite/satelite.ino` en el Arduino satélite (necesita la librería **DHT sensor library** de Adafruit).
2. Cargar `tierra/tierra.ino` en el Arduino de tierra y conectarlo al PC por USB.
3. Instalar las librerías de Python: `pip install pyserial matplotlib numpy`.
4. Revisar el puerto en `PUERTO` al principio de `interfaz/estacion_de_tierra.py` (por defecto `COM7`).
5. Ejecutar `python interfaz/estacion_de_tierra.py`.

## Protocolo de aplicación

Cada mensaje es una línea de texto con campos separados por `:`; el primer campo es el código del mensaje. La descripción completa está al principio de `interfaz/protocolos.py` y de `satelite/satelite.ino`.

| Satélite → Tierra | Significado |
| --- | --- |
| `1:<temp>:<hum>` | Temperatura (ºC) y humedad (%) |
| `2:<distF>:<distT>:<ang>:<modo>` | Distancias de los dos sensores (cm), ángulo del servo y modo |
| `3:` / `5:` | Alarma: fallo del DHT11 / fallo de un sensor de distancia |
| `4:` | Latido |
| `6:<media>` | Media de las 10 últimas temperaturas calculada en el satélite |
| `9:<0\|1>` | Estado del enlace (lo genera el Arduino de tierra) |

| Tierra → Satélite | Significado |
| --- | --- |
| `1:<ms>` / `6:<ms>` | Periodo de envío de T/H / de distancia |
| `2:<dir>` | Joystick (-1, 0, 1) |
| `3:` | Volver a modo rastreo |
| `4:<0\|1>` / `5:<0\|1>` | Parar o reanudar el envío de T/H / de distancia |
| `7:<ang>` | Orientar el radar a un ángulo fijo (0-359) |
| `8:<0\|1>` | Dónde se calcula la media: 0 = tierra, 1 = satélite |

## Tests unitarios

- **Python** (desde la raíz del repositorio): `python -m unittest discover tests`
  - `test_cola_circular.py`: cola circular de la interfaz.
  - `test_protocolos.py`: creación y lectura de los mensajes del protocolo.
- **Arduino** (abrir el sketch y mirar el Monitor Serie a 9600):
  - `test_sensor_distancia`: los dos HC-SR04 y el servo.
  - `test_joystick`: calibración y dirección del joystick.
  - `test_validacion_tramas`: validación de tramas en el Arduino de tierra (no necesita sensores).
  - `test_media_satelite`: media de temperaturas del satélite (no necesita sensores).
## Uso de IA
Hemos usado la IA como apoyo en la versión 2. Nos ayudó a:
- Reorganizar y estructurar el código (separar protocolos.py y cola_circular.py, comentar el protocolo y documentar cada función).
- Detectar y corregir errores (p. ej. pérdida de comandos por SoftwareSerial, validación de tramas en el Arduino de tierra).
- Configurar el servo (Timer1) y el joystick (calibración y zona muerta con histéresis).
- Diseñar las gráficas de la interfaz (temperatura/humedad/media y radar polar).
- Explicarnos construcciones como el operador ternario (`a if cond else b`; en C `cond ? a : b`).
Hemos revisado y probado todo el código y sabemos explicarlo.
