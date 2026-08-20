#include "RemoteModule.h"
#include "Logger.h"

RemoteModule::RemoteModule(Rs485Bus *bus, uint8_t address, const char *name,
                           uint32_t timeout_ms, uint32_t retry_ms)
    : bus_(bus),
      address_(address),
      name_(name),
      timeout_ms_(timeout_ms),
      gate_(retry_ms),
      last_success_ms_(0),
      error_count_(0),
      valid_(false) {}

bool RemoteModule::Exchange(uint16_t write_reg, const uint16_t *write,
                            uint8_t write_count, uint16_t read_reg,
                            uint16_t *read, uint8_t read_count)
{
  if (bus_ == nullptr)
  {
    return false;
  }

  uint32_t now = millis();
  if (!gate_.ShouldAttempt(now))
  {
    return false;
  }

  bool ok = true;

  if (write_count > 0)
  {
    ok = bus_->WriteMultipleRegisters(address_, write_reg, write_count, write);
  }

  // Only read once the write went through: two timeouts for one absent module
  // would double what it costs the poll loop.
  if (ok && read_count > 0)
  {
    ok = bus_->ReadHoldingRegisters(address_, read_reg, read_count, read);
  }

  if (!ok)
  {
    if (error_count_ < kMaxErrors)
    {
      error_count_++;
    }
    gate_.RecordFailure(now);
    Logger::Warning("%s: exchange failed @%d (error %X, count %d)%s", name_,
                    address_, bus_->GetLastError(), error_count_,
                    gate_.IsBackedOff() ? ", backing off" : "");
    return false;
  }

  gate_.RecordSuccess();
  last_success_ms_ = now;
  error_count_     = 0;
  valid_           = true;
  return true;
}

bool RemoteModule::IsAvailable() const
{
  if (!valid_)
  {
    return false;
  }
  return (millis() - last_success_ms_) < timeout_ms_;
}
