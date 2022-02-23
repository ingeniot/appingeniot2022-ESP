#include <Arduino.h>
#include "Colors.h"
#include "IoTicosSplitter.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
//****************************************************************************
// Local config
// Global constants
// String dId = "12345678";
// String webhook_auth = "8suMV0MDTM";
// String webhook_endpoint = "https://panel.ingeniot.com.ar:3003/api/getdevicecredentials";
// const char* mqtt_server = "panel.ingeniot.com.ar";
//String webhook_endpoint = "http://192.168.1.106:3001/api/getDeviceConfig";
//const char* mqtt_server = "192.168.1.106";

 String dId = "12345678";
 String webhook_auth = "kCJSZfxbx8";
 String webhook_endpoint = "https://ap.ingeniot.com.ar:3003/api/getdevicecredentials";
 const char* mqtt_server = "app.ingeniot.com.ar";

long mqtt_port = 1883;

//WiFi

const char* wifi_ssid = "ingeniot";
const char* wifi_password = "ingeniot2828";
//*****************************************************************************

// Global variables
WiFiClient esp_client; 
PubSubClient mqtt_client(esp_client);
DynamicJsonDocument mqtt_data_doc(2048);

long last_reconnect_attemp = 0;

IoTicosSplitter splitter;
long var_last_send[20];
String last_received_msg = "";
String last_received_topic = "";
int temp = 0;
int hum = 0;
int prev_temp = 0;
int prev_hum = 0;


//Pins

#define led 2

//Function declarations
void clear();
bool get_device_config();
void check_mqtt_connection();
void reconnect();

void process_sensors();
void process_actuators();
void send_data_to_broker();
void mqtt_callback(char *topic, byte *payload, unsigned int length);
void process_incoming_msg(String topic, String incoming);
void print_stats();
void clear();


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
  
  Serial.print(boldGreen + "\n\n WiFi connection success" + fontReset);
  Serial.print("\n    Local IP -> ");
  Serial.print(boldBlue);
  Serial.print(WiFi.localIP());
  Serial.print(fontReset);
  mqtt_client.setCallback(mqtt_callback);
  Serial.print(boldGreen + "\n\n  Configuro Callbak!!" + fontReset);

}

void loop() {
  // put your main code here, to run repeatedly:
    check_mqtt_connection();


}

//USER FUNTIONS ⤵
void process_sensors()
{

  //get temp simulation
  if (temp == 20)
    {
    temp=30;
    }
  else
  {
    temp=20;
  }

  //int temp = random(1, 100);
  mqtt_data_doc["variables"][0]["last"]["value"] = temp;

  //save temp?
  int dif = temp - prev_temp;
  if (dif < 0)
  {
    dif *= -1;
  }

  if (dif >= 5)
  {
    mqtt_data_doc["variables"][0]["last"]["save"] = 1;
  }
  else
  {
    mqtt_data_doc["variables"][0]["last"]["save"] = 0;
  }

  prev_temp = temp;

  //get humidity simulation
  if(hum == 40)
    {
      hum=60;
    }
    else
    {
      hum=40;
    }
  //int hum = random(1, 50);
  mqtt_data_doc["variables"][1]["last"]["value"] = hum;

  //save hum?
  dif = hum - prev_hum;
  if (dif < 0)
  {
    dif *= -1;
  }

  if (dif >= 20)
  {
    mqtt_data_doc["variables"][1]["last"]["save"] = 1;
  }
  else
  {
    mqtt_data_doc["variables"][1]["last"]["save"] = 0;
  }

  prev_hum = hum;

  //get led status
  mqtt_data_doc["variables"][4]["last"]["value"] = (HIGH == digitalRead(led));
}

void process_actuators()
{
  if (mqtt_data_doc["variables"][2]["last"]["value"] == "on")
  {
    digitalWrite(led, HIGH);
    mqtt_data_doc["variables"][2]["last"]["value"] = "";
    var_last_send[4] = 0;
  }
  else if (mqtt_data_doc["variables"][3]["last"]["value"] == "off")
  {
    digitalWrite(led, LOW);
    mqtt_data_doc["variables"][3]["last"]["value"] = "";
    var_last_send[4] = 0;
  }

}

//TEMPLATE ⤵
void process_incoming_msg(String topic, String incoming){

  last_received_topic = topic;
  last_received_msg = incoming;

  String variable = splitter.split(topic, '/', 2);
      Serial.print(Red + "\n\n         Procesando mensaje de entrada ");
  for (int i = 0; i < mqtt_data_doc["variables"].size(); i++ ){

    if (mqtt_data_doc["variables"][i]["variable"] == variable){
      
      DynamicJsonDocument doc(256);
      deserializeJson(doc, incoming);
      mqtt_data_doc["variables"][i]["last"] = doc;

      long counter = mqtt_data_doc["variables"][i]["counter"];
      counter++;
      mqtt_data_doc["variables"][i]["counter"] = counter;

    }

  }

  process_actuators();

}

void mqtt_callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print(boldGreen + "\n\n         Ejecutando Callback de mqtt" + fontReset);
  String incoming = "";

  for (int i = 0; i < length; i++)
  {
    incoming += (char)payload[i];
  }

  incoming.trim();
  Serial.print(boldGreen + "\n\n Topic" +  String(topic) + fontReset);  
  Serial.print(boldGreen + "\n\n Incoming" + incoming + fontReset);
  process_incoming_msg(String(topic), incoming);

}

void send_data_to_broker()
{

  long now = millis();

  for (int i = 0; i < mqtt_data_doc["variables"].size(); i++)
  {

    if (mqtt_data_doc["variables"][i]["variableType"] == "output")
    {
      continue;
    }

    int freq = mqtt_data_doc["variables"][i]["variableSendFreq"];
    if(freq < 30)
      {
        freq=30;
      }
    if (now - var_last_send[i] > freq * 1000)
    {
      var_last_send[i] = millis();

      String str_root_topic = mqtt_data_doc["topic"];
      String str_variable = mqtt_data_doc["variables"][i]["variable"];
      String topic = str_root_topic + str_variable + "/sdata";

      String toSend = "";

      serializeJson(mqtt_data_doc["variables"][i]["last"], toSend);

      mqtt_client.publish(topic.c_str(), toSend.c_str());


      //STATS
      long counter = mqtt_data_doc["variables"][i]["counter"];
      counter++;
      mqtt_data_doc["variables"][i]["counter"] = counter;

    }
  }
}


void clear() {
  Serial.write(27);     //ESC command
  Serial.print("[2J");  //clear screen command
  Serial.write(27);
  Serial.print("[H");   //cursor to home command
}
void reconnect()
{
  if (!get_device_config())
  {
    Serial.println(boldRed + "\n\n      Error getting mqtt credentials :( \n\n RESTARTING IN 10 SECONDS");
    Serial.println(fontReset);
    delay(10000);
    ESP.restart();
  }

  //Setting up Mqtt Server
  mqtt_client.
  setServer(mqtt_server, mqtt_port);

  Serial.print(underlinePurple + "\n\n\nTrying MQTT Connection" + fontReset + Purple + "  ⤵");

  String str_client_id = "device_" + dId + "_" + random(1, 9999);
  const char *username = mqtt_data_doc["username"];
  const char *password = mqtt_data_doc["password"];
  String str_topic = mqtt_data_doc["topic"];

  if (mqtt_client.connect(str_client_id.c_str(), username, password))
  {

    delay(2000);
    if(mqtt_client.state() == 0)
    {
    Serial.print(boldGreen + "\n\n         Mqtt Client Connected :) " + str_client_id.c_str()+ fontReset);        

    String sub_topic = str_topic + "+/actdata";
    //boolean subscribe  = mqtt_client.subscribe((str_topic + "+/actdata").c_str());
    boolean subscribe  = mqtt_client.subscribe(sub_topic.c_str());

    if (subscribe)
      {
      Serial.print(boldGreen + "\n\n         Suscripcion a " +  sub_topic  + fontReset);
      }
    else
      {
      Serial.print(boldRed + "\n\n        No logro suscribirse a " +  sub_topic + fontReset);
      }  
    Serial.print(boldGreen + "\n\n         Estado " +  mqtt_client.state() + fontReset);
    }
  }
  else
  {
    Serial.print(boldRed + "\n\n         Mqtt Client Connection Failed :( " + fontReset);
  }
}

void check_mqtt_connection()
{

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(Red + "\n\n         Ups WiFi Connection Failed :( ");
    Serial.println(" -> Restarting..." + fontReset);
    delay(15000);
    ESP.restart();
  }

  if (!mqtt_client.connected())
  {

    long now = millis();

    if (now - last_reconnect_attemp > 5000)
    {
      last_reconnect_attemp = millis();
      reconnect();
      last_reconnect_attemp = 0;
  
    }
  }
  else
  {
    mqtt_client.loop();
    process_sensors();
    send_data_to_broker();
    print_stats();
  }
}

bool get_device_config(){
  Serial.print(underlinePurple + "\n\n Getting device config from Webhook" + fontReset + Purple);
  delay(1000);
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
    Serial.print(boldRed + "\n\n Error in response"+ fontReset + "  error->" + response_code);    
    http.end();
    return false;
  }
  if(response_code == 200){
    String response_body = http.getString();
    Serial.print(boldGreen + "\n\n  Device config obtained sucessfully!!" + fontReset);
    deserializeJson(mqtt_data_doc,response_body);
    http.end();
 
    String mqtt_username = mqtt_data_doc["username"];
    String mqtt_password = mqtt_data_doc["password"];
    String mqtt_topic = mqtt_data_doc["topic"];
    int send_period = mqtt_data_doc["variables"][0]["variablePeriod"];
    Serial.println("\n" + mqtt_username);
    Serial.println("\n" + mqtt_password+"\n");
    Serial.println(send_period);    
    delay(1000);
    return true;
  }
  return false;
}



long lastStats = 0;

void print_stats()
{
  long now = millis();

  if (now - lastStats > 2000)
  {
    lastStats = millis();
    clear();

    Serial.print("\n");
    Serial.print(Purple + "\n╔══════════════════════════╗" + fontReset);
    Serial.print(Purple + "\n║       SYSTEM STATS       ║" + fontReset);
    Serial.print(Purple + "\n╚══════════════════════════╝" + fontReset);
    Serial.print("\n\n");
    Serial.print("\n\n");

    Serial.print(boldCyan + "#" + " \t Name" + " \t\t Var" + " \t\t Type" + " \t\t Count" + " \t\t Last V" + fontReset + "\n\n");

    for (int i = 0; i < mqtt_data_doc["variables"].size(); i++)
    {

      String variableFullName = mqtt_data_doc["variables"][i]["variableFullName"];
      String variable = mqtt_data_doc["variables"][i]["variable"];
      String variableType = mqtt_data_doc["variables"][i]["variableType"];
      String lastMsg = mqtt_data_doc["variables"][i]["last"];
      long counter = mqtt_data_doc["variables"][i]["counter"];

      Serial.println(String(i) + " \t " + variableFullName.substring(0,5) + " \t\t " + variable.substring(0,10) + " \t " + variableType.substring(0,5) + " \t\t " + String(counter).substring(0,10) + " \t\t " + lastMsg);
    }

    Serial.print(boldGreen + "\n\n Free RAM -> " + fontReset + ESP.getFreeHeap() + " Bytes");

    Serial.print(boldGreen + "\n\n Last Incomming Msg -> " + fontReset + last_received_msg);
  }
}

