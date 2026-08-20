#include "config.h"
#include "ExtensionPort.h"
#include "Logger.h"

ExtensionPort::ExtensionPort(Rs485Bus *bus)
    : RemoteModule(bus, MODBUS_EXTENSION_ADDRESS, "ExtensionPort",
                   kTimeoutMs, kRetryMs),
      pending_result_(kExtResultOk) {}

void ExtensionPort::Begin()
{
  Logger::Info("ExtensionPort: module @%d on bus %s, protocol v%d",
               GetAddress(), bus()->GetName(), EXT_PROTOCOL_VERSION);
}

bool ExtensionPort::Update()
{
  uint16_t block[EXT_TELEMETRY_COUNT];
  ExtEncodeTelemetry(telemetry_, block);

  uint16_t mailbox[EXT_COMMAND_COUNT] = {0, 0, 0, 0};

  if (!Exchange(EXT_REG_TELEMETRY, block, EXT_TELEMETRY_COUNT,
                EXT_REG_COMMAND, mailbox, EXT_COMMAND_COUNT))
  {
    // Leave the last known command alone. It has either been executed already —
    // the filter on the other core keeps it from running twice — or it never
    // arrived, and inventing an empty mailbox here would just churn.
    return false;
  }

  pending_result_ = ExtDecodeCommand(mailbox, pending_);

  if (pending_.sequence != 0 && pending_result_ != kExtResultOk)
  {
    Logger::Warning("ExtensionPort: command %d refused (opcode %d, result %d)",
                    pending_.sequence, pending_.opcode, pending_result_);
  }

  return true;
}
