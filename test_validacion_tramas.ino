/*
 * ============================================================
 *  TEST UNITARIO - Validación de tramas en el Arduino de tierra
 * ============================================================
 *  Comprueba que lineaValida() (copiada tal cual de tierra.ino) acepta
 *  las tramas correctas del protocolo y descarta las corrompidas que
 *  pueden llegar por SoftwareSerial (bytes perdidos o cambiados).
 *
 *  No necesita sensores: basta con cargarlo en cualquier Arduino y abrir
 *  el Monitor Serie a 9600. Cada caso imprime OK o FALLO y al final
 *  aparece el resumen. Si se cambia lineaValida() en tierra.ino hay que
 *  copiarla aquí de nuevo.
 * ============================================================
 */

/* Copia de tierra.ino.
 * Parámetros: l (línea sin '\n'). Resultado: true si la trama es válida. */
bool lineaValida(const char *l) {
  int separadores;
  bool sinDatos = false;
  switch (l[0]) {
    case '1': separadores = 2; break;
    case '2': separadores = 4; break;
    case '6': separadores = 1; break;
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
  if (l[len - 1] == ':' || strstr(l, "::") != NULL) return false;

  if (l[0] == '2') {
    if (l[len - 2] != ':' || l[len - 1] < '0' || l[len - 1] > '2') return false;
  }
  return true;
}

int total = 0, fallos = 0;

/* Ejecuta un caso e imprime el resultado.
 * Parámetros: linea (trama de prueba), esperado (si debe ser válida).
 * Resultado: ninguno (actualiza total y fallos). */
void comprobar(const char *linea, bool esperado) {
  bool r = lineaValida(linea);
  total++;
  if (r != esperado) fallos++;
  Serial.print(r == esperado ? "OK     " : "FALLO  ");
  Serial.print('"');
  Serial.print(linea);
  Serial.print("\" -> ");
  Serial.println(r ? "valida" : "descartada");
}

void setup() {
  Serial.begin(9600);
  Serial.println("Test de lineaValida() (tierra.ino)");

  Serial.println("-- Tramas correctas: deben ser validas --");
  comprobar("1:23.5:40.0", true);       // Temperatura y humedad
  comprobar("2:35:-1:120:0", true);     // Distancias en modo rastreo
  comprobar("2:-2:-2:90:2", true);      // Distancia parada, modo fijo
  comprobar("3:", true);                // Fallo DHT11
  comprobar("4:", true);                // Latido
  comprobar("5:", true);                // Fallo sensor distancia
  comprobar("6:24.37", true);           // Media calculada en el satélite

  Serial.println("-- Tramas corrompidas: deben descartarse --");
  comprobar("", false);                 // Vacía
  comprobar("4", false);                // Falta ':'
  comprobar("7:90", false);             // Código que el satélite no envía
  comprobar("1:23.5", false);           // Falta la humedad
  comprobar("1:23.5:", false);          // Último campo vacío
  comprobar("1:23.5::40", false);       // Campo vacío en medio
  comprobar("1:2a.5:40", false);        // Carácter corrompido
  comprobar("2:35:120:0", false);       // Formato antiguo (3 campos)
  comprobar("2:35:-1:120:7", false);    // Modo inexistente
  comprobar("2:35:-1:120:10", false);   // Modo de dos cifras
  comprobar("3:x", false);              // Alarma con datos
  comprobar("6:", false);               // Media sin valor

  Serial.print("Resultado: ");
  Serial.print(total - fallos);
  Serial.print(" de ");
  Serial.print(total);
  Serial.println(fallos == 0 ? " correctos. TEST SUPERADO" : " correctos. TEST FALLIDO");
}

void loop() {}
