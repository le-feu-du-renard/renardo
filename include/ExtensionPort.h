#ifndef EXTENSION_PORT_H
#define EXTENSION_PORT_H

#include <Arduino.h>
#include "ExtensionProtocol.h"
#include "RemoteModule.h"
#include "Rs485Bus.h"

// Client for whatever is plugged into the extension port on RS485.
//
// One connector for optional modules — data logger, energy metering, a WiFi or
// LoRa gateway, a deported panel. The dryer gains a feature by gaining a module,
// and **none of them is ever load-bearing for regulation**: this is the whole
// difference with HydraulicRemote, which carries a heat source. Nothing here may
// stop a session, and an absent module must cost the poll loop as little as
// possible — hence the longer backoff.
//
// The relationship is the inverse of the hydraulic one: the dryer reports to the
// module and reads the module's orders, rather than commanding it and reading
// its measurements. Only the register blocks differ though — address, health and
// backoff come from RemoteModule.
//
// Register map is declared in config.h (EXT_REG_*) and the wire format in
// ExtensionProtocol.h, which the module firmware compiles too.
class ExtensionPort : public RemoteModule
{
public:
  // Same silence window as the hydraulic module.
  static constexpr uint32_t kTimeoutMs = 30000;

  // Poked back to life three times less often: it carries nothing the dryer
  // needs, so an empty port should cost the poll loop as little as possible.
  static constexpr uint32_t kRetryMs = 30000;

  explicit ExtensionPort(Rs485Bus *bus);

  void Begin();

  // What the next Update() will report. Set from the core that owns the bus.
  void SetTelemetry(const ExtensionTelemetryRecord &telemetry) { telemetry_ = telemetry; }

  // One exchange: push the telemetry, then read the command mailbox.
  bool Update();

  // The command sitting in the mailbox as of the last successful exchange, and
  // the verdict on it. Sequence 0 means there is nothing there.
  //
  // Validation happens here, but **execution does not**: the command crosses to
  // the core that owns the dryer, and the replay filter lives there with it.
  const ExtensionCommand &GetPendingCommand() const { return pending_; }
  ExtensionResult         GetPendingResult()  const { return pending_result_; }

private:
  ExtensionTelemetryRecord telemetry_;
  ExtensionCommand         pending_;
  ExtensionResult          pending_result_;
};

#endif // EXTENSION_PORT_H
