// Touch chip (GT911) for the Sticky, read by polling like Seeed's community firmware
// (github.com/danpodeanu/seeed-reterminal-E100X common/src/gt911_touch.cpp).
// WHY: ESPHome's gt911 driver waits for the chip's interrupt line, which never fired on this board.
#pragma once
#include "esphome.h"
#include "driver/gpio.h"
#include <cstring>
#include <string>

namespace sticky_touch {

static const gpio_num_t PIN_RESET = GPIO_NUM_41;
static const gpio_num_t PIN_INT = GPIO_NUM_21;
static const uint16_t REG_COMMAND = 0x8040;
static const uint16_t REG_CONFIG = 0x8047;
static const uint16_t REG_PRODUCT = 0x8140;
static const uint16_t REG_STATUS = 0x814E;
static const uint16_t REG_POINT = 0x814F;

// WHY: no settings are sent to the chip. It keeps its factory settings in its own memory; a 5-round, 15-dot
// tap test (2026-11-19) showed the same accuracy with and without sending them, so none are shipped here.

struct Gt911 {
  esphome::i2c::I2CDevice dev;
  uint8_t addr = 0;
  bool touching = false;
  uint32_t last_report_ms = 0;
  uint32_t touches = 0;
  // diagnostics shown in HA as "Touch status"
  uint32_t reads_ok = 0, reads_failed = 0, reports = 0;
  uint8_t last_st = 0;
  char product[5] = {0};
  bool confirmed = false;
  std::string status = "not started";

  bool ok(esphome::i2c::ErrorCode e) const { return e == esphome::i2c::ERROR_OK; }

  // The INT pin level while RESET is released picks the address: low = 0x5D, high = 0x14
  void reset_for(uint8_t a) {
    gpio_reset_pin(PIN_RESET);
    gpio_reset_pin(PIN_INT);
    gpio_set_direction(PIN_RESET, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_INT, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_RESET, 0);
    gpio_set_level(PIN_INT, a == 0x14 ? 1 : 0);
    esphome::delay(20);
    gpio_set_level(PIN_RESET, 1);
    esphome::delay(20);
    gpio_set_level(PIN_INT, 0);
    esphome::delay(50);
    gpio_set_direction(PIN_INT, GPIO_MODE_INPUT);
    esphome::delay(80);
  }

  bool begin(esphome::i2c::I2CBus *bus) {
    dev.set_i2c_bus(bus);
    addr = 0;
    for (uint8_t a : {(uint8_t) 0x5D, (uint8_t) 0x14}) {
      reset_for(a);
      dev.set_i2c_address(a);
      memset(product, 0, sizeof(product));
      if (!ok(dev.read_register16(REG_PRODUCT, reinterpret_cast<uint8_t *>(product), 4))) continue;
      // the first byte of the chip's settings is their version; 0 or unreadable means the chip has none
      uint8_t version = 0;
      confirmed = ok(dev.read_register16(REG_CONFIG, &version, 1)) && version != 0;
      clear();
      addr = a;
      char buf[64];
      if (confirmed) snprintf(buf, sizeof(buf), "Ready (GT%s at 0x%02X, chip settings v%u)", product, a, version);
      else snprintf(buf, sizeof(buf), "GT%s at 0x%02X has no settings: see README, Touch", product, a);
      status = buf;
      ESP_LOGI("touch", "%s", buf);
      return true;
    }
    status = "Touch chip not found";
    ESP_LOGW("touch", "%s", status.c_str());
    return false;
  }

  void clear() {
    uint8_t zero = 0;
    dev.write_register16(REG_STATUS, &zero, 1);
  }

  // true once per finger-down, with the raw point of that first contact
  bool poll(int &rx, int &ry) {
    if (!addr) return false;
    uint32_t now = esphome::millis();
    // WHY: if the lift report is missed, treat 250 ms of silence as the finger lifting
    if (touching && now - last_report_ms > 250) touching = false;
    uint8_t st = 0;
    if (!ok(dev.read_register16(REG_STATUS, &st, 1))) { reads_failed++; return false; }
    reads_ok++;
    if (st != last_st) ESP_LOGI("touch", "status 0x%02X", st);
    last_st = st;
    if (!(st & 0x80)) return false;
    reports++;
    uint8_t n = st & 0x0F;
    if (n == 0 || n > 5) {
      clear();
      touching = false;
      return false;
    }
    uint8_t p[8] = {0};
    bool got = ok(dev.read_register16(REG_POINT, p, sizeof(p)));
    clear();
    if (!got) return false;
    last_report_ms = now;
    if (touching) return false;
    touching = true;
    touches++;
    rx = p[1] | (p[2] << 8);
    ry = p[3] | (p[4] << 8);
    return true;
  }

  std::string summary() const {
    char b[120];
    snprintf(b, sizeof(b), "%s | reads %u ok %u failed | reports %u | taps %u | last 0x%02X", addr ? "on" : "off",
             (unsigned) reads_ok, (unsigned) reads_failed, (unsigned) reports, (unsigned) touches, last_st);
    return b;
  }
};

static Gt911 T;

}  // namespace sticky_touch
