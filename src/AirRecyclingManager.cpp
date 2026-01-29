#include "AirRecyclingManager.h"

AirRecyclingManager::AirRecyclingManager(DFRobot_GP8403 *dac, uint8_t channel)
    : dac_(dac),
      channel_(channel),
      recycling_rate_(kDefaultRecyclingRate)
{
}

void AirRecyclingManager::Begin()
{
  Serial.println("AirRecyclingManager initialized");

  // Apply default recycling rate
  ApplyRecyclingRate();
}

void AirRecyclingManager::SetRecyclingRate(float rate)
{
  // Clamp to valid range
  if (rate < kMinRecyclingRate)
    rate = kMinRecyclingRate;
  if (rate > kMaxRecyclingRate)
    rate = kMaxRecyclingRate;

  if (recycling_rate_ != rate)
  {
    recycling_rate_ = rate;

    Serial.print("Air recycling rate set to: ");
    Serial.print(recycling_rate_);
    Serial.println("%");

    ApplyRecyclingRate();
  }
}

void AirRecyclingManager::Update()
{
  // This method is called periodically to allow future enhancements
  // For now, the DAC value is only updated when the rate changes
}

void AirRecyclingManager::ApplyRecyclingRate()
{
  // Convert recycling rate (0-100%) to voltage in millivolts (0-10000mV)
  // The DAC will output a voltage proportional to the recycling rate
  // At 100% recycling: 10000mV = 10V output
  uint16_t voltage_mv = static_cast<uint16_t>((recycling_rate_ / 100.0f) * 10000.0f);

  Serial.print("Air recycling applying rate to: ");
  Serial.print(voltage_mv);
  Serial.println("mV");

  dac_->setDACOutVoltage(voltage_mv, channel_);
}
