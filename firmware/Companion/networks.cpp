#include "networks.h"
#include <WiFi.h>
#include <Preferences.h>

namespace {
struct Credential { char ssid[33], password[65]; };
struct Store { Credential saved[8]; char preferred[33]; };
Store store{};
Credential trying{};
Preferences nv;
String message;
uint32_t started;
bool scanning;
struct Seen { String ssid; int signal; bool locked; };
Seen nearby[20];
int seenCount;

int locate(const String &ssid) {
    for (int i=0;i<8;i++) if (ssid == store.saved[i].ssid) return i;
    return -1;
}
bool save(const Store &next) {
    if (nv.putBytes("networks2", &next, sizeof(next)) != sizeof(next)) return false;
    store = next;
    return true;
}
}
void networksBegin() {
    nv.begin("dnd-network1");
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    if (nv.getBytesLength("networks2") == sizeof(store)) nv.getBytes("networks2", &store, sizeof(store));
    else {
        String old = nv.getString("ssid");
        if (old.length()) {
            Store next{};
            strlcpy(next.saved[0].ssid,old.c_str(),sizeof(next.saved[0].ssid));
            strlcpy(next.saved[0].password,nv.getString("pass").c_str(),sizeof(next.saved[0].password));
            strlcpy(next.preferred,old.c_str(),sizeof(next.preferred));
            if (save(next)) { nv.remove("ssid"); nv.remove("pass"); }
        }
    }
    if (store.preferred[0]) connectNetwork(store.preferred, "", true);
    configTime(0,0,"time.cloudflare.com","pool.ntp.org");
}
const char *connectNetwork(const String &ssid, const String &pass, bool reuse) {
    if (!ssid.length() || ssid.length()>32 || pass.length()>64) return "Check the network name and password.";
    int i = locate(ssid);
    if (reuse && i<0) return "This network is no longer saved.";
    if (i<0 && locate("")<0) return "Forget a saved network first (8 maximum).";
    Credential next{};
    strlcpy(next.ssid,ssid.c_str(),sizeof(next.ssid));
    strlcpy(next.password,reuse ? store.saved[i].password : pass.c_str(),sizeof(next.password));
    if (WiFi.isConnected() && WiFi.SSID()==ssid && reuse) { message="Connected"; return nullptr; }
    trying = next;
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false,false,150);
    WiFi.begin(trying.ssid,trying.password);
    started = millis(); message = "Connecting to " + ssid;
    return nullptr;
}
const char *forgetNetwork(const String &ssid) {
    int i = locate(ssid);
    if (!ssid.length() || i<0) return "Network not found.";
    Store next = store;
    next.saved[i] = {};
    if (ssid==next.preferred) next.preferred[0]=0;
    if (!save(next)) return "Could not save network settings.";
    if (ssid==WiFi.SSID() || ssid==trying.ssid) {
        trying = {};
        WiFi.setAutoReconnect(false);
        WiFi.disconnect(false,false);
    }
    message = "Network forgotten";
    return nullptr;
}
void scanNetworks() {
    if (scanning) return;
    WiFi.scanDelete();
    scanning = WiFi.scanNetworks(true, false, false, 110) == WIFI_SCAN_RUNNING;
}
void networksPoll() {
    if (trying.ssid[0]) {
        if (WiFi.isConnected() && WiFi.SSID()==trying.ssid) {
            int i = locate(trying.ssid);
            if (i<0) i=locate("");
            Store next=store;
            if (i>=0) {
                next.saved[i]=trying;
                strlcpy(next.preferred,trying.ssid,sizeof(next.preferred));
            }
            bool ok = i>=0 && (!memcmp(&next,&store,sizeof(next)) || save(next));
            message = ok ? "Connected" : "Connected; could not save settings";
            WiFi.setAutoReconnect(true);
            trying={};
        } else if (millis()-started > 24000) {
            WiFi.disconnect(false,false);
            message="Connection failed. Check the password or use another saved network.";
            trying={};
        }
    }
    if (!scanning) return;
    int n = WiFi.scanComplete();
    if (n==WIFI_SCAN_RUNNING) return;
    scanning=false;
    if (n<0) { message="Scan failed. Try scanning again."; return; }
    seenCount=0;
    for (int i=0; i<n && i<255; i++) {
        String name=WiFi.SSID(i);
        if (!name.length()) continue;
        int j=0;
        while (j<seenCount && nearby[j].ssid!=name) j++;
        int signal=WiFi.RSSI(i);
        if (j==seenCount) {
            if (seenCount==20) continue;
            seenCount++;
        } else if (nearby[j].signal>=signal) continue;
        nearby[j]={name,signal,WiFi.encryptionType(i)!=WIFI_AUTH_OPEN};
    }
    WiFi.scanDelete();
}
void networkStatus(JsonDocument &doc) {
    doc["online"]=WiFi.isConnected();
    doc["network"]=WiFi.isConnected()?WiFi.SSID():"";
    doc["networkMessage"]=message;
    doc["scanning"]=scanning;
    auto saved=doc["saved"].to<JsonArray>();
    for (const auto &e:store.saved) if (e.ssid[0]) saved.add(e.ssid);
    auto found=doc["nearby"].to<JsonArray>();
    for (int i=0;i<seenCount;i++) {
        auto row=found.add<JsonArray>();
        row.add(nearby[i].ssid); row.add(nearby[i].signal); row.add(nearby[i].locked);
    }
}
