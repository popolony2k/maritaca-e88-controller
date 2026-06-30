// Copyright (c) M5Stack. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "LED_PaperMono_Class.hpp"
#include "../../M5Unified.hpp"

#if defined (CONFIG_IDF_TARGET_ESP32S3)

namespace m5
{
// RGBLED_R = PMIC -> LED_EN_PP
// RGBLED_G = PYB -> G8
// RGBLED_B = PYB -> G2

  static constexpr size_t led_count = 1;
  static constexpr uint8_t m5pm1_i2c_addr = 0x6E;
  static constexpr uint8_t m5ioe1_i2c_addr = 0x4F;
  static constexpr uint32_t i2c_freq = 100000;

  bool LED_PaperMono_Class::begin(void)
  {
    // PM1 LED_EN (red), set PP mode and enable LED output
    M5.In_I2C.bitOff(m5pm1_i2c_addr, 0x13, 0x20, i2c_freq);

    // IOE1 IO2 (blue), IO8 (green) output
    M5.In_I2C.bitOn(m5ioe1_i2c_addr, 0x03, 0x82, i2c_freq);

    // IOE1 IO2 (blue), IO8 (green) push-pull
    M5.In_I2C.bitOff(m5ioe1_i2c_addr, 0x13, 0x82, i2c_freq);


    {
      setBrightness(_brightness);
      return true;
    }
    return false;
  }

  void LED_PaperMono_Class::setColors(const RGBColor* values, size_t index, size_t length)
  {
    if (index + length > led_count) {
      length = led_count - index;
    }
    std::copy(values, values + length, &_rgb_buffer + index);
  }

  void LED_PaperMono_Class::setBrightness(const uint8_t brightness)
  {
    _brightness = brightness;
    std::array<uint8_t, led_count> br_buffer;
    br_buffer.fill(brightness);
    // writeRegister(0x80, br_buffer.data(), br_buffer.size());
  }

  void LED_PaperMono_Class::display(void)
  {
    // RED = PMIC -> LED_EN_PP
    // GREEN = PYB -> G8
    // BLUE = PYB -> G2 
    uint32_t br = _brightness + 1;
    br = br * br;

    uint16_t r = _rgb_buffer.R8();
    uint16_t g = _rgb_buffer.G8();
    uint16_t b = _rgb_buffer.B8();
    r = (r * br) >> 8;
    g = (g * br) >> 8;
    b = (b * br) >> 8;

    if (r < 2048) {
      M5.In_I2C.bitOff(m5pm1_i2c_addr, 0x06, 0x10, i2c_freq);
    } else {
      M5.In_I2C.bitOn(m5pm1_i2c_addr, 0x06, 0x10, i2c_freq);
    }

    if (g < 2048) {
      M5.In_I2C.bitOff(m5ioe1_i2c_addr, 0x05, 0x80, i2c_freq);
    } else {
      M5.In_I2C.bitOn(m5ioe1_i2c_addr, 0x05, 0x80, i2c_freq);
    }

    if (b < 2048) {
      M5.In_I2C.bitOff(m5ioe1_i2c_addr, 0x05, 0x02, i2c_freq);
    } else {
      M5.In_I2C.bitOn(m5ioe1_i2c_addr, 0x05, 0x02, i2c_freq);
    }

    { // PWM2 (IO8) for green
      if (g > 4095) g = 4095;
      uint8_t data[2] = { uint8_t(g & 0xFF), uint8_t((g >> 8) | 0x80) };
      M5.In_I2C.writeRegister(m5ioe1_i2c_addr, 0x1D, data, sizeof(data), i2c_freq);
    }
  }
}

#endif
