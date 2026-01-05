#include "GP8403.h"

GP8403::GP8403(TwoWire* wire, uint8_t address)
  : wire_(wire),
    address_(address),
    range_(kRange0To10V) {
}

bool GP8403::Begin(Range range) {
  range_ = range;

  // Set output range register
  WriteRegister(kRegOutputRange, static_cast<uint16_t>(range));

  // Initialize both channels to 0V
  SetValue(kBothChannels, 0);

  Serial.print("GP8403 DAC initialized at 0x");
  Serial.print(address_, HEX);
  Serial.print(" with range: ");
  Serial.print(range == kRange0To10V ? "0-10V" : "0-5V");
  Serial.println();

  return true;
}

void GP8403::SetVoltage(Channel channel, float voltage) {
  // Clamp voltage to valid range
  float max_voltage = GetMaxVoltage();
  if (voltage < 0.0f) voltage = 0.0f;
  if (voltage > max_voltage) voltage = max_voltage;

  // Convert voltage to DAC value
  uint16_t dac_value = static_cast<uint16_t>((voltage / max_voltage) * kMaxDacValue);
  SetValue(channel, dac_value);
}

void GP8403::SetValue(Channel channel, uint16_t value) {
  // Clamp to 12-bit range
  if (value > kMaxDacValue) {
    value = kMaxDacValue;
  }

  uint8_t reg = GetChannelRegister(channel);
  WriteRegister(reg, value);
}

void GP8403::SetPercent(Channel channel, float percent) {
  // Clamp to 0-100%
  if (percent < 0.0f) percent = 0.0f;
  if (percent > 100.0f) percent = 100.0f;

  // Convert percentage to voltage
  float voltage = (percent / 100.0f) * GetMaxVoltage();
  SetVoltage(channel, voltage);
}

float GP8403::GetMaxVoltage() const {
  return (range_ == kRange0To10V) ? 10.0f : 5.0f;
}

void GP8403::WriteRegister(uint8_t reg, uint16_t value) {
  wire_->beginTransmission(address_);
  wire_->write(reg);
  wire_->write(static_cast<uint8_t>(value >> 8));  // MSB
  wire_->write(static_cast<uint8_t>(value & 0xFF));  // LSB
  wire_->endTransmission();
}

uint8_t GP8403::GetChannelRegister(Channel channel) const {
  switch (channel) {
    case kChannel0:
      return kRegOutputChannel0;
    case kChannel1:
      return kRegOutputChannel1;
    case kBothChannels:
      return kRegOutputBoth;
    default:
      return kRegOutputChannel0;
  }
}
