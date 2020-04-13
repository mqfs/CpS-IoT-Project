#include "WiFiUtils.h"

WiFiUtils wiFiUtils;

void setup() {
  // put your setup code here, to run once:
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  if(wiFiUtils.connectNetwork("CpS-IoT", "arduino8")) {
    Serial.println("Connected");
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);
  }else{
    Serial.println("Fail to connect");
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  if(WiFi.status() == WL_CONNECTED) {
    delay(50);
    digitalWrite(LED_BUILTIN, HIGH);
    delayMicroseconds(1000);
    digitalWrite(LED_BUILTIN, LOW);
  }
  wiFiUtils.detectNewNeighbors(10000, 3000);
  wiFiUtils.listenForNdpPacket(10000, 500);
  wiFiUtils.listenForDtpPacket(10002, 500);
  wiFiUtils.printAllNeighbors();
  wiFiUtils.checkCurrentNeighborsConnectivity(10001, 10000, 30000);
  Serial.println("All connectable neighbors:");
  wiFiUtils.printAllNeighbors();
  wiFiUtils.getLocalMinVal(10003, 10002, 5000);
  wiFiUtils.printAllNeighborsVal();
  wiFiUtils.leaderElection(10004, 10002, 1000);
  wiFiUtils.informLeader(10004, 10002, 1000);
  wiFiUtils.printCurrentLeader();
  if(wiFiUtils.hasLeader) {
    Serial.println("Leader Election protocol time consumption: ");
    Serial.print("Start at: ");
    Serial.println(wiFiUtils.leaderElectionStartMillis);
    Serial.print("End at: ");
    Serial.println(wiFiUtils.leaderElectionEndMillis);
    Serial.print("Duration: ");
    Serial.println(wiFiUtils.leaderElectionEndMillis - wiFiUtils.leaderElectionStartMillis);
    delay(200);
    digitalWrite(LED_BUILTIN, HIGH);
    delayMicroseconds(4000);
    digitalWrite(LED_BUILTIN, LOW);
  }
}
