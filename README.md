# Wi-Fi Internet Radio

ESP32-S3 기반 인터넷 라디오. 국내 주요 FM 방송 12개 채널을 HLS/AAC 스트리밍으로 수신하고 LCD에 채널 정보를 표시합니다.

## 하드웨어

| 항목 | 사양 |
|------|------|
| MCU | ESP32-S3-N16R8 (16MB Flash, 8MB OPI PSRAM) |
| 디스플레이 | 1.3" ST7789 240×240 IPS LCD |
| 오디오 출력 | I2S 모노 앰프 (내장) |
| 버튼 | 외부 버튼 모듈 (3.3V 구동, 4개 사용) |
| 연결 | USB-C (프로그래밍: USB OTG / COM4) |

### 핀 배치

| 핀 | 기능 |
|----|------|
| GPIO 7 | I2S DOUT (스피커) |
| GPIO 15 | I2S BCLK |
| GPIO 16 | I2S LRC |
| GPIO 21 | TFT SCK |
| GPIO 40 | TFT DC |
| GPIO 41 | TFT CS |
| GPIO 42 | TFT 백라이트 |
| GPIO 45 | TFT RST |
| GPIO 47 | TFT MOSI |
| GPIO 11 | 버튼: 볼륨+ |
| GPIO 12 | 버튼: 볼륨- |
| GPIO 13 | 버튼: 다음 채널 |
| GPIO 14 | 버튼: 재생/정지 |

> 버튼 모듈 VCC는 반드시 **3.3V**에 연결하세요. 5V 연결 시 GPIO 손상 위험.

## 소프트웨어

### 개발 환경

- Arduino CLI 1.4.1
- ESP32 Arduino Core 3.3.10 (`esp32:esp32`)

### 라이브러리

| 라이브러리 | 버전 | 용도 |
|-----------|------|------|
| ESP32-audioI2S | 3.4.6 | HLS/AAC 스트리밍 재생 |
| GFX Library for Arduino | 1.6.6 | ST7789 LCD 드라이버 |
| ArduinoJson | 7.4.3 | KBS API JSON 파싱 |

### 빌드 설정

```
FQBN: esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc
Upload Port: COM4 (USB OTG)
```

## 설치

1. `wifi_radio/secrets.h` 파일을 생성합니다:

```cpp
#pragma once
#define WIFI_SSID "your_ssid"
#define WIFI_PASS "your_password"
```

2. Arduino CLI로 컴파일 및 업로드:

```bash
arduino-cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc" wifi_radio/
arduino-cli upload --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc" --port COM4 wifi_radio/
```

## 채널 목록

| # | 채널 | 스트림 방식 |
|---|------|------------|
| 1 | EBS FM | HLS (정적) |
| 2 | EBS 반디 | HLS (정적) |
| 3 | TBS FM | HLS (정적) |
| 4 | TBS eFM | HLS (정적) |
| 5 | CBS Music FM | HLS (정적) |
| 6 | CBS 표준FM | HLS (정적) |
| 7 | KBS 1FM | HLS (API) |
| 8 | KBS 2FM | HLS (API) |
| 9 | MBC FM4U | HLS (API) |
| 10 | MBC 표준FM | HLS (API) |
| 11 | SBS Power FM | HLS (API) |
| 12 | SBS Love FM | HLS (API) |

## 조작 방법

| 버튼 | 동작 |
|------|------|
| 재생/정지 | 현재 채널 재생 또는 정지 |
| 다음 채널 | 다음 채널로 이동 (정지 상태) |
| 볼륨+ | 볼륨 증가 (누르고 있으면 연속 증가) |
| 볼륨- | 볼륨 감소 (누르고 있으면 연속 감소) |
