#include "Rs485Bus.h"
#include "Logger.h"

Rs485Bus *Rs485Bus::instances_[Rs485Bus::kMaxBuses] = {nullptr, nullptr};
uint8_t   Rs485Bus::instance_count_ = 0;

Rs485Bus::Rs485Bus(SerialUART &port, uint8_t tx_pin, uint8_t rx_pin, uint8_t de_pin,
                   const char *name)
    : port_(port),
      tx_pin_(tx_pin),
      rx_pin_(rx_pin),
      de_pin_(de_pin),
      name_(name),
      slot_(kMaxBuses),
      last_error_(0),
      ready_(false)
{
  if (instance_count_ < kMaxBuses)
  {
    slot_ = instance_count_++;
    instances_[slot_] = this;
  }
}

bool Rs485Bus::Begin(uint32_t baudrate)
{
  if (slot_ >= kMaxBuses)
  {
    Logger::Error("Rs485Bus %s: no trampoline slot available", name_);
    return false;
  }

  port_.setTX(tx_pin_);
  port_.setRX(rx_pin_);
  port_.begin(baudrate, SERIAL_8N1);

  pinMode(de_pin_, OUTPUT);
  digitalWrite(de_pin_, LOW); // Start in receive mode

  ready_ = true;
  Logger::Info("Rs485Bus %s: ready at %lu baud (TX=%d RX=%d DE=%d)",
               name_, baudrate, tx_pin_, rx_pin_, de_pin_);
  return true;
}

void Rs485Bus::BeginTransaction(uint8_t address)
{
  // ModbusMaster binds address and stream in begin(); it must be re-issued for
  // every slave. The direction callbacks are re-armed here because begin()
  // clears them.
  node_.begin(address, port_);
  if (slot_ == 0)
  {
    node_.preTransmission(PreTransmission0);
    node_.postTransmission(PostTransmission0);
  }
  else
  {
    node_.preTransmission(PreTransmission1);
    node_.postTransmission(PostTransmission1);
  }

  delay(kInterFrameDelayMs);
}

bool Rs485Bus::ReadHoldingRegisters(uint8_t address, uint16_t start_register,
                                    uint8_t count, uint16_t *out)
{
  if (!ready_ || out == nullptr || count == 0)
  {
    return false;
  }

  BeginTransaction(address);
  last_error_ = node_.readHoldingRegisters(start_register, count);
  if (last_error_ != ModbusMaster::ku8MBSuccess)
  {
    return false;
  }

  for (uint8_t i = 0; i < count; i++)
  {
    out[i] = node_.getResponseBuffer(i);
  }
  return true;
}

bool Rs485Bus::WriteSingleRegister(uint8_t address, uint16_t reg, uint16_t value)
{
  if (!ready_)
  {
    return false;
  }

  BeginTransaction(address);
  last_error_ = node_.writeSingleRegister(reg, value);
  return last_error_ == ModbusMaster::ku8MBSuccess;
}

bool Rs485Bus::WriteMultipleRegisters(uint8_t address, uint16_t start_register,
                                      uint8_t count, const uint16_t *values)
{
  if (!ready_ || values == nullptr || count == 0)
  {
    return false;
  }

  BeginTransaction(address);
  for (uint8_t i = 0; i < count; i++)
  {
    node_.setTransmitBuffer(i, values[i]);
  }
  last_error_ = node_.writeMultipleRegisters(start_register, count);
  return last_error_ == ModbusMaster::ku8MBSuccess;
}

void Rs485Bus::PreTransmission()
{
  digitalWrite(de_pin_, HIGH); // Enable transmit
}

void Rs485Bus::PostTransmission()
{
  port_.flush();               // Wait for the last byte to fully leave the UART
  digitalWrite(de_pin_, LOW);  // Return to receive
}

// Static trampolines — one pair per bus slot, bound in BeginTransaction().

void Rs485Bus::PreTransmission0()
{
  if (instances_[0] != nullptr) instances_[0]->PreTransmission();
}

void Rs485Bus::PostTransmission0()
{
  if (instances_[0] != nullptr) instances_[0]->PostTransmission();
}

void Rs485Bus::PreTransmission1()
{
  if (instances_[1] != nullptr) instances_[1]->PreTransmission();
}

void Rs485Bus::PostTransmission1()
{
  if (instances_[1] != nullptr) instances_[1]->PostTransmission();
}
