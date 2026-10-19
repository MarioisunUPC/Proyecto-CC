#include <DHT.h>

#define DHTPIN 2
#define DHTTYPE DHT11

#define LED_AMARILLO 7      // Lectura / Envío
#define LED_ROJO 8          // Confirmación de envío

DHT dht(DHTPIN, DHTTYPE);

unsigned long nextHT = 0;
bool enviarDatos = true;    // Variable de control (Paso 11)

void setup() {
  Serial.begin(9600);
  pinMode(LED_AMARILLO, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);
  dht.begin();
}

void loop() {
  // 1. Escuchamos órdenes enviadas desde Python por la línea USB (Paso 11)
  if (Serial.available() > 0) {
    String comando = Serial.readStringUntil('\n');
    comando.trim(); // Elimina espacios o saltos de línea sobrantes

    if (comando == "Parar") {
      enviarDatos = false;
    } else if (comando == "Reanudar") {
      enviarDatos = true;
    }
  }

  // 2. Transmisión de datos cada 3 segundos si enviarDatos está activo
  if (millis() >= nextHT) {
    nextHT = millis() + 3000;

    if (enviarDatos) {
      digitalWrite(LED_AMARILLO, HIGH);

      float h = dht.readHumidity();
      float t = dht.readTemperature();

      delay(100);
      digitalWrite(LED_AMARILLO, LOW);

      if (!isnan(h) && !isnan(t)) {
        // Formato comprimido requerido en el Paso 9: "T:34.2:H:20"
        Serial.print("T:");
        Serial.print(t, 1);
        Serial.print(":H:");
        Serial.println(h, 1);

        digitalWrite(LED_ROJO, HIGH);
        delay(150);
        digitalWrite(LED_ROJO, LOW);
      }
    }
  }
}