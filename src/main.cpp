#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#include "List.h"

// Wifi cofiguration Client and Access Point
const int WLAN_CLIENT = 0;
int wifiType = 0; // 0= Client 1= AP, Is set in setup()

const char *AP_ssid = "JimNMEA";        // ESP32 as AP
const char *CL_ssid = "MyWLAN";         // ESP32 as client in network

const char *AP_password = "password";   // AP password
const char *CL_password = "password";   // Client password

// Put IP address details here
IPAddress AP_local_ip(192, 168, 15, 1); // Static address for AP
IPAddress AP_gateway(192, 168, 15, 1);
IPAddress AP_subnet(255, 255, 255, 0);

IPAddress CL_local_ip(192, 168, 178, 211); // Static address for Client Network. Please adjust to your AP IP and DHCP range!
IPAddress CL_gateway(192, 168, 178, 1);
IPAddress CL_subnet(255, 255, 255, 0);


// Server kram
//WebServer web_server(80);   // Web server on TCP port 80
const size_t MaxClients = 10;
const uint16_t ServerPort = 2222; // Define the TCP port, where server sends data. Use this e.g. on OpenCPN. Use 39150 for Navionis AIS
WiFiServer server(ServerPort, MaxClients);
using tWiFiClientPtr = std::shared_ptr<WiFiClient>;
LinkedList<tWiFiClientPtr> clients;

// UPD broadcast for Navionics, OpenCPN, etc.
const char* udpAddress = "192.168.15.255"; // UDP broadcast address. Must be in the network of the ESP32
const int udpPort = 2000; // port 2000

void setupWifi()
{
  int wifi_retry = 0;
  if (WLAN_CLIENT == 1)
  {
    Serial.println("Start WLAN Client"); // WiFi Mode Client

    WiFi.config(CL_local_ip, CL_gateway, CL_subnet, CL_gateway);
    delay(100);
    WiFi.begin(CL_ssid, CL_password);

    while (WiFi.status() != WL_CONNECTED && wifi_retry < 20)
    { // Check connection, try 10 seconds
      wifi_retry++;
      delay(500);
      Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi client connected");
    Serial.println("IP client address: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    // Init wifi AP
    Serial.println("Start WLAN AP"); // WiFi Mode AP
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_ssid, AP_password);
    delay(100);
    WiFi.softAPConfig(AP_local_ip, AP_gateway, AP_subnet);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
    wifiType = 1;
  }

  server.begin();
}

//****************************************************************************************
void SendBufToClients(const char *buf) {
  for (auto it = clients.begin() ; it != clients.end(); it++) {
    if ( (*it) != NULL && (*it)->connected() ) {
      (*it)->println(buf);
    }
  }
}


//****************************************************************************************
void AddClient(WiFiClient & client) {
  Serial.println("New Client.");
  clients.push_back(tWiFiClientPtr(new WiFiClient(client)));
}


//****************************************************************************************
void StopClient(LinkedList<tWiFiClientPtr>::iterator & it) {
  Serial.println("Client Disconnected.");
  (*it)->stop();
  it = clients.erase(it);
}


//****************************************************************************************
void CheckConnections() {
  WiFiClient client = server.available();   // listen for incoming clients

  if ( client ) AddClient(client);

  for (auto it = clients.begin(); it != clients.end(); it++) {
    if ( (*it) != NULL ) {
      if ( !(*it)->connected() ) {
        StopClient(it);
      } else {
        if ( (*it)->available() ) {
          char c = (*it)->read();
          if ( c == 0x03 ) StopClient(it); // Close connection by ctrl-c
        }
      }
    } else {
      it = clients.erase(it); // Should have been erased by StopClient
    }
  }
}


void setup()
{
  // put your setup code here, to run once:

  Serial.begin(115200);
  Serial1.begin(38400, SERIAL_8N1, GPIO_NUM_4, GPIO_NUM_16);
  Serial.println("start...");
  
  setupWifi();
  // Wifi init
  
}

ulong lastTime = 0;

char buffer[150];
int bufIter = 0;

void loop()
{
  int incomingByte = 0;
  while (Serial1.available() > 0)
  {
    u_int8_t b = Serial1.read();
    Serial.write(b);
    buffer[bufIter++] = b;
    if (b == '\n')
    {
      buffer[bufIter] = '\0';
      SendBufToClients(buffer);
      bufIter = 0;
      Serial.printf("Send tcp message\n");
    }
  }

  while (Serial.available() > 0)
  {
    short b = Serial.read();
    Serial1.write(b);
    Serial.print(b);
  }

  //CheckWiFi();
  CheckConnections();
  /*if(lastTime + 3000 < millis())
  {
    Serial.printf("watchdog: %u\r", lastTime);
    lastTime = millis();
  }*/
}
