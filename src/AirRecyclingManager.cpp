#include "AirRecyclingManager.h"

AirRecyclingManager::AirRecyclingManager(DFRobot_GP8403* dac, uint8_t channel)
  : dac_(dac),
    channel_(channel),
    recycling_rate_(kDefaultRecyclingRate) {
}

void AirRecyclingManager::Begin() {
  Serial.println("AirRecyclingManager initialized");

  // Apply default recycling rate
  ApplyRecyclingRate();
}

void AirRecyclingManager::SetRecyclingRate(float rate) {
  // Clamp to valid range
  if (rate < kMinRecyclingRate) rate = kMinRecyclingRate;
  if (rate > kMaxRecyclingRate) rate = kMaxRecyclingRate;

  if (recycling_rate_ != rate) {
    recycling_rate_ = rate;

    Serial.print("Air recycling rate set to: ");
    Serial.print(recycling_rate_);
    Serial.println("%");

    ApplyRecyclingRate();
  }
}

void AirRecyclingManager::Update() {
  // This method is called periodically to allow future enhancements
  // For now, the DAC value is only updated when the rate changes
}

void AirRecyclingManager::ApplyRecyclingRate() {
  // Convert recycling rate (0-100%) to DAC value (0-4095)
  // The DAC will output a voltage proportional to the recycling rate
  uint16_t dac_value = static_cast<uint16_t>((recycling_rate_ / 100.0f) * kMaxDacValue);
  dac_->setDACOutVoltage(dac_value, channel_);
}
