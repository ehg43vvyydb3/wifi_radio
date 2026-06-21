#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Audio.h>
#include <Arduino_GFX_Library.h>
#include <Preferences.h>
#include "secrets.h"

// ── 핀 ────────────────────────────────────────────────────────────────────────
#define I2S_BCLK   15
#define I2S_LRC    16
#define I2S_DOUT    7
#define TFT_BL     42
#define POT_VOL     1
#define BTN_PLAY   13
#define BTN_NEXT   14

// ── TFT ───────────────────────────────────────────────────────────────────────
Arduino_DataBus *bus = new Arduino_HWSPI(40, 41, 21, 47, -1);
Arduino_ST7789  *gfx = new Arduino_ST7789(bus, 45, 0, true, 240, 240);

// ── 오디오 / 설정 ─────────────────────────────────────────────────────────────
Audio       audio;
Preferences prefs;

// ── 방송국 ────────────────────────────────────────────────────────────────────
enum UrlType { STATIC_URL, KBS_API, MBC_API, SBS_API };
struct Station { const char* name; const char* url; UrlType type; };

const Station STATIONS[] = {
    { "EBS FM",         "https://ebsonair.ebs.co.kr/fmradiofamilypc/familypc1m/playlist.m3u8",               STATIC_URL },
    { "EBS Bandi",      "https://ebsonair.ebs.co.kr/cloud1/iradio/playlist.m3u8",                             STATIC_URL },
    { "TBS FM",         "https://cdnfm.tbs.seoul.kr/tbs/_definst_/tbs_fm_web_360.smil/playlist.m3u8",        STATIC_URL },
    { "TBS eFM",        "https://cdnefm.tbs.seoul.kr/tbs/_definst_/tbs_efm_web_360.smil/playlist.m3u8",      STATIC_URL },
    { "CBS Music FM",   "https://m-aac.cbs.co.kr/mweb_cbs939/_definst_/cbs939.stream/playlist.m3u8",         STATIC_URL },
    { "CBS Std FM",     "https://m-aac.cbs.co.kr/mweb_cbs981/_definst_/cbs981.stream/playlist.m3u8",         STATIC_URL },
    { "KBS 1FM",        "https://kbs-api-proxy.bsod.workers.dev/24",                                          KBS_API    },
    { "KBS 2FM",        "https://kbs-api-proxy.bsod.workers.dev/25",                                          KBS_API    },
    { "MBC FM4U",       "https://sminiplay.imbc.com/aacplay.ashx?agent=webapp&channel=mfm",                   MBC_API    },
    { "MBC Std FM",     "https://sminiplay.imbc.com/aacplay.ashx?agent=webapp&channel=sfm",                   MBC_API    },
    { "SBS Power FM",   "https://apis.sbs.co.kr/play-api/1.0/livestream/powerpc/powerfm?protocol=hls&ssl=Y", SBS_API    },
    { "SBS Love FM",    "https://apis.sbs.co.kr/play-api/1.0/livestream/lovepc/lovefm?protocol=hls&ssl=Y",   SBS_API    },
};
const int NUM_STATIONS = sizeof(STATIONS) / sizeof(STATIONS[0]);

// ── WiFi ──────────────────────────────────────────────────────────────────────
uint8_t bestBSSID[6];
bool    hasBSSID  = false;
int8_t  lastRSSI  = -80;

// ── 상태 ──────────────────────────────────────────────────────────────────────
int  curIdx         = 0;
bool playing        = false;
int  curVol         = 50;   // 0~100
bool lowPower       = false;
bool wifiConnecting = false;
bool needReconnect  = false;
unsigned long reconnectAt = 0;
char statusMsg[40]  = "OK = Play";

// ── 색상 ──────────────────────────────────────────────────────────────────────
#define GREY   0x4208
#define LGREEN 0x07E0

// ── WiFi: 최강 AP 스캔 ────────────────────────────────────────────────────────
void scanBestAP() {
    int n = WiFi.scanNetworks();
    int8_t best = -127;
    for (int i = 0; i < n; i++) {
        if (WiFi.SSID(i) == WIFI_SSID && WiFi.RSSI(i) > best) {
            best = WiFi.RSSI(i);
            lastRSSI = best;
            memcpy(bestBSSID, WiFi.BSSID(i), 6);
            hasBSSID = true;
        }
    }
    WiFi.scanDelete();
    Serial.printf("[WiFi] scan: best RSSI %d, found=%d\n", lastRSSI, hasBSSID);
}

void connectWiFi() {
    if (hasBSSID)
        WiFi.begin(WIFI_SSID, WIFI_PASS, 0, bestBSSID);
    else
        WiFi.begin(WIFI_SSID, WIFI_PASS);
}

// ── 신호 강도 바 (x=214-235, bottom-aligned to y=17) ─────────────────────────
static void drawSignalBars() {
    // RSSI 기준: >= -50: 4칸, >= -60: 3칸, >= -70: 2칸, else: 1칸
    int bars = (lastRSSI >= -50) ? 4 : (lastRSSI >= -60) ? 3 : (lastRSSI >= -70) ? 2 : 1;
    bool ok  = (WiFi.status() == WL_CONNECTED);
    for (int i = 0; i < 4; i++) {
        int h = 4 + i * 4;   // 4, 8, 12, 16 px
        gfx->fillRect(214 + i * 6, 17 - h, 4, h,
                      !ok ? RGB565_RED : (i < bars ? LGREEN : GREY));
    }
}

// ── 볼륨 슬라이더 (x=85-174, y=6-12) ─────────────────────────────────────────
static void drawVolSlider() {
    gfx->drawRect(85, 6, 90, 7, GREY);
    gfx->fillRect(86, 7, 88, 5, RGB565_BLACK);
    int w = map(curVol, 0, 100, 0, 88);
    if (w > 0) gfx->fillRect(86, 7, w, 5, RGB565_CYAN);
}

// ── 상태바 ────────────────────────────────────────────────────────────────────
static void drawStatusBar() {
    gfx->fillRect(0, 0, 240, 18, RGB565_BLACK);
    gfx->setTextSize(1);
    gfx->setTextColor(GREY);
    gfx->setCursor(4, 5);
    gfx->printf("%02d/%02d", curIdx + 1, NUM_STATIONS);
    drawVolSlider();
    drawSignalBars();
}

static void drawStationName() {
    gfx->fillRect(0, 19, 240, 121, RGB565_BLACK);
    const char* name = STATIONS[curIdx].name;
    gfx->setTextSize(2);
    gfx->setTextColor(RGB565_WHITE);
    int16_t tw = strlen(name) * 12;
    int16_t tx = max(4, (240 - tw) / 2);
    gfx->setCursor(tx, 71);
    gfx->print(name);
}

static void drawStatusMsg() {
    gfx->fillRect(0, 141, 240, 75, RGB565_BLACK);
    gfx->drawFastHLine(0, 140, 240, GREY);
    gfx->setTextSize(1);
    gfx->setTextColor(playing ? LGREEN : RGB565_YELLOW);
    gfx->setCursor(4, 150);
    gfx->print(statusMsg);
    gfx->setTextColor(GREY);
    gfx->setCursor(4, 228);
    gfx->print("[POT] Vol  [SW1] Play  [SW2] Next");
}

void drawAll() {
    gfx->fillScreen(RGB565_BLACK);
    drawStatusBar();
    gfx->drawFastHLine(0, 18, 240, GREY);
    drawStationName();
    gfx->drawFastHLine(0, 140, 240, GREY);
    drawStatusMsg();
}

void setStatus(const char* msg, bool isPlaying = false) {
    playing = isPlaying;
    strncpy(statusMsg, msg, sizeof(statusMsg) - 1);
    statusMsg[sizeof(statusMsg) - 1] = '\0';
    drawStatusMsg();
}

// ── URL 조회 (KBS/MBC/SBS 동적 URL) ──────────────────────────────────────────
String resolveUrl(int idx) {
    const Station& st = STATIONS[idx];
    if (st.type == STATIC_URL) return String(st.url);

    setStatus("Fetching URL...");
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, st.url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int code = http.GET();
    if (code != HTTP_CODE_OK) { Serial.printf("[URL] HTTP %d\n", code); http.end(); return ""; }
    String body = http.getString();
    http.end();

    if (st.type == KBS_API) {
        JsonDocument doc;
        auto err = deserializeJson(doc, body);
        if (err) { Serial.printf("[JSON] %s\n", err.c_str()); return ""; }
        return doc["channel_item"][0]["service_url"].as<String>();
    }
    body.trim();
    return body;
}

// ── 재생 제어 ─────────────────────────────────────────────────────────────────
void startPlay(int idx) {
    audio.stopSong();
    playing = false;
    String url = resolveUrl(idx);
    if (url.isEmpty()) { setStatus("URL error"); return; }
    Serial.println("[Play] " + url.substring(0, 80));
    audio.connecttohost(url.c_str());
    setStatus("Buffering...");
}

// ── 저전력 모드 ───────────────────────────────────────────────────────────────
void enterLowPower() {
    lowPower       = true;
    needReconnect  = false;
    wifiConnecting = false;
    audio.stopSong();
    playing = false;
    WiFi.disconnect(true);   // WiFi 라디오까지 OFF
    gfx->fillScreen(RGB565_BLACK);
    digitalWrite(TFT_BL, LOW);
    Serial.println("[LP] Enter low power");
}

void exitLowPower() {
    lowPower = false;
    digitalWrite(TFT_BL, HIGH);
    audio.setVolume(map(curVol, 0, 100, 0, 21));
    drawAll();
    setStatus("Reconnecting WiFi...");
    WiFi.mode(WIFI_STA);
    connectWiFi();           // 저장된 bestBSSID로 재연결 (논블로킹)
    wifiConnecting = true;
    Serial.println("[LP] Exit low power, reconnecting...");
}

// ── 볼륨 폴링 (100ms 주기) ────────────────────────────────────────────────────
void checkPotVolume() {
    static unsigned long lastT = 0;
    if (millis() - lastT < 100) return;
    lastT = millis();

    int newVol  = map(analogRead(POT_VOL), 0, 4095, 0, 100);
    if (newVol == curVol) return;

    int prevVol = curVol;
    curVol = newVol;

    if (newVol == 0 && prevVol > 0 && !lowPower) {
        enterLowPower();
    } else if (newVol > 0 && prevVol == 0 && lowPower) {
        exitLowPower();
    } else if (newVol > 0 && !lowPower) {
        audio.setVolume(map(curVol, 0, 100, 0, 21));
        drawStatusBar();
    }
}

// ── RSSI 주기 업데이트 (5초) ──────────────────────────────────────────────────
void updateRSSI() {
    static unsigned long lastT = 0;
    if (millis() - lastT < 5000) return;
    lastT = millis();
    if (WiFi.status() == WL_CONNECTED) {
        lastRSSI = WiFi.RSSI();
        drawSignalBars();
    }
}

// ── 버튼 헬퍼 (액티브 LOW / 풀업) ────────────────────────────────────────────
bool btnEdge(int pin) {
    if (digitalRead(pin) != LOW) return false;
    delay(25);
    if (digitalRead(pin) != LOW) return false;
    while (digitalRead(pin) == LOW) audio.loop();
    delay(25);
    return true;
}

// ── 오디오 콜백 ───────────────────────────────────────────────────────────────
void audio_info(const char* info) {
    Serial.printf("[audio] %s\n", info);
    if (strstr(info, "Connected") || strstr(info, "connected")) {
        setStatus("Connected", true);
    } else if (playing && !lowPower && curVol > 0 &&
               (strstr(info, "eof") || strstr(info, "EOF") || strstr(info, "StopClient"))) {
        needReconnect = true;
        reconnectAt   = millis() + 3000;
        setStatus("Reconnecting...");
        playing = false;
    }
}

// 스트림 EOF 전용 콜백 (라이브러리 버전에 따라 호출될 수도 있음)
void audio_eof_stream(const char* info) {
    Serial.printf("[eof] %s\n", info);
    if (!lowPower && curVol > 0 && !needReconnect) {
        needReconnect = true;
        reconnectAt   = millis() + 3000;
        setStatus("Reconnecting...");
        playing = false;
    }
}

void audio_showstreamtitle(const char* title) {
    Serial.printf("[title] %s\n", title);
    char buf[40];
    snprintf(buf, sizeof(buf), "%s", title);
    setStatus(buf, true);
}

void audio_bitrate(const char* info) {
    if (!playing) setStatus("Playing", true);
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    gfx->begin();
    gfx->fillScreen(RGB565_BLACK);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(1);

    // 마지막 채널 복원
    prefs.begin("radio", false);
    curIdx = prefs.getInt("ch", 0);
    if (curIdx < 0 || curIdx >= NUM_STATIONS) curIdx = 0;

    pinMode(BTN_PLAY, INPUT_PULLUP);
    pinMode(BTN_NEXT, INPUT_PULLUP);

    // WiFi 스캔 → 최강 AP 선택 → 연결
    gfx->setCursor(4, 100);
    gfx->print("Scanning WiFi...");
    WiFi.mode(WIFI_STA);
    scanBestAP();

    gfx->fillRect(0, 95, 240, 30, RGB565_BLACK);
    gfx->setCursor(4, 100);
    gfx->printf("Connecting (RSSI %d)", lastRSSI);
    connectWiFi();

    int dots = 0, row = 115;
    while (WiFi.status() != WL_CONNECTED) {
        delay(400);
        gfx->print(".");
        if (++dots > 28) { dots = 0; gfx->fillRect(0, row, 240, 10, RGB565_BLACK); gfx->setCursor(4, row); }
    }
    Serial.println("\n[WiFi] " + WiFi.localIP().toString());
    lastRSSI = WiFi.RSSI();

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    curVol = map(analogRead(POT_VOL), 0, 4095, 0, 100);
    audio.setVolume(map(curVol, 0, 100, 0, 21));

    drawAll();
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
    audio.loop();
    checkPotVolume();
    if (lowPower) return;

    updateRSSI();

    // 저전력 복귀 후 WiFi 재연결 완료 → 재생 시작
    if (wifiConnecting && WiFi.status() == WL_CONNECTED) {
        wifiConnecting = false;
        lastRSSI = WiFi.RSSI();
        drawStatusBar();
        startPlay(curIdx);
    }

    // 스트림 끊김 자동 재연결
    if (needReconnect && millis() >= reconnectAt) {
        needReconnect = false;
        startPlay(curIdx);
    }

    if (btnEdge(BTN_NEXT)) {
        audio.stopSong();
        needReconnect = false;
        curIdx = (curIdx + 1) % NUM_STATIONS;
        prefs.putInt("ch", curIdx);   // 채널 저장
        playing = false;
        strncpy(statusMsg, "OK = Play", sizeof(statusMsg));
        drawAll();
    } else if (btnEdge(BTN_PLAY)) {
        if (playing) {
            audio.stopSong();
            needReconnect = false;
            setStatus("Stopped");
            playing = false;
        } else {
            startPlay(curIdx);
        }
    }
}
