// GhostScribe — LovyanGFX driver for the Waveshare ESP32-S3-GEEK 1.14" ST7789 (SPI).
#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel;
  lgfx::Bus_SPI      _bus;
  lgfx::Light_PWM    _light;
public:
  LGFX(void) {
    { auto c = _bus.config();
      c.spi_host = SPI2_HOST; c.spi_mode = 0;
      c.freq_write = 40000000; c.freq_read = 16000000;
      c.spi_3wire = false; c.use_lock = true; c.dma_channel = SPI_DMA_CH_AUTO;
      c.pin_sclk = 12; c.pin_mosi = 11; c.pin_miso = -1; c.pin_dc = 8;
      _bus.config(c); _panel.setBus(&_bus);
    }
    { auto c = _panel.config();
      c.pin_cs = 10; c.pin_rst = 9; c.pin_busy = -1;
      c.panel_width = 135; c.panel_height = 240;
      c.offset_x = 52; c.offset_y = 40; c.offset_rotation = 0;
      c.readable = false; c.invert = true; c.rgb_order = false;
      c.dlen_16bit = false; c.bus_shared = false;
      _panel.config(c);
    }
    { auto c = _light.config();
      c.pin_bl = 7; c.invert = false; c.freq = 1000; c.pwm_channel = 0;
      _light.config(c); _panel.setLight(&_light);
    }
    setPanel(&_panel);
  }
};

// Global display instance — defined once, used everywhere.
extern LGFX tft;

// ---- Drawing API ----
void drawHeader();
void drawWord(int bandY, const String &word, uint16_t color);
void drawDynamic(State st, bool force, bool armed,
                 uint32_t nextFireAt, uint32_t intervalStart,
                 uint32_t typedCount, bool phase2);
void showCurrentNext(const String &currentText, const String &nextText);
void toggleDim();
void toggleScreen();
