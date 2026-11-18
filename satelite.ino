/*
 * ============================================================
 *  ARDUINO SATÉLITE - Versión 2
 * ============================================================
 *  - Lee temperatura/humedad (DHT11) y la envía periódicamente.
 *  - Calcula (si se le pide) la media de las 10 últimas temperaturas.
 *  - Radar de 360º: dos HC-SR04 montados espalda con espalda sobre
 *    un servo de 180º (frontal = ángulo del servo, trasero = +180º).
 *  - Detecta fallos del DHT11 y de los sensores de distancia.
 *
 *  PROTOCOLO DE APLICACIÓN  (líneas de texto terminadas en '\n',
 *  campos separados por ':'; el primer campo es el código)
 *
 *  Satélite -> Tierra
 *    1:<temp>:<hum>                 Temperatura (ºC) y humedad (%)
 *    2:<distF>:<distT>:<ang>:<modo> Distancias (cm) del sensor frontal y
 *                                   trasero. -1 = sin eco (nada a la vista),
 *                                   -2 = envío de distancia parado.
 *                                   ang = ángulo del servo (0-180); el
 *                                   sensor trasero apunta a ang+180.
 *                                   modo: 0 = rastreo, 1 = manual, 2 = fijo
 *    3:                             Alarma: fallo del sensor DHT11
 *    4:                             Latido (el satélite sigue vivo)
 *    5:                             Alarma: fallo de un sensor de distancia
 *    6:<media>                      Media de las 10 últimas temperaturas
 *                                   (solo si la media se calcula aquí)
 *
 *  Tierra -> Satélite
 *    1:<ms>     Periodo de envío de T/H (1000 - 60000 ms)
 *    2:<dir>    Joystick: -1 izquierda, 0 parado, 1 derecha (modo manual)
 *    3:         Volver al modo rastreo (barrido automático)
 *    4:<0|1>    Parar (0) / reanudar (1) el envío de T/H
 *    5:<0|1>    Parar (0) / reanudar (1) el envío de distancia
 *    6:<ms>     Periodo de envío de distancia (50 - 5000 ms)
 *    7:<ang>    Orientar el radar a un ángulo fijo (0 - 359º)
 *    8:<0|1>    Dónde se calcula la media: 0 = tierra, 1 = satélite
 *
 *  Todos los comandos Tierra->Satélite fijan un ESTADO (no son "toggles"),
 *  así que repetirlos no tiene efectos secundarios. La interfaz los envía
 *  dos veces para no perderlos si llegan mientras este Arduino transmite
 *  (SoftwareSerial no puede recibir y transmitir a la vez).
 * ============================================================
 */

#include <DHT.h>

#define ENLACE_HARDWARE 0  // 1 = usar Serial (pines 0/1), 0 = SoftwareSerial
const int PIN_RX = 10;     // RX software (va al TX del Arduino de tierra)
const int PIN_TX = 11;     // TX software (va al RX del Arduino de tierra)

#if ENLACE_HARDWARE
  #define enlace Serial
#else
  #include <SoftwareSerial.h>
  SoftwareSerial enlace(PIN_RX, PIN_TX);
#endif

// ----------------------------- PINES -----------------------------
const int PIN_DHT    = 2;
const int PIN_TRIG   = 7;   // Sensor frontal
const int PIN_ECHO   = 6;
const int PIN_TRIG2  = 12;  // Sensor trasero
const int PIN_ECHO2  = 13;
const int PIN_SERVO  = 9;   // OC1A: el servo se controla con el Timer1
const int PIN_LED_TX = 8;

#define DHTTYPE DHT11
const long BAUDIOS = 9600;

// ------------- PERIODOS (modificables desde tierra) --------------
unsigned long periodoTH_MS   = 3000;
unsigned long periodoDist_MS = 100;
const unsigned long PERIODO_TH_MIN   = 1000,  PERIODO_TH_MAX   = 60000;
const unsigned long PERIODO_DIST_MIN = 50,    PERIODO_DIST_MAX = 5000;

const unsigned long PERIODO_SERVO_MS   = 30;
const unsigned long PERIODO_LATIDO_MS  = 2000;
const unsigned long TIMEOUT_FALLO_MS   = 5000;   // Tiempo leyendo mal el DHT para dar alarma
const unsigned long TIMEOUT_JOY_MS     = 500;    // Sin órdenes del joystick -> se para el servo
const unsigned long DURACION_LED_MS    = 30;
const unsigned long ECHO_INICIO_US     = 5000;   // Máx. espera a que ECHO suba (si no, sensor KO)
const unsigned long ECHO_TIMEOUT_US    = 12000;  // Máx. duración del eco (~2 m)
const int NUM_FALLOS_DIST = 20;                  // Medidas seguidas sin respuesta -> alarma

// ------------------------ CONFIGURACIÓN SERVO ---------------------
const int PASO_SERVO      = 1;
const int ANGULO_MIN      = 0;
const int ANGULO_MAX      = 180;
const int SENTIDO_DERECHA = -1;
const int SERVO_US_MIN    = 544;
const int SERVO_US_MAX    = 2400;

const int N_MEDIA = 10;     // Nº de temperaturas para la media

DHT dht(PIN_DHT, DHTTYPE);

// ---------------------- ESTADO DEL SATÉLITE -----------------------
int  angulo      = 90;      // Ángulo actual del servo (0-180)
bool modoManual  = false;   // Joystick
bool modoFijo    = false;   // Ángulo fijado desde la interfaz
int  dirBarrido  = 1;
int  dirJoystick = 0;

bool enviarTH     = true;
bool enviarDist   = true;
bool calcMediaSat = false;

// Cola circular con las últimas temperaturas (media en el satélite)
float temps[N_MEDIA];
int idxTemp = 0;
int conteoTemps = 0;

unsigned long ultimoServo = 0, ultimoDist = 0, ultimoTH = 0, ultimoLatido = 0, ultimoJoy = 0;

bool leyendoMalTH = false;
unsigned long inicioFalloTH = 0;
bool falloTH = false;
int  fallosDist1 = 0, fallosDist2 = 0;   // Medidas seguidas sin respuesta de cada sensor
bool falloDist = false;

bool ledEncendido = false;
unsigned long tLedTx = 0;

char bufferRx[32];
byte idxRx = 0;

// =================================================================
//  UTILIDADES
// =================================================================

/* Enciende el LED de transmisión (se apaga solo en gestionarLedTx).
 * Parámetros: ninguno. Resultado: ninguno. */
void encenderLedTx() {
  digitalWrite(PIN_LED_TX, HIGH);
  ledEncendido = true;
  tLedTx = millis();
}

/* Apaga el LED de transmisión cuando ha pasado DURACION_LED_MS.
 * Parámetros: ninguno. Resultado: ninguno. */
void gestionarLedTx() {
  if (ledEncendido && (millis() - tLedTx >= DURACION_LED_MS)) {
    digitalWrite(PIN_LED_TX, LOW);
    ledEncendido = false;
  }
}

/* Lee caracteres de un canal serie sin bloquear y va formando una línea.
 * Parámetros: canal (puerto), buf (buffer destino), idx (posición actual,
 *   se conserva entre llamadas), tam (tamaño del buffer).
 * Resultado: true cuando hay una línea completa (no vacía) en buf. */
bool leerLinea(Stream &canal, char *buf, byte &idx, byte tam) {
  while (canal.available()) {
    char c = canal.read();
    if (c == '\n') {
      buf[idx] = 0;
      bool hayLinea = (idx > 0);
      idx = 0;
      if (hayLinea) return true;
    } else if (c != '\r') {
      if (idx < tam - 1) buf[idx++] = c;
      else idx = 0;                       // Línea demasiado larga: se descarta
    }
  }
  return false;
}

// =================================================================
//  SENSORES DE DISTANCIA (HC-SR04)
// =================================================================

/* Mide la distancia con un HC-SR04.
 * Distingue "no hay obstáculo" (el sensor responde pero el eco no vuelve)
 * de "sensor averiado/desconectado" (el pin ECHO nunca se pone a HIGH).
 * Parámetros: pinTrig, pinEcho (pines del sensor); sensorOk (salida: false
 *   si el sensor no ha respondido).
 * Resultado: distancia en cm, o -1 si no hay eco válido. */
long medirDistanciaCm(int pinTrig, int pinEcho, bool &sensorOk) {
  sensorOk = true;
  if (digitalRead(pinEcho) == HIGH) return -1;   // Aún ocupado con la medida anterior

  digitalWrite(pinTrig, LOW);
  delayMicroseconds(2);
  digitalWrite(pinTrig, HIGH);
  delayMicroseconds(10);
  digitalWrite(pinTrig, LOW);

  // 1) Esperar a que el sensor levante ECHO (lo hace tras emitir el pulso)
  unsigned long t0 = micros();
  while (digitalRead(pinEcho) == LOW) {
    if (micros() - t0 > ECHO_INICIO_US) {
      sensorOk = false;                          // No responde: desconectado o roto
      return -1;
    }
  }
  // 2) Medir cuánto tiempo está ECHO a HIGH (ida y vuelta del sonido)
  unsigned long inicio = micros();
  while (digitalRead(pinEcho) == HIGH) {
    if (micros() - inicio > ECHO_TIMEOUT_US) return -1;  // Sin eco: nada cerca
  }
  unsigned long duracion = micros() - inicio;
  long cm = (long)(duracion * 0.0343 / 2.0);
  if (cm < 2 || cm > 400) return -1;
  return cm;
}

/* Actualiza el contador de fallos de un sensor de distancia.
 * Parámetros: ok (el sensor ha respondido), contador (fallos seguidos).
 * Resultado: ninguno (modifica contador). */
void contarFallo(bool ok, int &contador) {
  if (ok) contador = 0;
  else if (contador < NUM_FALLOS_DIST) contador++;
}

// =================================================================
//  SERVO (Timer1 en modo PWM, 50 Hz)
// =================================================================

/* Mueve el servo al ángulo indicado.
 * Parámetros: grados (0-180). Resultado: ninguno. */
void servoEscribir(int grados) {
  long us = map(grados, 0, 180, SERVO_US_MIN, SERVO_US_MAX);
  OCR1A = us * 2;                                  // Tick de 0,5 us (prescaler 8)
}

/* Configura el Timer1 para generar la señal del servo y lo coloca.
 * Parámetros: grados (ángulo inicial). Resultado: ninguno. */
void servoIniciar(int grados) {
  pinMode(PIN_SERVO, OUTPUT);
  TCCR1A = _BV(COM1A1) | _BV(WGM11);               // Fast PWM, TOP = ICR1
  TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS11);    // Prescaler 8
  ICR1 = 39999;                                    // 20 ms
  servoEscribir(grados);
}

/* Orienta el radar hacia una dirección de 0 a 359º.
 * De 0 a 180 apunta el sensor frontal; de 181 a 359 se gira el servo
 * a (ang - 180) para que sea el sensor trasero el que apunte.
 * Parámetros: ang (grados 0-359). Resultado: ninguno. */
void fijarAngulo(long ang) {
  ang = ((ang % 360) + 360) % 360;
  angulo = (ang <= 180) ? ang : ang - 180;
  modoFijo = true;
  modoManual = false;
  dirJoystick = 0;
  servoEscribir(angulo);
}

// =================================================================
//  RECEPCIÓN DE COMANDOS
// =================================================================

/* Interpreta un comando recibido de tierra (ver protocolo arriba).
 * Parámetros: cmd (línea recibida, p. ej. "6:200").
 * Resultado: ninguno (modifica el estado del satélite). */
void procesarComando(char *cmd) {
  char *sep = strchr(cmd, ':');
  if (sep == NULL || sep == cmd) return;          // Sin código: se ignora
  int codigo = atoi(cmd);
  bool hayValor = (sep[1] != '\0');
  long valor = atol(sep + 1);

  switch (codigo) {
    case 1:   // Periodo T/H
      if (hayValor && valor >= (long)PERIODO_TH_MIN && valor <= (long)PERIODO_TH_MAX)
        periodoTH_MS = valor;
      break;

    case 2:   // Joystick
      if (valor == 1 || valor == -1) {
        dirJoystick = valor;
        modoFijo = false;
        modoManual = true;
        ultimoJoy = millis();
      } else {
        dirJoystick = 0;
      }
      break;

    case 3:   // Modo rastreo
      modoManual = false;
      modoFijo = false;
      dirJoystick = 0;
      break;

    case 4:   // Parar / reanudar T/H
      if (hayValor) enviarTH = (valor != 0);
      break;

    case 5:   // Parar / reanudar distancia
      if (hayValor) {
        enviarDist = (valor != 0);
        fallosDist1 = fallosDist2 = 0;            // No arrastrar alarmas antiguas
        falloDist = false;
      }
      break;

    case 6:   // Periodo distancia
      if (hayValor && valor >= (long)PERIODO_DIST_MIN && valor <= (long)PERIODO_DIST_MAX)
        periodoDist_MS = valor;
      break;

    case 7:   // Ángulo fijo
      if (hayValor) fijarAngulo(valor);
      break;

    case 8:   // Dónde se calcula la media
      if (hayValor) calcMediaSat = (valor != 0);
      break;
  }
}

/* Comprueba si ha llegado un comando completo y lo procesa.
 * Parámetros: ninguno. Resultado: ninguno. */
void tareaRecepcion() {
  if (leerLinea(enlace, bufferRx, idxRx, sizeof(bufferRx))) {
    procesarComando(bufferRx);
  }
}

// =================================================================
//  TAREAS PERIÓDICAS
// =================================================================

/* Mueve el servo según el modo: fijo (no se mueve), manual (joystick)
 * o rastreo (barrido continuo 0-180-0).
 * Parámetros: ninguno. Resultado: ninguno. */
void tareaServo() {
  if (millis() - ultimoServo < PERIODO_SERVO_MS) return;
  ultimoServo = millis();

  if (modoFijo) {
    return;                                       // Ángulo bloqueado desde la interfaz
  } else if (modoManual) {
    if (dirJoystick != 0 && (millis() - ultimoJoy > TIMEOUT_JOY_MS)) dirJoystick = 0;
    angulo += dirJoystick * SENTIDO_DERECHA * PASO_SERVO;
  } else {
    angulo += dirBarrido * PASO_SERVO;
    if (angulo >= ANGULO_MAX)      { angulo = ANGULO_MAX; dirBarrido = -1; }
    else if (angulo <= ANGULO_MIN) { angulo = ANGULO_MIN; dirBarrido = 1; }
  }
  angulo = constrain(angulo, ANGULO_MIN, ANGULO_MAX);
  servoEscribir(angulo);
}

/* Mide con los dos sensores y envía la trama 2:<distF>:<distT>:<ang>:<modo>.
 * Si el envío está parado manda distancias -2 (para que tierra siga
 * viendo el ángulo y el modo). Actualiza la detección de fallos.
 * Parámetros: ninguno. Resultado: ninguno. */
void tareaDistancia() {
  if (millis() - ultimoDist < periodoDist_MS) return;
  ultimoDist = millis();

  int modo = modoFijo ? 2 : (modoManual ? 1 : 0);
  long d1 = -2, d2 = -2;

  if (enviarDist) {
    bool ok1, ok2;
    d1 = medirDistanciaCm(PIN_TRIG, PIN_ECHO, ok1);
    d2 = medirDistanciaCm(PIN_TRIG2, PIN_ECHO2, ok2);
    contarFallo(ok1, fallosDist1);
    contarFallo(ok2, fallosDist2);
    falloDist = (fallosDist1 >= NUM_FALLOS_DIST) || (fallosDist2 >= NUM_FALLOS_DIST);
  }

  enlace.print("2:");
  enlace.print(d1);
  enlace.print(':');
  enlace.print(d2);
  enlace.print(':');
  enlace.print(angulo);
  enlace.print(':');
  enlace.println(modo);
  encenderLedTx();
}

/* Guarda una temperatura en la cola circular del satélite.
 * Parámetros: t (temperatura en ºC). Resultado: ninguno. */
void guardarTemperatura(float t) {
  temps[idxTemp] = t;
  idxTemp = (idxTemp + 1) % N_MEDIA;
  if (conteoTemps < N_MEDIA) conteoTemps++;
}

/* Calcula la media de las temperaturas guardadas.
 * Parámetros: ninguno. Resultado: media en ºC (0 si no hay datos). */
float mediaTemperaturas() {
  if (conteoTemps == 0) return 0;
  float suma = 0;
  for (int i = 0; i < conteoTemps; i++) suma += temps[i];
  return suma / conteoTemps;
}

/* Lee el DHT11, detecta fallos y envía 1:<t>:<h> (y 6:<media> si la
 * media se calcula en el satélite).
 * Parámetros: ninguno. Resultado: ninguno. */
void tareaTemperatura() {
  if (millis() - ultimoTH < periodoTH_MS) return;
  ultimoTH = millis();

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    if (!leyendoMalTH) {
      leyendoMalTH = true;
      inicioFalloTH = millis();
    }
    if (millis() - inicioFalloTH >= TIMEOUT_FALLO_MS) falloTH = true;
    return;
  }

  leyendoMalTH = false;
  falloTH = false;
  guardarTemperatura(t);

  if (!enviarTH) return;

  enlace.print("1:"); enlace.print(t, 1); enlace.print(':'); enlace.println(h, 1);
  if (calcMediaSat) {
    enlace.print("6:");
    enlace.println(mediaTemperaturas(), 2);
  }
  encenderLedTx();
}

/* Envía el latido y, si procede, las alarmas de fallo de sensores.
 * Parámetros: ninguno. Resultado: ninguno. */
void tareaLatido() {
  if (millis() - ultimoLatido < PERIODO_LATIDO_MS) return;
  ultimoLatido = millis();

  if (falloTH)   enlace.println("3:");
  if (falloDist) enlace.println("5:");
  enlace.println("4:");
}

// =================================================================

void setup() {
  pinMode(PIN_LED_TX, OUTPUT);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_TRIG2, OUTPUT);
  pinMode(PIN_ECHO2, INPUT);

  enlace.begin(BAUDIOS);
  dht.begin();
  servoIniciar(angulo);
}

void loop() {
  tareaRecepcion();
  tareaServo();
  tareaDistancia();
  tareaTemperatura();
  tareaLatido();
  gestionarLedTx();
}
