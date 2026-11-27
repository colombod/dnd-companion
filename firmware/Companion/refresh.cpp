#include "refresh.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>
#include <ctime>

#include "body.h"

static QueueHandle_t results;
static bool busy;
static SourceTag requestTag;

static const char *fetch(uint32_t id, RefreshResult &result) {
    if (WiFi.status() != WL_CONNECTED) return "Wi-Fi offline";
    for (int i = 0; i < 60 && time(nullptr) < 1700000000; ++i) delay(100);
    if (time(nullptr) < 1700000000) return "Waiting for network time";
    NetworkClientSecure tls;
    tls.useBuiltinCACertBundle();
    tls.setHandshakeTimeout(9);
    HTTPClient http;
    http.setConnectTimeout(9000);
    http.setTimeout(9000);
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
        snprintf(result.error, sizeof(result.error), status > 0 ? "Character service: HTTP %d" : "HTTPS connection failed (%d)", status);
        return result.error;
    }
    constexpr int limit = 2 * 1024 * 1024;
    int size = http.getSize();
    if (size > limit) return "Sheet exceeds 2 MiB";
    Body body(size > 0 ? size : limit);
    if (!body.data) return "Insufficient PSRAM";
    int n = http.writeToStream(&body);
    if (n <= 0 || body.getWriteError() || (size > 0 && size != n)) return "Download incomplete or too large";
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
        String etag = http.header("ETag"), modified = http.header("Last-Modified");
        if (etag.length() < sizeof(result.sheet.tag.etag)) strlcpy(result.sheet.tag.etag, etag.c_str(), sizeof(result.sheet.tag.etag));
        if (modified.length() < sizeof(result.sheet.tag.lastModified)) strlcpy(result.sheet.tag.lastModified, modified.c_str(), sizeof(result.sheet.tag.lastModified));
    }
    return error;
}
static void worker(void *ctx) {
    uint32_t id = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ctx));
    RefreshResult result{};
    result.sheet.id = id;
    const char *error = fetch(id, result);
    if (!error) result.sheet.synced = time(nullptr);
    else if (error != result.error) strlcpy(result.error, error, sizeof(result.error));
    xQueueSend(results, &result, portMAX_DELAY);
    vTaskDelete(nullptr);
}
bool startRefresh(uint32_t id, const SourceTag &tag) {
    if (busy || !id) return false;
    if (!results) results = xQueueCreate(1, sizeof(RefreshResult));
    if (!results) return false;
    requestTag = tag;
    busy = xTaskCreate(worker, "sheet-https", 14336, reinterpret_cast<void *>(uintptr_t(id)), 1, nullptr) == pdPASS;
    return busy;
}
bool takeRefresh(RefreshResult &result) {
    if (!busy || xQueueReceive(results, &result, 0) != pdTRUE) return false;
    busy = false;
    return true;
}
