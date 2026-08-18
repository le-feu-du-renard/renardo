#ifndef LORA_PROTOCOL_H
#define LORA_PROTOCOL_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>

// Wire format for the link to the Commander, kept free of any radio dependency
// so both sides of it can be tested on the host.
//
// Frames are packed and byte-summed. Over the air there is no transport to lean
// on: a frame can arrive corrupted, duplicated, or out of order, and every one
// of those has to be rejected here rather than acted upon.

// Version 2 dropped the outlet probe's two fields from the telemetry frame.
// Version 3 split the single damper opening into one per register, extraction
// and recycling, when the dryer gained a second one.
//
// The frame is fixed-layout and byte-summed, so a frame of a different length
// cannot be read by an older receiver at all — hence a version bump rather than
// zero-filled fields. `IsTelemetryValid` rejects any other version, so a
// Commander still speaking v2 goes quiet instead of decoding the checksum as a
// position. **The Commander has to be updated in the same breath as this.**
#define LORA_PROTOCOL_VERSION 3
#define LORA_TELEMETRY_MAGIC 0xD7
#define LORA_COMMAND_MAGIC 0xD8

// Actuator and state bits in TelemetryPacket::flags.
enum LoraFlag : uint8_t
{
  kLoraFlagRunning      = 1 << 0,
  kLoraFlagFan          = 1 << 1,
  kLoraFlagElectric     = 1 << 2,
  kLoraFlagHydraulic    = 1 << 3,
  kLoraFlagDamperOpen   = 1 << 4,
  kLoraFlagSensorFault  = 1 << 5,
  kLoraFlagHydraulicOff = 1 << 6, // module unreachable
};

enum LoraCommandCode : uint8_t
{
  kLoraCommandNone       = 0,
  kLoraCommandStart      = 1,
  kLoraCommandStop       = 2,
  kLoraCommandSetTemp    = 3,
  kLoraCommandSetHumidity = 4,
};

// Uplink. Temperatures and humidities travel as tenths, signed: the water loop
// can legitimately be below zero.
struct __attribute__((packed)) TelemetryPacket
{
  uint8_t  magic;
  uint8_t  version;
  uint16_t device_id;
  uint32_t sequence;
  uint32_t uptime_s;
  uint32_t session_elapsed_s;

  int16_t  inlet_temperature;
  int16_t  inlet_humidity;
  int16_t  water_temperature;
  int16_t  tank_temperature;
  int16_t  target_temperature;
  int16_t  target_humidity;

  uint8_t  phase;
  uint8_t  flags;
  // Measured opening of each register, %, kLoraNoPosition when that one has no
  // usable feedback. Both travel: the registers are asymmetric, so one figure
  // cannot stand for the other, and a register that stops arriving is only
  // visible if its own opening is on the wire.
  uint8_t  extraction_position;
  uint8_t  recycling_position;
  uint8_t  last_command_ack; // sequence number of the last command executed

  uint16_t checksum;
};

// Downlink.
struct __attribute__((packed)) CommandPacket
{
  uint8_t  magic;
  uint8_t  version;
  uint16_t device_id;
  uint8_t  sequence;
  uint8_t  command;
  int16_t  argument;   // tenths, meaning depends on `command`
  uint16_t checksum;
};

// What the dryer reports, in engineering units.
//
// Deliberately not the display model: the screen shows a subset — it has no
// room for the water setpoint or the session sequence — and the server wants
// the full picture, so tying the two together would mean one of them carrying
// fields for the other's benefit.
struct TelemetryData
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
  uint8_t  phase;

  bool running;
  bool fan_on;
  bool electric_on;
  bool hydraulic_on;
  bool hydraulic_online;
  bool damper_open;
  bool sensor_fault;

  TelemetryData()
      : inlet_temperature(NAN), inlet_humidity(NAN),
        water_temperature(NAN), tank_temperature(NAN),
        target_temperature(NAN), target_humidity(NAN),
        extraction_position(NAN), recycling_position(NAN),
        session_elapsed_s(0), phase(0),
        running(false), fan_on(false), electric_on(false),
        hydraulic_on(false), hydraulic_online(false),
        damper_open(false), sensor_fault(false) {}
};

// Value used when a reading is unavailable, so the server can tell a missing
// probe from a real zero.
constexpr int16_t kLoraInvalidValue = INT16_MIN;

// Same idea for the openings, which are whole percents in a byte: 0 % is a
// closed register, this is no feedback at all. 100 % being a legal reading, the
// sentinel has to sit outside the range rather than at its top.
constexpr uint8_t kLoraNoPosition = 0xFF;

// Converts a reading to tenths, mapping NaN to kLoraInvalidValue.
int16_t LoraEncodeValue(float value);
float   LoraDecodeValue(int16_t raw);

// Converts an opening to whole percents, mapping NaN to kLoraNoPosition and
// clamping the rest into 0..100 so a drifting feedback can never land on the
// sentinel and read as "no feedback".
uint8_t LoraEncodePosition(float percent);
float   LoraDecodePosition(uint8_t raw);

// Byte sum over everything before the trailing checksum field.
uint16_t LoraChecksum(const void *data, size_t length);

void SealTelemetry(TelemetryPacket &packet);
bool IsTelemetryValid(const TelemetryPacket &packet);

void SealCommand(CommandPacket &packet);
bool IsCommandValid(const CommandPacket &packet, uint16_t expected_device_id);

// Tracks which downlink sequence numbers have already been executed.
//
// The Commander repeats a command until it sees the acknowledgement, so the
// same frame arrives several times; executing a START twice is harmless, but
// executing a stale "set temperature" that was superseded is not. Sequence
// numbers wrap in a byte, so "already seen" is decided on a signed window
// rather than a plain comparison.
class LoraCommandFilter
{
public:
  LoraCommandFilter() : last_sequence_(0), primed_(false) {}

  // True when this frame should be executed now.
  bool ShouldExecute(uint8_t sequence);

  // Last sequence executed, echoed back in the telemetry so the sender can
  // stop repeating.
  uint8_t GetLastSequence() const { return last_sequence_; }

  void Reset()
  {
    last_sequence_ = 0;
    primed_ = false;
  }

private:
  uint8_t last_sequence_;
  bool    primed_;
};

#endif // LORA_PROTOCOL_H
