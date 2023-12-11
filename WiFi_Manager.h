#include <WiFi.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#define NAME_AP "HealthCheck"
#define ESP_DRD_USE_SPIFFS true
#define JSON_CONFIG_FILE "/wifi_config.json"
#define MAX_WIFI_NETWORKS 5

struct WifiNetwork
{
  String ssid;
  String password;
};

WifiNetwork wifiNetworks[MAX_WIFI_NETWORKS];
int numWifiNetworks = 0;
int cursorNumWifiNetworks = 0;
void configModeCallback(WiFiManager *myWiFiManager)
{
  Serial.println("Entered Configuration Mode");
  Serial.print("Config SSID: ");
  Serial.println(myWiFiManager->getConfigPortalSSID());
  Serial.print("Config IP Address: ");
  Serial.println(WiFi.softAPIP());
}

void saveConfigFile()
{
  DynamicJsonDocument json(1024);
  JsonArray wifiNetworksArray = json.createNestedArray("networks");
  for (int i = 0; i < numWifiNetworks; i++)
  {
    JsonObject wifiNetwork = wifiNetworksArray.createNestedObject();
    wifiNetwork["ssid"] = wifiNetworks[i].ssid;
    wifiNetwork["password"] = wifiNetworks[i].password;
  }

  File configFile = SPIFFS.open(JSON_CONFIG_FILE, "w");
  if (!configFile)
  {
    Serial.println("Failed to open config file for writing");
    return;
  }

  serializeJsonPretty(json, configFile);
  configFile.close();
}
void saveConfigCallback()
{
  bool isExisted = false;
  for (int i = 0; i < numWifiNetworks; i++)
  {
    if (wifiNetworks[i].ssid == WiFi.SSID() && wifiNetworks[i].password == WiFi.psk())
    {
      isExisted = true;
    }
  }
  if (!isExisted)
  {
    if (numWifiNetworks < MAX_WIFI_NETWORKS)
    {
      cursorNumWifiNetworks = ++numWifiNetworks;

      wifiNetworks[cursorNumWifiNetworks - 1].ssid = WiFi.SSID();
      wifiNetworks[cursorNumWifiNetworks - 1].password = WiFi.psk();
    }
    else
    {
      if (cursorNumWifiNetworks == MAX_WIFI_NETWORKS)
      {
        cursorNumWifiNetworks = 0;
      }
      cursorNumWifiNetworks++;
      wifiNetworks[cursorNumWifiNetworks - 1].ssid = WiFi.SSID();
      wifiNetworks[cursorNumWifiNetworks - 1].password = WiFi.psk();
    }
  }
  saveConfigFile();
}
void loadConfigFile()
{
  if (SPIFFS.begin(true))
  {
    if (SPIFFS.exists(JSON_CONFIG_FILE))
    {
      File configFile = SPIFFS.open(JSON_CONFIG_FILE, "r");
      if (configFile)
      {
        DynamicJsonDocument json(1024);
        DeserializationError error = deserializeJson(json, configFile);
        if (!error)
        {
          JsonArray wifiNetworksArray = json["networks"];
          int i = 0;
          for (const auto &wifiNetwork : wifiNetworksArray)
          {
            if (wifiNetwork["ssid"].as<String>().isEmpty() && wifiNetwork["password"].as<String>().isEmpty())
            {
              break; // Dừng lại nếu cả ssid và password đều rỗng
            }
            wifiNetworks[i].ssid = wifiNetwork["ssid"].as<String>();
            wifiNetworks[i].password = wifiNetwork["password"].as<String>();
            i++;
            if (i >= MAX_WIFI_NETWORKS)
            {
              break;
            }
          }
          numWifiNetworks = i;
        }
        else
        {
          Serial.println("Failed to parse config file");
        }
        configFile.close();
      }
    }
  }
  else
  {
    Serial.println("Failed to mount FS");
  }
}
bool checkList_andConnect_WiFi()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    bool connected = false;
    for (int i = 0; i < numWifiNetworks; i++)
    {
      Serial.print("Trying to connect to network: ");
      Serial.println(wifiNetworks[i].ssid);
      Serial.print("Password: ");
      Serial.println(wifiNetworks[i].password);
      Serial.println();
      WiFi.begin(wifiNetworks[i].ssid.c_str(), wifiNetworks[i].password.c_str());
      int connectStatus = WiFi.waitForConnectResult();

      if (connectStatus == WL_CONNECTED)
      {
        Serial.println("Connected to network: " + wifiNetworks[i].ssid);
        connected = true;
        break;
      }
      else
      {
        Serial.println("Connection from file failed");
        WiFi.disconnect(true);
        //      delay(1000);
      }
    }
    return connected;
  }
  else
    return true;
}

bool checkWiFiConnection(WiFiManager &wm)
{
  //     Serial.println("*******Saved WiFi networks:");
  //     for (int i = 0; i < numWifiNetworks; i++) {
  //       Serial.printf("%d. SSID: ", i+1);
  //       Serial.print(wifiNetworks[i].ssid + "   ");
  //       Serial.print("Password: ");
  //       Serial.println(wifiNetworks[i].password);
  //       Serial.println();
  //     }
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi disconnected, reconnecting...");
    loadConfigFile();
    bool isConnected = checkList_andConnect_WiFi();

    if (!isConnected)
    {
      wm.startConfigPortal(NAME_AP); // bật pop up cấu hình wifi
      int connectStatus = WiFi.waitForConnectResult();
      if (connectStatus == WL_CONNECTED)
      {
        Serial.println("Connected to network: " + WiFi.SSID());
        return true;
      }
      else
      {
        Serial.println("Connected failed...");
        return false;
      }
    }
  }
  else
  {
    //    Serial.println("Connecting to "+ WiFi.SSID()+"........");
    return true;
  }
}
void setup_WiFiManager(WiFiManager &wm)
{
  // Uncomment if we need to format filesystem
  // SPIFFS.format();
  loadConfigFile();

  // Kiểm tra và kết nối các mạng WiFi đã được lưu
  bool isConnected = checkList_andConnect_WiFi();

  // Nếu không có mạng WiFi phù hợp nào, tiếp tục với chế độ cấu hình
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.setAPCallback(configModeCallback);

  if (!isConnected)
  {
    wm.startConfigPortal(NAME_AP);
  }
}
