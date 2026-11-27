#include "ui_language.h"
#include "photo_portal.h"
#include "portraits.h"
#include "face.h"
#include "body.h"
#include "setup_wifi.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>

namespace {
WebServer *web;
bool onAP() {return setupActive && web->client().localIP()==WiFi.softAPIP();}
uint8_t *upload;
size_t used;
uint32_t uploadId, uploadSource;
ArtLayer uploadLayer;
size_t uploadLimit;
bool uploadOk;
struct Preview {uint8_t *data; size_t size; uint32_t id, source;};
Preview cached{};
struct Request {String url;uint32_t id, source;};
QueueHandle_t results;
bool busy;
uint32_t usedAt;
bool trusted(const String &url) {
    return url.startsWith("https://www.dndbeyond.com/avatars/") || url.startsWith("https://media.dndbeyond.com/avatars/") ||
           url.startsWith("https://imgsrv.dndbeyond.com/avatars/") || url.startsWith("https://www.dndbeyond.com/content/");
}
void download(void *arg) {
    auto *q=static_cast<Request *>(arg);
    Preview result{nullptr,0,q->id,q->source};
    {
        NetworkClientSecure tls; tls.useBuiltinCACertBundle();tls.setHandshakeTimeout(8);
        HTTPClient http;http.setConnectTimeout(8000);http.setTimeout(8000);
        constexpr int limit=4*1024*1024;
        if (http.begin(tls,q->url) && http.GET()==200 && http.getSize()<=limit) {
            int n=http.getSize();
            Body body(n>0?n:limit);
            if (body.data && http.writeToStream(&body)>0 && !body.getWriteError() && (n<0 || body.length==size_t(n))) {
                result.data=body.data; result.size=body.length;body.data=nullptr;
            }
        }
    }
    delete q;xQueueSend(results,&result,portMAX_DELAY);vTaskDelete(nullptr);
}
void clearPreview() {free(cached.data);cached={};}
void source() {
    if (!onAP()) {web->send(403);return;}
    uint32_t id=strtoul(web->arg("id").c_str(),nullptr,10);
    auto *sheet=getSheet(id);
    if (!sheet || !trusted(sheet->avatar)) {web->send(404,"text/plain",ui::tr("No supported avatar URL"));return;}
    usedAt=millis();
    uint32_t fingerprint=iconSource(sheet->avatar);
    if (cached.id==id && cached.source==fingerprint) {
        if (!cached.data) {web->send(422,"text/plain",ui::tr("Profile icon unavailable. Try again later."));return;}
        if (!web->hasArg("offset")) {web->send(200,"text/plain",String(cached.size));return;}
        size_t offset=strtoul(web->arg("offset").c_str(),nullptr,10);
        if (offset>=cached.size) {web->send(416);return;}
        size_t n=std::min(size_t(12288),cached.size-offset);
        web->send_P(200,"application/octet-stream",reinterpret_cast<const char *>(cached.data+offset),n);
        return;
    }
    if (!busy) {
        if (!WiFi.isConnected()) {web->send(409,"text/plain",ui::tr("Table Wi-Fi is offline"));return;}
        clearPreview();
        if (!results) results=xQueueCreate(1,sizeof(Preview));
        auto *q=new(std::nothrow) Request{sheet->avatar,id,fingerprint};
        busy=results && q && xTaskCreate(download,"avatar-https",10000,q,1,nullptr)==pdPASS;
        if (!busy) {delete q;web->send(503,"text/plain",ui::tr("Try again when the dial is idle"));return;}
    }
    web->send(202,"text/plain",ui::tr("Fetching avatar"));
}
void receive() {
    if (!onAP() || !web->hasUpload()) return;
    auto &u=web->upload();
    if (u.status==UPLOAD_FILE_START) {
        free(upload);upload=nullptr;used=0;uploadOk=false;
        uploadId=strtoul(web->arg("id").c_str(),nullptr,10);
        String kind=web->arg("kind");
        uploadLayer=kind=="icon"?ArtLayer::Icon:ArtLayer::Background;
        uploadSource=strtoul(web->arg("source").c_str(),nullptr,10);
        auto *sheet=getSheet(uploadId);
        uploadLimit=uploadLayer==ArtLayer::Icon?iconLimit:backgroundLimit;
        if (sheet && (kind=="background" || (kind=="icon" && uploadSource && uploadSource==iconSource(sheet->avatar))))
            upload=static_cast<uint8_t *>(ps_malloc(uploadLimit));
        uploadOk=upload!=nullptr;
    } else if (u.status==UPLOAD_FILE_WRITE && uploadOk) {
        if (u.currentSize>uploadLimit-used) uploadOk=false;
        else {memcpy(upload+used,u.buf,u.currentSize);used+=u.currentSize;}
    } else if (u.status==UPLOAD_FILE_ABORTED) {free(upload);upload=nullptr;uploadOk=false;}
}
}
void photoPoll() {
    Preview done{};
    if (busy && xQueueReceive(results,&done,0)==pdTRUE) {clearPreview();cached=done;busy=false;usedAt=millis();}
    if (cached.id && (!setupActive || millis()-usedAt>90000)) clearPreview();
}
void photoRoutes(WebServer &s) {
    web=&s;
    s.on("/avatar/source",HTTP_GET,source);
    s.on("/avatar",HTTP_POST,[] {
        if (!onAP()) {web->send(403);return;}
        const auto *sheet=getSheet(uploadId);
        bool ok=upload && uploadOk && sheet && (uploadLayer==ArtLayer::Background || uploadSource==iconSource(sheet->avatar)) &&
                saveArtwork(uploadId,uploadLayer,upload,used,uploadSource);
        free(upload);upload=nullptr;uploadOk=false;used=0;
        if (ok) artworkChanged();
        web->send(ok?200:422,"text/plain",ui::tr(ok?"Artwork saved":"Check image dimensions, size and selected character."));
    },receive);
    s.on("/avatar/reset",HTTP_POST,[] {
        if (!onAP()) {web->send(403);return;}
        uint32_t id=strtoul(web->arg("id").c_str(),nullptr,10);
        if (!getSheet(id)) {web->send(404);return;}
        bool ok=removeBackground(id);artworkChanged();web->send(ok?200:500,"text/plain",ui::tr(ok?"Background removed.":"Could not remove background."));
    });
}
