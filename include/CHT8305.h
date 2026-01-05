#ifndef CHT8305_H
#define CHT8305_H

#include <Arduino.h>
#include <Wire.h>

/**
 * Driver for CHT8305 temperature and humidity sensor
 */
class CHT8305 {
 public:
  CHT8305(TwoWire* wire, uint8_t address = 0x40);

  bool Begin();
  bool Update();

  float GetTemperature() const { return temperature_; }
  float GetHumidity() const { return humidity_; }
  bool IsValid() const { return valid_; }

 private:
  TwoWire* wire_;
  uint8_t address_;
  float temperature_;
  float humidity_;
  bool valid_;

  // CHT8305 registers
  static constexpr uint8_t kRegTemperature = 0x00;
  static constexpr uint8_t kRegHumidity = 0x01;
  static constexpr uint8_t kRegConfig = 0x02;

  uint16_t ReadRegister(uint8_t reg);
};

#endif  // CHT8305_H
