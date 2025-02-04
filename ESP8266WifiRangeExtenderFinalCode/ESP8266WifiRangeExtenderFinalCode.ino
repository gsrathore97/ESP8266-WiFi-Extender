#define NAPT 1000
#define NAPT_PORT 10

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <lwip/napt.h>
#include <lwip/dns.h>
#include "EEPROMHelper.h"
#include "HomePage.h"

EEPROMHelper eepromHelper;
ESP8266WebServer webServer(80);

unsigned long rstPressStartBeforeTime = 0;

void setup() {
  Serial.begin(115200);
  eepromHelper.begin();
  pinMode(D4, OUTPUT);
  pinMode(D3, INPUT);

  // Check if Wi-Fi credentials are available
  if (eepromHelper.getSSID() != "") {
    // Attempt to connect to Wi-Fi
    unsigned long wifiConnectStartTime = millis();
    bool connected = false;
    while (millis() - wifiConnectStartTime <= 60000) { // Try for 60 seconds
      if (connectWiFi(eepromHelper.getSSID(), eepromHelper.getPass())) {
        connected = true;
        break;
      }
      delay(100);
    }

    if (connected) {
      // Successfully connected to Wi-Fi, set up extender
      setupExtender(eepromHelper.getSSID(), eepromHelper.getPass(), eepromHelper.getAPSSID(), eepromHelper.getAPPass());
      return;
    }
  }

  // If Wi-Fi credentials are not available or connection fails, start AP mode
  startAPMode();
}

void loop() {
  // Handle hardware reset button
  if (!digitalRead(D3)) {
    if (millis() - rstPressStartBeforeTime >= 3000) {
      eepromHelper.clear();
      delay(50);
      ESP.restart();
    }
  } else {
    rstPressStartBeforeTime = millis();
  }

  // Check Wi-Fi connection status
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, restarting...");
    delay(100);
    ESP.restart();
  }
}

void startAPMode() {
  Serial.println("Starting AP mode...");
  WiFi.softAP("ConfigAP " + (String)ESP.getChipId(), "8860055120");
  webServer.on("/", handleHomePage);
  webServer.on("/submit", handleSubmit);
  webServer.begin();

  unsigned long apModeStartTime = millis();
  unsigned long lastToggleTime = millis();

  while (millis() - apModeStartTime <= 90000) { // Run AP mode for 120 seconds
    webServer.handleClient();

    // Toggle LED every 400ms to indicate AP mode
    if (millis() - lastToggleTime >= 400) {
      digitalWrite(D4, !digitalRead(D4));
      lastToggleTime = millis();
    }
  }

  // AP mode timeout, restart the device
  Serial.println("AP mode timeout, restarting...");
  delay(100);
  ESP.restart();
}

void setupExtender(String ssid, String pass, String apSSID, String apPass) {
  Serial.println("Setting up Wi-Fi extender...");
  auto& server = WiFi.softAPDhcpServer();
  server.setDns(WiFi.dnsIP(0));
  WiFi.softAPConfig(IPAddress(172, 217, 28, 254), IPAddress(172, 217, 28, 254), IPAddress(255, 255, 255, 0));
  WiFi.softAP(apSSID, apPass);
  err_t ret = ip_napt_init(NAPT, NAPT_PORT);
  
  if (ret == ERR_OK) {
    ret = ip_napt_enable_no(SOFTAP_IF, 1);
    if (ret == ERR_OK) {
      digitalWrite(D4, LOW); // LED off when extender is ready
      Serial.println("Extender setup complete.");
    }
  }

  if (ret != ERR_OK) {
    digitalWrite(D4, HIGH); // LED on if NAPT initialization fails
    Serial.println("Extender setup failed.");
  }
}

bool connectWiFi(String ssid, String pass) {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  unsigned long wifiConnectStartTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - wifiConnectStartTime > 60000) { // Timeout after 60 seconds
      Serial.println("Wi-Fi connection failed.");
      return false;
    }
    digitalWrite(D4, !digitalRead(D4)); // Toggle LED while connecting
    delay(100);
  }

  digitalWrite(D4, LOW); // LED off when connected
  Serial.println("Wi-Fi connected.");
  return true;
}

void handleSubmit() {
  if (webServer.hasArg("ssid") && webServer.hasArg("pass") && webServer.hasArg("ap_ssid") && webServer.hasArg("ap_pass")) {
    eepromHelper.setSSID(webServer.arg("ssid"));
    eepromHelper.setPass(webServer.arg("pass"));
    eepromHelper.setAPSSID(webServer.arg("ap_ssid"));
    eepromHelper.setAPPass(webServer.arg("ap_pass"));
    delay(50);
    ESP.restart();
  }
}

void handleHomePage() {
  String homePage = FPSTR(HOME_PAGE);
  webServer.send(200, "text/html", homePage);
}