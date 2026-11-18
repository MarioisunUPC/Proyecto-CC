/*
 * ============================================================
 *  TEST UNITARIO - Joystick del Arduino de tierra
 * ============================================================
 *  Comprueba el joystick sin el resto del sistema. Usa el mismo pin y
 *  las mismas funciones (calibrarJoystick y leerDireccionJoystick) que
 *  tierra.ino. Abrir el Monitor Serie a 9600.
 *
 *  1. Al arrancar NO toques el joystick: se calibra el centro.
 *  2. Cada 200 ms se imprime la lectura, la desviación y la dirección.
 *  3. Comprueba:
 *     - En reposo: Dir 0.
 *     - Hacia un lado: Dir 1 (derecha); hacia el otro: Dir -1 (izquierda).
 *       Si salen al revés, poner JOY_INVERTIR = true en tierra.ino.
 *     - Histéresis: la dirección se activa con |desv| > 150 y solo vuelve
 *       a 0 cuando |desv| < 100, así no "tiembla" cerca del límite.
 * ============================================================
 */

const int PIN_JOY_X = A0;
const int JOY_ZONA_MUERTA = 150;
const int JOY_ZONA_SALIDA = 100;
const bool JOY_INVERTIR = false;

int dirJoystick = 0;
int joyCentro = 512;
int ultimaDesv = 0;

/* Copia de tierra.ino. Calcula la posición de reposo (media de 32 lecturas).
 * Parámetros: ninguno. Resultado: ninguno (actualiza joyCentro). */
void calibrarJoystick() {
  long suma = 0;
  for (int i = 0; i < 32; i++) {
    suma += analogRead(PIN_JOY_X);
    delay(5);
  }
  joyCentro = suma / 32;
}

/* Copia de tierra.ino (guarda además la desviación para imprimirla).
 * Parámetros: ninguno. Resultado: -1, 0 o 1. */
int leerDireccionJoystick() {
  long suma = 0;
  for (byte i = 0; i < 4; i++) suma += analogRead(PIN_JOY_X);
  int desv = (int)(suma / 4) - joyCentro;
  if (JOY_INVERTIR) desv = -desv;
  ultimaDesv = desv;

  if (dirJoystick == 0) {
    if (desv > JOY_ZONA_MUERTA)       dirJoystick = 1;
    else if (desv < -JOY_ZONA_MUERTA) dirJoystick = -1;
  } else {
    if (abs(desv) < JOY_ZONA_SALIDA) dirJoystick = 0;
    else dirJoystick = (desv > 0) ? 1 : -1;
  }
  return dirJoystick;
}

void setup() {
  Serial.begin(9600);
  Serial.println("Test del joystick. No lo toques: calibrando...");
  calibrarJoystick();
  Serial.print("Centro = ");
  Serial.println(joyCentro);
  if (joyCentro < 400 || joyCentro > 624)
    Serial.println("AVISO: centro muy desplazado. Revisa el cableado (VRx a A0, 5V y GND).");
  Serial.println("Mueve el joystick a izquierda y derecha.");
}

void loop() {
  int dir = leerDireccionJoystick();
  Serial.print("Lectura ");
  Serial.print(joyCentro + ultimaDesv);
  Serial.print("\tDesv ");
  Serial.print(ultimaDesv);
  Serial.print("\tDir ");
  Serial.print(dir);
  Serial.println(dir == 0 ? " (reposo)" : (dir == 1 ? " (derecha)" : " (izquierda)"));
  delay(200);
}
