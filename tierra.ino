/*
 * ============================================================
 *  ARDUINO DE TIERRA - Versión 2
 * ============================================================
 *  Hace de puente entre el satélite (SoftwareSerial) y el PC (Serial/USB):
 *  - Satélite -> PC: valida cada trama y la reenvía al PC. Enciende LEDs
 *    de recepción y de alarma.
 *  - PC -> Satélite: reenvía tal cual los comandos de la interfaz Python.
 *  - Lee el joystick y envía 2:<dir> al satélite (modo manual).
 *  - Si no llega nada del satélite en TIMEOUT_COM_MS envía 9:1 al PC
 *    (fallo de comunicación) y 9:0 cuando vuelve a recibir.
 *
 *  PROTOCOLO (ver también satelite.ino y protocolos.py)
 *  Satélite -> Tierra:
 *    1:<temp>:<hum>   2:<distF>:<distT>:<ang>:<modo>   3:   4:   5:   6:<media>
 *  Tierra (este Arduino) -> PC:
 *    9:<0|1>          Estado del enlace radio (1 = fallo, 0 = recuperado)
 *  Tierra -> Satélite:
 *    1:<ms> 2:<dir> 3: 4:<0|1> 5:<0|1> 6:<ms> 7:<ang> 8:<0|1>
 * ============================================================
 */

#include <SoftwareSerial.h>

const int PIN_RX              = 10;  // RX software (va al TX del satélite)
const int PIN_TX              = 11;  // TX software (va al RX del satélite)
const int PIN_JOY_X           = A0;
const int PIN_LED_RX          = 8;   // Parpadea al recibir datos
const int PIN_LED_ALARMA_TH   = 7;   // Fallo sensor temperatura/humedad
const int PIN_LED_ALARMA_DIST = 6;   // Fallo sensor de distancia
const int PIN_LED_ALARMA_COM  = 5;   // Fallo de comunicación

const long BAUDIOS = 9600;
const int  JOY_ZONA_MUERTA = 150;    // Desviación para empezar a mover
const int  JOY_ZONA_SALIDA = 100;    // Desviación para volver a reposo (histéresis)
const bool JOY_INVERTIR = false;
const unsigned long JOY_PERIODO_MS   = 100;
const unsigned long TIMEOUT_COM_MS   = 5000;
const unsigned long DURACION_LED_MS  = 50;

SoftwareSerial mySerial(PIN_RX, PIN_TX);

char bufSat[32];  byte idxSat = 0;   // Línea en construcción desde el satélite
char bufPc[32];   byte idxPc  = 0;   // Línea en construcción desde el PC

unsigned long ultimoMensajeSat = 0;
bool falloCom = false;

unsigned long ultimoJoy = 0;
int dirAnterior = 0;
int dirJoystick = 0;
int joyCentro = 512;

bool ledRxEncendido = false;
unsigned long tLedRx = 0;

/* Lee caracteres de un canal serie sin bloquear y va formando una línea.
 * Parámetros: canal (puerto), buf (buffer destino), idx (posición actual),
 *   tam (tamaño del buffer).
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

/* Enciende el LED de recepción (se apaga solo en gestionarLedRx).
 * Parámetros: ninguno. Resultado: ninguno. */
void parpadearLedRx() {
  digitalWrite(PIN_LED_RX, HIGH);
  ledRxEncendido = true;
  tLedRx = millis();
}

/* Apaga el LED de recepción cuando ha pasado DURACION_LED_MS.
 * Parámetros: ninguno. Resultado: ninguno. */
void gestionarLedRx() {
  if (ledRxEncendido && (millis() - tLedRx >= DURACION_LED_MS)) {
    digitalWrite(PIN_LED_RX, LOW);
    ledRxEncendido = false;
  }
}

/* Comprueba que una trama del satélite tiene el formato del protocolo.
 * Sirve para descartar líneas corrompidas en el enlace (SoftwareSerial
 * pierde bytes si recibe mientras transmite).
 * Parámetros: l (línea sin '\n').
 * Resultado: true si la trama es válida. */
bool lineaValida(const char *l) {
  int separadores;            // Nº de ':' que debe tener la trama
  bool sinDatos = false;      // Tramas de solo código ("3:", "4:", "5:")
  switch (l[0]) {
    case '1': separadores = 2; break;               // 1:t:h
    case '2': separadores = 4; break;               // 2:dF:dT:ang:modo
    case '6': separadores = 1; break;               // 6:media
    case '3': case '4': case '5': separadores = 1; sinDatos = true; break;
    default: return false;
  }
  if (l[1] != ':') return false;

  size_t len = strlen(l);
  if (sinDatos) return len == 2;

  int n = 0;
  for (const char *p = l + 1; *p; p++) {
    if (*p == ':') n++;
    else if (!((*p >= '0' && *p <= '9') || *p == '.' || *p == '-')) return false;
  }
  if (n != separadores) return false;
  if (l[len - 1] == ':' || strstr(l, "::") != NULL) return false;   // Campos vacíos

  if (l[0] == '2') {          // El modo es un único dígito 0, 1 o 2
    if (l[len - 2] != ':' || l[len - 1] < '0' || l[len - 1] > '2') return false;
  }
  return true;
}

/* Procesa una trama del satélite: la reenvía al PC, reinicia el control
 * de comunicación y actualiza los LEDs.
 * Parámetros: linea (trama recibida). Resultado: ninguno. */
void procesarLineaSatelite(char *linea) {
  if (!lineaValida(linea)) return;
  Serial.println(linea);
  ultimoMensajeSat = millis();

  if (falloCom) {
    falloCom = false;
    digitalWrite(PIN_LED_ALARMA_COM, LOW);
    Serial.println("9:0");
  }

  int codigo = atoi(linea);
  char *campo = strchr(linea, ':') + 1;   // Primer campo de datos

  switch (codigo) {
    case 1:                               // Datos T/H: el sensor funciona
      parpadearLedRx();
      digitalWrite(PIN_LED_ALARMA_TH, LOW);
      break;
    case 2: {                             // Distancias: si algún sensor mide, apaga alarma
      parpadearLedRx();
      long dF = atol(campo);
      long dT = atol(strchr(campo, ':') + 1);
      if (dF >= 0 || dT >= 0) digitalWrite(PIN_LED_ALARMA_DIST, LOW);
      break;
    }
    case 3:                               // Fallo DHT11
      digitalWrite(PIN_LED_ALARMA_TH, HIGH);
      break;
    case 5:                               // Fallo sensor distancia
      digitalWrite(PIN_LED_ALARMA_DIST, HIGH);
      break;
    case 6:                               // Media calculada en el satélite
      parpadearLedRx();
      break;
  }
}

/* Calcula la posición de reposo del joystick (media de 32 lecturas).
 * Parámetros: ninguno. Resultado: ninguno (actualiza joyCentro). */
void calibrarJoystick() {
  long suma = 0;
  for (int i = 0; i < 32; i++) {
    suma += analogRead(PIN_JOY_X);
    delay(5);
  }
  joyCentro = suma / 32;
}

/* Lee el eje X del joystick con histéresis.
 * Parámetros: ninguno. Resultado: -1 (izquierda), 0 (reposo) o 1 (derecha). */
int leerDireccionJoystick() {
  long suma = 0;
  for (byte i = 0; i < 4; i++) suma += analogRead(PIN_JOY_X);
  int desv = (int)(suma / 4) - joyCentro;
  if (JOY_INVERTIR) desv = -desv;

  if (dirJoystick == 0) {
    if (desv > JOY_ZONA_MUERTA)       dirJoystick = 1;
    else if (desv < -JOY_ZONA_MUERTA) dirJoystick = -1;
  } else {
    if (abs(desv) < JOY_ZONA_SALIDA) dirJoystick = 0;
    else dirJoystick = (desv > 0) ? 1 : -1;
  }
  return dirJoystick;
}

/* Cada JOY_PERIODO_MS envía 2:<dir> al satélite mientras se mueve el
 * joystick (y un 2:0 al soltarlo).
 * Parámetros: ninguno. Resultado: ninguno. */
void tareaJoystick() {
  if (millis() - ultimoJoy < JOY_PERIODO_MS) return;
  ultimoJoy = millis();

  int dir = leerDireccionJoystick();
  if (dir != 0 || dirAnterior != 0) {
    mySerial.print("2:");
    mySerial.println(dir);
  }
  dirAnterior = dir;
}

/* Activa la alarma de comunicación si el satélite lleva TIMEOUT_COM_MS
 * sin enviar nada (el latido llega cada 2 s).
 * Parámetros: ninguno. Resultado: ninguno. */
void tareaTimeoutCom() {
  if (!falloCom && (millis() - ultimoMensajeSat > TIMEOUT_COM_MS)) {
    falloCom = true;
    digitalWrite(PIN_LED_ALARMA_COM, HIGH);
    Serial.println("9:1");
  }
}

void setup() {
  pinMode(PIN_LED_RX, OUTPUT);
  pinMode(PIN_LED_ALARMA_TH, OUTPUT);
  pinMode(PIN_LED_ALARMA_DIST, OUTPUT);
  pinMode(PIN_LED_ALARMA_COM, OUTPUT);

  Serial.begin(BAUDIOS);
  mySerial.begin(BAUDIOS);
  calibrarJoystick();
  ultimoMensajeSat = millis();
}

void loop() {
  if (leerLinea(mySerial, bufSat, idxSat, sizeof(bufSat))) {
    procesarLineaSatelite(bufSat);
  }

  if (leerLinea(Serial, bufPc, idxPc, sizeof(bufPc))) {
    mySerial.println(bufPc);              // Comando de la interfaz -> satélite
  }

  tareaJoystick();
  tareaTimeoutCom();
  gestionarLedRx();
}
