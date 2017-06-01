// Adafruit Feather Huzzah ESP8266 LoRa receiver for data relay to Raspberry Pi IoT Gateway
// Author : Tanmoy Dutta
// March 2017

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RH_RF95.h>
#include <ArduinoJson.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

// WiFi access credentials 
#define  WIFI_SSID         "XXXXXXXXXXXXXXX"         // WiFi SSID
#define  WIFI_PASSWORD     "XXXXXXXXXXXXXXX"         // WiFI Password

#define MQTT_PI_SERVER      "XX.XX.XX.XX"           // MQTT Queue Manager IP address
#define MQTT_PI_SERVER_PORT  1883                    // MQTT Port, use 8883 for SSL

WiFiClient client;

Adafruit_MQTT_Client mqtt(&client, MQTT_PI_SERVER, MQTT_PI_SERVER_PORT);          // MQTT Client initialization
Adafruit_MQTT_Publish telemetry = Adafruit_MQTT_Publish(&mqtt, "home/lora/json");  // MQTT Topic setup in publish mode


// JSON Buffer
DynamicJsonBuffer jsonBuffer;


#define OLED_RESET 2
// Instance of the display
Adafruit_SSD1306 display(OLED_RESET);

#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

#define RFM95_CS 16
#define RFM95_RST 0
#define RFM95_INT 15

// Blinky on receipt
#define LED 5

// Set radion frequency
#define RF95_FREQ 433.0

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

/*----------------------------------------------------------------------------
Function : setup()
Description : 
------------------------------------------------------------------------------*/
void setup() {
  Serial.begin(115200);
  _initOLED();
  _initWiFi();
  _initMQTT();
  _initLoRa();
}


/*----------------------------------------------------------------------------
Function : loop()
Description : Main program loop
------------------------------------------------------------------------------*/
void loop() {
  if (rf95.available()) {
    // Should be a message for us now   
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    
    if (rf95.recv(buf, &len)) {
      digitalWrite(LED, HIGH);
      delay(10);

      char bufChar[len];
      for(int i = 0; i < len; i++) {
        bufChar[i] = char(buf[i]);
        Serial.print(String(bufChar[i]));
      }

      Serial.println();
      Serial.println("Len==" + String(len));
      //Serial.println("String(bufChar)==" + String(bufChar));

      ///////////////////////MQTT Code
      sendMessage(String(bufChar));
      //////////////////////////////////
      
      // Send a reply
      uint8_t outgoingData[] = "{\"Status\" : \"Ack\"}";
      rf95.send(outgoingData, sizeof(outgoingData));
      rf95.waitPacketSent();
      //displayOnOLED("Sent a reply");
      digitalWrite(LED, LOW);
    }
    else {
      displayOnOLED("Receive failed");
    }
  }
}

/*----------------------------------------------------------------------------
Function : displayOnOLED()
Description : Subroutine to display text on the OLED
------------------------------------------------------------------------------*/
void displayOnOLED (String data) {
  display.setCursor(0,0);
  display.clearDisplay();
  display.print(data);
  display.display();
}

/*----------------------------------------------------------------------------
Function : _initOLED ()
Description : Subroutine to initialize the OLED Display
------------------------------------------------------------------------------*/
void _initOLED () {
  //Initialize the display
  display.begin(SSD1306_SWITCHCAPVCC, 0x3D);  // initialize with the I2C addr 0x3D (for the 128x64)
  display.setRotation(2);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.clearDisplay();
}

void donothing() {
}

/*----------------------------------------------------------------------------
Function : _initWiFi()
Description : Connect to WiFi access point
------------------------------------------------------------------------------*/
void _initWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  display.setCursor(0,0);
  display.clearDisplay();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    displayOnOLED("Waiting for WiFi...");
  }
  displayOnOLED("WiFi Connected!");
  delay(1000);

}

/*----------------------------------------------------------------------------
Function : _initLoRa()
Description : Connect to WiFi access point
------------------------------------------------------------------------------*/
void _initLoRa() {
  attachInterrupt(digitalPinToInterrupt(RFM95_INT), donothing, CHANGE);
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);

  displayOnOLED("LoRa RX WiFi Repeater");
  delay(1000);

  // manual reset
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  while (!rf95.init()) {
    displayOnOLED("LoRa radio init failed");
    while (1);
  }
  displayOnOLED("LoRa radio init OK!");
  delay(1000);
  
  if (!rf95.setFrequency(RF95_FREQ)) {
    displayOnOLED("setFrequency failed");
    while (1);
  }
  displayOnOLED("Freq set to 433 MHz");
  delay(1000);
  
  rf95.setTxPower(5, false);
  displayOnOLED("LoRa Listening...");
  delay(1000);
}

/*----------------------------------------------------------------------------
Function : _initMQTT()
Description : Connect to MQTT Queue Manager
------------------------------------------------------------------------------*/
void _initMQTT() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  displayOnOLED("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  displayOnOLED("MQTT Connected!");
  delay(1000);
}

/*----------------------------------------------------------------------------
Function : sendMessage()
Description : Send message to the MQTT Server
------------------------------------------------------------------------------*/
void sendMessage(String msg) {
  String s = "{ \"M\":";
  s += msg;
  s += " }}";

  int len = s.length();
  char charBuf[len];
  s.toCharArray(charBuf, len);

  if (! telemetry.publish(charBuf)) {
    displayOnOLED("Sending failed :(");
  } else {
    displayOnOLED("Message sent :)");
  }
}

