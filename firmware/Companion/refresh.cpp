#include "refresh.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>
#include <ctime>
#include <atomic>

#include "body.h"

static QueueHandle_t results;
static bool busy;
static SourceTag requestTag;

static std::atomic<uint32_t> downloadedKiB{};
uint32_t refreshKiB() {return downloadedKiB.load(std::memory_order_relaxed);}

class SheetBody : public Body {
public:
    SheetBody(size_t capacity,uint32_t start) : Body(capacity), started(start) {}
    size_t write(const uint8_t *bytes,size_t count) override {
        if (millis()-started>=60000) {expired=true;setWriteError();return 0;}
        size_t n=Body::write(bytes,count);
        downloadedKiB.store(length/1024,std::memory_order_relaxed);
        return n;
    }
    bool expired=false;
private:
    uint32_t started;
};

static const char *fetch(uint32_t id, RefreshResult &result, uint32_t started, bool &retry) {
    if (WiFi.status() != WL_CONNECTED) return "Wi-Fi offline";
    for (int i = 0; i < 100 && time(nullptr) < 1700000000; ++i) delay(100);
    if (time(nullptr) < 1700000000) return "Waiting for network time";
    NetworkClientSecure tls;
    tls.useBuiltinCACertBundle();
    tls.setHandshakeTimeout(12);
    HTTPClient http;
    http.setConnectTimeout(10000);
    http.setTimeout(12000);
    String url = "https://character-service.dndbeyond.com/character/v5/character/" + String(id);
    if (!http.begin(tls, url)) return "Could not open character service";
    http.setUserAgent("DnD-Companion/0.1");
    http.addHeader("Accept-Encoding", "identity");
    const char *headers[] = {"ETag", "Last-Modified"};
    http.collectHeaders(headers, 2);
    if (requestTag.etag[0]) http.addHeader("If-None-Match", requestTag.etag);
    else if (requestTag.lastModified[0]) http.addHeader("If-Modified-Since", requestTag.lastModified);
    int status = http.GET();
    if (status == 304 && (requestTag.etag[0] || requestTag.lastModified[0])) {
        result.notModified = true;
        return nullptr;
    }
    if (status != 200) {
        if (status==401 || status==403 || status==404) return "Check public character link";
        if (status==429) return "Beyond is busy. Try again later.";
        retry=status==HTTPC_ERROR_CONNECTION_LOST || status==HTTPC_ERROR_READ_TIMEOUT ||
              status==HTTPC_ERROR_CONNECTION_REFUSED || status==502 || status==503 || status==504;
        snprintf(result.error, sizeof(result.error), status > 0 ? "Character service: HTTP %d" : "HTTPS connection failed (%d)", status);
        return result.error;
    }
    constexpr int limit = 2 * 1024 * 1024;
    int size = http.getSize();
    if (size > limit) return "Sheet exceeds 2 MiB";
    SheetBody body(size > 0 ? size : limit,started);
    if (!body.data) return "Insufficient PSRAM";
    int n = http.writeToStream(&body);
    if (body.expired) return "Sheet download timed out";
    if (body.getWriteError()) return "Sheet exceeds download size";
    if (n<=0 || (size>=0 && size!=n)) {
        retry=true;
        return WiFi.isConnected() ? "Sheet download interrupted" : "Wi-Fi offline";
    }
    String etag=http.header("ETag"), modified=http.header("Last-Modified");
    http.end();
    // A bounded reader prevents the parser reading beyond received bytes.
    class Reader {
    public:
        Reader(const uint8_t *p, size_t n) : ptr(p), left(n) {}
        int read() { if (!left) return -1; --left; return *ptr++; }
        size_t readBytes(char *out, size_t n) {
            n = n < left ? n : left;
            memcpy(out, ptr, n); ptr += n; left -= n; return n;
        }
    private:
        const uint8_t *ptr; size_t left;
    } reader(body.data, body.length);
    RollJsonMemory memory;
    JsonDocument book(&memory);
    const char *error = parseSheet(reader, id, result.sheet,&book);
    if(!error){
        result.bookSize=measureMsgPack(book);
        if(!result.bookSize || result.bookSize>96*1024)return "Spellbook exceeds device limit";
        result.book=static_cast<uint8_t *>(ps_malloc(result.bookSize));
        if(!result.book || serializeMsgPack(book,result.book,result.bookSize)!=result.bookSize)return "Could not cache spellbook";
        uint32_t h=2166136261u;
        for(size_t i=0;i<result.bookSize;i++)h=(h^result.book[i])*16777619u;
        result.sheet.bookHash=h?h:1;
    }
    if (!error) {
        if (etag.length() < sizeof(result.sheet.tag.etag)) strlcpy(result.sheet.tag.etag, etag.c_str(), sizeof(result.sheet.tag.etag));
        if (modified.length() < sizeof(result.sheet.tag.lastModified)) strlcpy(result.sheet.tag.lastModified, modified.c_str(), sizeof(result.sheet.tag.lastModified));
    }
    return error;
}
static void worker(void *ctx) {
    uint32_t id = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ctx));
    RefreshResult result{};
    result.sheet.id = id;
    uint32_t started=millis();
    bool retry=false;
    const char *error=fetch(id,result,started,retry);
    if (error && retry && WiFi.isConnected() && millis()-started<25000) {
        delay(650);
        result.error[0]=0;
        downloadedKiB.store(0,std::memory_order_relaxed);
        error=fetch(id,result,started,retry);
    }
    if (!error) result.sheet.synced = time(nullptr);
    else if (error != result.error) strlcpy(result.error, error, sizeof(result.error));
    xQueueSend(results, &result, portMAX_DELAY);
    vTaskDelete(nullptr);
}
bool startRefresh(uint32_t id, const SourceTag &tag) {
    if (busy || !id) return false;
    if (!results) results = xQueueCreate(1, sizeof(RefreshResult));
    if (!results) return false;
    downloadedKiB.store(0,std::memory_order_relaxed);
    requestTag = tag;
    busy = xTaskCreate(worker, "sheet-https", 14336, reinterpret_cast<void *>(uintptr_t(id)), 1, nullptr) == pdPASS;
    return busy;
}
bool takeRefresh(RefreshResult &result) {
    if (!busy || xQueueReceive(results, &result, 0) != pdTRUE) return false;
    busy = false;
    return true;
}
