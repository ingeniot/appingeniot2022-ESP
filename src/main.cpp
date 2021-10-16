#include <Arduino.h>
#include "Colors.h"
#include "IoTicosSplitter.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

// Global constants
String dId = "12345";
String webhook_auth = "yfmoKTz9dC";
String webhook_endpoint = "http://192.168.0.104:3001/api/getDeviceConfig";
const char* mqtt_server = "192.168.0.104";
long mqtt_port = 1883;

//WiFi

const char* wifi_ssid = "CasaNB";
const char* wifi_password = "Bpjm2828";

// Global variables
WiFiClient wifi_client; 
PubSubClient mqtt_client(wifi_client);
DynamicJsonDocument mqtt_data_doc(2048);

long last_reconnect_attemp = 0;


//Pins

#define led 2



//Function declarations
void clear();
bool get_device_config();
void check_mqtt_connection();
bool reconnect();



void setup() {
  // put your setup code here, to run once:
  Serial.begin(921600);
  Serial.println("Hello world");
  pinMode(led, OUTPUT);
  clear();
  Serial.println("XXXXXXXXXX");
  WiFi.begin(wifi_ssid,wifi_password);
  Serial.print(Green + "Connecting..." + fontReset);
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
    counter++;
    if(counter > 10){
      Serial.print(Red + "\n Wifi connection failed");
      Serial.print("\n Restarting..."+ fontReset);
      delay(2000);
      ESP.restart();
    }
  }
  /*
  Serial.print(boldGreen + "\n\n WiFi connection succes" + fontReset);
  Serial.print("\n    Local IP -> ");
  Serial.print(boldBlue);
  Serial.print(WiFi.localIP());
  Serial.print(fontReset);
  get_device_config();*/
}

void loop() {
  // put your main code here, to run repeatedly:
    check_mqtt_connection();


}

void clear() {
  Serial.write(27);     //ESC command
  Serial.print("[2J");  //clear screen command
  Serial.write(27);
  Serial.print("[H");   //cursor to home command
}

bool get_device_config(){
  Serial.print(underlinePurple + "\n\n Getting device config from Webhook" + fontReset + Purple);
  delay(2000);
  String to_send = "dId=" + dId + "&password=" + webhook_auth;
  HTTPClient http;
  http.begin(webhook_endpoint);
  http.addHeader("Content-Type","application/x-www-form-urlencoded");
  int response_code = http.POST(to_send);
  if(response_code < 0){
    Serial.print(boldRed + "\n\n Error sending Post Request"+ fontReset);
    http.end();
    return false;
  }
  if(response_code != 200){
    Serial.print(boldRed + "\n\n Error in response"+ fontReset + "error->" + response_code);    
    http.end();
    return false;
  }
  if(response_code == 200){
    String response_body = http.getString();
    Serial.print(boldGreen + "\n\n  Device config obtained sucessfully!!" + fontReset);
    deserializeJson(mqtt_data_doc,response_body);
    String mqtt_username = mqtt_data_doc["username"];
    String mqtt_password = mqtt_data_doc["password"];
    String mqtt_topic = mqtt_data_doc["topic"];
    int send_period = mqtt_data_doc["variables"][0]["variablePeriod"];
    Serial.println(mqtt_username);
    Serial.println(mqtt_password);
    Serial.println(send_period);    
    return true;
  }
  return false;
}

void check_mqtt_connection(){
  if((WiFi.status() != WL_CONNECTED)){
    Serial.print(Red +"\n\n Wifi connection failed");
    Serial.print(Red +"\n\n Restarting..." + fontReset);
    delay(20000);
    ESP.restart();
  }
  if(!mqtt_client.connected()){
    long now = millis();
    if( (now - last_reconnect_attemp) > 5000){
      last_reconnect_attemp = millis();
      if(reconnect()){
        last_reconnect_attemp = 0;
      }
    }
  }
  else{
    mqtt_client.loop();
  }

}

bool reconnect(){
  if(!get_device_config()){
    Serial.println(boldRed + "\n\n Error getting device config \n\n Restarting in 10 seconds");
    Serial.println(fontReset);
    delay(10000);
    ESP.restart();
  }
  //setting up mqtt server 
  mqtt_client.setServer(mqtt_server, mqtt_port);
  Serial.print(underlinePurple + "\n\n\n Trying MQTT connection" + fontReset);
  String str_client_id = "device_" + dId + "_" + random(1,9999);
  const char* username = mqtt_data_doc["username"]; 
  const char* password = mqtt_data_doc["password"]; 
  String str_topic = mqtt_data_doc["topic"];

  if(mqtt_client.connect(str_client_id.c_str(), username, password)){
    Serial.print(boldGreen+"MQTT connection success!" + fontReset);
    delay(2000);
    mqtt_client.subscribe((str_topic+"+/actdata").c_str());
    return true;
  } else{
    Serial.print(boldRed+"MQTT connection failes" + fontReset);
  }
  return false;
}

