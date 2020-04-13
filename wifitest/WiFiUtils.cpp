#include "WiFiUtils.h"

bool WiFiUtils::connectNetwork(String SSID, String password) {
  WiFi.end();
  int status = WL_IDLE_STATUS, cnt = 5;
  char SSIDChars[40], passwordChars[40];
  SSID.toCharArray(SSIDChars, SSID.length() + 1);
  password.toCharArray(passwordChars, password.length() + 1);
  while (cnt > 0 && status != WL_CONNECTED) {
    status = WiFi.begin(SSIDChars, passwordChars);
    delay(3000);
    cnt--;
  }
  if(status != WL_CONNECTED) {
    return false;
  }
  localIPAddress = WiFi.localIP();
  return true;
}

bool WiFiUtils::sendUdpPacket(WiFiUDP *senderUdp, IPAddress ip, int port, byte data[], int size, int tryTimes) {
  int status = -1;
  for(int i = 0; i < tryTimes && status != 1; i++) {
    senderUdp->beginPacket(ip, port);
    senderUdp->write(data, size);
    status = senderUdp->endPacket();
  }
  return status == 1 ? true : false;
}

void WiFiUtils::encapsulateNdpHeader(NdpHeader *ndpHeader, byte type, byte flag, IPAddress targetIPAddress) {
  int ndpHeaderSize = sizeof(NdpHeader);
  byte arr[6], temp[ndpHeaderSize];
  ndpHeader->type = type;
  ndpHeader->code = (byte)0;
  ndpHeader->checksum[0] = (byte)0;
  ndpHeader->checksum[1] = (byte)0;
  ndpHeader->flag = flag;
  for(int i = 0; i < 3; i++) {
    ndpHeader->reserved[i] = (byte)0;
  }
  for(int i = 0; i < 4; i++) {
    ndpHeader->targetIPAddress[i] = (byte)targetIPAddress[i];
  }
  WiFi.macAddress(arr);
  for(int i = 0; i < 6; i++) {
    ndpHeader->sourceLinklayerAddress[i] = arr[i];
  }
  memcpy(temp, ndpHeader, ndpHeaderSize);
  int checksum = calculateChecksum(temp, ndpHeaderSize);
  ByteArrayToInt16 converter;
  converter.val = checksum;
  ndpHeader->checksum[0] = converter.bytes[1];
  ndpHeader->checksum[1] = converter.bytes[0];
}

void WiFiUtils::encapsulateDtpHeader(DtpHeader *dtpHeader, byte type, byte dataLength[]) {
  int dtpHeaderSize = sizeof(DtpHeader);
  byte arr[6], temp[dtpHeaderSize];
  dtpHeader->type = type;
  dtpHeader->code = (byte)0;
  dtpHeader->checksum[0] = (byte)0;
  dtpHeader->checksum[1] = (byte)0;
  dtpHeader->dataLength[0] = dataLength[0];
  dtpHeader->dataLength[1] = dataLength[1];
  for(int i = 0; i < 10; i++) {
    dtpHeader->reserved[i] = (byte)0;
  }
  WiFi.macAddress(arr);
  for(int i = 0; i < 6; i++) {
    dtpHeader->sourceLinklayerAddress[i] = arr[i];
  }
  memcpy(temp, dtpHeader, dtpHeaderSize);
  int16_t checksum = calculateChecksum(temp, dtpHeaderSize);
  ByteArrayToInt16 converter;
  converter.val = checksum;
  dtpHeader->checksum[0] = converter.bytes[1];
  dtpHeader->checksum[1] = converter.bytes[0];
}

void WiFiUtils::detectNewNeighbors(int port, unsigned long interval) {
  static Timer *timer;
  if(timer == nullptr) {
    timer = new Timer;
  }
  timer->curMillis = millis();
  if(timer->preMillis == 0) {
    timer->preMillis = millis();
    timer->curMillis = timer->preMillis + interval;
  }
  if(timer->curMillis < timer->preMillis || timer->curMillis - timer->preMillis < interval) {
    return;
  }
  timer->preMillis = timer->curMillis;
  int ndpHeaderSize = sizeof(NdpHeader);
  byte temp[ndpHeaderSize];
  NdpHeader ndpHeader;
  IPAddress broadcastip(255, 255, 255, 255);
  encapsulateNdpHeader(&ndpHeader, (byte)135, (byte)0 << 5, broadcastip);
  memcpy(temp, &ndpHeader, ndpHeaderSize);
  sendUdpPacket(&ndpUdp, broadcastip, port, temp, ndpHeaderSize, 5);
}

void WiFiUtils::listenForNdpPacket(int port, unsigned long interval) {
  static Timer *timer;
  static bool *isListening;
  if(isListening == nullptr) {
    isListening = new bool;
    *isListening = false;
  }
  if(timer == nullptr) {
    timer = new Timer;
  }
  timer->curMillis = millis();
  if(timer->preMillis == 0) {
    timer->preMillis = millis();
    timer->curMillis = timer->preMillis + interval;
  }
  if(timer->curMillis < timer->preMillis || timer->curMillis - timer->preMillis < interval) {
    return;
  }
  timer->preMillis = timer->curMillis;
  int size;
  byte *pByte;
  NdpHeader ndpHeader;
  if(!*isListening) {
    ndpUdp.begin(port);
    *isListening = !*isListening;
  }
  for(int i = 0; i < 10; i++) {
    size = ndpUdp.parsePacket();
    if(size != 0) {
      pByte = (byte *)malloc(sizeof(byte) * size);
      ndpUdp.read(pByte, size);
      if(calculateChecksum(pByte, sizeof(NdpHeader)) != 0) {
        free(pByte);
        continue;
      }
      byte type;
      memcpy(&type, pByte, 1);
      if(type == (byte)135 || type == (byte)136) {
        memcpy(&ndpHeader, pByte, size);
      }else{
        free(pByte);
        continue;
      }
      if(ndpHeader.type == (byte)135 && (ndpHeader.targetIPAddress[0] == 0xFF) && (ndpHeader.targetIPAddress[1] == 0xFF) && (ndpHeader.targetIPAddress[2] == 0xFF) && (ndpHeader.targetIPAddress[3] == 0xFF)) {//neighbor detecting reply
        int ndpHeaderSize = sizeof(NdpHeader);
        byte temp[ndpHeaderSize];
        NdpHeader sendNdpHeader;
        IPAddress remoteIPAddress = ndpUdp.remoteIP();
        encapsulateNdpHeader(&sendNdpHeader, (byte)136, (byte)2 << 5, remoteIPAddress);
        memcpy(temp, &sendNdpHeader, ndpHeaderSize);
        sendUdpPacket(&ndpUdp, remoteIPAddress, port, temp, ndpHeaderSize, 5);
      }else if(ndpHeader.type == (byte)135 && (ndpHeader.targetIPAddress[0] == localIPAddress[0]) && (ndpHeader.targetIPAddress[1] == localIPAddress[1]) && (ndpHeader.targetIPAddress[2] == localIPAddress[2]) && (ndpHeader.targetIPAddress[3] == localIPAddress[3])) {//ping
        int ndpHeaderSize = sizeof(NdpHeader);
        byte temp[ndpHeaderSize];
        NdpHeader sendNdpHeader;
        IPAddress remoteIPAddress = ndpUdp.remoteIP();
        encapsulateNdpHeader(&sendNdpHeader, (byte)136, (byte)0 << 5, remoteIPAddress);
        memcpy(temp, &sendNdpHeader, ndpHeaderSize);
        sendUdpPacket(&ndpUdp, remoteIPAddress, ndpUdp.remotePort(), temp, ndpHeaderSize, 5);
      }else if(ndpHeader.type == (byte)136 && (ndpHeader.targetIPAddress[0] == localIPAddress[0]) && (ndpHeader.targetIPAddress[1] == localIPAddress[1]) && (ndpHeader.targetIPAddress[2] == localIPAddress[2]) && (ndpHeader.targetIPAddress[3] == localIPAddress[3])) {//neighbor detecting receive
        if(ndpHeader.flag == ((byte)2 << 5)) {
          bool isEqual = false;
          for(int i = 0; i < currentNeighborsSize; i++) {
            for(int j = 0, cnt = 0; j < 6; j++) {
              if(neighborsMACAddress[i][j] == ndpHeader.sourceLinklayerAddress[j]) {
                cnt++;
                if(cnt == 6) {
                  isEqual = !isEqual;
                }
              }
            }
            if(isEqual) {
              neighborsIPAddress[i] = ndpUdp.remoteIP();
              break;
            }
          }
          if(!isEqual) {
            memcpy(neighborsMACAddress[currentNeighborsSize], ndpHeader.sourceLinklayerAddress, 6);
            isLive[currentNeighborsSize] = true;
            neighborsIPAddress[currentNeighborsSize++] = ndpUdp.remoteIP();
            isCheckConnectivityCompleted = false;
          }
        }else{
          
        }
      }
      free(pByte);
    }
  }
}

void WiFiUtils::listenForDtpPacket(int port, unsigned long interval) {
  static Timer *timer;
  static bool *isListening;
  if(isListening == nullptr) {
    isListening = new bool;
    *isListening = false;
  }
  if(timer == nullptr) {
    timer = new Timer;
  }
  timer->curMillis = millis();
  if(timer->preMillis == 0) {
    timer->preMillis = millis();
    timer->curMillis = timer->preMillis + interval;
  }
  if(timer->curMillis < timer->preMillis || timer->curMillis - timer->preMillis < interval) {
    return;
  }
  timer->preMillis = timer->curMillis;
  int size;
  byte *pByte, *pData;
  DtpHeader dtpHeader;
  if(!*isListening) {
    dtpUdp.begin(port);
    *isListening = !*isListening;
  }
  for(int i = 0; i < 10; i++) {
    size = dtpUdp.parsePacket();
    if(size != 0) {
      pByte = (byte *)malloc(sizeof(byte) * size);
      dtpUdp.read(pByte, size);
      if(calculateChecksum(pByte, sizeof(DtpHeader)) != 0) {
        free(pByte);
        continue;
      }
      byte type;
      memcpy(&type, pByte, 1);
      if(type == (byte)0) {
        memcpy(&dtpHeader, pByte, sizeof(DtpHeader));
      }else if(type == (byte)2){
        memcpy(&dtpHeader, pByte, sizeof(DtpHeader));
        ByteArrayToInt converter;
        for(int j = 0; j < 2; j++) {
          converter.bytes[j] = dtpHeader.dataLength[j];
        }
        converter.bytes[2] = 0x0;
        converter.bytes[3] = 0x0;
        pData = (byte *)malloc(sizeof(byte) * converter.val);
        memcpy(pData, pByte + sizeof(DtpHeader), converter.val);
      }else{
        free(pByte);
        continue;
      }
      if(dtpHeader.type == (byte)0) {//find local min
        int dtpHeaderSize = sizeof(DtpHeader);
        byte temp[dtpHeaderSize + sizeof(int)];
        DtpHeader sendDtpHeader;
        IPAddress remoteIPAddress = dtpUdp.remoteIP();
        ByteArrayToInt intConverter;
        intConverter.val = sizeof(int);
        encapsulateDtpHeader(&sendDtpHeader, (byte)1, intConverter.bytes);
        memcpy(temp, &sendDtpHeader, dtpHeaderSize);
        intConverter.val = randNum;
        for(int j = 0; j < sizeof(int); j++) {
          temp[dtpHeaderSize + j] = intConverter.bytes[j];
        }
        sendUdpPacket(&dtpUdp, remoteIPAddress, dtpUdp.remotePort(), temp, dtpHeaderSize + sizeof(int), 5);
      }else if(dtpHeader.type == (byte)2){//inform leader
        ByteArrayToInt intConverter;
        for(int j = 0; j < 4; j++) {
          intConverter.bytes[j] = pData[j];
        }
        hasLeader = true;
        isLeaderElectionCompleted = true;
        leaderIPAddress = dtpUdp.remoteIP();
        leaderVal = intConverter.val;
        free(pData);
        if(!hasRecordTime) {
          leaderElectionEndMillis = millis();
          hasRecordTime = true;
        }
      }else{
        
      }
      free(pByte);
    }
  }
}

void WiFiUtils::checkCurrentNeighborsConnectivity(int listenPort, int remotePort, unsigned long interval) {
  if(currentNeighborsSize < totalNeighborsSize) {
    return;
  }
  static Timer *timer;
  if(timer == nullptr) {
    timer = new Timer;
  }
  timer->curMillis = millis();
  if(timer->preMillis == 0) {
    timer->preMillis = millis();
    timer->curMillis = timer->preMillis + interval;
  }
  if(timer->curMillis < timer->preMillis || timer->curMillis - timer->preMillis < interval) {
    return;
  }
  timer->preMillis = timer->curMillis;
  isCheckConnectivityCompleted = false;
  int size;
  NdpHeader ndpHeader;
  WiFiUDP pingUdp;
  pingUdp.begin(listenPort);
  int ndpHeaderSize = sizeof(NdpHeader);
  byte temp[ndpHeaderSize], *pByte;
  for(int i = 0; i < currentNeighborsSize; i++) {
    isLive[i] = false;
    encapsulateNdpHeader(&ndpHeader, (byte)135, (byte)0 << 5, neighborsIPAddress[i]);
    memcpy(temp, &ndpHeader, ndpHeaderSize);
    sendUdpPacket(&pingUdp, neighborsIPAddress[i], remotePort, temp, ndpHeaderSize, 5);
  }
  for(int c = 0; c < 8; c++) {
    listenForNdpPacket(10000, 500);
    listenForDtpPacket(10002, 500);
    for(int i = 0; i < 10; i++) {
      size = pingUdp.parsePacket();
      if(size != 0) {
        pByte = (byte *)malloc(sizeof(byte) * size);
        pingUdp.read(pByte, size);
        if(calculateChecksum(pByte, sizeof(NdpHeader)) != 0) {
          free(pByte);
          continue;
        }
        memcpy(&ndpHeader, pByte, size);
        if(ndpHeader.type == (byte)136 && (ndpHeader.targetIPAddress[0] == localIPAddress[0]) && (ndpHeader.targetIPAddress[1] == localIPAddress[1]) && (ndpHeader.targetIPAddress[2] == localIPAddress[2]) && (ndpHeader.targetIPAddress[3] == localIPAddress[3])) {
          bool isEqual = false;
          for(int j = 0; j < currentNeighborsSize; j++) {
            for(int k = 0, cnt = 0; k < 6; k++) {
              if(neighborsMACAddress[j][k] == ndpHeader.sourceLinklayerAddress[k]) {
                cnt++;
                if(cnt == 6) {
                  isEqual = !isEqual;
                }
              }
            }
            if(isEqual) {
              IPAddress tempip = pingUdp.remoteIP();
              isEqual = !isEqual;
              for(int k = 0, cnt = 0; k < 4; k++) {
                if(tempip[k] == neighborsIPAddress[i][k]) {
                  cnt++;
                  if(cnt == 4) {
                    isEqual = !isEqual;
                  }
                }
              }
              if(isEqual) {
                isLive[j] = true;
              }
              break;
            }
          }
        }
        free(pByte);
      }
    }
    delay(1000);
  }
  pingUdp.flush();
  pingUdp.stop();
//  int count = 0;
//  for(int i = 0, j = currentNeighborsSize - 1; i < currentNeighborsSize; i++) {//delete neighbors which not live
//    if(!isLive[i]) {
//      count++;
//      for(; j > i && !isLive[j]; j--);
//      if(isLive[j]) {
//        neighborsIPAddress[i] = neighborsIPAddress[j];
//        for(int k = 0; k < 6; k++) {
//          neighborsMACAddress[i][k] = neighborsMACAddress[j][k];
//        }
//        neighborsVal[i] = neighborsVal[j];
//        isLive[i] = true;
//      }
//    }
//  }
//  currentNeighborsSize -= count;
  isCheckConnectivityCompleted = true;
}

int WiFiUtils::getLocalMinVal(int listenPort, int remotePort, unsigned long interval) {
  if(currentNeighborsSize < totalNeighborsSize || !isCheckConnectivityCompleted || isGetLocalMinCompleted) {
    return -1;
  }
  static Timer *timer;
  if(timer == nullptr) {
    timer = new Timer;
  }
  timer->curMillis = millis();
  if(timer->preMillis == 0) {
    timer->preMillis = millis();
    timer->curMillis = timer->preMillis + interval;
  }
  if(timer->curMillis < timer->preMillis || timer->curMillis - timer->preMillis < interval) {
    return -1;
  }
  timer->preMillis = timer->curMillis;
  isGetLocalMinCompleted = false;
  int size, minVal = randNum;
  DtpHeader dtpHeader;
  WiFiUDP localMinValUdp;
  localMinValUdp.begin(listenPort);
  int dtpHeaderSize = sizeof(DtpHeader);
  byte temp[dtpHeaderSize], *pByte, *pData;
  for(int i = 0; i < currentNeighborsSize; i++) {
    encapsulateDtpHeader(&dtpHeader, (byte)0, 0);
    memcpy(temp, &dtpHeader, dtpHeaderSize);
    sendUdpPacket(&localMinValUdp, neighborsIPAddress[i], remotePort, temp, dtpHeaderSize, 5);
  }
  for(int c = 0; c < 8; c++) {
    listenForNdpPacket(10000, 500);
    listenForDtpPacket(10002, 500);
    for(int i = 0; i < 10; i++) {
      size = localMinValUdp.parsePacket();
      if(size != 0) {
        pByte = (byte *)malloc(sizeof(byte) * size);
        localMinValUdp.read(pByte, size);
        if(calculateChecksum(pByte, sizeof(DtpHeader)) != 0) {
          free(pByte);
          continue;
        }
        memcpy(&dtpHeader, pByte, sizeof(DtpHeader));
        if(dtpHeader.type == (byte)1) {
          bool isEqual = false;
          for(int j = 0; j < currentNeighborsSize; j++) {
            for(int k = 0, cnt = 0; k < 6; k++) {
              if(neighborsMACAddress[j][k] == dtpHeader.sourceLinklayerAddress[k]) {
                cnt++;
                if(cnt == 6) {
                  isEqual = !isEqual;
                }
              }
            }
            if(isEqual) {
              IPAddress tempip = localMinValUdp.remoteIP();
              isEqual = !isEqual;
              for(int k = 0, cnt = 0; k < 4; k++) {
                if(tempip[k] == neighborsIPAddress[j][k]) {
                  cnt++;
                  if(cnt == 4) {
                    isEqual = !isEqual;
                  }
                }
              }
              if(isEqual) {
                ByteArrayToInt intConverter;
                intConverter.bytes[0] = dtpHeader.dataLength[0];
                intConverter.bytes[1] = dtpHeader.dataLength[1];
                intConverter.bytes[2] = 0x0;
                intConverter.bytes[3] = 0x0;
                pData = (byte *)malloc(sizeof(byte) * intConverter.val);
                memcpy(pData, pByte + sizeof(DtpHeader), intConverter.val);
                intConverter.bytes[0] = pData[0];
                intConverter.bytes[1] = pData[1];
                intConverter.bytes[2] = pData[2];
                intConverter.bytes[3] = pData[3];
                minVal = min(minVal, intConverter.val);
                neighborsVal[j] = intConverter.val;
//                Serial.print(localMinValUdp.remoteIP());
//                Serial.print(" ");
//                Serial.println(intConverter.val);
                free(pData);
              }
              break;
            }
          }
        }
        free(pByte);
      }
    }
    delay(1000);
  }
  localMinValUdp.flush();
  localMinValUdp.stop();
  for(int i = 0; i < currentNeighborsSize; i++) {
    if(neighborsVal[i] != -1 && i == currentNeighborsSize - 1) {
      isGetLocalMinCompleted = true;
    }else if(neighborsVal[i] == -1){
      break;
    }
  }
  return minVal;
}

void WiFiUtils::leaderElection(int listenPort, int remotePort, unsigned long interval) {
  if(isLeaderElectionCompleted || !isGetLocalMinCompleted) {
    return;
  }
  static Timer *timer;
  if(timer == nullptr) {
    timer = new Timer;
  }
  timer->curMillis = millis();
  if(timer->preMillis == 0) {
    timer->preMillis = millis();
    leaderElectionStartMillis = timer->preMillis;
    timer->curMillis = timer->preMillis + interval;
  }
  if(timer->curMillis < timer->preMillis || timer->curMillis - timer->preMillis < interval) {
    return;
  }
  timer->preMillis = timer->curMillis;
  int minVal = randNum;
  IPAddress minIPAddress = localIPAddress;
  bool isDuplicate = false;
  for(int i = 0; i < currentNeighborsSize; i++) {
    if(neighborsVal[i] < minVal) {
      minVal = neighborsVal[i];
      minIPAddress = neighborsIPAddress[i];
      isDuplicate = false;
    }else if(neighborsVal[i] == minVal) {
      isDuplicate = true;
    }
  }
  if(isDuplicate) {
    if(isIPAddressEqual(localIPAddress, minIPAddress)) {//negotiate leader with same min value
      int count = 0;
      for(int i = 0; i < currentNeighborsSize; i++) {
        if(neighborsVal[i] == minVal) {
          count++;
        }
      }
      if(minVal == randNum) {
        count++;
      }
      IPAddress *ips = new IPAddress[count];
      for(int i = 0, j = 0; i < currentNeighborsSize; i++) {
        if(neighborsVal[i] == minVal) {
          ips[j++] = neighborsIPAddress[i];
        }
      }
      if(minVal == randNum) {
        ips[count - 1] = localIPAddress;
      }
      for(int i = 0; i < count; i++) {
        for(int j = 0; j < count - 1 - i; j++) {
          if(compareIPAddress(ips[j], ips[j + 1]) > 0) {
            IPAddress ip = ips[j + 1];
            ips[j + 1] = ips[j];
            ips[j] = ip;
          }
        }
      }
      if(compareIPAddress(ips[0], localIPAddress) == 0) {
        isLeader = true;
        hasLeader = true;
        isLeaderElectionCompleted = true;
        leaderIPAddress = ips[0];
        leaderVal = minVal;
        if(!hasRecordTime) {
          leaderElectionEndMillis = millis();
          hasRecordTime = true;
        }
        informLeader(listenPort, remotePort, 1000);
      }
    }
  }else{
    if(isIPAddressEqual(localIPAddress, minIPAddress)) {//I am leader
      isLeader = true;
      hasLeader = true;
      isLeaderElectionCompleted = true;
      leaderIPAddress = minIPAddress;
      leaderVal = minVal;
      if(!hasRecordTime) {
        leaderElectionEndMillis = millis();
        hasRecordTime = true;
      }
      informLeader(listenPort, remotePort, 1000);
    }
  }
}

void WiFiUtils::informLeader(int listenPort, int remotePort, unsigned long interval) {
  if(!isLeader) {
    return;
  }
  static Timer *timer;
  if(timer == nullptr) {
    timer = new Timer;
  }
  timer->curMillis = millis();
  if(timer->preMillis == 0) {
    timer->preMillis = millis();
    timer->curMillis = timer->preMillis + interval;
  }
  if(timer->curMillis < timer->preMillis || timer->curMillis - timer->preMillis < interval) {
    return;
  }
  timer->preMillis = timer->curMillis;
  DtpHeader dtpHeader;
  WiFiUDP leaderElectionUdp;
  leaderElectionUdp.begin(listenPort);
  int dtpHeaderSize = sizeof(DtpHeader);
  byte temp[dtpHeaderSize + sizeof(int)];
  ByteArrayToInt intConverter;
  intConverter.val = sizeof(int);
  encapsulateDtpHeader(&dtpHeader, (byte)2, intConverter.bytes);
  memcpy(temp, &dtpHeader, dtpHeaderSize);
  intConverter.val = leaderVal;
  for(int j = 0; j < sizeof(int); j++) {
    temp[dtpHeaderSize + j] = intConverter.bytes[j];
  }
  IPAddress broadip(255, 255, 255, 255);
  sendUdpPacket(&leaderElectionUdp, broadip, remotePort, temp, dtpHeaderSize + sizeof(int), 5);
}

int16_t WiFiUtils::calculateChecksum(byte bytes[],int size) {
  ByteArrayToInt converter;
  int cksum=0;
  int i = 0;
  while(size > 1)
  {
    converter.bytes[0] = bytes[i + 1];
    converter.bytes[1] = bytes[i];
    converter.bytes[2] = 0x0;
    converter.bytes[3] = 0x0;
    cksum += converter.val;
    i += 2;
    size -= 2;
  }
  if(size)
  {
    converter.bytes[0] = 0x0;
    converter.bytes[1] = bytes[i];
    converter.bytes[2] = 0x0;
    converter.bytes[3] = 0x0;
    cksum += converter.val;
  }
  cksum = (cksum >> 16) + (cksum & 0xffff);
  //return 0;
  return (int16_t)(~cksum);
}

bool WiFiUtils::isIPAddressEqual(IPAddress addr1, IPAddress addr2) {
  for(int i = 0; i < 4; i++) {
    if(addr1[i] == addr2[i] && i == 3) {
      return true;
    }else if(addr1[i] != addr2[i]){
      return false;
    }
  }
  return false;
}

int WiFiUtils::compareIPAddress(IPAddress addr1, IPAddress addr2) {
  for(int i = 0; i < 4; i++) {
    if(addr1[i] < addr2[i]) {
      return -1;
    }else if(addr1[i] > addr2[i]) {
      return 1;
    }else if(addr1[i] == addr2[i] && i == 3){
      return 0;
    }
  }
  return 0;
}

void WiFiUtils::printAllNeighbors() {
  Serial.println("All neighbors start:");
  for(int i = 0; i < currentNeighborsSize; i++) {
    for(int j = 0; j < 4; j++) {
      Serial.print(neighborsIPAddress[i][j]);
      if(j != 3) {
        Serial.print(".");
      }
    }
    Serial.print(" ");
    for(int j = 0; j < 6; j++) {
      Serial.print(neighborsMACAddress[i][j], HEX);
      if(j != 5) {
        Serial.print("-");
      }
    }
    Serial.println();
  }
  Serial.println("End");
}

void WiFiUtils::printAllNeighborsVal() {
  Serial.println("All neighbors value start:");
  for(int i = 0; i < currentNeighborsSize; i++) {
    for(int j = 0; j < 4; j++) {
      Serial.print(neighborsIPAddress[i][j]);
      if(j != 3) {
        Serial.print(".");
      }
    }
    Serial.print(" ");
    for(int j = 0; j < 6; j++) {
      Serial.print(neighborsMACAddress[i][j], HEX);
      if(j != 5) {
        Serial.print("-");
      }
    }
    Serial.print(" ");
    Serial.print(neighborsVal[i]);
    Serial.println();
  }
  for(int i = 0; i < 4; i++) {
    Serial.print(localIPAddress[i]);
    if(i != 3) {
      Serial.print(".");
    }
  }
  Serial.print(" ");
  byte mac[6];
  WiFi.macAddress(mac);
  for(int i = 0; i < 6; i++) {
    Serial.print(mac[i], HEX);
    if(i != 5) {
      Serial.print("-");
    }
  }
  Serial.print(" ");
  Serial.print(randNum);
  Serial.println();
  Serial.println("End");
}

void WiFiUtils::printCurrentLeader() {
  if(!hasLeader) {
    return;
  }
  Serial.println("Current leader start:");
  for(int i = 0; i < 4; i++) {
    Serial.print(leaderIPAddress[i]);
    if(i != 3) {
      Serial.print(".");
    }
  }
  Serial.print(" ");
  Serial.print(leaderVal);
  Serial.println();
  Serial.println("End");
}
