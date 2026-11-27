#pragma once
#include "Arduino.h"
constexpr int WIFI_STA=1, WIFI_AUTH_OPEN=0, WIFI_SCAN_RUNNING=-1;
struct TestWiFi {
    bool online=false, autoReconnect=false;
    String associated, attempted, supplied;
    int scan=-2;
    void persistent(bool) {} void mode(int) {}
    void setAutoReconnect(bool enabled){autoReconnect=enabled;}
    bool isConnected(){return online;}
    String SSID(){return associated;}
    String SSID(int i){return i==2?"Hidden Keep":"Table";}
    int RSSI(int i){return -55+i;}
    int encryptionType(int){return 1;}
    void begin(const char *name,const char *pass){attempted=name;supplied=pass;}
    void disconnect(bool,bool,int=100){online=false;associated="";}
    void scanDelete(){scan=-2;}
    int scanNetworks(bool,bool,bool,int){return scan=WIFI_SCAN_RUNNING;}
    int scanComplete(){return scan;}
};
inline TestWiFi WiFi;
