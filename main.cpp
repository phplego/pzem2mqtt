#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <WiFiManager.h>
#include <ESP8266WebServer.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <PZEM004Tv30.h>
#include <SSD1306.h>

#include "Globals.h"
#include "utils.h"
#include "WebService.h"
#include "ChangesDetector.h"
#include "MeasureService.h"


#define MQTT_HOST "192.168.1.157"   // MQTT host (e.g. m21.cloudmqtt.com)
#define MQTT_PORT 11883             // MQTT port (e.g. 18076)
#define MQTT_USER "change-me"       // Ingored if brocker allows guest connection
#define MQTT_PASS "change-me"       // Ingored if brocker allows guest connection

#define DEVICE_ID "pzem004t"        // Used for MQTT topics

const char * Globals::appVersion = "1.17";


//const char* gConfigFile = "/config.json";

WiFiManager         wifiManager;                            // WiFi Manager
WiFiClient          client;                                 // WiFi Client

PZEM004Tv30         pzem((uint8_t)0 /*D3*/, (uint8_t)2 /*D4*/);
ChangesDetector<1> changesDetector;

SSD1306             display(0x3c, 1 /*RX*/, 3 /*TX*/, GEOMETRY_128_32);


Adafruit_MQTT_Client mqtt(&client, MQTT_HOST, MQTT_PORT);   // MQTT client

// Setup MQTT topics
String output_topic = String() + "wifi2mqtt/" + DEVICE_ID;
Adafruit_MQTT_Publish   mqtt_publish        = Adafruit_MQTT_Publish     (&mqtt, output_topic.c_str());


WebService webService(&wifiManager);

MeasureService measureService;



unsigned long    lastPublishTime             = 0;
unsigned long    publishInterval             = 60*1000;



void publishState()
{
    Serial.println("publishState()");

    //FSInfo fsInfo;
    //SPIFFS.info(fsInfo);


    String jsonStr1 = "";

    jsonStr1 += "{";
    //jsonStr1 += "\"memory\": " + String(system_get_free_heap_size()) + ", ";
    //jsonStr1 += "\"totalBytes\": " + String(fsInfo.totalBytes) + ", ";
    //jsonStr1 += "\"usedBytes\": " + String(fsInfo.usedBytes) + ", ";
    jsonStr1 += String("\"power\": ") + measureService.getPower() + ", ";
    jsonStr1 += String("\"voltage\": ") + String(measureService.getVoltage()) + ", ";
    jsonStr1 += String("\"current\": ") + String(measureService.getCurrent()) + ", ";
    jsonStr1 += String("\"energy\": ") + String(measureService.getEnergy()) + ", ";
    jsonStr1 += String("\"pf\": ") + String(measureService.getPowerFactor()) + ", ";
    jsonStr1 += String("\"uptime\": ") + String(millis() / 1000) + ", ";
    jsonStr1 += String("\"version\": \"") + Globals::appVersion + "\"";
    jsonStr1 += "}";

    // Publish state to output topic
    mqtt_publish.publish(jsonStr1.c_str());

    // Remember publish time
    lastPublishTime = millis();

    // Remember published values (to detect changes)
    changesDetector.remember();

    Serial.print("MQTT published: ");
    Serial.println(jsonStr1);

}


void displayLoop() 
{
    display.clear();
    display.setFont(ArialMT_Plain_10);

    String str = "";
    
    str = String("") + measureService.getPower() + "W";
    display.drawString(0, 0, str);
    
    str = String("") + String(measureService.getVoltage()) + "V";
    display.drawString(50, 0, str);

    str = String("") + String(measureService.getCurrent()) + "A ";
    display.drawString(0, 10, str);
    
    str = String("") + String(measureService.getEnergy()) + "kW/h";
    display.drawString(50, 10, str);

    str = String("pf: ") + String(measureService.getPowerFactor()) + " ";
    display.drawString(0, 20, str);
    str = String("") + String(millis() / 1000) + "S ";
    display.drawString(50, 20, str);

    display.display();
}



void setup()
{
    //Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);

    // Init display
    display.init();
    display.setContrast(1, 5, 0);
    display.flipScreenVertically();
    display.drawString(0, 0, "PZEM004T");
    display.display();

    SPIFFS.begin();
   
    // Load config
    // loadConfig(gConfigFile, [](DynamicJsonDocument json){
    //     Serial.println("State recovered from json.");
    // });

    String apName = String("esp-") + DEVICE_ID + "-v" + Globals::appVersion + "-" + ESP.getChipId();
    apName.replace('.', '_');
    WiFi.hostname(apName);
    wifiManager.setAPStaticIPConfig(IPAddress(10, 0, 1, 1), IPAddress(10, 0, 1, 1), IPAddress(255, 255, 255, 0));
    wifiManager.autoConnect(apName.c_str(), "12341234"); // IMPORTANT! Blocks execution. Waits until connected

    // Wait for WIFI connection

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(10);
        Serial.print(".");
    }

    Serial.print("\nConnected to ");
    Serial.println(WiFi.SSID());
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    webService.init();

    changesDetector.treshold = 15; // 15 watt

    // Provide values to ChangesDetecter
    changesDetector.setGetValuesCallback([](float buf[]){
        buf[0] = measureService.getPower();
        //buf[1] = temperatureService.getTemperature(1);
    });

    // Publish state on changes detected
    changesDetector.setChangesDetectedCallback([](){
        // do the publish
        publishState();
    });

    // Setup measurement service
    measureService.init();


    ArduinoOTA.begin(false);
}



void loop()
{
    if (!WiFi.isConnected())
    {
        Serial.println("No WiFi connection.. wait for 3 sec and skip loop");
        delay(3000);
        return;
    }


    ArduinoOTA.handle();
    measureService.loop();
    changesDetector.loop();
    webService.loop();
    displayLoop();
    

    // Ensure the connection to the MQTT server is alive (this will make the first
    // connection and automatically reconnect when disconnected).  See the MQTT_connect()
    MQTT_connect(&mqtt);
    
    // wait X milliseconds for subscription messages
    mqtt.processPackets(10);
    
    // publish state every publishInterval milliseconds
    if(!lastPublishTime || millis() > lastPublishTime + publishInterval)
    {
        // do the publish
        publishState();
        
        // redraw display
        displayLoop();
    }

    //delay(50);
}

