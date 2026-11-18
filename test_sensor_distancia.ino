/*
 * ============================================================
 *  TEST UNITARIO - Sensores de distancia HC-SR04 + servo
 * ============================================================
 *  Comprueba, sin el resto del sistema, que los dos sensores de
 *  distancia y el servo funcionan. Abrir el Monitor Serie a 9600.
 *
 *  Fase 1: servo en 90º. Cada 300 ms imprime para cada sensor:
 *     OK <cm>      -> mide correctamente
 *     SIN ECO      -> responde pero no hay nada a menos de ~2 m
 *     NO RESPONDE  -> sensor desconectado o averiado
 *  Pon la mano a 10, 20 y 30 cm y comprueba los valores con una regla.
 *  Desconecta un sensor: debe aparecer NO RESPONDE.
 *
 *  Escribe 'b' en el Monitor Serie para hacer un barrido 0-180º
 *  (fase 2) imprimiendo ángulo y distancias; 'p' para volver a parar.
 *
 *  Usa los mismos pines y la misma función de medida que satelite.ino.
 * ============================================================
 */

const int PIN_TRIG  = 7,  PIN_ECHO  = 6;   // Frontal
const int PIN_TRIG2 = 12, PIN_ECHO2 = 13;  // Trasero
const int PIN_SERVO = 9;
const unsigned long ECHO_INICIO_US  = 5000;
const unsigned long ECHO_TIMEOUT_US = 12000;

bool barriendo = false;
int angulo = 90, dir = 1;

/* Igual que en satelite.ino: distancia en cm o -1; sensorOk = false si
 * el sensor no ha respondido. */
long medirDistanciaCm(int pinTrig, int pinEcho, bool &sensorOk) {
  sensorOk = true;
  if (digitalRead(pinEcho) == HIGH) return -1;
  digitalWrite(pinTrig, LOW);  delayMicroseconds(2);
  digitalWrite(pinTrig, HIGH); delayMicroseconds(10);
  digitalWrite(pinTrig, LOW);
  unsigned long t0 = micros();
  while (digitalRead(pinEcho) == LOW) {
    if (micros() - t0 > ECHO_INICIO_US) { sensorOk = false; return -1; }
  }
  unsigned long inicio = micros();
  while (digitalRead(pinEcho) == HIGH) {
    if (micros() - inicio > ECHO_TIMEOUT_US) return -1;
  }
  long cm = (long)((micros() - inicio) * 0.0343 / 2.0);
  if (cm < 2 || cm > 400) return -1;
  return cm;
}

/* Mueve el servo (Timer1, igual que en satelite.ino).
 * Parámetros: grados (0-180). Resultado: ninguno. */
void servoEscribir(int grados) {
  OCR1A = map(grados, 0, 180, 544, 2400) * 2;
}

/* Imprime el resultado de un sensor.
 * Parámetros: nombre (texto), cm (medida), ok (respondió). Resultado: ninguno. */
void imprimir(const char *nombre, long cm, bool ok) {
  Serial.print(nombre);
  if (!ok)          Serial.print("NO RESPONDE");
  else if (cm < 0)  Serial.print("SIN ECO");
  else { Serial.print("OK "); Serial.print(cm); Serial.print(" cm"); }
}

void setup() {
  Serial.begin(9600);
  pinMode(PIN_TRIG, OUTPUT);  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_TRIG2, OUTPUT); pinMode(PIN_ECHO2, INPUT);
  pinMode(PIN_SERVO, OUTPUT);
  TCCR1A = _BV(COM1A1) | _BV(WGM11);
  TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS11);
  ICR1 = 39999;
  servoEscribir(angulo);
  Serial.println("Test sensores de distancia. 'b' = barrido, 'p' = parar en 90.");
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'b') barriendo = true;
    if (c == 'p') { barriendo = false; angulo = 90; servoEscribir(angulo); }
  }

  if (barriendo) {
    angulo += dir * 5;
    if (angulo >= 180) { angulo = 180; dir = -1; }
    if (angulo <= 0)   { angulo = 0;   dir = 1; }
    servoEscribir(angulo);
    delay(150);                       // Tiempo para que el servo llegue
  } else {
    delay(300);
  }

  bool ok1, ok2;
  long d1 = medirDistanciaCm(PIN_TRIG, PIN_ECHO, ok1);
  long d2 = medirDistanciaCm(PIN_TRIG2, PIN_ECHO2, ok2);

  Serial.print("Ang "); Serial.print(angulo); Serial.print("\t");
  imprimir("Frontal: ", d1, ok1);
  Serial.print("\t");
  imprimir("Trasero: ", d2, ok2);
  Serial.println();
}
