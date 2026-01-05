#include "CHT8305.h"

CHT8305::CHT8305(TwoWire* wire, uint8_t address)
  : wire_(wire), address_(address), temperature_(0.0), humidity_(0.0), valid_(false) {
}

bool CHT8305::Begin() {
  wire_->beginTransmission(address_);
  uint8_t error = wire_->endTransmission();

  if (error != 0) {
    Serial.print("CHT8305 not found at address 0x");
    Serial.println(address_, HEX);
    return false;
  }

  // Sensor configuration (normal mode, continuous acquisition)
  wire_->beginTransmission(address_);
  wire_->write(kRegConfig);
  wire_->write(0x10);  // Default configuration
  wire_->write(0x00);
  wire_->endTransmission();

  Serial.println("CHT8305 initialized successfully");
  return true;
}

bool CHT8305::Update() {
  // Read temperature
  uint16_t temperature_raw = ReadRegister(kRegTemperature);
  if (temperature_raw == 0xFFFF) {
    valid_ = false;
    return false;
  }

  // Read humidity
  uint16_t humidity_raw = ReadRegister(kRegHumidity);
  if (humidity_raw == 0xFFFF) {
    valid_ = false;
    return false;
  }

  // Convert according to CHT8305 datasheet
  temperature_ = (temperature_raw / 65535.0) * 165.0 - 40.0;
  humidity_ = (humidity_raw / 65535.0) * 100.0;

  // Limit humidity between 0 and 100%
  if (humidity_ < 0.0) humidity_ = 0.0;
  if (humidity_ > 100.0) humidity_ = 100.0;

  valid_ = true;
  return true;
}

uint16_t CHT8305::ReadRegister(uint8_t reg) {
  wire_->beginTransmission(address_);
  wire_->write(reg);
  if (wire_->endTransmission() != 0) {
    return 0xFFFF;
  }

  delay(10);  // Wait for conversion

  if (wire_->requestFrom(address_, (uint8_t)2) != 2) {
    return 0xFFFF;
  }

  uint16_t value = wire_->read() << 8;
  value |= wire_->read();

  return value;
}
