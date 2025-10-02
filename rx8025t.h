#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace rx8025t {

class rx8025tComponent : public time::RealTimeClock, public i2c::I2CDevice {
 public:
  //rx8025tComponent(uint8_t address = 0x32) : i2c::I2CDevice(address) {}

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void read_time();
  void write_time();

 protected:
  bool read_rtc_();
  bool write_rtc_();

  union rx8025tReg {
    struct {
      uint8_t second : 4;
      uint8_t second_10 : 3;
      bool unused_0 : 1;

      uint8_t minute : 4;
      uint8_t minute_10 : 3;
      bool unused_1 : 1;

      uint8_t hour : 4;
      uint8_t hour_10 : 2;
      uint8_t unused_2 : 2;

      uint8_t day : 4;
      uint8_t day_10 : 2;
      uint8_t weekday : 3;
      bool unused_3 : 1;

      uint8_t month : 4;
      uint8_t month_10 : 1;
      uint8_t unused_4 : 3;

      uint8_t year : 4;
      uint8_t year_10 : 4;
    } reg;
    mutable uint8_t raw[7];  // 7 bajtov pre rx8025t
  } rx8025t_;
};

// Action triedy pre ESPHome automations
template<typename... Ts> class WriteAction : public Action<Ts...>, public Parented<rx8025tComponent> {
 public:
  void play(Ts... x) override { this->parent_->write_time(); }
};

template<typename... Ts> class ReadAction : public Action<Ts...>, public Parented<rx8025tComponent> {
 public:
  void play(Ts... x) override { this->parent_->read_time(); }
};

}  // namespace rx8025t
}  // namespace esphome
