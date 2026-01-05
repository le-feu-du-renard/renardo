#ifndef GP8403_H
#define GP8403_H

#include <Arduino.h>
#include <Wire.h>

/**
 * GP8403 2-Channel I2C to 0-10V DAC Driver
 *
 * This DAC converts digital values to analog voltage outputs.
 * Used for air recycling rate control in the dryer system.
 */
class GP8403 {
 public:
  /**
   * DAC channel selection
   */
  enum Channel {
    kChannel0 = 0,
    kChannel1 = 1,
    kBothChannels = 2
  };

  /**
   * DAC voltage output range
   */
  enum Range {
    kRange0To5V = 0,
    kRange0To10V = 1
  };

  /**
   * Constructor
   * @param wire I2C bus instance
   * @param address I2C address (default 0x58)
   */
  GP8403(TwoWire* wire, uint8_t address = 0x58);

  /**
   * Initialize the DAC
   * @param range Voltage output range (0-5V or 0-10V)
   * @return true if initialization successful
   */
  bool Begin(Range range = kRange0To10V);

  /**
   * Set output voltage on a channel
   * @param channel Channel to set (0, 1, or both)
   * @param voltage Voltage to output (0.0 to max voltage based on range)
   */
  void SetVoltage(Channel channel, float voltage);

  /**
   * Set output value as raw DAC value
   * @param channel Channel to set (0, 1, or both)
   * @param value Raw DAC value (0-4095 for 12-bit resolution)
   */
  void SetValue(Channel channel, uint16_t value);

  /**
   * Set output as percentage
   * @param channel Channel to set (0, 1, or both)
   * @param percent Percentage (0.0-100.0)
   */
  void SetPercent(Channel channel, float percent);

  /**
   * Get the current voltage range
   */
  Range GetRange() const { return range_; }

  /**
   * Get maximum voltage for current range
   */
  float GetMaxVoltage() const;

 private:
  static constexpr uint8_t kRegOutputRange = 0x01;
  static constexpr uint8_t kRegOutputChannel0 = 0x02;
  static constexpr uint8_t kRegOutputChannel1 = 0x04;
  static constexpr uint8_t kRegOutputBoth = 0x06;
  static constexpr uint16_t kMaxDacValue = 4095;  // 12-bit DAC

  TwoWire* wire_;
  uint8_t address_;
  Range range_;

  void WriteRegister(uint8_t reg, uint16_t value);
  uint8_t GetChannelRegister(Channel channel) const;
};

#endif  // GP8403_H
