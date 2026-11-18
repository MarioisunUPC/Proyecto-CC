/*
 * ============================================================
 *  TEST UNITARIO - Media de temperaturas calculada en el satélite
 * ============================================================
 *  Comprueba la cola circular del satélite (guardarTemperatura y
 *  mediaTemperaturas, copiadas tal cual de satelite.ino):
 *    - Sin datos la media es 0.
 *    - Con menos de 10 valores, la media es la de los que hay.
 *    - Con más de 10, solo cuentan los 10 últimos.
 *  Son los mismos casos que test_cola_circular.py, para comprobar que
 *  tierra y satélite calculan lo mismo.
 *
 *  No necesita sensores: cargarlo en cualquier Arduino y abrir el
 *  Monitor Serie a 9600.
 * ============================================================
 */

const int N_MEDIA = 10;
float temps[N_MEDIA];
int idxTemp = 0;
int conteoTemps = 0;

/* Copia de satelite.ino. Guarda una temperatura en la cola circular.
 * Parámetros: t (ºC). Resultado: ninguno. */
void guardarTemperatura(float t) {
  temps[idxTemp] = t;
  idxTemp = (idxTemp + 1) % N_MEDIA;
  if (conteoTemps < N_MEDIA) conteoTemps++;
}

/* Copia de satelite.ino. Media de las temperaturas guardadas.
 * Parámetros: ninguno. Resultado: media en ºC (0 si no hay datos). */
float mediaTemperaturas() {
  if (conteoTemps == 0) return 0;
  float suma = 0;
  for (int i = 0; i < conteoTemps; i++) suma += temps[i];
  return suma / conteoTemps;
}

/* Vacía la cola para empezar un caso nuevo.
 * Parámetros: ninguno. Resultado: ninguno. */
void vaciar() {
  idxTemp = 0;
  conteoTemps = 0;
}

int total = 0, fallos = 0;

/* Compara la media obtenida con la esperada e imprime el resultado.
 * Parámetros: caso (descripción), esperada (ºC). Resultado: ninguno. */
void comprobar(const char *caso, float esperada) {
  float m = mediaTemperaturas();
  bool ok = fabs(m - esperada) < 0.01;
  total++;
  if (!ok) fallos++;
  Serial.print(ok ? "OK     " : "FALLO  ");
  Serial.print(caso);
  Serial.print(": media = ");
  Serial.print(m, 2);
  Serial.print(" (esperada ");
  Serial.print(esperada, 2);
  Serial.println(")");
}

void setup() {
  Serial.begin(9600);
  Serial.println("Test de la media de temperaturas (satelite.ino)");

  vaciar();
  comprobar("Cola vacia", 0);

  vaciar();
  guardarTemperatura(20); guardarTemperatura(22); guardarTemperatura(24);
  comprobar("3 valores (20, 22, 24)", 22);

  vaciar();
  for (int i = 1; i <= 10; i++) guardarTemperatura(i);
  comprobar("Exactamente 10 valores (1..10)", 5.5);

  vaciar();
  for (int i = 1; i <= 15; i++) guardarTemperatura(i);
  comprobar("15 valores (1..15): cuentan 6..15", 10.5);

  vaciar();
  for (int i = 0; i < 10; i++) guardarTemperatura(20);
  guardarTemperatura(30);                       // Sustituye al más antiguo
  comprobar("Diez 20 y despues un 30", 21);

  Serial.print("Resultado: ");
  Serial.print(total - fallos);
  Serial.print(" de ");
  Serial.print(total);
  Serial.println(fallos == 0 ? " correctos. TEST SUPERADO" : " correctos. TEST FALLIDO");
}

void loop() {}
