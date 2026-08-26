#ifndef EXTENSION_PROTOCOL_H
#define EXTENSION_PROTOCOL_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "config.h"

// Wire format for the extension port, kept free of the bus and of ModbusMaster
// so both sides of it can be built and tested on the host. The module author
// compiles this same header.
//
// **This is the one file both repositories must keep byte-identical.**
// scripts/check_shared_headers.sh diffs the two copies; a producer and a
// collector built from copies that have drifted will either desync silently
// or, more likely, disagree on EXT_PROTOCOL_VERSION and refuse each other
// loudly (see Rs485Slave/FrameReader's version checks) — loud is the
// intended failure, this header is what keeps it that way.
//
// The master is whichever module owns the RS485 segment, so both directions
// are driven from here: it writes the telemetry block with one FC16 and
// reads the command mailbox with one FC03, every poll cycle.
//
// **Telemetry is a generic {metric_id, value} table, not a fixed set of
// named registers, and the collector is never told what an id means by its
// own code.** A producer owns its own catalog of metric ids and names (the
// dryer firmware's DryerMetricIds.h is one example) and this header never
// names a single one of them — it only defines the envelope a catalog rides
// in, plus the one mechanism by which a producer tells the collector what its
// ids mean: one {id, name} announcement riding inside every telemetry block,
// cycling through the producer's whole catalog over time. The collector
// learns this into a RAM table (data-orchestra's RuntimeMetricCatalog) that
// starts out knowing nothing and is entirely populated over the wire — there
// is no compiled-in name anywhere on the collector's side.
//
// The announcement is one entry per block rather than the whole catalog at
// once because the master (the producer) is the only one that may speak
// unprompted here — the collector can never ask for a specific id, so the
// producer just keeps cycling and the collector's table converges within one
// lap and re-converges after either side reboots.
//
// The command mailbox stays a fixed, named layout. Commands are a two-way
// control surface: whoever posts one already has to know the producer's
// vocabulary (there is no such thing as a generic "set the thing to five"),
// so there is nothing to gain by making this half generic too. It is a
// deliberate scope boundary, not an oversight.

// How many ASCII characters a catalog announcement's name carries, and how
// many registers that packs into at two characters per register (big-endian,
// the same byte order every other multi-byte field on this wire uses).
// Chosen to keep the announcement's fixed cost (kExtCatalogNameRegisters + 1)
// small against a telemetry block's per-cycle budget: every producer's
// metric names have to fit this, which is why the dryer's own catalog uses
// short forms ("inlet_temp", not "inlet_temperature").
constexpr uint8_t kExtCatalogNameChars     = 16;
constexpr uint8_t kExtCatalogNameRegisters = kExtCatalogNameChars / 2;

// ===== Telemetry: header registers, offsets from EXT_REG_TELEMETRY =====
enum ExtensionTelemetryRegister : uint8_t
{
  kExtRegVersion     = 0,
  kExtRegCount       = 1, // number of {id, value} tuples that follow
  kExtRegUptimeHigh  = 2,
  kExtRegUptimeLow   = 3,
  kExtRegAckSequence = 4,
  kExtRegAckResult   = 5,
  kExtRegCatalogId   = 6, // the metric_id this cycle's announcement names
  kExtRegCatalogName = 7, // kExtCatalogNameRegisters registers follow
};

// Registers of the command mailbox, offsets from EXT_REG_COMMAND.
enum ExtensionCommandRegister : uint8_t
{
  kExtCmdRegSequence = 0,
  kExtCmdRegOpcode   = 1,
  kExtCmdRegArgument = 2,
  kExtCmdRegVersion  = 3,
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

// A reading the producer has no value for. Distinct from a real zero, and
// negative because a reading can legitimately go below zero (e.g. a water
// loop below freezing).
constexpr int16_t kExtInvalidValue = INT16_MIN;

// Bit 15 of a metric_id on the wire selects how its paired value register is
// encoded; the low 15 bits are the catalog id. Two kinds cover everything a
// producer has needed so far:
//
//   kExtMetricKindTenths  a signed tenths-of-a-unit reading, kExtInvalidValue
//                         sentinel meaning "no reading" — the shape every
//                         float, percent and boolean (0.0/1.0) metric uses.
//   kExtMetricKindCounter a raw whole-unit uint16_t, no sentinel, wraps at
//                         65535 — for a value too large for tenths' ~3277
//                         unit range (e.g. a session length in seconds) where
//                         wrapping is an acceptable, documented limitation
//                         rather than a reason to widen the envelope.
//
// One bit costs nothing and keeps every tuple the same shape; a future kind
// can claim more of the id space if one is ever needed.
constexpr uint16_t kExtMetricKindMask = 0x8000;
constexpr uint16_t kExtMetricIdMask   = 0x7FFF;

enum ExtensionMetricKind : uint16_t
{
  kExtMetricKindTenths  = 0,
  kExtMetricKindCounter = kExtMetricKindMask,
};

// Room for growth above what any producer seen so far has needed (the
// dryer's catalog uses under 20). Sizes every fixed array both sides of the
// link allocate, so raising it costs registers on the wire as well as RAM.
constexpr size_t kExtMaxMetrics = 24;

// version, count, uptime x2, ack x2, catalog announcement (id + name).
constexpr uint8_t kExtTelemetryHeaderCount =
    7 + kExtCatalogNameRegisters;

// The metric_id that CollectMetricSamples (MetricSamples.cpp) uses when it
// turns ExtensionTelemetryRecord::uptime_s into its own sample. Uptime is a
// fact about every producer, not something a producer's own catalog names,
// so it gets one id reserved out of the generic engine rather than out of any
// producer's numbering — pinned at the top of the id space (bit 15 already
// means something else, see kExtMetricKindMask) so a producer that starts
// numbering from 0 can never collide with it by accident.
constexpr uint16_t kExtMetricUptimeS = kExtMetricIdMask;

// The longest a telemetry block can ever be: header plus every metric slot
// filled. What Rs485Slave and OrchestraFrame size their buffers to, and the
// upper bound HandleWriteRegisters/FrameReader check an incoming count
// against — a block is otherwise free to carry anywhere from zero metrics up
// to this.
constexpr uint8_t kExtTelemetryMaxCount =
    kExtTelemetryHeaderCount + static_cast<uint8_t>(kExtMaxMetrics) * 2;

// One {id, value} reading, exactly as it rides on the wire.
struct ExtensionMetricTuple
{
  uint16_t metric_id; // low 15 bits: catalog id. bit 15: ExtensionMetricKind
  uint16_t value;

  ExtensionMetricTuple() : metric_id(0), value(0) {}
};

// What a producer reports, as a generic table plus the handful of fields
// every producer has regardless of what it measures: how long it has been up
// (also what the LoRa duplicate filter keys on — see OrchestraFrame.h), the
// echo of the last command it accepted, and this cycle's catalog
// announcement — which id, and what the producer calls it.
struct ExtensionTelemetryRecord
{
  uint32_t uptime_s;
  uint8_t  metric_count;
  ExtensionMetricTuple metrics[kExtMaxMetrics];

  // Echoed back so the module can stop repeating a command.
  uint16_t ack_sequence;
  uint16_t ack_result;

  // This cycle's {id, name} announcement. The producer cycles through its
  // whole catalog over successive telemetry blocks; catalog_metric_name is
  // always null-terminated here even though the wire packs it without a
  // terminator (see ExtEncodeTelemetry/ExtDecodeTelemetry).
  uint16_t catalog_metric_id;
  char     catalog_metric_name[kExtCatalogNameChars + 1];

  ExtensionTelemetryRecord()
      : uptime_s(0), metric_count(0), metrics{},
        ack_sequence(0), ack_result(kExtResultOk),
        catalog_metric_id(0), catalog_metric_name{} {}
};

// One command as read out of the mailbox and validated.
struct ExtensionCommand
{
  uint16_t sequence; // 0 = the mailbox is empty
  uint16_t opcode;
  float    argument; // engineering units; meaning depends on the opcode

  ExtensionCommand() : sequence(0), opcode(kExtCmdNone), argument(NAN) {}
};

// Tenths, with NAN mapped to the sentinel. The one numeric encoding every
// kExtMetricKindTenths value and every command argument uses.
int16_t ExtEncodeValue(float value);
float   ExtDecodeValue(int16_t raw);

// Appends one tenths-encoded metric to `record`. Returns false when the
// record already holds kExtMaxMetrics entries. `metric_id` must not have bit
// 15 set — that bit is this function's to set.
bool ExtPutMetricValue(ExtensionTelemetryRecord &record, uint16_t metric_id, float value);

// Appends one raw whole-unit counter metric. See kExtMetricKindCounter.
bool ExtPutMetricCounter(ExtensionTelemetryRecord &record, uint16_t metric_id, uint32_t value);

// Packs `name` into kExtCatalogNameRegisters registers, two ASCII bytes per
// register, big-endian — the same byte order every other multi-byte field on
// this wire uses. Longer than kExtCatalogNameChars is truncated; shorter is
// zero-padded. No terminator is stored: the fixed register count is the only
// bound, which is what ExtDecodeCatalogName relies on.
void ExtEncodeCatalogName(const char *name, uint16_t *out);

// Inverse of ExtEncodeCatalogName. `out` must hold at least
// kExtCatalogNameChars + 1 bytes; always null-terminated on return, even
// when the name fills every character slot.
void ExtDecodeCatalogName(const uint16_t *in, char *out);

// Fills the whole telemetry block: header registers, then two registers per
// metric in `record`. `out` must hold at least
// kExtTelemetryHeaderCount + record.metric_count * 2 registers (safe to size
// it for EXT_TELEMETRY_MAX_COUNT always). Returns the number of registers
// actually written, which is what the caller writes onto the wire — never a
// fixed count, since the table is variable-length.
size_t ExtEncodeTelemetry(const ExtensionTelemetryRecord &record, uint16_t *out);

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
// air. Modbus cannot duplicate a read — but the producer re-reads the *same*
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
