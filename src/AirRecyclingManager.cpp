#include "AirRecyclingManager.h"

AirRecyclingManager::AirRecyclingManager(GP8403* dac, GP8403::Channel channel)
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
  // Convert recycling rate (0-100%) to DAC output
  // The DAC will output a voltage proportional to the recycling rate
  dac_->SetPercent(channel_, recycling_rate_);
}
