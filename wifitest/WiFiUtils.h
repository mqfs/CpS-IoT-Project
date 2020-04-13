#include "Arduino.h"
#include <SPI.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>

union ByteArrayToInt {
  int val;
  byte bytes[4];
};

union ByteArrayToInt16 {
  int16_t val;
  byte bytes[2];
};

struct Timer {
  unsigned long preMillis = 0;
  unsigned long curMillis = 0;
};

struct NdpHeader {
  byte type;
  byte code;
  byte checksum[2];
  byte flag;
  byte reserved[3];
  byte targetIPAddress[4];
  byte sourceLinklayerAddress[6];
};

struct DtpHeader {
  byte type;
  byte code;
  byte checksum[2];
  byte dataLength[2];
  byte reserved[10];
  byte sourceLinklayerAddress[6];
};

class WiFiUtils {
  public:
    WiFiUDP ndpUdp, dtpUdp;
    IPAddress *neighborsIPAddress;
    IPAddress leaderIPAddress;
    int leaderVal;
    int *neighborsVal;
    byte **neighborsMACAddress;
    bool *isLive;
    bool isCheckConnectivityCompleted, isGetLocalMinCompleted, isLeaderElectionCompleted, isLeader, hasLeader, hasRecordTime;
    IPAddress localIPAddress;
    int currentNeighborsSize, defaultNeighborsSize, totalNeighborsSize;
    int leaderElectionStartMillis, leaderElectionEndMillis;
    int randNum;
    
    WiFiUtils() {
      randomSeed(analogRead(A0));
      randNum = (int)random(0, 256);
      currentNeighborsSize = 0;
      defaultNeighborsSize = 20;
      totalNeighborsSize = 3;
      neighborsIPAddress = new IPAddress[defaultNeighborsSize];
      neighborsVal = new int[defaultNeighborsSize];
      for(int i = 0; i < defaultNeighborsSize; i++) {
        neighborsVal[i] = -1;
      }
      isLive = new bool[defaultNeighborsSize];
      isCheckConnectivityCompleted = false;
      isGetLocalMinCompleted = false;
      isLeaderElectionCompleted = false;
      isLeader = false;
      hasLeader = false;
      hasRecordTime = false;
      leaderElectionStartMillis = 0;
      leaderElectionEndMillis = 0;
      neighborsMACAddress = new byte *[defaultNeighborsSize];
      for(int i = 0; i < defaultNeighborsSize; i++) {
        neighborsMACAddress[i] = new byte[6];
      }
    }
    bool connectNetwork(String SSID, String password);
    bool sendUdpPacket(WiFiUDP *senderUdp, IPAddress ip, int port, byte data[], int size, int tryTimes);
    void encapsulateNdpHeader(NdpHeader *ndpHeader, byte type, byte flag, IPAddress targetIPAddress);
    void encapsulateDtpHeader(DtpHeader *dtpHeader, byte type, byte dataLength[]);
    void detectNewNeighbors(int port, unsigned long interval);
    void listenForNdpPacket(int port, unsigned long interval);
    void listenForDtpPacket(int port, unsigned long interval);
    void checkCurrentNeighborsConnectivity(int listenPort, int remotePort, unsigned long interval);
    int getLocalMinVal(int listenPort, int remotePort, unsigned long interval);
    void leaderElection(int listenPort, int remotePort, unsigned long interval);
    void informLeader(int listenPort, int remotePort, unsigned long interval);
    int16_t calculateChecksum(byte bytes[],int size);
    bool isIPAddressEqual(IPAddress addr1, IPAddress addr2);
    int compareIPAddress(IPAddress addr1, IPAddress addr2);
    void printAllNeighbors();
    void printAllNeighborsVal();
    void printCurrentLeader();
};
