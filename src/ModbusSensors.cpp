#include "ModbusSensors.h"
#include "config.h"
#include "Logger.h"

static constexpr uint8_t kMaxErrors = 10;

ModbusSensors::ModbusSensors() : error_count_(0) {}

void ModbusSensors::Begin(uint32_t baudrate)
{
  // Configure UART0 pins before opening the port (GPIO 12/13 = UART0 = Serial1)
  Serial1.setTX(RS485_TX_PIN);
  Serial1.setRX(RS485_RX_PIN);
  Serial1.begin(baudrate, SERIAL_8N1);

  pinMode(RS485_DE_PIN, OUTPUT);
  digitalWrite(RS485_DE_PIN, LOW);  // Start in receive mode

  // ModbusMaster reuses the same physical node instance per call to Begin().
  // Direction callbacks are set once; the address is updated per-request in ReadSensor().
  node_.preTransmission(PreTransmission);
  node_.postTransmission(PostTransmission);

  Logger::Info("ModbusSensors: initialized on UART0 (Serial1) at %lu baud", baudrate);
}

bool ModbusSensors::ReadSensor(uint8_t address, float &temperature, float &humidity)
{
  node_.begin(address, Serial1);

  // Read 2 holding registers starting at MODBUS_REG_HUMIDITY (FC03)
  // Register layout: 0x0000 = humidity, 0x0001 = temperature
  uint8_t result = node_.readHoldingRegisters(MODBUS_REG_HUMIDITY, 2);
  if (result != ModbusMaster::ku8MBSuccess)
  {
    if (error_count_ < kMaxErrors)
    {
      error_count_++;
    }
    Logger::Warning("ModbusSensors: read failed for address %d (error 0x%02X, count %d)",
                    address, result, error_count_);
    return false;
  }

  error_count_ = 0;
  humidity    = static_cast<float>(node_.getResponseBuffer(0)) / MODBUS_RAW_SCALE;
  temperature = static_cast<float>(node_.getResponseBuffer(1)) / MODBUS_RAW_SCALE;
  return true;
}

// Static callbacks — toggle the DE/RE pin to switch UART direction

void ModbusSensors::PreTransmission()
{
  digitalWrite(RS485_DE_PIN, HIGH);  // Enable transmit
}

void ModbusSensors::PostTransmission()
{
  Serial1.flush();                   // Wait for last byte to fully leave the UART
  digitalWrite(RS485_DE_PIN, LOW);   // Return to receive
}
