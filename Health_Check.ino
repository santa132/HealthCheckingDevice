#include <SSD1306.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "WiFi_Manager.h"
#include "MQTT.h" 
#include "myMax30105.h"

uint32_t ir, red;
int count = 0, modee = 2; 
bool fl_display, fl_connect = false;
uint32_t tsLastReport = 0;

MAX30105 sensor;
WiFiManager wm;
SSD1306  display(0x3C, 21, 22);
#define REPORTING_PERIOD_MS 1000

int32_t hr, spO2, pre_hr = 0, pre_spO2 = 0;

void CheckHealth(int32_t hr,int32_t spO2)
{
    String str_hr, str_sp; 
    if (hr < 60) str_hr = "Low HeartRate";
    else if (hr >= 60 && hr <= 120) str_hr = "Nornal HeartRate";
    else str_hr = "High HeartRate";
    
    if (spO2 < 94) str_sp = "Low SPO2";
    else if (spO2 >= 94 && spO2 <= 100) str_sp = "Nornal SPO2";
    else str_sp = "High SPO2";
    display.clear();
    display.drawString(0, 0, str_hr);
    display.drawString(0, 15, str_sp);
    delay(1000);
    display.display();
    initMax();
    sensor.check();
    getIrRed(sensor, ir, red);
}
 
void DisplayData(float hr, float spO2, bool fl)
{
    display.clear();
    if (fl) {
      if (count >= 5) {
        display.drawString(0, 0, "HR: " + String(hr) + " bpm");
        display.drawString(0, 15, "SpO2: " + String(spO2) +" %");
        
        if (modee == 1) {
          display.drawString(0, 30, "Offline mode");
          display.drawString(0, 45, "Time: " + String(count) + "s");
        }
        else {
          display.drawString(0, 30, "Online mode");
        }
      
        display.display();
        delay(1000);
        CheckHealth(hr, spO2);
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
int32_t processHeartRate(bool fl, int32_t hr){
    if (!fl) return 0;
    if (pre_hr == 0) pre_hr = hr;
    else {
      if (hr >= 120 || hr <= 60 ) hr = pre_hr; 
      else pre_hr = hr;
    }
    return hr;
}
int32_t processSpO2(bool fl, int32_t spO2){
    if (!fl) return 0;
    if (pre_spO2 == 0) pre_spO2 = spO2;
    else {
      if (spO2 >= 100 || spO2 <= 90 ) spO2 = pre_spO2; 
      else pre_spO2 = spO2;
    }
    return spO2;
}
void Wifi_MQTT_Connect(){
    setup_WiFiManager(wm);
    Serial.setTimeout(500);
    client.setServer(MQTT_SERVER, MQTT_PORT);
    client.setCallback(callback);     
    connect_to_broker();
}
void initMax(){
    if (!sensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
    {
      Serial.println(F("MAX30105 was not found. Please check wiring/power."));
      while (1);
    }
    byte ledBrightness = 100; //Options: 0=Off to 255=50mA
    byte sampleAverage = 4; //Options: 1, 2, 4, 8, 16, 32
    byte ledMode = 2; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
    byte sampleRate = 100; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
    int pulseWidth = 411; //Options: 69, 118, 215, 411
    int adcRange = 4096; //Options: 2048, 4096, 8192, 16384
  
    sensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings
}
void SelectMode(int *modee){ 
    uint32_t ts = 0 ;
    // Mode 1:online, 2:offline
    count = 0; *modee = 0;
    initMax();
    Serial.println("Select mode: ");
    Serial.println("Put in 5s to select online mode");
    display.clear();
    display.drawString(0, 0, "Select mode:");
    display.drawString(0, 15, "Put in 5s to ");
    display.drawString(0, 30, "use online mode ");
    display.display();
    while (*modee == 0 && count < 10){
       sensor.check();
       ir = sensor.getIR();
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
      initMax();
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
    initMax();
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
           initMax();
       }
       else client.loop();
   }
    sensor.check();  
    getSensorMax(sensor, hr, spO2);
    getIrRed(sensor, ir, red);
    hr = processHeartRate(fl_display, hr);
    spO2 = processSpO2(fl_display, spO2);

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
        DisplayData(hr, spO2, fl_display);
        Serial.print("Heart rate: ");
        Serial.println(hr);
        Serial.print("Pulse Oximeter: ");
        Serial.print(spO2);
        Serial.println("%");
        Serial.print("IR=");
        Serial.println(ir);
        Serial.println("*********************************");        
       if (modee == 2 && fl_display && count >= 5) {
           String hr_str = "Heart beat rate: " + String(hr) + "bpm";
           String spo2_str = "Pulse Oximeter: " + String(spO2) + "%";
           String message = hr_str + "\n" + spo2_str;
           publish_message(message);
       }
        tsLastReport = millis();
    }    
}
