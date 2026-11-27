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
uint32_t started, queuedAt, lastAttempt;
bool joining, disconnectPending, scanRequested, scanning;
int nextSaved;
constexpr uint32_t retryDelay = 15000;
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
const char *credential(const String &ssid, const String &pass, bool reuse, Credential &next) {
    if (!ssid.length() || ssid.length()>32 || pass.length()>64) return "Check the network name and password.";
    int i = locate(ssid);
    if (reuse && i<0) return "This network is no longer saved.";
    if (i<0 && locate("")<0) return "Forget a saved network first (8 maximum).";
    strlcpy(next.ssid,ssid.c_str(),sizeof(next.ssid));
    strlcpy(next.password,reuse ? store.saved[i].password : pass.c_str(),sizeof(next.password));
    return nullptr;
}
bool remember(const Credential &entry,bool prefer) {
    int i=locate(entry.ssid);
    if (i<0) i=locate("");
    if (i<0) return false;
    Store next=store;
    next.saved[i]=entry;
    if (prefer || !next.preferred[0]) strlcpy(next.preferred,entry.ssid,sizeof(next.preferred));
    return !memcmp(&next,&store,sizeof(next)) || save(next);
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
    WiFi.setAutoReconnect(false);
    nextSaved=std::max(0,locate(store.preferred));
    lastAttempt=millis()-retryDelay;
    configTime(0,0,"time.cloudflare.com","pool.ntp.org");
}
const char *connectNetwork(const String &ssid, const String &pass, bool reuse) {
    Credential next{};
    if (const char *error=credential(ssid,pass,reuse,next)) return error;
    if (trying.ssid[0]) return "A connection is in progress.";
    if (WiFi.isConnected() && WiFi.SSID()==ssid && reuse) { message="Connected"; return nullptr; }
    trying=next;
    joining=false;
    queuedAt=millis();
    message="Connecting to " + ssid;
    return nullptr;
}
const char *saveNetwork(const String &ssid,const String &pass,bool reuse) {
    Credential next{};
    if (const char *error=credential(ssid,pass,reuse,next)) return error;
    if (ssid==trying.ssid) return "A connection is in progress.";
    if (!remember(next,false)) return "Could not save network settings.";
    message="Network saved";
    return nullptr;
}
const char *forgetNetwork(const String &ssid) {
    int i = locate(ssid);
    if (!ssid.length()) return "Network not found.";
    if (i<0) return nullptr;
    Store next = store;
    next.saved[i] = {};
    if (ssid==next.preferred) next.preferred[0]=0;
    if (!save(next)) return "Could not save network settings.";
    if (ssid==trying.ssid) {
        disconnectPending=joining;
        trying={};
        joining=false;
    }
    if (WiFi.isConnected() && ssid==WiFi.SSID()) disconnectPending=true;
    queuedAt=millis();
    message = "Network forgotten";
    return nullptr;
}
void scanNetworks() {
    scanRequested=true;
}
void networksPoll(bool setupActive) {
    const uint32_t now=millis();
    // Let the setup POST reach the phone before the shared radio changes channel.
    if (disconnectPending && now-queuedAt>=400) {
        WiFi.disconnect(false,false);
        disconnectPending=false;
        lastAttempt=now;
    }
    if (trying.ssid[0] && !scanning && !disconnectPending) {
        if (!joining && now-queuedAt>=400) {
            WiFi.disconnect(false,false,150);
            WiFi.begin(trying.ssid,trying.password);
            started=now; joining=true;
        } else if (joining && WiFi.isConnected() && WiFi.SSID()==trying.ssid) {
            message=remember(trying,true) ? "Connected" : "Connected; could not save settings";
            nextSaved=std::max(0,locate(trying.ssid));
            trying={}; joining=false; lastAttempt=now;
        } else if (joining && now-started>=20000) {
            WiFi.disconnect(false,false);
            message="Connection failed. Check the password or use another saved network.";
            trying={}; joining=false; lastAttempt=now;
        }
    }
    if (!setupActive && !WiFi.isConnected() && !trying.ssid[0] && !disconnectPending &&
        !scanning && now-lastAttempt>=retryDelay) {
        lastAttempt=now;
        for (int step=0;step<8;step++) {
            int i=nextSaved;
            nextSaved=(i+1)%8;
            if (store.saved[i].ssid[0]) {connectNetwork(store.saved[i].ssid,"",true);break;}
        }
    }
    if (scanRequested && !scanning && !trying.ssid[0] && !disconnectPending) {
        scanRequested=false;
        WiFi.scanDelete();
        scanning=WiFi.scanNetworks(true,false,false,110)==WIFI_SCAN_RUNNING;
        if (!scanning) message="Scan failed. Try scanning again.";
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
    doc["scanning"]=scanning || scanRequested;
    doc["connecting"]=trying.ssid;
    auto saved=doc["saved"].to<JsonArray>();
    for (const auto &e:store.saved) if (e.ssid[0]) saved.add(e.ssid);
    auto found=doc["nearby"].to<JsonArray>();
    for (int i=0;i<seenCount;i++) {
        auto row=found.add<JsonArray>();
        row.add(nearby[i].ssid); row.add(nearby[i].signal); row.add(nearby[i].locked);
    }
}
