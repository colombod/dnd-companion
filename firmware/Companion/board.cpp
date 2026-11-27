#include "board.h"
#include <Wire.h>
#include <lvgl.h>
#include <SensorDRV2605.hpp>
#include "bidi_switch_knob.h"
#include <atomic>
#include <HWCDC.h>

extern "C" void display_init();
static SensorDRV2605 motor;
static bool motorPresent;
static std::atomic<int> steps{0};
static uint32_t pulses, touches, turns;
static int touchChip = -1;
static int rail;
static bool usbHost;
int railMillivolts(){return rail;}
void samplePower() {
    uint32_t total=0;
    for (int i=0;i<8;i++) total+=analogReadMilliVolts(1);
    rail=(total*2+4)/8;
    usbHost=HWCDC::isPlugged();
}
String powerSummary() {
    if (usbHost) return "USB";
    if (rail>4350) return "External";
    if (rail<2800 || rail>4250) return "Power ?";
    // Approximate unloaded 1S Li-ion curve. Board senses the supply rail through 2:1.
    const int mv[]={3200,3500,3650,3730,3830,3950,4100,4200};
    const int pct[]={0,5,15,30,50,70,90,100};
    int estimate=0;
    for (int i=1;i<8;i++) {
        if (rail>=mv[i]) estimate=pct[i];
        else if (rail>mv[i-1]) { estimate=pct[i-1]+(rail-mv[i-1])*(pct[i]-pct[i-1])/(mv[i]-mv[i-1]);break; }
    }
    return "~"+String((estimate+2)/5*5)+"%";
}

static void right(void *, void *) { steps.fetch_add(1); }
static void left(void *, void *) { steps.fetch_sub(1); }

static bool readTouch(uint8_t reg, uint8_t *buf, size_t count) {
    Wire.beginTransmission(0x15);
    Wire.write(reg);
    if (Wire.endTransmission(false) || Wire.requestFrom(uint8_t(0x15), count) != count) return false;
    return Wire.readBytes(buf, count) == count;
}

void boardBegin() {
    Wire.begin(11, 12, 300000);
    Wire.setTimeOut(15);
    uint8_t id;
    if (readTouch(0xA7, &id, 1)) touchChip = id;
    motorPresent = motor.begin(Wire, 11, 12);
    if (motorPresent) {
        motor.selectLibrary(1);
        motor.setMode(SensorDRV2605::MODE_INTTRIG);
        motor.setWaveform(0, 2);
        motor.setWaveform(1, 0);
    }
    knob_config_t pins{8, 7};
    auto k = iot_knob_create(&pins);
    ESP_ERROR_CHECK(k ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(iot_knob_register_cb(k, KNOB_LEFT, left, nullptr));
    ESP_ERROR_CHECK(iot_knob_register_cb(k, KNOB_RIGHT, right, nullptr));
    display_init();
    pinMode(47, OUTPUT);
    digitalWrite(47, HIGH);
    analogSetPinAttenuation(1,ADC_11db);
    samplePower();
}

bool finger(uint16_t &x, uint16_t &y) {
    uint8_t data[5];
    if (!readTouch(0x02, data, 5) || (data[0] & 15) != 1) return false;
    x = ((data[1] & 15) << 8) | data[2];
    y = ((data[3] & 15) << 8) | data[4];
    if (x >= 360 || y >= 360) return false;
    touches++;
    return true;
}
int wheelMovement() {
    int n = steps.exchange(0);
    turns += abs(n);
    return n;
}
bool pulse() {
    if (!motorPresent) return false;
    motor.run();
    pulses++;
    return true;
}
void boardStatus() {
    Serial.printf("POWER rail_mV=%d source=%s chargeTelemetry=unavailable\n",rail,powerSummary().c_str());
    Serial.printf("HW lcdTransfers=%u touchID=%d touchSamples=%lu detents=%lu motor=%d pulses=%lu psram=%u heap=%u\n",
                  display_transfers(), touchChip, (unsigned long)touches, (unsigned long)turns, motorPresent,
                  (unsigned long)pulses, ESP.getPsramSize(), ESP.getFreeHeap());
}
