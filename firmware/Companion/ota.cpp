#include "ota.h"
#include "body.h"
#include "release_config.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <NetworkClientSecure.h>
#include <SHA2Builder.h>
#include <WiFi.h>
#include <atomic>
#include <ctime>
#include <esp_app_desc.h>
#include <esp_ota_ops.h>

// The core otherwise confirms the image before the display and library initialize.
extern "C" bool verifyRollbackLater() { return true; }

namespace ota {
static View screen;
static QueueHandle_t completed;
static std::atomic<unsigned> progress{0};
static uint32_t restartAt;
const View &view() { return screen; }
const char *version() { return COMPANION_VERSION; }
const char *buildId() { return esp_app_get_elf_sha256_str(); }
bool busy() {
    return screen.stage == Stage::Checking || screen.stage == Stage::Installing ||
           screen.stage == Stage::Restarting;
}
static bool parseVersion(const char *s, unsigned parts[3]) {
    for (unsigned i = 0; i < 3; ++i) {
        unsigned digits = 0, value = 0;
        while (*s >= '0' && *s <= '9') {
            if (++digits > 4 || (digits > 1 && value == 0)) return false;
            value = 10 * value + (*s++ - '0');
        }
        if (!digits || *s != (i == 2 ? '\0' : '.')) return false;
        parts[i] = value;
        if (i < 2) ++s;
    }
    return true;
}
// A receipt belongs to one completed transition. It names the exact previous image
// and the candidate that may offer it, even when the two slots have been reused.
struct RestoreReceipt {
    uint32_t sourceAddress;
    uint8_t sourceHash[32], candidateElf[32];
    char version[16];
};
static RestoreReceipt prior{};
static const esp_partition_t *restoreSlot() {
    const auto *other = esp_ota_get_next_update_partition(nullptr);
    esp_ota_img_states_t state;
    if (!other || other->address != prior.sourceAddress ||
        memcmp(esp_app_get_description()->app_elf_sha256, prior.candidateElf, 32) ||
        esp_ota_get_state_partition(other, &state) != ESP_OK || state != ESP_OTA_IMG_VALID) return nullptr;
    return other;
}
const char *previousVersion() { return prior.version[0] ? prior.version : nullptr; }
static void loadReceipt() {
    Preferences prefs;
    RestoreReceipt r{};
    if (!prefs.begin("dnd-ota", true) || prefs.getBytesLength("previous") != sizeof(r) ||
        prefs.getBytes("previous", &r, sizeof(r)) != sizeof(r)) return;
    unsigned parts[3];
    if (r.version[sizeof(r.version)-1] || !parseVersion(r.version, parts)) return;
    prior = r;
    const auto *slot = restoreSlot();
    uint8_t hash[32];
    if (!slot || esp_partition_get_sha256(slot, hash) != ESP_OK || memcmp(hash, prior.sourceHash, 32)) prior = {};
}
void previewRestore() {
    if (busy() || !previousVersion() || !restoreSlot()) return;
    screen.stage = Stage::RestorePreview;
}
void cancelPreview() {
    if (screen.stage == Stage::RestorePreview || screen.stage == Stage::Available) screen = {};
}
void restore(bool sheetBusy) {
    if (screen.stage != Stage::RestorePreview) return;
    screen.stage = Stage::Failed;
    screen.message = "Finish sheet sync first";
    if (sheetBusy) return;
    const auto *slot = restoreSlot();
    uint8_t digest[32];
    if (slot && esp_partition_get_sha256(slot, digest) == ESP_OK && !memcmp(digest, prior.sourceHash, 32) &&
        esp_ota_check_rollback_is_possible()) {
        // This reboots on success and invalidates the candidate, preventing a restore loop.
        esp_ota_mark_app_invalid_rollback_and_reboot();
    }
    prior = {};
    screen.message = "Restore unavailable";
}
static const char *get(HTTPClient &http, NetworkClientSecure &tls, const char *url) {
    if (WiFi.status() != WL_CONNECTED) return "Wi-Fi offline";
    for (int i = 0; i < 50 && time(nullptr) < 1700000000; ++i) delay(100);
    if (time(nullptr) < 1700000000) return "Waiting for network time";
#ifdef COMPANION_TEST_CA
    tls.setCACert(COMPANION_TEST_CA);
#else
    tls.useBuiltinCACertBundle();
#endif
    tls.setHandshakeTimeout(12);
    http.setConnectTimeout(12000);
    http.setTimeout(12000);
    // Core 4.0.0 preserves the initial HTTPS protocol across redirects.
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setRedirectLimit(5);
    if (!http.begin(tls, url)) return "Release server unavailable";
    http.setUserAgent("DnD-Companion/" COMPANION_VERSION);
    http.addHeader("Accept-Encoding", "identity");
    int status = http.GET();
    if (status == 404) return "Release unavailable";
    return status == HTTP_CODE_OK ? nullptr : "Release server unavailable";
}
static const char *check(View &out) {
    NetworkClientSecure tls;
    HTTPClient http;
    if (const char *err = get(http, tls, COMPANION_FEED)) return err;
    constexpr size_t limit = 2048;
    Body body(limit);
    if (!body.data || http.getSize() > int(limit) || http.writeToStream(&body) <= 0 || body.getWriteError())
        return "Invalid release details";
    JsonDocument json;
    if (deserializeJson(json, body.data, body.length) || json["format"] != 2 ||
        json["target"] != COMPANION_TARGET) return "Incompatible release";
    const char *tag = json["version"] | "";
    const char *hash = json["sha256"] | "";
    const char *url = json["url"] | "";
    const char *note = json["note"] | "";
    unsigned local[3], remote[3];
    if (!parseVersion(version(), local) || !parseVersion(tag, remote) ||
        strlen(hash) != 64 || strspn(hash, "0123456789abcdef") != 64 ||
        strncmp(url, "https://", 8) || strlen(url) >= sizeof(out.release.url) ||
        !note[0] || strlen(note) >= sizeof(out.release.note)) return "Invalid release details";
    const auto *slot = esp_ota_get_next_update_partition(nullptr);
    uint32_t size = json["size"] | 0u;
    if (!slot || !size || size > slot->size) return "Incompatible release";
    out.stage = Stage::Current;
    for (int i = 0; i < 3; i++) {
        if (remote[i] == local[i]) continue;
        if (remote[i] > local[i]) out.stage = Stage::Available;
        break;
    }
    if (out.stage == Stage::Available) {
        bool compatible = false;
        for (JsonVariantConst from : json["otaFrom"].as<JsonArrayConst>()) {
            if (from.is<const char *>() && !strcmp(from.as<const char *>(), version())) compatible = true;
        }
        if (!compatible) return "USB update required";
    }
    strlcpy(out.release.version, tag, sizeof(out.release.version));
    strlcpy(out.release.note, note, sizeof(out.release.note));
    strlcpy(out.release.digest, hash, sizeof(out.release.digest));
    strlcpy(out.release.url, url, sizeof(out.release.url));
    out.release.size = size;
    return nullptr;
}

class ImageWriter : public Stream {
public:
    ImageWriter(esp_ota_handle_t handle, const Release &r) : handle(handle), release(r) {
        snprintf(identity, sizeof(identity), "DIAL/%s/release/%s", COMPANION_TARGET, r.version);
        identityBytes = strlen(identity) + 1;
        hash.begin();
    }
    size_t write(uint8_t byte) override { return write(&byte, 1); }
    size_t write(const uint8_t *data, size_t n) override {
        if (getWriteError() || n > release.size - written || esp_ota_write(handle, data, n) != ESP_OK) {
            setWriteError();
            return 0;
        }
        // Match the null-terminated product/version even across two network chunks.
        // The marker has no internal uppercase D, so only its first byte can restart it.
        for (size_t i = 0; i < n && matched < identityBytes; ++i)
            matched = data[i] == identity[matched] ? matched + 1 : size_t(data[i] == 'D');
        hash.add(data, n);
        written += n;
        progress.store(100 * written / release.size);
        return n;
    }
    bool verified() {
        hash.calculate();
        return !getWriteError() && written == release.size && matched == identityBytes &&
               hash.toString() == release.digest;
    }
    int available() override { return 0; }
    int peek() override { return -1; }
    int read() override { return -1; }
    void flush() override {}
private:
    esp_ota_handle_t handle;
    const Release &release;
    SHA256Builder hash;
    uint32_t written = 0;
    char identity[100];
    size_t matched = 0, identityBytes;
};

static const char *install(const Release &r) {
    NetworkClientSecure tls;
    HTTPClient http;
    if (const char *err = get(http, tls, r.url)) return err;
    if (http.getSize() >= 0 && uint32_t(http.getSize()) != r.size) return "Image verification failed";
    const auto *slot = esp_ota_get_next_update_partition(nullptr);
    esp_ota_handle_t handle;
    const auto *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    RestoreReceipt receipt{};
    Preferences prefs;
    if (!running || esp_ota_get_state_partition(running, &state) != ESP_OK || state != ESP_OTA_IMG_VALID ||
        esp_partition_get_sha256(running, receipt.sourceHash) != ESP_OK || !prefs.begin("dnd-ota") ||
        (prefs.isKey("previous") && !prefs.remove("previous"))) return "Restore unavailable";
    receipt.sourceAddress = running->address;
    strlcpy(receipt.version, version(), sizeof(receipt.version));
    if (!slot || r.size > slot->size || esp_ota_begin(slot, OTA_WITH_SEQUENTIAL_WRITES, &handle) != ESP_OK)
        return "Update slot unavailable";
    ImageWriter writer(handle, r);
    if (http.writeToStream(&writer) != int(r.size) || !writer.verified()) {
        esp_ota_abort(handle);
        return "Image verification failed";
    }
    esp_app_desc_t desc;
    if (esp_ota_end(handle) != ESP_OK || esp_ota_get_partition_description(slot, &desc) != ESP_OK)
        return "Image verification failed";
    memcpy(receipt.candidateElf, desc.app_elf_sha256, sizeof(receipt.candidateElf));
    if (prefs.putBytes("previous", &receipt, sizeof(receipt)) != sizeof(receipt)) return "Restore unavailable";
    if (esp_ota_set_boot_partition(slot) != ESP_OK)
        return "Image verification failed";
    return nullptr;
}
static void worker(void *arg) {
    View result = *static_cast<View *>(arg);
    const bool installing = result.stage == Stage::Installing;
    const char *err = installing ? install(result.release) : check(result);
    if (err) {
        result.stage = Stage::Failed;
        result.message = err;
    } else if (installing) result.stage = Stage::Restarting;
    xQueueSend(completed, &result, portMAX_DELAY);
    vTaskDelete(nullptr);
}
void start(bool confirm, bool sheetBusy) {
    if (busy() || (confirm && screen.stage != Stage::Available)) return;
    if (!confirm) screen = View{};
    else prior = {}; // Installing may overwrite the only restore slot.
    if (!completed) completed = xQueueCreate(1, sizeof(View));
    screen.stage = confirm ? Stage::Installing : Stage::Checking;
    screen.message = "";
    screen.percent = 0;
    progress.store(0);
    // The worker owns an immutable copy while the UI polls progress.
    static View request;
    request = screen;
    if (sheetBusy || !completed || xTaskCreate(worker, "dial-ota", 12288, &request, 1, nullptr) != pdPASS) {
        screen.stage = Stage::Failed;
        screen.message = sheetBusy ? "Finish sheet sync first" : "Update could not start";
    }
}
bool poll() {
    if (completed && xQueueReceive(completed, &screen, 0) == pdTRUE) {
        if (screen.stage == Stage::Restarting) restartAt = millis();
        if (screen.stage == Stage::Failed && !previousVersion()) loadReceipt();
        return true;
    }
    if (screen.stage == Stage::Restarting && millis() - restartAt >= 1200) ESP.restart();
    unsigned value = progress.load();
    if (screen.stage != Stage::Installing || value / 5 == screen.percent / 5) return false;
    screen.percent = value;
    return true;
}
bool needsBootCheck() {
    const auto *slot = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    return esp_ota_get_state_partition(slot, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY;
}
void confirmHealthyBoot(bool savesReadable) {
    if (needsBootCheck() && (!savesReadable || esp_ota_mark_app_valid_cancel_rollback() != ESP_OK)) {
        esp_ota_mark_app_invalid_rollback_and_reboot();
        return;
    }
    loadReceipt();
}
void describe() {
    const auto *slot = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    int validity = esp_ota_get_state_partition(slot, &state) == ESP_OK ? int(state) : -1;
    Serial.printf("FIRMWARE image=%s slot=%s validity=%d stage=%d build=%s offered=%s percent=%u message=%s\n",
                  COMPANION_IDENTITY, slot->label, validity, int(screen.stage), buildId(),
                  screen.release.version, screen.percent, screen.message);
    Serial.printf("RESTORE previous=%s\n", previousVersion() ? previousVersion() : "none");
}
}
