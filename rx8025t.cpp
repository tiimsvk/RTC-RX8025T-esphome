#include "rx8025t.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rx8025t {

static const char *const TAG = "rx8025t";

// Helper funkcia na konverziu bitovej masky z RTC na ESPHome day_of_week (1=Ne ... 7=So)
// Datasheet [cite: 548]
static uint8_t mask_to_day(uint8_t mask) {
  switch (mask) {
    case 0x01: return 1; // Nedeľa
    case 0x02: return 2; // Pondelok
    case 0x04: return 3; // Utorok
    case 0x08: return 4; // Streda
    case 0x10: return 5; // Štvrtok
    case 0x20: return 6; // Piatok
    case 0x40: return 7; // Sobota
    default: return 1; // Fallback na nedeľu
  }
}

// Helper funkcia na konverziu ESPHome day_of_week (1=Ne ... 7=So) na bitovú masku pre RTC
// Datasheet [cite: 548]
static uint8_t day_to_mask(uint8_t day) {
  if (day == 0) return 0x40; // Ochrana, ak by ESPHome dalo 0, mapuj na Sobotu (7)
  if (day > 7) day = 7;
  return 1 << (day - 1); // 1=Sun -> 1<<0 = 0x01; 7=Sat -> 1<<6 = 0x40
}

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
	    .day_of_week = mask_to_day(rx8025t_.reg.weekday), // OPRAVENÉ
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
  rx8025t_.reg.weekday = day_to_mask(now.day_of_week); // OPRAVENÉ
  rx8025t_.reg.hour = now.hour % 10;
  rx8025t_.reg.hour_10 = now.hour / 10;
  rx8025t_.reg.minute = now.minute % 10;
  rx8025t_.reg.minute_10 = now.minute / 10;
  rx8025t_.reg.second = now.second % 10;
  rx8025t_.reg.second_10 = now.second / 10;

  this->write_rtc_();
}

bool rx8025tComponent::read_rtc_() {
  // Čítame 7 bajtov od adresy 0x00
  if (!this->read_bytes(0, this->rx8025t_.raw, sizeof(this->rx8025t_.raw))) {
    ESP_LOGE(TAG, "Can't read I2C data.");
    return false;
  }

  ESP_LOGD(TAG, "Read  %0u%0u:%0u%0u:%0u%0u 20%0u%0u-%0u%0u-%0u%0u (Weekday mask: 0x%02X)",
           rx8025t_.reg.hour_10, rx8025t_.reg.hour,
           rx8025t_.reg.minute_10, rx8025t_.reg.minute,
           rx8025t_.reg.second_10, rx8025t_.reg.second,
           rx8025t_.reg.year_10, rx8025t_.reg.year,
           rx8025t_.reg.month_10, rx8025t_.reg.month,
           rx8025t_.reg.day_10, rx8025t_.reg.day,
           rx8025t_.reg.weekday);

  // Dodatočná kontrola - register 0x0E (Flag register)
  // Musíme prečítať VLF (Voltage Low Flag) [cite: 506]
  uint8_t flag_reg = 0;
  if (this->read_register(0x0E, &flag_reg, 1)) {
    if (flag_reg & 0b00000010) { // Check VLF bit (bit 1) [cite: 506]
      ESP_LOGW(TAG, "RTC Voltage Low Flag (VLF) is set. Time data may be invalid!");
    }
  }

  return true;
}

bool rx8025tComponent::write_rtc_() {
  // Zapisujeme 7 bajtov od adresy 0x00
  if (!this->write_bytes(0, this->rx8025t_.raw, sizeof(this->rx8025t_.raw))) {
    ESP_LOGE(TAG, "Can't write I2C data.");
    return false;
  }

  ESP_LOGD(TAG, "Write %0u%0u:%0u%0u:%0u%0u 20%0u%0u-%0u%0u-%0u%0u (Weekday mask: 0x%02X)",
           rx8025t_.reg.hour_10, rx8025t_.reg.hour,
           rx8025t_.reg.minute_10, rx8025t_.reg.minute,
           rx8025t_.reg.second_10, rx8025t_.reg.second,
           rx8025t_.reg.year_10, rx8025t_.reg.year,
           rx8025t_.reg.month_10, rx8025t_.reg.month,
           rx8025t_.reg.day_10, rx8025t_.reg.day,
           rx8025t_.reg.weekday);

  // Po úspešnom zápise musíme vymazať VLF (Voltage Low Flag) bit [cite: 507]
  // Inak by sme po reštarte stále čítali, že dáta sú neplatné.
  // Čítame register 0x0E
  uint8_t flag_reg = 0;
  if (this->read_register(0x0E, &flag_reg, 1)) {
    if (flag_reg & 0b00000010) { // Ak je VLF bit nastavený
      ESP_LOGD(TAG, "Clearing VLF flag.");
      flag_reg &= ~0b00000010; // Vynuluj VLF bit (zápisom 0) [cite: 507]
      this->write_register(0x0E, &flag_reg, 1);
    }
  }

  return true;
}

}  // namespace rx8025t
}  // namespace esphome
