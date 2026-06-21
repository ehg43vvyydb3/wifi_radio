// 스피커 핀 테스트 - 440Hz 사인파 출력
// 소리 안 나면 아래 CONFIG 2로 바꿔서 다시 업로드

#include <driver/i2s.h>
#include <math.h>

// ── CONFIG 1 (먼저 시도) ──────────────────────
#define I2S_BCLK  15
#define I2S_LRCLK 16
#define I2S_DOUT   7

// ── CONFIG 2 (소리 없으면 이걸로 교체) ─────────
// #define I2S_BCLK   7
// #define I2S_LRCLK 15
// #define I2S_DOUT  16

#define SAMPLE_RATE 16000
#define TONE_FREQ   440   // Hz
#define VOLUME      8000  // 0~32767, 너무 크면 왜곡

static float phase = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Speaker test start");
  Serial.printf("BCLK=%d  LRCLK=%d  DOUT=%d\n", I2S_BCLK, I2S_LRCLK, I2S_DOUT);

  i2s_config_t cfg = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate          = SAMPLE_RATE,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count        = 8,
    .dma_buf_len          = 256,
    .use_apll             = false,
  };

  i2s_pin_config_t pins = {
    .bck_io_num   = I2S_BCLK,
    .ws_io_num    = I2S_LRCLK,
    .data_out_num = I2S_DOUT,
    .data_in_num  = I2S_PIN_NO_CHANGE,
  };

  i2s_driver_install(I2S_NUM_1, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_1, &pins);
  Serial.println("I2S ready. 440Hz tone playing...");
}

void loop() {
  const int BUF = 256;
  int16_t buf[BUF];
  const float step = 2.0f * M_PI * TONE_FREQ / SAMPLE_RATE;

  for (int i = 0; i < BUF; i++) {
    buf[i] = (int16_t)(VOLUME * sinf(phase));
    phase += step;
    if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;
  }

  size_t written;
  i2s_write(I2S_NUM_1, buf, sizeof(buf), &written, portMAX_DELAY);
}
