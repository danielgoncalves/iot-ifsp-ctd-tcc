/**
 *  UnidadeMedicao.ino
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
 *    Subscriber @ p/container/set
 *    Publisher @ p/container/level
 *
 *  Bibliotecas Utilizadas:
 *  
 *  - Neotimer             1.1.6    by Jose Rullan
 *  - NewPing              1.9.7    by Tim Eckel
 *  - WiFi (Built-In)      1.2.7    by Arduino
 *  - PubSubClient         2.8.0    by Nick O'Leary
 *  - ArduinoJson          6.21.2   by Benoit Blanchon  https://arduinojson.org/
 *
 *  Placa selecionada no IDE Arduino:
 *
 *  - DOIT ESP32 DEVKIT V1
 *  
 */
#include <neotimer.h>
#include <NewPing.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

static const int SONAR_ID = 1;
static const int SONAR_TRIGGER_PIN = 18;
static const int SONAR_ECHO_PIN = 19;
static const int SONAR_MAX_DISTANCE_CM = 200;  // 200cm = 2m
static const int SONAR_READ_INTERVAL = 2000;  // Intervalo entre leituras, em milisegundos

static const int BLINK_INTERVAL = 500;  // Intervalo de piscagem para o LED onboard

static const char* WIFI_SSID = "YOUR_SSID";
static const char* WIFI_PWD = "YOUR_WIFI_SECRET";

static const char* MQTT_HOST_ADDRESS = "IP_OR_DNS";
static const int MQTT_HOST_PORT = 1883;
static const char* MQTT_TOPIC_SUB_SET = "p/container/set";
static const char* MQTT_TOPIC_PUB_LEVEL = "p/container/level";
static const char* MQTT_TOPIC_PUB_PUMP = "p/pump";

static const int PUMP_SECONDS = 2;

unsigned int criticalLevel_ms = 455;  // Nível crítico padrão: 455ms
unsigned int criticalLevel_cm = 7;  // Nível crítico padrão (em centímetros): 7cm

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

bool onboardLEDState = false;
Neotimer onboardLEDTimer = Neotimer();
Neotimer sonarTimer = Neotimer();

NewPing sonar(
  SONAR_TRIGGER_PIN, 
  SONAR_ECHO_PIN,
  SONAR_MAX_DISTANCE_CM
);


void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  setupSerial();
  setupWiFi();
  setupMqttClient();
  setupTimers();

  sinalizeCriticalLevelsReset();
}


void loop() {
  blinkOnboardLED();
  readSonar();
  pullData();
}


static void setupSerial() {
  Serial.begin(115200);
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
  String clientId = "sonar-";
  clientId += String(WiFi.macAddress());

  mqttClient.setServer(MQTT_HOST_ADDRESS, MQTT_HOST_PORT);
  mqttClient.setCallback(dataIncomingHandler);
}


static void connectMqttClient() {
  String clientId = "sonar-";
  clientId += String(WiFi.macAddress());

  Serial.println("Sonar MQTT client: attempting to connect...");

  while (!mqttClient.connected()) {
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("Sonar MQTT client: connected OK");
    } else {
      Serial.print("Sonar MQTT client: connection failed with state: ");
      Serial.println(mqttClient.state());
      // aguarda antes de uma nova tentativa de conectar;
      // enquanto isso, pisca rapidamente o LED onboard
      sinalizePaused(3000);  // pisca por 3 segundos
    }
  }

  mqttClient.subscribe(MQTT_TOPIC_SUB_SET);
  Serial.print("Sonar MQTT client: subscribed to: ");
  Serial.println(MQTT_TOPIC_SUB_SET);
}


static void setupTimers() {
  // ajusta os intervalos de leitura do ultrasonico e de
  // indicação de operação do LED onboard
  sonarTimer.set(SONAR_READ_INTERVAL);
  onboardLEDTimer.set(BLINK_INTERVAL);
}


static void blinkOnboardLED() {
  if (onboardLEDTimer.repeat()) {
    onboardLEDState = !onboardLEDState;
    digitalWrite(LED_BUILTIN, onboardLEDState ? HIGH : LOW);
  }
}


static void readSonar() {
  if (sonarTimer.repeat()) {
    unsigned int dist_ms = sonar.ping();
    unsigned int dist_cm = sonar.convert_cm(dist_ms);
    
    // Serial.print("Sensor: ");
    // Serial.print(dist_ms);
    // Serial.print("ms; ");
    // Serial.print(dist_cm);
    // Serial.println("cm;");

    doPublishLevel(dist_ms, dist_cm);
    doCheckLevel(dist_ms, dist_cm);
  }
}


static void pullData() {
  if (!mqttClient.connected()) {
    connectMqttClient();
  }
  mqttClient.loop();
}


static void doPublishLevel(unsigned int dist_ms, unsigned int dist_cm) {
  // para calcular o tamanho do buffer necessário:
  // https://arduinojson.org/v6/example/generator/
  StaticJsonDocument<128> doc;
  
  //
  // Referência do payload:
  //
  //   {
  //     "origin": {
  //       "from": "measureUnit",
  //       "id": 1
  //     },
  //     "level": {
  //       "ms": 911,
  //       "cm": 15
  //     }
  //   }
  //
  JsonObject origin = doc.createNestedObject("origin");
  origin["from"] = "measureUnit";
  origin["id"] = SONAR_ID;

  JsonObject level = doc.createNestedObject("level");
  level["ms"] = dist_ms;
  level["cm"] = dist_cm;

  String output = "";
  serializeJson(doc, output);  // DEBUG: serializeJsonPretty(doc, Serial);
  mqttClient.publish(MQTT_TOPIC_PUB_LEVEL, output.c_str());
}


static void doCheckLevel(unsigned int dist_ms, unsigned int dist_cm) {
  if ((dist_ms < criticalLevel_ms) || (dist_cm < criticalLevel_cm)) {
    Serial.print("Levels CRITICAL: ");
    Serial.print(dist_ms);
    Serial.print("ms, ");
    Serial.print(dist_cm);
    Serial.println("cm");
    doCommandPump();
  }
  else {
    Serial.print("Levels OK: ");
    Serial.print(dist_ms);
    Serial.print("ms, ");
    Serial.print(dist_cm);
    Serial.println("cm");
  }
}


static void doCommandPump() {
  // para calcular o tamanho do buffer necessário:
  // https://arduinojson.org/v6/example/generator/
  StaticJsonDocument<128> doc;
  
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
  JsonObject origin = doc.createNestedObject("origin");
  origin["from"] = "measureUnit";
  origin["id"] = SONAR_ID;

  JsonObject dest = doc.createNestedObject("destination");
  dest["to"] = "pumpUnit";
  dest["id"] = SONAR_ID;

  JsonObject pump = doc.createNestedObject("pump");
  pump["seconds"] = PUMP_SECONDS;

  String output = "";
  serializeJson(doc, output);  // DEBUG: serializeJsonPretty(doc, Serial);
  mqttClient.publish(MQTT_TOPIC_PUB_PUMP, output.c_str());
}


static void dataIncomingHandler(char* topic, byte* payload, unsigned int len) {
  Serial.print("Data incoming, topic: ");
  Serial.print(topic);
  Serial.print(": Msg Len: ");
  Serial.println(len);

  if (String(topic) == MQTT_TOPIC_SUB_SET) {
    //
    // Referência do payload:
    //
    //   {
    //     "origin": {
    //       "from": "monitorUnit",
    //       "id": 1
    //     },
    //     "destination": {
    //       "to": "measureUnit",
    //       "id": 1
    //     },
    //     "level": {
    //       "ms": 747,
    //       "cm": 13
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

    if (String(dest) == "measureUnit") {
      unsigned int unitId = doc["destination"]["id"];
      Serial.print("  ... dest id: ");
      Serial.println(unitId);

      if (unitId == SONAR_ID) {
        criticalLevel_ms = doc["level"]["ms"];
        criticalLevel_cm = doc["level"]["cm"];
        sinalizeCriticalLevelsReset();
      }
    }
    else {
      Serial.print("  ... dest is not a measure unit: ");
      Serial.println(dest);
    }
  }
}


static void sinalizeCriticalLevelsReset() {
  Serial.print("Critical Levels RESET: ");
  Serial.print(criticalLevel_ms);
  Serial.print("ms, ");
  Serial.print(criticalLevel_cm);
  Serial.println("cm");
  
  // indica que os níveis críticos foram ajustados
  // piscando o LED onboard rapidamente
  sinalizePaused(3000);  // pisca por 3 segundos
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
