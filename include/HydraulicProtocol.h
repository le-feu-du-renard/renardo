#ifndef HYDRAULIC_PROTOCOL_H
#define HYDRAULIC_PROTOCOL_H

#include <math.h>
#include <stdint.h>

#include "config.h"

// Wire format for the link between the dryer and this hydraulic module, kept
// free of the bus and of any Modbus library so both sides of it can be built
// and tested on the host. **This file exists identically in both repositories**
// and the two copies must stay byte-identical; a disagreement here is the one
// class of fault neither side can detect on its own.
//
// The dryer is the Modbus master, so both directions are driven from there: it
// writes the command block with one FC16 and reads the telemetry block with one
// FC03, every poll cycle. This module never speaks unprompted.
//
// The relationship is the inverse of the extension port's: the dryer commands
// here and reads measurements back, rather than reporting and taking orders.
//
// Register addresses are in config.h (HYDRO_REG_*), which is also mirrored
// between the two repositories.
//
// The tenths-and-sentinel encoding below is deliberately a duplicate of
// ExtensionProtocol.h's rather than a shared dependency. The two are separate
// wire formats that happen to agree today, and coupling them would mean a
// change to one silently altering the other.

// Registers of the command block, offsets from HYDRO_REG_STATE.
enum HydraulicCommandRegister : uint8_t
{
  kHydroCmdRegState        = 0,
  kHydroCmdRegWaterTarget  = 1,
  kHydroCmdRegDryerAirTemp = 2,
};

// Registers of the telemetry block, offsets from HYDRO_REG_WATER_TEMP.
enum HydraulicTelemetryRegister : uint8_t
{
  kHydroRegWaterTemp     = 0,
  kHydroRegTankTemp      = 1,
  kHydroRegStatus        = 2,
  kHydroRegFakeWaterTemp = 3,
  kHydroRegPumpSpeed     = 4,
};

// Status bits, reported in HYDRO_REG_STATUS.
//
// Bit 2 is the one worth understanding from the dryer's side: the module has
// accepted the permission and is deliberately not circulating, because the tank
// is not warm enough for circulating to move heat in the useful direction. That
// is normal operation, not a fault.
enum HydraulicStatusFlag : uint16_t
{
  kHydroFlagCirculating     = 1 << 0,
  kHydroFlagPermission      = 1 << 1, // as received from the dryer, echoed back
  kHydroFlagTankTooCold     = 1 << 2, // local interlock holding the pump off
  kHydroFlagWaterProbeFault = 1 << 3,
  kHydroFlagTankProbeFault  = 1 << 4,
  kHydroFlagFakeProbeFault  = 1 << 5, // wiper position unknown or uncalibrated
  kHydroFlagWatchdogTripped = 1 << 6, // bus silent, everything shut down
  kHydroFlagSetpointMissed  = 1 << 7, // regulating, but not reaching the target
};

// A reading nobody has a value for. Distinct from a real zero, and negative
// because the water loop can legitimately read below freezing.
//
// The map had no sentinel before this revision: the dryer decoded every value
// as signed tenths and believed it, which is why the dryer's own module
// stand-in initialises its registers to plausible figures rather than to zero.
// With this in place a dead probe reads as "no value" instead of as 0.0 C.
constexpr int16_t kHydroInvalidValue = INT16_MIN;

// Same idea for the pump speed, which is whole percents. 0 % is a stopped pump
// and 100 % a legal reading, so the sentinel has to sit outside the range
// rather than at either end of it.
constexpr uint16_t kHydroNoSpeed = 0xFFFF;

// What the dryer asks of the module. NAN means "no value".
struct HydraulicCommand
{
  // A **run permission, not a start pulse**. The dryer raises it once when a
  // session starts with the hydraulic source enabled and holds it for the whole
  // session, across every phase transition, dropping it only when the session
  // ends or an interlock fails. A module that treated each 1 as an edge would
  // do nothing for hours.
  bool run_permitted;

  // Fixed water setpoint the module is to hold, in C.
  float water_target;

  // The dryer's inlet air temperature, so the module can tell whether
  // circulating would actually move heat into the dryer rather than out of it.
  // NAN when the dryer has no fresh reading — the interlock then falls back to
  // the water setpoint alone, which is correct but more cautious.
  float dryer_air_temperature;

  HydraulicCommand()
      : run_permitted(false), water_target(NAN), dryer_air_temperature(NAN) {}
};

// What the module reports back, in engineering units. NAN means "no reading".
struct HydraulicTelemetry
{
  float water_temperature;      // measured, circulating loop
  float tank_temperature;       // measured, storage tank
  float fake_water_temperature; // what the valve controller is being told
  float pump_speed_percent;     // commanded speed, NAN when unknown

  bool circulating;
  bool permission;
  bool tank_too_cold;
  bool water_probe_fault;
  bool tank_probe_fault;
  bool fake_probe_fault;
  bool watchdog_tripped;
  bool setpoint_missed;

  HydraulicTelemetry()
      : water_temperature(NAN), tank_temperature(NAN),
        fake_water_temperature(NAN), pump_speed_percent(NAN),
        circulating(false), permission(false), tank_too_cold(false),
        water_probe_fault(false), tank_probe_fault(false),
        fake_probe_fault(false), watchdog_tripped(false),
        setpoint_missed(false) {}
};

// Tenths, with NAN mapped to the sentinel.
int16_t HydroEncodeValue(float value);
float   HydroDecodeValue(int16_t raw);

// Whole percents, NAN mapped to the sentinel and everything else clamped into
// 0..100 so a drifting figure can never land on the sentinel and read as
// "no speed reported at all".
uint16_t HydroEncodeSpeed(float percent);
float    HydroDecodeSpeed(uint16_t raw);

// Packs and unpacks the status bits.
uint16_t HydroEncodeStatus(const HydraulicTelemetry &telemetry);
void     HydroDecodeStatus(uint16_t bits, HydraulicTelemetry &telemetry);

// Whole blocks. `registers` holds HYDRO_COMMAND_COUNT or HYDRO_TELEMETRY_COUNT
// entries respectively.
//
// Both directions are declared for both blocks even though each firmware only
// needs one of each: having the round trip available is what lets the unit
// tests check that an encode followed by a decode returns what went in, which
// is the property that actually keeps the two repositories in agreement.
void HydroEncodeCommand(const HydraulicCommand &command, uint16_t *registers);
void HydroDecodeCommand(const uint16_t *registers, HydraulicCommand &command);

void HydroEncodeTelemetry(const HydraulicTelemetry &telemetry, uint16_t *registers);
void HydroDecodeTelemetry(const uint16_t *registers, HydraulicTelemetry &telemetry);

#endif // HYDRAULIC_PROTOCOL_H
