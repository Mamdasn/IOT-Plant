#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <AHT10.h>
#include <Wire.h>

// AHT15 configuration
//   Board:                                    SDA                    SCL                    Level
//NodeMCU 1.0, WeMos D1 Mini............... GPIO4/D2               GPIO5/D1               3.3v/5v
uint8_t readStatus = 0;
AHT10 myAHT15(AHT10_ADDRESS_0X38, AHT15_SENSOR);


// WiFi
const char *ssid = "WiFi SSID"; // Enter your WiFi name
const char *password = "WiFi Password";  // Enter WiFi password

// MQTT Broker
const char *mqtt_broker = "broker.emqx.io";

const char *subscribed_topic = "Controller/Fan_Pump_Controls";

const char *publish_topic_Fan_status = "Controller/Fan_Status";
const char *publish_topic_Fan_status_binary = "Controller/Fan_Status_binary";

const char *publish_topic_Pump_status = "Controller/Pump_Status";
const char *publish_topic_Pump_status_binary = "Controller/Pump_Status_binary";

const char *publish_topic_Control_state_binary = "Controller/Control_State_binary";

const char *publish_topic_temperature = "Controller/temperature";
const char *publish_topic_soil_moisture = "Controller/Soil_Moisture";

const char *mqtt_username = "emqx";
const char *mqtt_password = "public";

const int mqtt_port = 1883;
const int Builtin_LED = 2;

const double Soil_Moisture_Pin = A0; 
const double Fan_Control_Pin = D8;
const double Fan_temperature_threshold = 30;
const double Pump_Control_Pin = D6;
const double Pump_soil_moisture_threshold = 20;
int control_state = 1;

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
 // Set software serial baud to 115200;
 Serial.begin(115200);
 pinMode(Fan_Control_Pin, OUTPUT);
 pinMode(Pump_Control_Pin, OUTPUT);
 pinMode(Soil_Moisture_Pin, INPUT);
 
 // connecting to a WiFi network
 WiFi.mode(WIFI_STA);
 WiFi.begin(ssid, password);
 while (WiFi.status() != WL_CONNECTED) {
     delay(500);
     Serial.println("Connecting to WiFi..");
 }
 Serial.println("Connected to the WiFi network");
 //connecting to a mqtt broker
 client.setServer(mqtt_broker, mqtt_port);
 client.setCallback(callback);
 while (!client.connected()) {
     String client_id = "esp8266-client-";
     client_id += String(WiFi.macAddress());
     Serial.printf("The client %s connects to the public mqtt broker\n", client_id.c_str());
     if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
         Serial.println("Public emqx mqtt broker connected");
     } else {
         Serial.print("failed with state ");
         Serial.print(client.state());
         delay(2000);
     }
 }
 // publish and subscribe
 client.publish(publish_topic_Fan_status, "Hello from NodeMCU");
  client.publish(publish_topic_Pump_status, "Hello from NodeMCU");
 client.subscribe(subscribed_topic);

  // initializing the AHT15
  while (myAHT15.begin() != true)
  {
    Serial.println(F("AHT15 not connected or fail to load calibration coefficients")); //(F()) save string to flash & keeps dynamic memory free
    delay(5000);
  }
  Serial.println(F("AHT15 OK"));

}

void callback(char *topic, byte *payload, unsigned int length) {
 Serial.print("Message arrived in topic: ");
 Serial.println(topic);
 Serial.print("Message:");
 for (int i = 0; i < length; i++) {
     Serial.print((char) payload[i]);
 }
 Serial.println();
 Serial.println("-----------------------");
  // Control State
  if ((char)payload[0] == '0') 
  {
    control_state = 0;
    client.publish(publish_topic_Control_state_binary, "0");
  }
  else if ((char)payload[0] == '1' )
  {
    control_state = 1;
    client.publish(publish_topic_Control_state_binary, "1");
  }
  // Fan Control
   if ((char)payload[0] == '2') 
  {
    digitalWrite(Fan_Control_Pin, HIGH);
    client.publish(publish_topic_Fan_status, "Fan is ON");
    client.publish(publish_topic_Fan_status_binary, "2");
    control_state = 0;
    client.publish(publish_topic_Control_state_binary, "0");
  }
  else if ((char)payload[0] == '3' )
  {
    digitalWrite(Fan_Control_Pin, LOW);
    client.publish(publish_topic_Fan_status, "Fan is OFF");
    client.publish(publish_topic_Fan_status_binary, "3");
    control_state = 0;
    client.publish(publish_topic_Control_state_binary, "0");
  }
  // Pump Control
  if ((char)payload[0] == '4') 
  {
    digitalWrite(Pump_Control_Pin, HIGH);
    client.publish(publish_topic_Pump_status, "Pump is ON");
    client.publish(publish_topic_Pump_status_binary, "4");
    control_state = 0;
    client.publish(publish_topic_Control_state_binary, "0");
  }
  else if ((char)payload[0] == '5' )
  {
    digitalWrite(Pump_Control_Pin, LOW);
    client.publish(publish_topic_Pump_status, "Pump is OFF");
    client.publish(publish_topic_Pump_status_binary, "5");
    control_state = 0;
    client.publish(publish_topic_Control_state_binary, "0");
  }

  
  
}


int AHT15_read_period = 5000;
unsigned long time_now = 0;

void loop() {
 client.loop();
  
  if (millis() >= time_now + AHT15_read_period){
    
      time_now += AHT15_read_period;
      readStatus = myAHT15.readRawData(); //read 6 bytes from AHT10 over I2C
      if (readStatus != AHT10_ERROR)
      {
        double temperature = myAHT15.readTemperature(AHT10_USE_READ_DATA);
//        double humidity = myAHT15.readHumidity(AHT10_USE_READ_DATA);
        double Soil_Moisture = 100*(1024-analogRead(Soil_Moisture_Pin))/1024;
        Serial.print(F("Temperature: ")); Serial.print(temperature); Serial.println(F("°C"));
        Serial.print(F("Soil Moisture...: ")); Serial.print(Soil_Moisture);    Serial.println(F("%"));


        char temperature_c[10];
        char Soil_Moisture_c[10];
        String(temperature).toCharArray(temperature_c, 10);
        String(Soil_Moisture).toCharArray(Soil_Moisture_c, 10);
        client.publish(publish_topic_temperature, temperature_c);
        client.publish(publish_topic_soil_moisture, Soil_Moisture_c);
        // conditional statements
       if (control_state == 1){
         if (temperature > Fan_temperature_threshold){
            digitalWrite(Fan_Control_Pin, HIGH);
            client.publish(publish_topic_Fan_status, "Fan in ON");
            client.publish(publish_topic_Fan_status_binary, "2");
          } else {
            digitalWrite(Fan_Control_Pin, LOW);
            client.publish(publish_topic_Fan_status, "Fan in OFF");
            client.publish(publish_topic_Fan_status_binary, "3");
          }
         if ( Soil_Moisture < Pump_soil_moisture_threshold){
            digitalWrite(Pump_Control_Pin, HIGH);
            client.publish(publish_topic_Pump_status, "Pump in ON");
            client.publish(publish_topic_Pump_status_binary, "4");
          } else {
            digitalWrite(Pump_Control_Pin, LOW);
            client.publish(publish_topic_Pump_status, "Pump in OFF");
            client.publish(publish_topic_Pump_status_binary, "5");
          }
       }
      }
      else
      {
        Serial.print(F("Failed to read - reset: ")); 
        Serial.println(myAHT15.softReset());         //reset 1-success, 0-failed
      }
  }
}