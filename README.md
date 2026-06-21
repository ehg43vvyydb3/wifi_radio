# Wi-Fi Internet Radio

ESP32-S3 기반 인터넷 라디오. 국내 주요 FM 방송 12개 채널을 HLS/AAC 스트리밍으로 수신하고 LCD에 채널 정보를 표시합니다.

## 하드웨어

| 항목 | 사양 |
|------|------|
| MCU | ESP32-S3-N16R8 (16MB Flash, 8MB OPI PSRAM) |
| 디스플레이 | 1.3" ST7789 240×240 IPS LCD |
| 오디오 출력 | I2S 모노 앰프 (내장) |
| 볼륨 조절 | 포텐셔미터 (ADC1 GPIO1) |
| 버튼 | 택타일 스위치 2개 (재생/정지, 다음 채널) |
| 연결 | USB-C (프로그래밍: USB OTG / COM4) |

### 핀 배치

| 핀 | 기능 |
|----|------|
| GPIO 1 | 볼륨 포텐셔미터 (ADC1) |
| GPIO 7 | I2S DOUT (스피커) |
| GPIO 13 | 버튼: 재생/정지 (액티브 LOW, 풀업) |
| GPIO 14 | 버튼: 다음 채널 (액티브 LOW, 풀업) |
| GPIO 15 | I2S BCLK |
| GPIO 16 | I2S LRC |
| GPIO 21 | TFT SCK |
| GPIO 40 | TFT DC |
| GPIO 41 | TFT CS |
| GPIO 42 | TFT 백라이트 |
| GPIO 45 | TFT RST |
| GPIO 47 | TFT MOSI |

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
| Preferences | (ESP32 내장) | 채널 NVS 저장 |

### 빌드 설정

```
FQBN: esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc
Upload Port: COM4 (USB OTG)
```

### 컴파일 & 업로드

```bash
arduino-cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc" wifi_radio/
arduino-cli upload --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=huge_app,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc" --port COM4 wifi_radio/
```

## 기능

### 채널 목록

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

### 조작 방법

| 입력 | 동작 |
|------|------|
| POT 회전 | 볼륨 조절 (0~100단계) |
| SW1 (GPIO13) | 재생 / 정지 토글 |
| SW2 (GPIO14) | 다음 채널로 이동 |

### 화면 레이아웃

```
┌─────────────────────────────────┐
│ 01/12  [══════════════  ] ||||  │  ← 채널번호 / 볼륨슬라이더 / 신호강도
├─────────────────────────────────┤
│                                 │
│           EBS FM                │  ← 방송국 이름
│                                 │
├─────────────────────────────────┤
│ Playing / Buffering / 곡 제목   │  ← 상태 메시지
│                                 │
│ [POT] Vol  [SW1] Play  [SW2] Next│
└─────────────────────────────────┘
```

- **볼륨 슬라이더**: 상태바 중앙, 시안색 채움 바
- **신호 강도**: 상태바 우측, 4단계 막대 (녹색=강, 회색=약, 빨강=끊김)

### 자동 기능

| 기능 | 동작 |
|------|------|
| 듀얼 AP 선택 | 부팅 시 동일 SSID 중 RSSI 최강 AP에 연결 |
| 스트림 자동 재연결 | 재생 중 끊기면 3초 후 자동 재시도 |
| 채널 기억 | 마지막 선택 채널을 NVS에 저장, 재부팅 후 복원 |
| 저전력 모드 | 볼륨=0 시 스트리밍·WiFi 종료 및 백라이트 OFF; 볼륨 복귀 시 자동 재연결 |
