#include "ElectricHeater.h"

ElectricHeater::ElectricHeater()
  : power_(0.0f) {
}

void ElectricHeater::Begin() {
  power_ = 0.0f;
  Serial.println("Electric heater initialized");
}

void ElectricHeater::Update() {
  // Nothing to update for binary heater
}

void ElectricHeater::SetPower(float power) {
  // Binary control: 0 or 1
  power_ = (power > 0.5f) ? 1.0f : 0.0f;
}
