#include "I2CSensor.h"
#include "Logger.h"

static constexpr uint8_t  kRegMeasure    = 0x00;
static constexpr uint32_t kMeasureDelayMs = 20;

I2CSensor::I2CSensor(TwoWire &bus, uint8_t address)
    : bus_(bus), address_(address), error_count_(0)
{
}

bool I2CSensor::Begin()
{
  bus_.beginTransmission(address_);
  uint8_t err = bus_.endTransmission();
  if (err != 0)
  {
    Logger::Error("I2CSensor 0x%02X: not found (err=%d)", address_, err);
    return false;
  }
  Logger::Info("I2CSensor 0x%02X: ready", address_);
  return true;
}

bool I2CSensor::Read(float &temperature, float &humidity)
{
  bus_.beginTransmission(address_);
  bus_.write(kRegMeasure);
  if (bus_.endTransmission() != 0)
  {
    if (++error_count_ >= kMaxErrors) RecoverBus();
    return false;
  }

  delay(kMeasureDelayMs);

  if (bus_.requestFrom(address_, (uint8_t)4) != 4)
  {
    if (++error_count_ >= kMaxErrors) RecoverBus();
    return false;
  }

  uint8_t data[4];
  for (uint8_t i = 0; i < 4; i++)
    data[i] = bus_.read();

  uint16_t raw_t = ((uint16_t)data[0] << 8) | data[1];
  uint16_t raw_h = ((uint16_t)data[2] << 8) | data[3];

  temperature = (float)raw_t * 165.0f / 65535.0f - 40.0f;
  humidity    = constrain((float)raw_h / 65535.0f * 100.0f, 0.0f, 100.0f);

  error_count_ = 0;
  return true;
}

void I2CSensor::RecoverBus()
{
  Logger::Warning("I2CSensor 0x%02X: %d errors — resetting bus", address_, error_count_);
  bus_.end();
  delay(10);
  bus_.begin();
  bus_.setClock(kBusClock);
  bus_.setTimeout(kBusTimeout);
  error_count_ = 0;
}
