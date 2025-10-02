#include "rx8025t.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rx8025t {

static const char *const TAG = "rx8025t";

void rx8025tComponent::setup() {
  if (!this->read_rtc_()) {
    this->mark_failed();
  }
}

void rx8025tComponent::update() {
  this->read_time();
}

void rx8025tComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "rx8025t:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  ESP_LOGCONFIG(TAG, "  Timezone: '%s'", this->timezone_.c_str());
}

float rx8025tComponent::get_setup_priority() const { return setup_priority::DATA; }

void rx8025tComponent::read_time() {
  if (!this->read_rtc_()) {
    return;
  }

  ESPTime rtc_time{
      .second = uint8_t(rx8025t_.reg.second + 10u * rx8025t_.reg.second_10),
      .minute = uint8_t(rx8025t_.reg.minute + 10u * rx8025t_.reg.minute_10),
      .hour = uint8_t(rx8025t_.reg.hour + 10u * rx8025t_.reg.hour_10),
	  .day_of_week = uint8_t(rx8025t_.reg.weekday),
      .day_of_month = uint8_t(rx8025t_.reg.day + 10u * rx8025t_.reg.day_10),
      .day_of_year = 1,
      .month = uint8_t(rx8025t_.reg.month + 10u * rx8025t_.reg.month_10),
      .year = uint16_t(rx8025t_.reg.year + 10u * rx8025t_.reg.year_10 + 2000),
      .is_dst = false,
      .timestamp = 0
  };
  rtc_time.recalc_timestamp_utc(false);

  if (!rtc_time.is_valid()) {
    ESP_LOGE(TAG, "Invalid RTC time, not syncing to system clock.");
    return;
  }

  time::RealTimeClock::synchronize_epoch_(rtc_time.timestamp);
}

void rx8025tComponent::write_time() {
  auto now = time::RealTimeClock::utcnow();
  if (!now.is_valid()) {
    ESP_LOGE(TAG, "Invalid system time, not syncing to RTC.");
    return;
  }

  rx8025t_.reg.year = (now.year - 2000) % 10;
  rx8025t_.reg.year_10 = (now.year - 2000) / 10 % 10;
  rx8025t_.reg.month = now.month % 10;
  rx8025t_.reg.month_10 = now.month / 10;
  rx8025t_.reg.day = now.day_of_month % 10;
  rx8025t_.reg.day_10 = now.day_of_month / 10;
  rx8025t_.reg.weekday = now.day_of_week;
  rx8025t_.reg.hour = now.hour % 10;
  rx8025t_.reg.hour_10 = now.hour / 10;
  rx8025t_.reg.minute = now.minute % 10;
  rx8025t_.reg.minute_10 = now.minute / 10;
  rx8025t_.reg.second = now.second % 10;
  rx8025t_.reg.second_10 = now.second / 10;

  this->write_rtc_();
}

bool rx8025tComponent::read_rtc_() {
  if (!this->read_bytes(0, this->rx8025t_.raw, sizeof(this->rx8025t_.raw))) {
    ESP_LOGE(TAG, "Can't read I2C data.");
    return false;
  }

  ESP_LOGD(TAG, "Read  %0u%0u:%0u%0u:%0u%0u 20%0u%0u-%0u%0u-%0u%0u",
           rx8025t_.reg.hour_10, rx8025t_.reg.hour,
           rx8025t_.reg.minute_10, rx8025t_.reg.minute,
           rx8025t_.reg.second_10, rx8025t_.reg.second,
           rx8025t_.reg.year_10, rx8025t_.reg.year,
           rx8025t_.reg.month_10, rx8025t_.reg.month,
           rx8025t_.reg.day_10, rx8025t_.reg.day);

  return true;
}

bool rx8025tComponent::write_rtc_() {
  if (!this->write_bytes(0, this->rx8025t_.raw, sizeof(this->rx8025t_.raw))) {
    ESP_LOGE(TAG, "Can't write I2C data.");
    return false;
  }

  ESP_LOGD(TAG, "Write %0u%0u:%0u%0u:%0u%0u 20%0u%0u-%0u%0u-%0u%0u",
           rx8025t_.reg.hour_10, rx8025t_.reg.hour,
           rx8025t_.reg.minute_10, rx8025t_.reg.minute,
           rx8025t_.reg.second_10, rx8025t_.reg.second,
           rx8025t_.reg.year_10, rx8025t_.reg.year,
           rx8025t_.reg.month_10, rx8025t_.reg.month,
           rx8025t_.reg.day_10, rx8025t_.reg.day);

  return true;
}

}  // namespace rx8025t
}  // namespace esphome
