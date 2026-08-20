#ifndef EXTENSION_PROTOCOL_H
#define EXTENSION_PROTOCOL_H

#include <math.h>
#include <stdint.h>

#include "config.h"

// Wire format for the extension port, kept free of the bus and of ModbusMaster
// so both sides of it can be built and tested on the host. The module author
// compiles this same header.
//
// The dryer is the Modbus master, so both directions are driven from here: it
// writes the telemetry block with one FC16 and reads the command mailbox with
// one FC03, every poll cycle.
//
// The layout descends from the v3 radio frame — that frame had already settled
// what is worth sending, and a decoder written for it still reads bits 0..7 of
// the flags unchanged. What the radio needed and Modbus does not is the framing:
// no magic byte, no checksum, no packing. Modbus supplies all three.

// Registers of the telemetry block, offsets from EXT_REG_TELEMETRY.
enum ExtensionTelemetryRegister : uint8_t
{
  kExtRegVersion       = 0,
  kExtRegFlags         = 1,
  kExtRegPhase         = 2,
  kExtRegInletTemp     = 3,
  kExtRegInletHumidity = 4,
  kExtRegWaterTemp     = 5,
  kExtRegTankTemp      = 6,
  kExtRegTargetTemp    = 7,
  kExtRegTargetHumidity = 8,
  kExtRegExtractionPos = 9,
  kExtRegRecyclingPos  = 10,
  kExtRegElapsedHigh   = 11,
  kExtRegElapsedLow    = 12,
  kExtRegUptimeHigh    = 13,
  kExtRegUptimeLow     = 14,
  kExtRegAckSequence   = 15,
  kExtRegAckResult     = 16,
};

// Registers of the command mailbox, offsets from EXT_REG_COMMAND.
enum ExtensionCommandRegister : uint8_t
{
  kExtCmdRegSequence = 0,
  kExtCmdRegOpcode   = 1,
  kExtCmdRegArgument = 2,
  kExtCmdRegVersion  = 3,
};

// Bits 0..7 keep the assignments the v3 radio used, so a decoder written for it
// still reads them. Bit 8 is new: the 16-bit register has room the byte did not.
enum ExtensionFlag : uint16_t
{
  kExtFlagRunning       = 1 << 0,
  kExtFlagFan           = 1 << 1,
  kExtFlagElectric      = 1 << 2,
  kExtFlagHydraulic     = 1 << 3,
  kExtFlagDamperOpen    = 1 << 4,
  kExtFlagSensorFault   = 1 << 5,
  kExtFlagHydraulicOff  = 1 << 6, // module unreachable
  kExtFlagAirflowFault  = 1 << 7, // every register shut: the air path is closed
  kExtFlagFeedbackFault = 1 << 8, // a register's position readback is unusable
};

enum ExtensionOpcode : uint16_t
{
  kExtCmdNone = 0,
  // Reserved and always refused. Starting a session is a physical gesture in
  // front of the machine; the opcode exists so a module author gets an answer
  // rather than silence.
  kExtCmdStart       = 1,
  kExtCmdStop        = 2,
  kExtCmdSetTemp     = 3,
  kExtCmdSetHumidity = 4,
};

// Written back in kExtRegAckResult so the module learns what became of its
// command instead of watching the telemetry and guessing.
enum ExtensionResult : uint16_t
{
  kExtResultOk           = 0,
  kExtResultUnknownOpcode = 1,
  kExtResultRefused      = 2, // understood, refused by policy
  kExtResultOutOfRange   = 3,
  kExtResultBadVersion   = 4,
};

// A reading the dryer has no value for. Distinct from a real zero, and negative
// because the water loop can legitimately read below freezing.
constexpr int16_t kExtInvalidValue = INT16_MIN;

// Same idea for a register opening, which is whole percents. 0 % is a shut
// register and 100 % a legal reading, so the sentinel has to sit outside the
// range rather than at either end of it.
constexpr uint16_t kExtNoPosition = 0xFFFF;

// What the dryer reports, in engineering units. NAN means "no reading".
struct ExtensionTelemetry
{
  float inlet_temperature;
  float inlet_humidity;
  float water_temperature;
  float tank_temperature;
  float target_temperature;
  float target_humidity;
  float extraction_position; // %, NAN when unknown
  float recycling_position;  // %, NAN when unknown

  uint32_t session_elapsed_s;
  uint32_t uptime_s;
  uint8_t  phase;

  bool running;
  bool fan_on;
  bool electric_on;
  bool hydraulic_on;
  bool hydraulic_online;
  bool damper_open;
  bool sensor_fault;
  bool airflow_fault;
  bool feedback_fault;

  // Echoed back so the module can stop repeating a command.
  uint16_t ack_sequence;
  uint16_t ack_result;

  ExtensionTelemetry()
      : inlet_temperature(NAN), inlet_humidity(NAN),
        water_temperature(NAN), tank_temperature(NAN),
        target_temperature(NAN), target_humidity(NAN),
        extraction_position(NAN), recycling_position(NAN),
        session_elapsed_s(0), uptime_s(0), phase(0),
        running(false), fan_on(false), electric_on(false),
        hydraulic_on(false), hydraulic_online(false), damper_open(false),
        sensor_fault(false), airflow_fault(false), feedback_fault(false),
        ack_sequence(0), ack_result(kExtResultOk) {}
};

// One command as read out of the mailbox and validated.
struct ExtensionCommand
{
  uint16_t sequence; // 0 = the mailbox is empty
  uint16_t opcode;
  float    argument; // engineering units; meaning depends on the opcode

  ExtensionCommand() : sequence(0), opcode(kExtCmdNone), argument(NAN) {}
};

// Tenths, with NAN mapped to the sentinel.
int16_t ExtEncodeValue(float value);
float   ExtDecodeValue(int16_t raw);

// Whole percents, NAN mapped to the sentinel and everything else clamped into
// 0..100 so a drifting feedback can never land on the sentinel and read as
// "no feedback at all".
uint16_t ExtEncodePosition(float percent);
float    ExtDecodePosition(uint16_t raw);

// Packs the flag bits out of the telemetry booleans.
uint16_t ExtEncodeFlags(const ExtensionTelemetry &telemetry);

// Fills the whole telemetry block. `out` holds EXT_TELEMETRY_COUNT registers.
void ExtEncodeTelemetry(const ExtensionTelemetry &telemetry, uint16_t *out);

// Reads and validates the mailbox. `in` holds EXT_COMMAND_COUNT registers.
//
// Returns kExtResultOk when `command` may be executed. On any other result
// `command` still carries the sequence, so the caller can acknowledge the
// refusal against the right one — a module that never learns its command was
// rejected would repeat it forever.
//
// An empty mailbox (sequence 0, or opcode kExtCmdNone) yields kExtResultOk with
// the opcode left at kExtCmdNone; there is nothing to refuse.
ExtensionResult ExtDecodeCommand(const uint16_t *in, ExtensionCommand &command);

// Remembers which mailbox sequence has already been executed.
//
// The radio needed a replay filter because a frame could arrive twice over the
// air. Modbus cannot duplicate a read — but the dryer re-reads the *same*
// mailbox every cycle, and would otherwise replay a resident command forever.
// So the filter is still needed, for the opposite reason, and with the bus
// offering neither loss nor reordering it collapses to an inequality: the signed
// window the radio's byte-wide sequence needed is gone.
class ExtensionCommandFilter
{
public:
  ExtensionCommandFilter() : last_sequence_(0) {}

  // True when this sequence has not been executed yet. Sequence 0 means the
  // mailbox is empty and never executes.
  bool ShouldExecute(uint16_t sequence) const
  {
    return sequence != 0 && sequence != last_sequence_;
  }

  void MarkExecuted(uint16_t sequence) { last_sequence_ = sequence; }

  uint16_t GetLastSequence() const { return last_sequence_; }

  void Reset() { last_sequence_ = 0; }

private:
  uint16_t last_sequence_;
};

#endif // EXTENSION_PROTOCOL_H
