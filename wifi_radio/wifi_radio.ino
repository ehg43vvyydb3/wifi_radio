#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Audio.h>
#include <Arduino_GFX_Library.h>

// ── WiFi (secrets.h에 SSID/PASS 정의, .gitignore로 제외) ────────────────────
#include "secrets.h"

// ── 핀 ────────────────────────────────────────────────────────────────────────
#define I2S_BCLK   15
#define I2S_LRC    16
#define I2S_DOUT    7
#define TFT_BL     42
#define POT_VOL     1  // 볼륨 포텐셔미터 (ADC1)
#define BTN_PLAY   13  // 재생/정지 (액티브 HIGH, 풀다운)
#define BTN_NEXT   14  // 다음 채널  (액티브 HIGH, 풀다운)

// ── TFT ───────────────────────────────────────────────────────────────────────
Arduino_DataBus *bus = new Arduino_HWSPI(40, 41, 21, 47, -1);
Arduino_ST7789  *gfx = new Arduino_ST7789(bus, 45, 0, true, 240, 240);

// ── 오디오 ────────────────────────────────────────────────────────────────────
Audio audio;

// ── 방송국 ────────────────────────────────────────────────────────────────────
enum UrlType { STATIC_URL, KBS_API, MBC_API, SBS_API };

struct Station {
    const char* name;
    const char* url;  // 정적: 스트림 URL / 동적: API 엔드포인트
    UrlType     type;
};

const Station STATIONS[] = {
    { "EBS FM",         "https://ebsonair.ebs.co.kr/fmradiofamilypc/familypc1m/playlist.m3u8",                    STATIC_URL },
    { "EBS Bandi",      "https://ebsonair.ebs.co.kr/cloud1/iradio/playlist.m3u8",                                 STATIC_URL },
    { "TBS FM",         "https://cdnfm.tbs.seoul.kr/tbs/_definst_/tbs_fm_web_360.smil/playlist.m3u8",             STATIC_URL },
    { "TBS eFM",        "https://cdnefm.tbs.seoul.kr/tbs/_definst_/tbs_efm_web_360.smil/playlist.m3u8",           STATIC_URL },
    { "CBS Music FM",   "https://m-aac.cbs.co.kr/mweb_cbs939/_definst_/cbs939.stream/playlist.m3u8",             STATIC_URL },
    { "CBS Std FM",     "https://m-aac.cbs.co.kr/mweb_cbs981/_definst_/cbs981.stream/playlist.m3u8",             STATIC_URL },
    { "KBS 1FM",        "https://kbs-api-proxy.bsod.workers.dev/24",                                              KBS_API    },
    { "KBS 2FM",        "https://kbs-api-proxy.bsod.workers.dev/25",                                              KBS_API    },
    { "MBC FM4U",       "https://sminiplay.imbc.com/aacplay.ashx?agent=webapp&channel=mfm",                       MBC_API    },
    { "MBC Std FM",     "https://sminiplay.imbc.com/aacplay.ashx?agent=webapp&channel=sfm",                       MBC_API    },
    { "SBS Power FM",   "https://apis.sbs.co.kr/play-api/1.0/livestream/powerpc/powerfm?protocol=hls&ssl=Y",      SBS_API    },
    { "SBS Love FM",    "https://apis.sbs.co.kr/play-api/1.0/livestream/lovepc/lovefm?protocol=hls&ssl=Y",        SBS_API    },
};
const int NUM_STATIONS = sizeof(STATIONS) / sizeof(STATIONS[0]);

// ── 상태 ──────────────────────────────────────────────────────────────────────
int  curIdx   = 0;
bool playing  = false;
int  curVol   = 14;   // 0~21
char statusMsg[40] = "OK = Play";

// ── 디스플레이 ────────────────────────────────────────────────────────────────
// 레이아웃 (240x240):
//   y  0-17 : 상태바 (채널번호 / WiFi)
//   y 18    : 구분선
//   y 19-139: 방송국 이름 (중앙)
//   y 140   : 구분선
//   y 141-215: 상태 메시지
//   y 216-239: 버튼 힌트

#define GREY   0x4208
#define LGREEN 0x07E0

static void drawStatusBar() {
    gfx->fillRect(0, 0, 240, 18, RGB565_BLACK);
    gfx->setTextSize(1);
    gfx->setTextColor(GREY);
    gfx->setCursor(4, 5);
    gfx->printf("%02d/%02d", curIdx + 1, NUM_STATIONS);
    // 볼륨 표시 (중앙)
    gfx->setTextColor(RGB565_CYAN);
    gfx->setCursor(100, 5);
    gfx->printf("V:%02d", curVol);
    // WiFi 상태 (우측)
    gfx->setCursor(186, 5);
    gfx->setTextColor(WiFi.status() == WL_CONNECTED ? LGREEN : RGB565_RED);
    gfx->print("WiFi");
}

static void drawStationName() {
    gfx->fillRect(0, 19, 240, 121, RGB565_BLACK);
    const char* name = STATIONS[curIdx].name;
    gfx->setTextSize(2);
    gfx->setTextColor(RGB565_WHITE);
    int16_t tw = strlen(name) * 12;
    int16_t tx = max(4, (240 - tw) / 2);
    gfx->setCursor(tx, 79 - 8);  // 중앙 y=79
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
    if (code != HTTP_CODE_OK) {
        Serial.printf("[URL] HTTP %d\n", code);
        http.end();
        return "";
    }

    String body = http.getString();
    http.end();

    if (st.type == KBS_API) {
        JsonDocument doc;
        auto err = deserializeJson(doc, body);
        if (err) { Serial.printf("[JSON] %s\n", err.c_str()); return ""; }
        return doc["channel_item"][0]["service_url"].as<String>();
    }

    // MBC_API, SBS_API: 평문 URL 반환
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

// ── 버튼 헬퍼 (액티브 HIGH / 풀다운) ─────────────────────────────────────────
bool btnEdge(int pin) {
    if (digitalRead(pin) != LOW) return false;
    delay(25);
    if (digitalRead(pin) != LOW) return false;
    while (digitalRead(pin) == LOW) audio.loop();
    delay(25);
    return true;
}

// ── 포텐셔미터 볼륨 (100ms 주기 폴링) ────────────────────────────────────────
void checkPotVolume() {
    static unsigned long lastT = 0;
    if (millis() - lastT < 100) return;
    lastT = millis();

    int newVol = map(analogRead(POT_VOL), 0, 4095, 0, 21);
    if (newVol != curVol) {
        curVol = newVol;
        audio.setVolume(curVol);
        drawStatusBar();
    }
}

// ── 오디오 콜백 ───────────────────────────────────────────────────────────────
void audio_info(const char* info) {
    Serial.printf("[audio] %s\n", info);
    if (strstr(info, "Connected") || strstr(info, "connected")) {
        setStatus("Connected", true);
    } else if (strstr(info, "eof") || strstr(info, "EOF")) {
        setStatus("Stream ended");
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
    // 비트레이트 수신 = 실제 재생 중
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
    gfx->setTextSize(2);
    gfx->setCursor(20, 105);
    gfx->print("WiFi connecting");

    pinMode(BTN_PLAY, INPUT_PULLUP);
    pinMode(BTN_NEXT, INPUT_PULLUP);

    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int dots = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(400);
        gfx->print(".");
        if (++dots > 15) { dots = 0; gfx->fillRect(0, 125, 240, 20, RGB565_BLACK); gfx->setCursor(20, 125); }
    }
    Serial.println("\n[WiFi] " + WiFi.localIP().toString());

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    curVol = map(analogRead(POT_VOL), 0, 4095, 0, 21);  // 초기 볼륨을 포텐셔미터 위치로
    audio.setVolume(curVol);

    drawAll();
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
    audio.loop();
    checkPotVolume();

    if (btnEdge(BTN_NEXT)) {
        audio.stopSong();
        curIdx = (curIdx + 1) % NUM_STATIONS;
        playing = false;
        strncpy(statusMsg, "OK = Play", sizeof(statusMsg));
        drawAll();
    } else if (btnEdge(BTN_PLAY)) {
        if (playing) {
            audio.stopSong();
            setStatus("Stopped");
            playing = false;
        } else {
            startPlay(curIdx);
        }
    }
}
