#include <SSD1306.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "MAX30100_PulseOximeter.h"
#include "WiFi_Manager.h"
#include "MQTT.h" 

float Hr, SpO2, pre_Hr = 0, pre_SpO2 = 0;
uint16_t ir, red;
int count = 0, modee = 2; 
bool fl_display, fl_connect = false;
uint32_t tsLastReport = 0;

MAX30100 sensor;
PulseOximeter pox;
WiFiManager wm;
SSD1306  display(0x3C, 21, 22);
#define REPORTING_PERIOD_MS 2000

void onBeatDetected()
{
    Serial.println("Beat Detected!");
}

void CheckHealth(float Hr,float SpO2){
    String str_hr, str_sp; 
    if (Hr < 60) str_hr = "Low HeartRate";
    else if (Hr >= 60 && Hr <= 120) str_hr = "Nornal HeartRate";
    else str_hr = "High HeartRate";
    
    if (SpO2 < 94) str_sp = "Low SPO2";
    else if (SpO2 >= 94 && SpO2 <= 100) str_sp = "Nornal SPO2";
    else str_sp = "High SPO2";
    display.clear();
    display.drawString(0, 0, str_hr);
    display.drawString(0, 15, str_sp);
    delay(1000);
    display.display();
    initPox();
    sensor.update();
    sensor.getRawValues(&ir, &red);
}
 
void DisplayData(float Hr, float SpO2, bool fl){
    display.clear();
      if (fl) {
        if ( count >= 5) {
        display.drawString(0, 0, "HR: " + String(Hr) + " bpm");
        display.drawString(0, 15, "SpO2: " + String(SpO2) +" %");
        
        if (modee == 1) {
          display.drawString(0, 30, "Offline mode");
          display.drawString(0, 45, "Time: " + String(count) + "s");
        }
        else display.drawString(0, 30, "Online mode");
        display.display();
        delay(2000);
        CheckHealth(Hr, SpO2);
        }
        else {
        display.drawString(0, 30, "Detecting....");
        display.display();
        }
      }
      else {
        display.drawString(0, 0, "No detected");
        display.drawString(0, 30, "Put on sensor");
        display.display();
      }
}
float readHeartRate(bool fl){
    if (!fl) return 0;
    float temp = pox.getHeartRate();
    if (pre_Hr == 0) pre_Hr = temp;
    else {
      if (temp >= 120 || temp <= 60 ) temp = pre_Hr; 
      else pre_Hr = temp;
    }
    return temp;
}
float readSpO2(bool fl){
    if (!fl) return 0;
    float temp = pox.getSpO2();
    if (pre_SpO2 == 0) pre_SpO2 = temp;
    else {
      if (temp >= 100 || temp <= 90 ) temp = pre_SpO2; 
      else pre_SpO2 = temp;
    }
    return temp;
}
void Wifi_MQTT_Connect(){
    setup_WiFiManager(wm);
    Serial.setTimeout(500);
    client.setServer(MQTT_SERVER, MQTT_PORT);
    client.setCallback(callback);     
    connect_to_broker();
}
void initPox(){
    if (!pox.begin()) {
      Serial.println("ERROR: Failed to initialize pulse oximeter");
    } else {
      Serial.println("SUCCESS to initialize pulse oximeter");
      }
    pox.setOnBeatDetectedCallback(onBeatDetected); 
}
void SelectMode(int *modee){ 
    uint32_t ts = 0 ;
    // Mode 1:online, 2:offline
    count = 0; *modee = 0;
    initPox();
    Serial.println("Select mode: ");
    Serial.println("Put in 5s to select online mode");
    display.clear();
    display.drawString(0, 0, "Select mode:");
    display.drawString(0, 15, "Put in 5s to ");
    display.drawString(0, 30, "use online mode ");
    display.display();
    while (*modee == 0 && count < 10){
       sensor.update();
       sensor.getRawValues(&ir, &red);
       if (ir > 2000) *modee = 2;
       if (millis() - ts > 1000)
       {
          display.clear();
          display.drawString(0, 0, "Select mode:");
          display.drawString(0, 15, "Put in 10s to ");
          display.drawString(0, 30, "use online mode ");
          display.drawString(0, 45, String(++count) + "s");
          Serial.print("IR=");
          Serial.println(ir);
          display.display();
          ts = millis();
        }
    }
    if (*modee == 0) {   
      *modee = 1;  
      Serial.println("Offline mode");  
      display.clear();
      display.drawString(0, 20, "Offline mode");
      display.display();
      delay(1000);
      fl_connect = false ;
      initPox();
    }
    else {
      Serial.println("Online mode");
      display.clear();
      display.drawString(0, 0, "Online mode");
      display.display();
      if(!checkList_andConnect_WiFi()) {
        display.drawString(0, 15, "Connect to: ");
        display.drawString(0, 30, NAME_AP);
        display.drawString(0, 45, "192.168.4.1");
        display.display();
        wm.startConfigPortal(NAME_AP);
        if (WiFi.status() == WL_CONNECTED){
              display.clear();
              display.drawString(0, 30, "Wifi connected");
              display.display();
              delay(1000);
          }  
      }
      else {
        if (WiFi.status() == WL_CONNECTED){
              display.drawString(0, 45, "Wifi connected");
              display.display();
              delay(1000);
        }
      }
    }
    count = 0;
}

void setup() {
    Serial.begin(115200);
    display.init();
    display.setFont(ArialMT_Plain_16);
    display.flipScreenVertically();
    Wifi_MQTT_Connect();
    Serial.print("Initializing pulse oximeter..");
    initPox();
}
void loop() {
    if (modee == 2) {
        if (WiFi.status() != WL_CONNECTED){
            display.clear();
            display.drawString(0, 0, "Lost Wifi");
            display.drawString(0, 15, "Try to reconnect");
            display.display();
            fl_connect = false;
            delay(1000);
        }
   
        if(!checkList_andConnect_WiFi() && !fl_connect) {
            display.clear();
            display.drawString(0, 0, "Try to connect");
            display.drawString(0, 15, "WiFi Failed");
            display.display();
            delay(2000);
            SelectMode(&modee);
        }  
        if (!client.connected() && modee == 2){
            connect_to_broker();
            initPox();
        }
        else client.loop();
    }
    pox.update();       
    sensor.update();  
    sensor.getRawValues(&ir, &red);
    Hr = readHeartRate(fl_display);
    SpO2 = readSpO2(fl_display);
    if (millis() - tsLastReport > REPORTING_PERIOD_MS)
    {  
        // chuyen online mode
        if (count >= 10 && ir < 2000 && modee != 2) {
            modee = 2;
            count = 0;
            display.clear();
            display.drawString(0, 0, "Change to");
            display.drawString(0, 15, "Online mode");
            display.display();
            delay(1000);
       }
       if (ir > 2000) {
           fl_display = true; count++;
       } else {
           fl_display = false; count = 0;             
       }
        DisplayData(Hr, SpO2, fl_display);
        Serial.print("Heart rate: ");
        Serial.println(Hr);
        Serial.print("Pulse Oximeter: ");
        Serial.print(SpO2);
        Serial.println("%");
        Serial.print("IR=");
        Serial.println(ir);
        Serial.println("*********************************");        
        if (modee == 2 && fl_display && count >= 5) {
            String hr_str = "Heart beat rate: " + String(Hr) + "bpm";
            String spo2_str = "Pulse Oximeter: " + String(SpO2) + "%";
            String message = hr_str + "\n" + spo2_str;
            publish_message(message);
        }
        tsLastReport = millis();
    }    
}
