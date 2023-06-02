/**
 *  UnidadeMonitoramento.ino
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
 *  Unidade de MonitoramentoQ:
 *    Subscriber @ p/container/level
 *    Publisher @ p/container/set
 *    Publisher @ p/pump
 *
 *  Limitação:
 *    Não há implementação de rotatividade de contêineres. A implementação
 *    considera apenas dois contêineres hardcoded. Por "contêiner", 
 *    compreenda como um par, onde há uma unidade de medida e outra de
 *    escoamento.
 *
 *  Bibliotecas Utilizadas:
 *  
 *  - Neotimer             1.1.6    by Jose Rullan
 *  - Debounce             0.1.0    by Aaron Kimball
 *  - WiFi (Built-In)      1.2.7    by Arduino
 *  - PubSubClient         2.8.0    by Nick O'Leary
 *  - LiquidCrystal I2C    1.1.2    by Marco Schwartz
 *  - ArduinoJson          6.21.2   by Benoit Blanchon  https://arduinojson.org/
 *  
 */
#include <neotimer.h>
#include <debounce.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define LCD_ADDRESS  0x27
#define LCD_ROWS      2
#define LCD_COLUMNS  16

#define SPINNER_INTERVAL  200

static const char* spinner = ".oOo* ";
static const int spinnerFrames = 6;

static const uint8_t BUTTON_SELECT_ID = 0x01;
static const uint8_t BUTTON_LEVEL_ID = 0x03;
static const uint8_t BUTTON_DRAIN_ID = 0x05;

static const int BUTTON_SELECT_PIN = 25;
static const int BUTTON_LEVEL_PIN = 26;
static const int BUTTON_DRAIN_PIN = 27;

static const int BLINK_INTERVAL = 500;  // Intervalo de piscagem para o LED onboard

static const char* WIFI_SSID = "Murilo";
static const char* WIFI_PWD = "12344321";

static const char* MQTT_HOST_ADDRESS = "192.168.61.195";
static const int MQTT_HOST_PORT = 1883;
static const char* MQTT_TOPIC_SUB_LEVEL = "p/container/level";
static const char* MQTT_TOPIC_PUB_SET = "p/container/set";
static const char* MQTT_TOPIC_PUB_PUMP = "p/pump";

static const int PUMP_SECONDS = 2;

unsigned int selectedContainer = 1;
unsigned int container1_ms = -1;
unsigned int container1_cm = -1;
unsigned int container2_ms = -1;
unsigned int container2_cm = -1;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

bool onboardLEDState = false;
static Neotimer onboardLEDTimer = Neotimer();
static Neotimer spinnerTimer = Neotimer();
static int spinnerIndex = 0;

static LiquidCrystal_I2C lcd(
  LCD_ADDRESS, 
  LCD_COLUMNS, 
  LCD_ROWS
);


static void buttonHandler(uint8_t buttonId, uint8_t buttonState) {
  Serial.print("Button ID: ");
  Serial.print(buttonId);
  Serial.print(", State:");
  Serial.print(buttonState);
  Serial.print("  ");
  
  if (buttonState == BTN_PRESSED) {
    if (buttonId == BUTTON_SELECT_ID) {
      Serial.println("Button SELECT pressed");
      doSelectNextContainer();
    } 
    else if (buttonId == BUTTON_LEVEL_ID) {
      Serial.println("Button LEVEL pressed");
      doSetLevelForContainer();
    } 
    else if (buttonId == BUTTON_DRAIN_ID) {
      Serial.println("Button DRAIN pressed");
      doPumpContainer();
    }
  }
}


static Button selectButton(BUTTON_SELECT_ID, buttonHandler);
static Button levelButton(BUTTON_LEVEL_ID, buttonHandler);
static Button drainButton(BUTTON_DRAIN_ID, buttonHandler);


void setup() {
  setupSerial();
  setupOnboardLED();
  setupSpinner();
  setupLcd();
  setupButtons();
  setupWiFi();
  setupMqttClient();

  presentValues();
}


void loop() {
  updateOnboardLED();
  updateSpinner();
  updateButtons();
  pullData();
}


static void setupSerial() {
  Serial.begin(115200);
  Serial.println("Iniciando monitoramento...");
}


static void setupOnboardLED() {
  pinMode(LED_BUILTIN, OUTPUT);
  onboardLEDTimer.set(BLINK_INTERVAL);
}


static void setupSpinner() {
  spinnerTimer.set(SPINNER_INTERVAL);
}


static void setupLcd() {
  lcd.init();
  lcd.backlight();
  lcd.clear();
}


static void setupButtons() {
  pinMode(BUTTON_SELECT_PIN, INPUT);
  pinMode(BUTTON_LEVEL_PIN, INPUT);
  pinMode(BUTTON_DRAIN_PIN, INPUT);
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
  String clientId = "monitor-";
  clientId += String(WiFi.macAddress());

  Serial.println("Monitor MQTT client: attempting to connect...");

  while (!mqttClient.connected()) {
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("Monitor MQTT client: connected OK");
    } else {
      Serial.print("Monitor MQTT client: connection failed with state: ");
      Serial.println(mqttClient.state());
      // aguarda antes de uma nova tentativa de conectar;
      // enquanto isso, pisca rapidamente o LED onboard
      sinalizePaused(3000);  // pisca por 3 segundos
    }
  }

  mqttClient.subscribe(MQTT_TOPIC_SUB_LEVEL);
  Serial.print("Monitor MQTT client: subscribed to: ");
  Serial.println(MQTT_TOPIC_SUB_LEVEL);
}


static void updateOnboardLED() {
  if (onboardLEDTimer.repeat()) {
    onboardLEDState = !onboardLEDState;
    digitalWrite(LED_BUILTIN, onboardLEDState ? HIGH : LOW);
  }
}


static void updateSpinner() {
  if (spinnerTimer.repeat()) {
    spinnerIndex++;
    if (spinnerIndex >= spinnerFrames) {
      spinnerIndex = 0;
    }
    lcd.setCursor(15, 1);
    lcd.print(spinner[spinnerIndex]);
  }
}


static void updateButtons() {
  selectButton.update(digitalRead(BUTTON_SELECT_PIN));
  levelButton.update(digitalRead(BUTTON_LEVEL_PIN));
  drainButton.update(digitalRead(BUTTON_DRAIN_PIN));
}


static void pullData() {
  if (!mqttClient.connected()) {
    connectMqttClient();
  }
  mqttClient.loop();
}


static void doSelectNextContainer() {
  selectedContainer++;
  if (selectedContainer > 2) {
    selectedContainer = 1;
  }
  presentSelection();
}


static void doSetLevelForContainer() {
  unsigned int ms = selectedContainer == 1 ? container1_ms : container2_ms;
  unsigned int cm = selectedContainer == 1 ? container1_cm : container2_cm;

  if ((ms == 1) || (cm == -1)) {
    Serial.println("Invalid Level. Not set.");
    return;
  }

  // para calcular o tamanho do buffer necessário:
  // https://arduinojson.org/v6/example/generator/
  StaticJsonDocument<256> doc;
  
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
  JsonObject origin = doc.createNestedObject("origin");
  origin["from"] = "monitorUnit";
  origin["id"] = 1;

  JsonObject dest = doc.createNestedObject("destination");
  dest["to"] = "measureUnit";
  dest["id"] = selectedContainer;

  JsonObject level = doc.createNestedObject("level");
  level["ms"] = ms;
  level["cm"] = cm;

  String output = "";
  serializeJson(doc, output);  // DEBUG: serializeJsonPretty(doc, Serial);
  mqttClient.publish(MQTT_TOPIC_PUB_SET, output.c_str());
}


static void doPumpContainer() {
  // para calcular o tamanho do buffer necessário:
  // https://arduinojson.org/v6/example/generator/
  StaticJsonDocument<256> doc;
  
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
  origin["from"] = "monitorUnit";
  origin["id"] = 1;

  JsonObject dest = doc.createNestedObject("destination");
  dest["to"] = "pumpUnit";
  dest["id"] = selectedContainer;

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

  if (String(topic) == MQTT_TOPIC_SUB_LEVEL) {
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
    Serial.println("On topic");
    
    StaticJsonDocument<256> doc;
    Serial.println("  ... doc OK");
    
    deserializeJson(doc, payload);
    Serial.println("  ... deserialized OK");
    
    const char* from = doc["origin"]["from"];

    if (String(from) == "measureUnit") {
      unsigned int originId = doc["origin"]["id"];
      unsigned int ms = doc["level"]["ms"];
      unsigned int cm = doc["level"]["cm"];

      Serial.print("  ... from: ");
      Serial.print(from);
      Serial.print(" id: ");
      Serial.print(originId);
      Serial.print(", ");
      Serial.print(ms);
      Serial.print("ms, ");
      Serial.print(cm);
      Serial.println("cm");

      if (originId == 1) {
        container1_ms = ms;
        container1_cm = cm;
      }
      else if (originId == 2) {
        container2_ms = ms;
        container2_cm = cm;
      }

      presentValues();
    }
    else {
      Serial.print("  ... from: ");
      Serial.print(from);
      Serial.println(" (unknown origin)");
    }
  }
}


static void presentValues() {
  lcd.clear();

  lcd.setCursor(1, 0);
  lcd.print("TNQ1");
  if (container1_cm == -1) {
    lcd.setCursor(4, 1);
    lcd.print("?");
  } 
  else {
    lcd.setCursor(0, 1);
    lcd.print(container1_cm);
    lcd.print("cm");
  }

  lcd.setCursor(8, 0);
  lcd.print("TNQ2");
  if (container2_cm == -1) {
    lcd.setCursor(11, 1);
    lcd.print("?");
  } 
  else {
    lcd.setCursor(7, 1);
    lcd.print(container2_cm);
    lcd.print("cm");
  }

  presentSelection();
}


static void presentSelection() {
  lcd.setCursor(0, 0);
  lcd.print(selectedContainer == 1 ? ">" : " ");
  lcd.setCursor(7, 0);
  lcd.print(selectedContainer == 2 ? ">" : " ");
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
