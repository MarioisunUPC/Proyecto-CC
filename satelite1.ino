#include <DHT.h>
#include <SoftwareSerial.h>

#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

SoftwareSerial mySerial(10, 11); // RX, TX

unsigned long lastTempTime = 0;
unsigned long nextTimeoutHT = 0;
bool esperandoTimeout = false;
bool enviarDatos = true; // Variable booleana para parar/reanudar

const int ledVerde = 13;

void setup() {
  mySerial.begin(9600); // Comunicación con el Arduino de Tierra
  dht.begin();
  pinMode(ledVerde, OUTPUT);
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. Escuchar comandos desde la estación de tierra
  if (mySerial.available()) {
    String comando = mySerial.readStringUntil('\n');
    comando.trim();
    if (comando == "Parar") enviarDatos = false;
    if (comando == "Reanudar") enviarDatos = true;
  }

  // 2. Lectura del sensor cada 3 segundos
  if (currentMillis - lastTempTime >= 3000) {
    lastTempTime = currentMillis;
    
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    // Gestión de fallos y timeouts
    if (isnan(h) || isnan(t)) {
      if (!esperandoTimeout) {
        esperandoTimeout = true;
        nextTimeoutHT = millis() + 5000; // 5 segundos de gracia
      }
    } else {
      esperandoTimeout = false; // El sensor volvió a funcionar
      
      // Si el usuario no ha pulsado "Parar", enviamos los datos
      if (enviarDatos) {
        // Formato optimizado: "T:34.2:H:20"
        mySerial.print("T:");
        mySerial.print(t);
        mySerial.print(":H:");
        mySerial.println(h);
        
        // Parpadeo breve del LED verde
        digitalWrite(ledVerde, HIGH);
        delay(50);
        digitalWrite(ledVerde, LOW);
      }
    }
  }

  // 3. Comprobar si saltó la alarma de fallo de sensor
  if (esperandoTimeout && (millis() >= nextTimeoutHT)) {
    mySerial.println("Fallo_Sensor");
    esperandoTimeout = false; // Reseteamos para que vuelva a contar o se mantenga
    nextTimeoutHT = millis() + 5000; 
  }
}