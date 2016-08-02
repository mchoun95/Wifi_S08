#include <Wifi_S08.h>

#define SSID "EECS-ConfRooms"
#define PASSWD ""
#define POLLPERIOD 5000

ESP8266 wifi;

String MAC;
unsigned long lastRequest = 0;

void setup() {
  Serial.begin(115200);
  wifi.begin();
  MAC = wifi.getMAC();
  wifi.connectWifi(SSID, PASSWD);
  while (!wifi.isConnected()); //wait for connection
}

void loop() {
  if (wifi.hasResponse()) {
    String response = wifi.getResponse();
    Serial.print("RESPONSE: ");
    Serial.println(response);
  }

  if (!wifi.isBusy() && millis()-lastRequest > POLLPERIOD) {
    String domain = "iesc-s2.mit.edu";
    String path = "/hello.html"; 
    wifi.sendRequest(GET, domain, 80, path, "");
    lastRequest = millis();
  }
}
