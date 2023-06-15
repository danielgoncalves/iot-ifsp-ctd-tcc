/**
 *  UnidadeEscoamento.ino
 *
 *  Parte do Trabalho final da disciplina.
 *
 *  Disciplina: Eletrônica Básica
 *  Professor: Marcos Rodrigues Costa
 *  Autores:
 *  
 *  - Daniel Gonçalves <goncalves.d@aluno.ifsp.edu.br>
 *  - Murilo dos Reis Tavares <murilo.tavares@aluno.ifsp.edu.br>
 *
 *  Unidade de Medição:
 *    Subscriber @ p/pump
 *
 *  Bibliotecas Utilizadas:
 *  
 *  - Neotimer             1.1.6    by Jose Rullan
 *  - WiFi (Built-In)      1.2.7    by Arduino
 *  - PubSubClient         2.8.0    by Nick O'Leary
 *  - ArduinoJson          6.21.2   by Benoit Blanchon  https://arduinojson.org/
 *  
 */
#include <neotimer.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Para evitar o acionamento da bomba em plataformas de simulação
#define SIMULATED_ENVIRONMENT

// Id da bomba deve ser o mesmo do respectivo contêiner
// (eg. SONAR_ID da Unidade de Medição)
static const int PUMP_ID = 1;

static const int PUMP_PIN = 27;
static const int PUMPING_LED_PIN = 33;

static const int BLINK_INTERVAL = 500;  // Intervalo de piscagem para o LED onboard

static const char* WIFI_SSID = "Murilo";
static const char* WIFI_PWD = "12344321";

static const char* MQTT_HOST_ADDRESS = "192.168.61.195";
static const int MQTT_HOST_PORT = 1883;
static const char* MQTT_TOPIC_SUB_PUMP = "p/pump";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

bool onboardLEDState = false;
Neotimer onboardLEDTimer = Neotimer();


void setup() {
  setupSerial();
  setupPins();
  setupWiFi();
  setupMqttClient();
  setupTimers();
}


void loop() {
  blinkOnboardLED();
  pullData();
}


static void setupSerial() {
  Serial.begin(115200);
}


static void setupPins() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PUMPING_LED_PIN, OUTPUT);
  #ifndef SIMULATED_ENVIRONMENT
    pinMode(PUMP_PIN, OUTPUT);
  #endif
}


static void setupWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PWD);
  
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected to the WiFi network OK");
  digitalWrite(LED_BUILTIN, HIGH);
  delay(2000);
  digitalWrite(LED_BUILTIN, LOW);
}


static void setupMqttClient() {
  mqttClient.setServer(MQTT_HOST_ADDRESS, MQTT_HOST_PORT);
  mqttClient.setCallback(dataIncomingHandler);
}


static void connectMqttClient() {
  String clientId = "pump-";
  clientId += String(WiFi.macAddress());

  Serial.println("Pump MQTT client: attempting to connect...");

  while (!mqttClient.connected()) {
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("Pump MQTT client: connected OK");
    } else {
      Serial.print("Pump MQTT client: connection failed with state: ");
      Serial.println(mqttClient.state());
      // aguarda antes de uma nova tentativa de conectar;
      // enquanto isso, pisca rapidamente o LED onboard
      sinalizePaused(3000);  // pisca por 3 segundos
    }
  }

  mqttClient.subscribe(MQTT_TOPIC_SUB_PUMP);
  Serial.print("Pump MQTT client: subscribed to: ");
  Serial.println(MQTT_TOPIC_SUB_PUMP);
}


static void setupTimers() {
  // ajusta os intervalos de indicação de operação do LED onboard
  onboardLEDTimer.set(BLINK_INTERVAL);
}


static void blinkOnboardLED() {
  if (onboardLEDTimer.repeat()) {
    onboardLEDState = !onboardLEDState;
    digitalWrite(LED_BUILTIN, onboardLEDState ? HIGH : LOW);
  }
}


static void pullData() {
  Serial.println("pulling data...");
  if (!mqttClient.connected()) {
    connectMqttClient();
  }
  mqttClient.loop();
}


static void doPump(unsigned int duration) {
  unsigned long t0 = millis();
  unsigned long t1 = t0 + duration;
  
  Serial.print("Pumping (");
  Serial.print(duration);
  Serial.print("ms): ");

  #ifndef SIMULATED_ENVIRONMENT
    digitalWrite(PUMP_PIN, HIGH);
  #endif
  digitalWrite(PUMPING_LED_PIN, HIGH);

  while (millis() < t1) {
    Serial.print(".");
  }

  #ifndef SIMULATED_ENVIRONMENT
    digitalWrite(PUMP_PIN, LOW);
  #endif
  digitalWrite(PUMPING_LED_PIN, LOW);

  Serial.println(" done");
}


static void dataIncomingHandler(char* topic, byte* payload, unsigned int len) {
  Serial.print("Data incoming, topic: ");
  Serial.print(topic);
  Serial.print(": Msg Len: ");
  Serial.println(len);

  if (String(topic) == MQTT_TOPIC_SUB_PUMP) {
    //
    // Referência do payload:
    //
    //   {
    //     "origin": {
    //       "from": "measureUnit",
    //       "id": 1
    //     },
    //     "destination": {
    //       "to": "pumpUnit",
    //       "id": 1
    //     },
    //     "pump": {
    //       "seconds": 3
    //     }
    //   }
    //
    Serial.println("On topic");
    
    StaticJsonDocument<256> doc;
    Serial.println("  ... doc OK");
    
    deserializeJson(doc, payload);
    Serial.println("  ... deserialized OK");
    
    const char* dest = doc["destination"]["to"];
    Serial.print("  ... dest: ");
    Serial.println(dest);

    if (String(dest) == "pumpUnit") {
      unsigned int unitId = doc["destination"]["id"];
      Serial.print("  ... dest id: ");
      Serial.println(unitId);

      if (unitId == PUMP_ID) {
        unsigned int duration = doc["pump"]["seconds"];
        doPump(duration * 1000);
      }
    }
    else {
      Serial.print("  ... dest is not a pump unit: ");
      Serial.println(dest);
    }
  }
}


static void sinalizePaused(unsigned int duration) {
  unsigned long t = millis();
  bool state = false;

  while (millis() < t + duration) {
    state = !state;
    digitalWrite(LED_BUILTIN, state ? HIGH : LOW);
    delay(50);
  }
}
