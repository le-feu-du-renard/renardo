#ifndef RS485_BUS_H
#define RS485_BUS_H

#include <Arduino.h>
#include <ModbusMaster.h>

// Modbus RTU master over a half-duplex RS485 transceiver (MAX3485).
//
// ModbusMaster drives the DE/RE line through plain function pointers that take
// no context argument, so v3 reached the pin through static callbacks holding a
// hard-coded macro. Each Rs485Bus instead claims one of kMaxBuses static
// trampoline slots at construction, which keeps the pin an instance member and
// leaves room for a second bus should one ever be added.
//
// The dryer runs a single bus carrying the inlet probe (@1), the extension port
// (@2) and the remote hydraulic module (@10).
//
// All calls are blocking. A bus instance must only be used from the core that
// owns it; there is no internal locking.
class Rs485Bus
{
public:
  static constexpr uint8_t kMaxBuses = 2;

  // Silence between two frames on the same bus. Modbus RTU requires 3.5
  // character times (~4 ms at 9600 baud); 10 ms leaves margin for slow slaves.
  static constexpr uint32_t kInterFrameDelayMs = 10;

  Rs485Bus(SerialUART &port, uint8_t tx_pin, uint8_t rx_pin, uint8_t de_pin, const char *name);

  // Configure the UART and the direction pin, then enable receive mode.
  // Returns false if no trampoline slot was available.
  bool Begin(uint32_t baudrate);

  // FC03 — read `count` consecutive holding registers into `out`.
  bool ReadHoldingRegisters(uint8_t address, uint16_t start_register,
                            uint8_t count, uint16_t *out);

  // FC06 — write one holding register.
  bool WriteSingleRegister(uint8_t address, uint16_t reg, uint16_t value);

  // FC16 — write `count` consecutive holding registers.
  bool WriteMultipleRegisters(uint8_t address, uint16_t start_register,
                              uint8_t count, const uint16_t *values);

  // Modbus result code of the last transaction (0 = success).
  uint8_t GetLastError() const { return last_error_; }

  const char *GetName() const { return name_; }

private:
  SerialUART &port_;
  uint8_t     tx_pin_;
  uint8_t     rx_pin_;
  uint8_t     de_pin_;
  const char *name_;
  uint8_t     slot_;
  uint8_t     last_error_;
  bool        ready_;
  ModbusMaster node_;

  // Prepare node_ for a transaction with `address` and honour the inter-frame gap.
  void BeginTransaction(uint8_t address);

  void PreTransmission();
  void PostTransmission();

  static Rs485Bus *instances_[kMaxBuses];
  static uint8_t   instance_count_;

  static void PreTransmission0();
  static void PostTransmission0();
  static void PreTransmission1();
  static void PostTransmission1();
};

#endif // RS485_BUS_H
