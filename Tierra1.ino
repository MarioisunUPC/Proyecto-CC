#include <SoftwareSerial.h>

// RX (pin 10) conecta a TX del módulo de radio/Satélite
// TX (pin 11) conecta a RX del módulo de radio/Satélite
SoftwareSerial radioSat(10, 11); 

const int ledVerde = 13; // LED de actividad / recepción
const int ledRojo = 12;  // LED de alarmas (fallo de sensor o pérdida de enlace)

unsigned long lastMessageTime = 0;
const unsigned long timeoutComms = 5000; // 5 segundos sin datos = enlace caído

void setup() {
  Serial.begin(9600);   // Comunicación USB con la PC (Puerto COM5)
  radioSat.begin(9600); // Comunicación con el Satélite (vía radio)
  
  pinMode(ledVerde, OUTPUT);
  pinMode(ledRojo, OUTPUT);
  
  lastMessageTime = millis();
}

void loop() {
  // 1. Escuchar comandos desde la PC (Python/Interfaz en COM5) y retransmitirlos al Satélite
  if (Serial.available() > 0) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();
    if (comando.length() > 0) {
      radioSat.println(comando); // Reenvía "Parar" o "Reanudar" al Satélite
    }
  }

  // 2. Escuchar datos que llegan del Satélite y enviarlos a la PC
  if (radioSat.available() > 0) {
    String data = radioSat.readStringUntil('\n');
    data.trim();
    
    if (data.length() > 0) {
      lastMessageTime = millis(); // Reinicia el temporizador de enlace activo

      if (data == "Fallo_Sensor") {
        digitalWrite(ledRojo, HIGH); // Activa alarma visual de hardware en tierra
        Serial.println("ALARMA: Sensor DHT11 fallando"); // Reporte a la PC
      } 
      else if (data.startsWith("T:")) {
        digitalWrite(ledRojo, LOW);   // Todo correcto: apaga alarma de fallo
        digitalWrite(ledVerde, HIGH); // Parpadeo de recepción exitosa
        
        Serial.println(data);         // Envía "T:XX.X:H:YY.Y" a la PC (COM5)
        
        delay(50);
        digitalWrite(ledVerde, LOW);
      }
    }
  }

  // 3. Comprobación de pérdida de enlace (Timeout de comunicación)
  if (millis() - lastMessageTime > timeoutComms) {
    digitalWrite(ledRojo, HIGH); // Encender alarma por corte de señal
    
    // Notificación a la PC (opcional, enviada de forma periódica si falla)
    static unsigned long lastAlarmNotify = 0;
    if (millis() - lastAlarmNotify > 3000) {
      Serial.println("ALARMA: Perdida de enlace con el satelite");
      lastAlarmNotify = millis();
    }
  }
}
