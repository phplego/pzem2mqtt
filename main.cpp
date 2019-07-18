#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <WiFiManager.h>
#include <EasyButton.h>

#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>

#include <PZEM004Tv30.h>

#include "utils.h"
#include "ChangesDetector.h"
#include "Queue.h"

#define APP_VERSION 1.1

#define MQTT_HOST "192.168.1.157"   // MQTT host (e.g. m21.cloudmqtt.com)
#define MQTT_PORT 11883             // MQTT port (e.g. 18076)
#define MQTT_USER "change-me"       // Ingored if brocker allows guest connection
#define MQTT_PASS "change-me"       // Ingored if brocker allows guest connection

#define DEVICE_ID       "pzem004t"       // Used for MQTT topics
#define WIFI_HOSTNAME   "esp-PZEM-004T"  // Name of WiFi network


const char* gConfigFile = "/config.json";

WiFiManager         wifiManager;                            // WiFi Manager
WiFiClient          client;                                 // WiFi Client

PZEM004Tv30         pzem(D5, D6);
EasyButton          button(0);                              // 0 - Flash button
ChangesDetector<10> changesDetector;


Adafruit_MQTT_Client mqtt(&client, MQTT_HOST, MQTT_PORT);   // MQTT client

// Setup MQTT topics
String output_topic = String() + "wifi2mqtt/" + DEVICE_ID;
Adafruit_MQTT_Publish   mqtt_publish        = Adafruit_MQTT_Publish     (&mqtt, output_topic.c_str());


Queue<10> powerQueue;


unsigned long    lastPublishTime             = 0;
unsigned long    publishInterval             = 60*1000;


//float power;
float voltage;
float current;
float energy;
float frequency;
float powerFactor;



void publishState()
{
    Serial.println("publishState()");

    String jsonStr1 = "";

    jsonStr1 += "{";
    //jsonStr1 += "\"memory\": " + String(system_get_free_heap_size()) + ", ";
    jsonStr1 += "\"power\": " + String(powerQueue.average()) + ", ";
    jsonStr1 += "\"voltage\": " + String(voltage) + ", ";
    jsonStr1 += "\"current\": " + String(current) + ", ";
    jsonStr1 += "\"energy\": " + String(energy) + ", ";
    jsonStr1 += "\"pf\": " + String(powerFactor) + ", ";
    jsonStr1 += "\"uptime\": " + String(millis() / 1000) + ", ";
    jsonStr1 += "\"version\": \"" + String(APP_VERSION) + "\"";
    jsonStr1 += "}";

    // Publish state to output topic
    mqtt_publish.publish(jsonStr1.c_str());

    // Remember publish time
    lastPublishTime = millis();

    // Remember published temperatures (to detect changes)
    changesDetector.remember();

    Serial.print("MQTT published: ");
    Serial.println(jsonStr1);
}





void setup()
{

    delay(10);
    Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
    
    
    // Load config
    loadConfig(gConfigFile, [](DynamicJsonDocument json){
        
        Serial.println("State recovered from json.");
    });

    String apName = String(WIFI_HOSTNAME) + "-v" + APP_VERSION;
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




    // Onboard button pressed (for testing purposes)
    button.onPressed([](){
        Serial.println("Flash button pressed!");
        publishState();     // publish to mqtt
    });


    changesDetector.treshold = 10; // 10 watt

    // Provide values to ChangesDetecter
    changesDetector.setGetValuesCallback([](float buf[]){
        buf[0] = powerQueue.average();
        //buf[1] = temperatureService.getTemperature(1);
    });

    // Publish state on changes detected
    changesDetector.setChangesDetectedCallback([](){
        publishState();
    });

    ArduinoOTA.begin();

}



void loop()
{
    ArduinoOTA.handle();
    button.read();
    changesDetector.loop();

    // Ensure the connection to the MQTT server is alive (this will make the first
    // connection and automatically reconnect when disconnected).  See the MQTT_connect()
    MQTT_connect(&mqtt);
    
    
    // wait X milliseconds for subscription messages
    mqtt.processPackets(10);


    float power = pzem.power();
    powerQueue.add(isnan(power) ? 0 : power);

    voltage     = pzem.voltage();
    current     = pzem.current();
    energy      = pzem.energy();
    frequency   = pzem.frequency();
    powerFactor = pzem.pf();

    Serial.print("Power (avg_10): "); Serial.print(powerQueue.average()); Serial.println("W");
    Serial.print("Voltage: "); Serial.print(voltage); Serial.println("V");
    Serial.print("Current: "); Serial.print(current); Serial.println("A");
    Serial.print("Energy: "); Serial.print(energy,3); Serial.println("kWh");
    Serial.print("Frequency: "); Serial.print(frequency, 1); Serial.println("Hz");
    Serial.print("PF: "); Serial.println(powerFactor);
    Serial.println();    


    
    // publish state every publishInterval milliseconds
    if(!lastPublishTime || millis() > lastPublishTime + publishInterval)
    {
        // do the publish
        publishState();
    }


    delay(1000);
}

