#include "setup_wifi.h"
#include <WiFi.h>
#include <WebServer.h>
#include "networks.h"
#include <esp_random.h>
#include <DNSServer.h>
#include "phone_page.h"
#include "photo_portal.h"

static WebServer server(80);
static DNSServer dns;
static uint32_t opened;
String accessName, accessPassword;
bool setupActive;

uint32_t characterLinkId(const String &input) {
    String url = input;
    url.trim();
    const char *prefix = "https://www.dndbeyond.com/characters/";
    const char *other = "https://dndbeyond.com/characters/";
    if (url.startsWith(prefix)) url.remove(0, strlen(prefix));
    else if (url.startsWith(other)) url.remove(0, strlen(other));
    else return 0;
    uint64_t id = 0;
    size_t i = 0;
    while (i < url.length() && url[i] >= '0' && url[i] <= '9') {
        id = id * 10 + (url[i++] - '0');
        if (id > UINT32_MAX) return 0;
    }
    if (!i || (i < url.length() && url[i] != '/' && url[i] != '?' && url[i] != '#')) return 0;
    return id;
}
static bool fromSetupNetwork() {
    if (setupActive && server.client().localIP() == WiFi.softAPIP()) return true;
    server.send(403, "text/plain", ui::tr("Join the dial setup network."));
    return false;
}
static uint32_t postedId() {
    String s=server.arg("id");
    if (!s.length() || s.length()>10) return 0;
    uint64_t n=0;
    for (size_t i=0;i<s.length();++i) {
        if (s[i]<'0' || s[i]>'9') return 0;
        n=n*10+(s[i]-'0');
    }
    return n<=UINT32_MAX ? n : 0;
}
void wifiBegin() {
    networksBegin();
    photoRoutes(server);
    server.on("/", HTTP_GET, [] {
        if (!fromSetupNetwork()) return;
        server.sendHeader("Cache-Control","no-store");
        server.send_P(200, "text/html; charset=utf-8", phonePage());
    });
    server.on("/status", HTTP_GET, [] {
        if (!fromSetupNetwork()) return;
        server.sendHeader("Cache-Control", "no-store");
        server.send(200, "application/json", libraryStatus());
    });
    server.onNotFound([] {
        if (!fromSetupNetwork()) return;
        server.sendHeader("Location", "http://192.168.4.1/");
        server.send(302, "text/plain", ui::tr("Open dial setup"));
    });
    server.on("/wifi", HTTP_POST, [] {
        if (!fromSetupNetwork()) return;
        const char *error = connectNetwork(server.arg("ssid"),server.arg("pass"),server.arg("reuse")=="1");
        server.send(error?400:202,"text/plain",ui::tr(error?error:"Connecting to the selected network."));
    });
    server.on("/wifi/scan", HTTP_POST, [] {
        if (!fromSetupNetwork()) return;
        scanNetworks(); server.send(202,"text/plain",ui::tr("Scanning nearby networks."));
    });
    server.on("/wifi/forget", HTTP_POST, [] {
        if (!fromSetupNetwork()) return;
        const char *error=forgetNetwork(server.arg("ssid"));
        server.send(error?400:200,"text/plain",ui::tr(error?error:"Network forgotten."));
    });
    server.on("/language",HTTP_POST,[] {
        if (!fromSetupNetwork()) return;
        bool ok=setDisplayLanguage(server.arg("language"));
        server.send(ok?200:400,"text/plain",ok?"OK":ui::tr("Could not save language."));
    });
    server.on("/picker/art",HTTP_POST,[] {
        if (!fromSetupNetwork()) return;
        bool ok=setPickerArt(server.arg("mode"));
        server.send(ok?200:400,"text/plain",ui::tr(ok?"Picker appearance saved.":"Could not save picker appearance."));
    });
    server.on("/character/edit", HTTP_POST, [] {
        if (!fromSetupNetwork()) return;
        const char *error=editLink(postedId(),characterLinkId(server.arg("link")));
        server.send(error?400:202,"text/plain",ui::tr(error?error:"Checking the replacement link."));
    });
    server.on("/character/remove", HTTP_POST, [] {
        if (!fromSetupNetwork()) return;
        const char *error=removeLink(postedId());
        server.send(error?400:200,"text/plain",ui::tr(error?error:"Character removed."));
    });
    server.on("/review", HTTP_POST, [] {
        if (!fromSetupNetwork()) return;
        if (server.arg("apply")!="0" && server.arg("apply")!="1") { server.send(400,"text/plain",ui::tr("Choose Apply or Keep current.")); return; }
        const char *error=decideReview(postedId(),server.arg("apply")=="1");
        server.send(error?409:200,"text/plain",ui::tr(error?error:"Decision saved."));
    });
    server.on("/character", HTTP_POST, [] {
        if (!fromSetupNetwork()) return;
        const char *error = queueLinks(server.arg("link"));
        server.send(error ? 400 : 202, "text/plain", ui::tr(error ? error : "Links queued on the dial."));
    });
}
bool setupOpen() {
    if (setupActive) return true;
    char secret[13];
    snprintf(secret, sizeof(secret), "%08lX%04lX", (unsigned long)esp_random(), (unsigned long)(esp_random() & 0xffff));
    accessPassword = secret;
    accessName = "Companion-" + String((uint16_t)(ESP.getEfuseMac() >> 32), HEX);
    WiFi.mode(WIFI_AP_STA);
    setupActive = WiFi.softAP(accessName.c_str(), secret);
    if (setupActive) { dns.start(53, "*", WiFi.softAPIP()); server.begin(); opened = millis(); scanNetworks(); }
    return setupActive;
}
void setupClose() {
    if (!setupActive) return;
    dns.stop();
    server.stop();
    WiFi.softAPdisconnect(true);
    accessPassword = "";
    setupActive = false;
}
void setupPoll() {
    networksPoll();
    photoPoll();
    if (!setupActive) return;
    server.handleClient();
    if (millis() - opened >= 600000) setupClose();
}
