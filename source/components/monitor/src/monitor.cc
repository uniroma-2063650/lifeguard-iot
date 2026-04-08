#include "monitor.hh"
#include <driver/gpio.h>
#include <esp_lcd_panel_commands.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_log.h>

constexpr const char *TAG = "Monitor";

constexpr uint32_t LCD_PIXEL_CLOCK_HZ = 400000;

Monitor::Monitor(i2c_master_bus_handle_t bus, uint8_t i2c_addr,
                 MonitorPinConfig pin_config)
    : pin_config(pin_config) {
  memset(bitmap.data(), 0, bitmap.size() * sizeof(bitmap[0]));
  memset(bitmap_translated.data(), 0,
         bitmap_translated.size() * sizeof(bitmap_translated[0]));

  gpio_config_t power_config = {
      .pin_bit_mask = ((uint64_t)1 << pin_config.power),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_ERROR_CHECK(gpio_config(&power_config));
  ESP_ERROR_CHECK(gpio_set_level(pin_config.power, 0));

  esp_lcd_panel_io_handle_t io_handle;
  esp_lcd_panel_io_i2c_config_t io_config = {.dev_addr = i2c_addr,
                                             .scl_speed_hz = LCD_PIXEL_CLOCK_HZ,
                                             .control_phase_bytes = 1,
                                             .dc_bit_offset = 6,
                                             .lcd_cmd_bits = 8,
                                             .lcd_param_bits = 8,
                                             .on_color_trans_done = nullptr,
                                             .user_ctx = nullptr,
                                             .flags = {
                                                 .dc_low_on_data = false,
                                                 .disable_control_phase = false,
                                             }};
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(bus, &io_config, &io_handle));

  ESP_LOGI(TAG, "Starting SSD1306 panel driver");
  esp_lcd_panel_ssd1306_config_t ssd1306_config = {
      .height = HEIGHT,
  };
  esp_lcd_panel_dev_config_t panel_config = {
      .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
      .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
      .bits_per_pixel = 1,
      .reset_gpio_num = pin_config.rst,
      .vendor_config = &ssd1306_config,
      .flags = {
          .reset_active_high = false,
      }};

  ESP_ERROR_CHECK(
      esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle));

  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
  const uint8_t display_brightness = 0xFF;
  ESP_ERROR_CHECK(
      esp_lcd_panel_io_tx_param(io_handle, 0x81, &display_brightness, 1));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
}

void Monitor::clear(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
  for (uint8_t y = y1; y <= y2; y++) {
    for (uint8_t x = x1; x <= x2; x++) {
      size_t pos = y * WIDTH + x;
      bitmap[pos / 8] &= ~(0x80 >> (pos % 8));
    }
  }
}

void Monitor::invert(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
  for (uint8_t y = y1; y <= y2; y++) {
    for (uint8_t x = x1; x <= x2; x++) {
      size_t pos = y * WIDTH + x;
      bitmap[pos / 8] ^= 0x80 >> (pos % 8);
    }
  }
}

void Monitor::square(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
  for (uint8_t x = x1; x <= x2; x++) {
    {
      size_t pos = y1 * WIDTH + x;
      bitmap[pos / 8] |= 0x80 >> (pos % 8);
    }
    {
      size_t pos = y2 * WIDTH + x;
      bitmap[pos / 8] |= 0x80 >> (pos % 8);
    }
  }
  for (uint8_t y = y1 + 1; y < y2; y++) {
    {
      size_t pos = y * WIDTH + x1;
      bitmap[pos / 8] |= 0x80 >> (pos % 8);
    }
    {
      size_t pos = y * WIDTH + x2;
      bitmap[pos / 8] |= 0x80 >> (pos % 8);
    }
  }
}

void Monitor::square_dotted(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
  for (uint8_t x = x1; x <= x2; x++) {
    {
      size_t pos = y1 * WIDTH + x;
      bool is_on = !((x - x1) & 1);
      bitmap[pos / 8] |= (uint8_t)is_on << 7 >> (pos % 8);
    }
    {
      size_t pos = y2 * WIDTH + x;
      bool is_on = !(((x - x1) ^ (y2 - y1)) & 1);
      bitmap[pos / 8] |= (uint8_t)is_on << 7 >> (pos % 8);
    }
  }
  for (uint8_t y = y1 + 1; y < y2; y++) {
    {
      size_t pos = y * WIDTH + x1;
      bool is_on = !((y - x1) & 1);
      bitmap[pos / 8] |= (uint8_t)is_on << 7 >> (pos % 8);
    }
    {
      size_t pos = y * WIDTH + x2;
      bool is_on = !(((x2 - x1) ^ (y - y1)) & 1);
      bitmap[pos / 8] |= (uint8_t)is_on << 7 >> (pos % 8);
    }
  }
}

#include "font.cc"

void Monitor::number(uint8_t x1, uint8_t y1, uint32_t value) {
  size_t digit_n = 0;
  {
    uint32_t tmp = value;
    do {
      digit_n++;
      tmp /= 10;
    } while (tmp != 0);
  }
  x1 += (FONT_WIDTH + 1) * (digit_n - 1);
  for (uint8_t digit_i = digit_n; digit_i-- > 0;) {
    uint8_t digit = value % 10;
    value /= 10;
    uint8_t *pixels = digits[digit];
    for (uint8_t y = 0; y < FONT_HEIGHT; y++) {
      for (uint8_t x = 0; x < FONT_WIDTH; x++) {
        size_t pos = (y1 + y) * WIDTH + (x1 + x);
        bool is_on = pixels[y] & ((1 << (FONT_WIDTH - 1)) >> x);
        bitmap[pos / 8] |= ((uint8_t)is_on << 7) >> (pos % 8);
      }
    }
    x1 -= FONT_WIDTH + 1;
  }
}

void Monitor::text(uint8_t x1, uint8_t y1, const char *value) {
  for (char character; (character = *value); value++) {
    uint8_t *pixels = font[(uint8_t)character];
    for (uint8_t y = 0; y < FONT_HEIGHT; y++) {
      for (uint8_t x = 0; x < FONT_WIDTH; x++) {
        size_t pos = (y1 + y) * WIDTH + (x1 + x);
        bool is_on = (pixels[y] & ((1 << (FONT_WIDTH - 1)) >> x));
        bitmap[pos / 8] |= ((uint8_t)is_on << 7) >> (pos % 8);
      }
    }
    x1 += FONT_WIDTH + 1;
  }
}

void Monitor::flush(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
  for (uint8_t y = y1; y <= y2; y++) {
    for (uint8_t x = x1; x <= x2; x++) {
      size_t bitmap_byte = (WIDTH * y + x) / 8;
      size_t bitmap_bit = x % 8;
      bool is_on = bitmap[bitmap_byte] & (0x80 >> bitmap_bit);
      size_t bitmap_translated_byte = (y / 8) * WIDTH + x;
      size_t bitmap_translated_bit = y % 8;
      bitmap_translated[bitmap_translated_byte] =
          (bitmap_translated[bitmap_translated_byte] &
           ~(1 << bitmap_translated_bit)) |
          ((uint8_t)is_on << bitmap_translated_bit);
    }
  }

  ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2 + 1,
                                            y2 + 1, bitmap_translated.data()));
}

void Monitor::draw_patient_data(
    const std::array<std::optional<PatientData>, 4> &data) {
  clear();
  for (uint8_t grid_y = 0; grid_y < 2; grid_y++) {
    for (uint8_t grid_x = 0; grid_x < 2; grid_x++) {
      const uint8_t i = (grid_y * 2) + grid_x;

      // Sizing:
      // - Outside: 63x31 pixels
      // - Inside: 61x29 pixels without padding
      //           59x27 pixels with padding (1 px)
      //           (4 lines of 11 characters, with 4x1 pixels left)

      const uint8_t grid_x1 = grid_x * 64 + 1;
      const uint8_t grid_y1 = grid_y * 32 + 1;
      const uint8_t grid_x2 = (grid_x + 1) * 64 - 1;
      const uint8_t grid_y2 = (grid_y + 1) * 32 - 1;
      const uint8_t grid_i_x1 = grid_x1 + 2;
      const uint8_t grid_i_y1 = grid_y1 + 2;

      text(grid_i_x1, grid_i_y1, "Patient ");
      number(grid_i_x1 + 8 * 5, grid_i_y1, i + 1);

      if (data[i].has_value()) {
        const PatientData &patient = data[i].value();
        square(grid_x1, grid_y1, grid_x2, grid_y2);
        text(grid_i_x1, grid_i_y1 + 1 * 7, "HR  : ");
        if (patient.heart_rate.has_value()) {
          number(grid_i_x1 + 6 * 5, grid_i_y1 + 1 * 7,
                 patient.heart_rate.value());
        } else {
          text(grid_i_x1 + 6 * 5, grid_i_y1 + 1 * 7, "N/A");
        }
        text(grid_i_x1, grid_i_y1 + 2 * 7, "SPo2: ");
        if (patient.spo2.has_value()) {
          number(grid_i_x1 + 6 * 5, grid_i_y1 + 2 * 7, patient.spo2.value());
        } else {
          text(grid_i_x1 + 6 * 5, grid_i_y1 + 2 * 7, "N/A");
        }
        text(grid_i_x1 + 2, grid_i_y1 + 3 * 7,
             patient.is_warning ? "  WARNING  " : "  HEALTHY  ");
        if (patient.is_warning) {
          invert(grid_x1 + 1, grid_y1 + 1, grid_x2 - 1, grid_y2 - 1);
        }
      } else {
        square_dotted(grid_x1, grid_y1, grid_x2, grid_y2);
        text(grid_i_x1 + 2, grid_i_y1 + 2 * 7, "NOT PRESENT");
      }
    }
  }
}
