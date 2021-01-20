/**
   I used this code with an ESP32 devkit 1
   and with a logic level converter :
   https://www.aliexpress.com/item/32482721005.html?spm=a2g0s.9042311.0.0.1dc64c4dDrGSej
   or https://protosupplies.com/product/i2c-logic-level-converter-with-regulator-module/
   I linked GPIO1 (Tx) and GPIO3 (RX) to BSCL and BSDA of the level shifter
   Linked the 3v3 pin of the esp32 to BVCC, and GND from esp32 to BGND

   Unsolderer the USB port from AC unit
   Linked the AC Unit GND to AGND
   Linked the AC Unit 5V to AVCC
   Linked the AC Unit RX to ASCL
   Linked the AC Unit TX to ASCL

   NOTE : In the mideaAC.h file, in the source folder, I removed this part :
   #ifdef __AVR__
      typedef void (*acSerialEvent)(ac_status_t * status);
    #else
      typedef std::function<void(ac_status_t * status)> acSerialEvent;
    #endif
       and replaced with :
    typedef std::function<void(ac_status_t * status)> acSerialEvent;


    Mqtt output command example :
    {"on":false,"turbo":false,"eco":false,"soll":23,"lamelle":false,"mode":4,"fan":off}
    send command on esp32/acbureau/command
    receive AC Unit state from : esp32/acbureau/state
*/
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <mideaAC.h>
#include <ArduinoJson.h>

// Replace the next variables with your SSID/Password combination
const char* ssid = "QMG";
const char* password = "XXX";

// Add your MQTT Broker IP address, example:
const char* mqtt_server = "192.168.1.XXX";

// Replace the next variables with your topic
const char* mqttState = "esp32/acbureau/state";
const char* mqttCommand = "esp32/acbureau/command";

acSerial acSeria;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;
#define USE_SERIAL Serial2


void ACevent(ac_status_t * status) {

  if (client.connected()) {

    DynamicJsonDocument doc(1024);
    JsonArray array = doc.to<JsonArray>();


    JsonObject statusObj = array.createNestedObject();
    statusObj["ist"]     = status->ist;
    statusObj["aussen"]  = status->aussen;

    JsonObject confObj = statusObj.createNestedObject("conf");
    confObj["on"]      = status->conf.on;
    confObj["turbo"]   = status->conf.turbo;
    confObj["eco"]     = status->conf.eco;
    confObj["soll"]    = status->conf.soll;

    confObj["lamelle"] = status->conf.lamelle != acLamelleOff;
    confObj["mode"]    = (uint8_t)status->conf.mode;

    switch (status->conf.fan) {
      case acFAN1:
        confObj["fan"] = 1;
        break;
      case acFAN2:
        confObj["fan"] = 2;
        break;
      case acFAN3:
        confObj["fan"] = 3;
        break;
      case acFANA:
        confObj["fan"] = 0;
        break;
      default:
        confObj["fan"] = (uint8_t)status->conf.fan;
        break;
    }

    String output;
    serializeJson(doc, output);
    USE_SERIAL.print("[ACevent] JSON: ");
    USE_SERIAL.println(output);
    char charBuf[output.length() + 1];
    output.toCharArray(charBuf, output.length() + 1);
    // send state through mqttt
    client.publish(mqttState, charBuf);
  } else {
    USE_SERIAL.printf("[ACevent] ----------------------------\n");
    acSeria.print_status(status);
  }

}

void setup() {
  USE_SERIAL.begin(115200);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  Serial.begin(9600);
  acSeria.begin((Stream *)&Serial, "Serial");
  acSeria.send_getSN();

  acSeria.onStatusEvent(ACevent);
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  USE_SERIAL.println();
  USE_SERIAL.print("Connecting to ");
  USE_SERIAL.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    USE_SERIAL.print(".");
  }

  USE_SERIAL.println("");
  USE_SERIAL.println("WiFi connected");
  USE_SERIAL.println("IP address: ");
  USE_SERIAL.println(WiFi.localIP());
}

void callback(char* topic, byte* message, unsigned int length) {
  USE_SERIAL.print("Message arrived on topic: ");
  USE_SERIAL.print(topic);
  USE_SERIAL.print(". Message: ");
  StaticJsonDocument<256> doc;
  deserializeJson(doc, message, length);
  if (strcmp(topic, mqttCommand) == 0) {

    bool on        = doc["on"];
    bool turbo     = doc["turbo"];
    bool eco       = doc["eco"];
    ac_mode_t mode = (ac_mode_t)((uint8_t)doc["mode"]);
    bool lamelle   = doc["lamelle"];

    uint8_t fan    = doc["fan"];
    uint8_t soll   = doc["soll"];

    acSeria.send_conf_h(on, soll, fan, mode, lamelle, turbo, eco);
  }
  //acSeria.send_conf_h(true, 21, 1, acModeHeat, false, false, false);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    USE_SERIAL.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client", "openhabian", NULL)) {
      USE_SERIAL.println("connected");
      // Subscribe
      client.subscribe(mqttCommand);
    } else {
      USE_SERIAL.print("failed, rc=");
      USE_SERIAL.print(client.state());
      USE_SERIAL.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  acSeria.loop();

  long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;
    acSeria.send_status(client.connected(), true);
    acSeria.request_status();
  }
}