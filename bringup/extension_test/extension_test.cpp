// Remote module stand-in — built by
// `pio run -e extension_test -t upload -t monitor`.
//
// Runs on a *second* Pico with its own MAX3485, landed on the same A/B pair as
// the dryer. It plays **both** remote modules the dryer talks to:
//
//   @2   the extension port — accepts the telemetry block, decodes it, and
//        hands back whatever command you type on the USB console
//   @10  the hydraulic module — answers with water and tank temperatures you
//        set from the console, and prints what the dryer asks of the circulator
//
// The second is not scope creep on the first: with nothing answering @10, the
// extension's telemetry shows dashes for both water figures and carries the
// hydraulic-offline flag forever, so half of what the port reports can never be
// seen. One board playing every remote node is also simply the cheaper rig.
//
// It answers, in the order the faults actually happen:
//   nothing arrives at all      -> A and B swapped, no termination, the dryer's
//                                  DE never rising, or the wrong address
//   frames arrive, CRC fails    -> wrong baud, noise, or both ends untermined
//   FC16 arrives, FC03 does not -> the dryer's write went through and its read
//                                  did not: check EXT_REG_COMMAND
//   commands are never acked    -> the mailbox is being read but the sequence
//                                  never changes, or Core 0 is not running
//                                  UpdateExtensionCommands()
//
// It compiles the production ExtensionProtocol, so what it prints is what the
// dryer sent, decoded by the same code the dryer encoded it with — and building
// this sketch at all is the check that the header really is compilable by a
// module, which is the claim HARDWARE.md makes about it.
//
// The Modbus slave underneath is hand-written. Only two function codes are
// needed, the project has no slave library, and a bring-up tool that pulls in a
// dependency to be a fake device is a poor trade. rs485_test builds its raw
// frames the same way, for its own reasons.

#include <Arduino.h>
#include <stdlib.h>

#include "config.h"
#include "ExtensionProtocol.h"

namespace
{

// ---------------------------------------------------------------------------
// The register files
//
// Exactly the blocks in HARDWARE.md. Anything addressed outside them draws an
// illegal-data-address exception rather than a polite zero — a silent zero is
// how a register map disagreement hides.

uint16_t g_ext_telemetry[EXT_TELEMETRY_COUNT]; // dryer writes, FC16
uint16_t g_ext_mailbox[EXT_COMMAND_COUNT];     // dryer reads, FC03

// The hydraulic map has no sentinel for "no reading": the dryer decodes every
// value as signed tenths and believes it. So these start at plausible figures
// rather than zero, which would look like a fault the moment the module
// answered.
uint16_t g_hydro_command[2] = {0, 0};              // 0x0000 state, 0x0001 setpoint
uint16_t g_hydro_telemetry[3] = {450, 520, 0x0000}; // 0x0010 water, 0x0011 tank, 0x0012 status

// ---------------------------------------------------------------------------
// Which addresses this board answers for

struct RegisterBlock
{
  uint16_t    base;
  uint16_t    count;
  uint16_t   *data;
  bool        master_may_write;
  const char *name;
};

struct SlaveNode
{
  uint8_t              address;
  const char          *name;
  const RegisterBlock *blocks;
  uint8_t              block_count;
  bool                *online;
};

// Both can be taken off the bus from the console, which is how the dryer's
// availability timeout and its backoff get exercised without unplugging
// anything.
bool g_extension_online = true;
bool g_hydraulic_online = true;

const RegisterBlock kExtensionBlocks[] = {
    {EXT_REG_TELEMETRY, EXT_TELEMETRY_COUNT, g_ext_telemetry, true, "telemetry"},
    {EXT_REG_COMMAND, EXT_COMMAND_COUNT, g_ext_mailbox, false, "command mailbox"},
};

const RegisterBlock kHydraulicBlocks[] = {
    {HYDRO_REG_STATE, 2, g_hydro_command, true, "command"},
    {HYDRO_REG_WATER_TEMP, 3, g_hydro_telemetry, false, "measurements"},
};

const SlaveNode kNodes[] = {
    {MODBUS_EXTENSION_ADDRESS, "extension", kExtensionBlocks, 2, &g_extension_online},
    {MODBUS_HYDRAULIC_ADDRESS, "hydraulic", kHydraulicBlocks, 2, &g_hydraulic_online},
};
constexpr uint8_t kNodeCount = sizeof(kNodes) / sizeof(kNodes[0]);

// ---------------------------------------------------------------------------
// Bus plumbing

// Modbus RTU ends a frame on 3.5 character times of silence: 3.65 ms at 9600
// baud, rounded up for the interrupt latency of a receiver that is also driving
// a USB console.
constexpr uint32_t kFrameGapMs = 5;

// The line must stay driven until the last stop bit has physically left. flush()
// waits for the FIFO, but dropping DE in the same breath has cut the final byte
// on slower cores before now — one character time of margin costs nothing at
// this baud rate.
constexpr uint32_t kDriveHoldUs = 1100;

constexpr uint8_t kFcReadHolding   = 0x03;
constexpr uint8_t kFcWriteMultiple = 0x10;

constexpr uint8_t kExcIllegalFunction = 0x01;
constexpr uint8_t kExcIllegalAddress  = 0x02;
constexpr uint8_t kExcIllegalValue    = 0x03;

// FC16 of the whole telemetry block is 9 + 2*17 = 43 bytes; 256 is room to see
// a malformed frame whole rather than truncating it before it can be printed.
constexpr uint16_t kBufferSize = 256;

uint8_t  g_frame[kBufferSize];
uint16_t g_frame_length = 0;

// ---------------------------------------------------------------------------
// Counters. A bus that never worked and a bus that stopped working are
// different faults, so the run before the first failure is worth keeping.

uint32_t g_frames_seen = 0;
uint32_t g_crc_errors = 0;
uint32_t g_not_for_us = 0;
uint32_t g_ext_writes = 0;
uint32_t g_ext_reads = 0;
uint32_t g_hydro_writes = 0;
uint32_t g_hydro_reads = 0;
uint32_t g_exceptions_sent = 0;
uint32_t g_last_ext_write_ms = 0;
uint32_t g_last_ext_read_ms = 0;
uint32_t g_last_hydro_ms = 0;

// ---------------------------------------------------------------------------
// The pending command, and what became of it

uint16_t    g_sequence = 0;         // last sequence we posted
uint16_t    g_pending_sequence = 0; // still waiting for an acknowledgement
const char *g_pending_label = "";

uint16_t Crc16(const uint8_t *frame, uint16_t length)
{
  uint16_t crc = 0xFFFF;
  for (uint16_t i = 0; i < length; i++)
  {
    crc ^= frame[i];
    for (uint8_t bit = 0; bit < 8; bit++)
    {
      crc = (crc & 1) ? static_cast<uint16_t>((crc >> 1) ^ 0xA001)
                      : static_cast<uint16_t>(crc >> 1);
    }
  }
  return crc;
}

void SendFrame(uint8_t address, uint8_t *frame, uint16_t payload_length)
{
  uint16_t crc = Crc16(frame, payload_length);
  frame[payload_length]     = static_cast<uint8_t>(crc & 0xFF); // low byte first
  frame[payload_length + 1] = static_cast<uint8_t>(crc >> 8);

  digitalWrite(RS485_DE_PIN, HIGH);
  Serial2.write(frame, payload_length + 2);
  Serial2.flush();
  delayMicroseconds(kDriveHoldUs);
  digitalWrite(RS485_DE_PIN, LOW);
  (void)address;
}

void SendException(uint8_t address, uint8_t function, uint8_t code)
{
  uint8_t reply[5];
  reply[0] = address;
  reply[1] = function | 0x80;
  reply[2] = code;
  SendFrame(address, reply, 3);

  g_exceptions_sent++;
  Serial.printf("  -> exception %02X on function %02X\n", code, function);
}

// ---------------------------------------------------------------------------
// Decoding what the dryer sent

const char *ResultName(uint16_t result)
{
  switch (result)
  {
  case kExtResultOk:            return "ok";
  case kExtResultUnknownOpcode: return "unknown opcode";
  case kExtResultRefused:       return "refused by policy";
  case kExtResultOutOfRange:    return "out of range";
  case kExtResultBadVersion:    return "protocol version";
  default:                      return "unrecognised result";
  }
}

const char *PhaseName(uint16_t phase)
{
  switch (phase)
  {
  case 0:  return "stop";
  case 1:  return "init";
  case 2:  return "brassage";
  case 3:  return "extraction";
  default: return "?";
  }
}

// Prints a reading, or dashes when the dryer sent the sentinel. Printing 0.0 for
// a missing probe is the exact confusion the sentinel exists to prevent, so it
// must not be reintroduced here.
void PrintValue(const char *label, uint16_t raw, const char *unit)
{
  float value = ExtDecodeValue(static_cast<int16_t>(raw));
  if (isnan(value))
  {
    Serial.printf("  %-14s --      (no reading)\n", label);
  }
  else
  {
    Serial.printf("  %-14s %6.1f %s\n", label, value, unit);
  }
}

void PrintPosition(const char *label, uint16_t raw)
{
  float value = ExtDecodePosition(raw);
  if (isnan(value))
  {
    Serial.printf("  %-14s --      (no feedback)\n", label);
  }
  else
  {
    Serial.printf("  %-14s %6.0f %%\n", label, value);
  }
}

void PrintFlags(uint16_t flags)
{
  Serial.printf("  flags          0x%04X ", flags);
  if (flags == 0)
  {
    Serial.print("(none)");
  }
  if (flags & kExtFlagRunning)       Serial.print("running ");
  if (flags & kExtFlagFan)           Serial.print("fan ");
  if (flags & kExtFlagElectric)      Serial.print("electric ");
  if (flags & kExtFlagHydraulic)     Serial.print("hydraulic ");
  if (flags & kExtFlagDamperOpen)    Serial.print("damper-open ");
  if (flags & kExtFlagSensorFault)   Serial.print("SENSOR-FAULT ");
  if (flags & kExtFlagHydraulicOff)  Serial.print("hydraulic-offline ");
  if (flags & kExtFlagAirflowFault)  Serial.print("AIRFLOW-BLOCKED ");
  if (flags & kExtFlagFeedbackFault) Serial.print("FEEDBACK-FAULT ");
  Serial.println();
}

void PrintTelemetry()
{
  uint32_t elapsed = (static_cast<uint32_t>(g_ext_telemetry[kExtRegElapsedHigh]) << 16) |
                     g_ext_telemetry[kExtRegElapsedLow];
  uint32_t uptime = (static_cast<uint32_t>(g_ext_telemetry[kExtRegUptimeHigh]) << 16) |
                    g_ext_telemetry[kExtRegUptimeLow];

  Serial.printf("\n--- telemetry, protocol v%d ---\n", g_ext_telemetry[kExtRegVersion]);

  if (g_ext_telemetry[kExtRegVersion] != EXT_PROTOCOL_VERSION)
  {
    Serial.printf("  ** the dryer speaks v%d, this sketch v%d — everything below\n",
                  g_ext_telemetry[kExtRegVersion], EXT_PROTOCOL_VERSION);
    Serial.println("     is being read against the wrong map **");
  }

  Serial.printf("  %-14s %s\n", "phase", PhaseName(g_ext_telemetry[kExtRegPhase]));
  PrintFlags(g_ext_telemetry[kExtRegFlags]);

  PrintValue("inlet", g_ext_telemetry[kExtRegInletTemp], "C");
  PrintValue("inlet RH", g_ext_telemetry[kExtRegInletHumidity], "%");
  PrintValue("water", g_ext_telemetry[kExtRegWaterTemp], "C");
  PrintValue("tank", g_ext_telemetry[kExtRegTankTemp], "C");
  PrintValue("target", g_ext_telemetry[kExtRegTargetTemp], "C");
  PrintValue("target RH", g_ext_telemetry[kExtRegTargetHumidity], "%");

  PrintPosition("extraction", g_ext_telemetry[kExtRegExtractionPos]);
  PrintPosition("recycling", g_ext_telemetry[kExtRegRecyclingPos]);

  Serial.printf("  %-14s %lus  (uptime %lus)\n", "session",
                (unsigned long)elapsed, (unsigned long)uptime);

  uint16_t ack_sequence = g_ext_telemetry[kExtRegAckSequence];
  uint16_t ack_result   = g_ext_telemetry[kExtRegAckResult];
  Serial.printf("  %-14s sequence %u, %s\n", "acknowledged", ack_sequence,
                ResultName(ack_result));

  if (g_pending_sequence != 0 && ack_sequence == g_pending_sequence)
  {
    Serial.printf("\n  >> %s (sequence %u) came back: %s\n", g_pending_label,
                  g_pending_sequence, ResultName(ack_result));
    if (ack_result == kExtResultOk)
    {
      Serial.println("     Accepted. The command stays in the mailbox on purpose:");
      Serial.println("     the dryer re-reads it every cycle, and it must not fire twice.");
    }
    g_pending_sequence = 0;
  }
  Serial.println();
}

// What the dryer is asking of the circulator and the three-way valve. Printed
// only when it changes: it is written every cycle and is usually the same.
void PrintHydraulicCommand()
{
  static bool     first = true;
  static uint16_t last_state = 0xFFFF;
  static uint16_t last_target = 0xFFFF;

  if (!first && g_hydro_command[0] == last_state && g_hydro_command[1] == last_target)
  {
    return;
  }
  first = false;
  last_state = g_hydro_command[0];
  last_target = g_hydro_command[1];

  Serial.printf("\n[hydraulic] the dryer asks: circulator %s, water setpoint %.1f C\n\n",
                g_hydro_command[0] ? "ON" : "off",
                static_cast<int16_t>(g_hydro_command[1]) / 10.0f);
}

// ---------------------------------------------------------------------------
// Request handling

const RegisterBlock *FindBlock(const SlaveNode &node, uint16_t start, uint16_t count)
{
  for (uint8_t i = 0; i < node.block_count; i++)
  {
    const RegisterBlock &block = node.blocks[i];
    if (start >= block.base && start + count <= block.base + block.count)
    {
      return &block;
    }
  }
  return nullptr;
}

void HandleReadHolding(const SlaveNode &node, uint16_t start, uint16_t count)
{
  const RegisterBlock *block = FindBlock(node, start, count);
  if (block == nullptr)
  {
    Serial.printf("[%s] FC03 for 0x%04X x%u — outside every block\n", node.name,
                  start, count);
    SendException(node.address, kFcReadHolding, kExcIllegalAddress);
    return;
  }

  // 3 header bytes + two per register + two of CRC, sized for the largest block
  // this board serves.
  uint8_t reply[3 + 2 * EXT_TELEMETRY_COUNT + 2];
  reply[0] = node.address;
  reply[1] = kFcReadHolding;
  reply[2] = static_cast<uint8_t>(count * 2);

  uint16_t offset = start - block->base;
  for (uint16_t i = 0; i < count; i++)
  {
    reply[3 + i * 2] = static_cast<uint8_t>(block->data[offset + i] >> 8);
    reply[4 + i * 2] = static_cast<uint8_t>(block->data[offset + i] & 0xFF);
  }
  SendFrame(node.address, reply, static_cast<uint16_t>(3 + count * 2));

  if (node.address == MODBUS_EXTENSION_ADDRESS)
  {
    g_ext_reads++;
    g_last_ext_read_ms = millis();
  }
  else
  {
    g_hydro_reads++;
    g_last_hydro_ms = millis();
  }
}

void HandleWriteMultiple(const SlaveNode &node, uint16_t start, uint16_t count,
                         const uint8_t *values)
{
  const RegisterBlock *block = FindBlock(node, start, count);
  if (block == nullptr)
  {
    Serial.printf("[%s] FC16 for 0x%04X x%u — outside every block\n", node.name,
                  start, count);
    SendException(node.address, kFcWriteMultiple, kExcIllegalAddress);
    return;
  }

  // The blocks this module reports from are ours to write and the dryer's to
  // read. A master writing there has the direction of the link backwards, and
  // saying so is more use than accepting it.
  if (!block->master_may_write)
  {
    Serial.printf("[%s] FC16 for 0x%04X — that is the %s block, read-only to the dryer\n",
                  node.name, start, block->name);
    SendException(node.address, kFcWriteMultiple, kExcIllegalAddress);
    return;
  }

  uint16_t offset = start - block->base;
  for (uint16_t i = 0; i < count; i++)
  {
    block->data[offset + i] =
        static_cast<uint16_t>((values[i * 2] << 8) | values[i * 2 + 1]);
  }

  uint8_t reply[8];
  reply[0] = node.address;
  reply[1] = kFcWriteMultiple;
  reply[2] = static_cast<uint8_t>(start >> 8);
  reply[3] = static_cast<uint8_t>(start & 0xFF);
  reply[4] = static_cast<uint8_t>(count >> 8);
  reply[5] = static_cast<uint8_t>(count & 0xFF);
  SendFrame(node.address, reply, 6);

  if (node.address == MODBUS_EXTENSION_ADDRESS)
  {
    g_ext_writes++;
    g_last_ext_write_ms = millis();
    PrintTelemetry();
  }
  else
  {
    g_hydro_writes++;
    g_last_hydro_ms = millis();
    PrintHydraulicCommand();
  }
}

void HandleFrame()
{
  g_frames_seen++;

  if (g_frame_length < 4)
  {
    Serial.printf("Runt frame, %u bytes — noise, or a gap shorter than %lu ms\n",
                  g_frame_length, (unsigned long)kFrameGapMs);
    return;
  }

  // Address before CRC, deliberately.
  //
  // This board hears the whole pair, including the dryer talking to the probe.
  // A master request and the answer that follows it are separated by the slave's
  // turnaround, which is routinely shorter than the 3.5 character times that end
  // a frame — so the two arrive glued into one buffer whose CRC cannot possibly
  // check out. Reporting that as a bus fault is exactly wrong: a passive
  // listener cannot tell a request from the reply behind it, and the only
  // traffic whose integrity this board can judge is the traffic addressed to it.
  const SlaveNode *node = nullptr;
  for (uint8_t i = 0; i < kNodeCount; i++)
  {
    if (g_frame[0] == kNodes[i].address)
    {
      node = &kNodes[i];
      break;
    }
  }

  if (node == nullptr)
  {
    g_not_for_us++;
    return;
  }

  uint16_t received_crc = static_cast<uint16_t>(g_frame[g_frame_length - 2]) |
                          static_cast<uint16_t>(g_frame[g_frame_length - 1] << 8);
  if (received_crc != Crc16(g_frame, g_frame_length - 2))
  {
    g_crc_errors++;
    Serial.printf("CRC error on a %u byte frame for @%d (%lu so far) — baud,\n",
                  g_frame_length, g_frame[0], (unsigned long)g_crc_errors);
    Serial.println("  noise, or a segment with termination at only one end");
    return;
  }

  // Taken off the bus from the console: hear the request, answer nothing. This
  // is what an unplugged module looks like, and it is how the dryer's
  // availability timeout and backoff get exercised.
  if (!*node->online)
  {
    return;
  }

  uint8_t  function = g_frame[1];
  uint16_t start = static_cast<uint16_t>((g_frame[2] << 8) | g_frame[3]);
  uint16_t count = static_cast<uint16_t>((g_frame[4] << 8) | g_frame[5]);

  switch (function)
  {
  case kFcReadHolding:
    if (count == 0 || count > EXT_TELEMETRY_COUNT)
    {
      SendException(node->address, function, kExcIllegalValue);
      return;
    }
    HandleReadHolding(*node, start, count);
    break;

  case kFcWriteMultiple:
    // addr fc start(2) count(2) bytecount(1) payload crc(2)
    if (g_frame_length < 9 + count * 2 || g_frame[6] != count * 2)
    {
      Serial.printf("[%s] FC16 byte count disagrees with the register count (%u vs %u)\n",
                    node->name, g_frame[6], count * 2);
      SendException(node->address, function, kExcIllegalValue);
      return;
    }
    HandleWriteMultiple(*node, start, count, &g_frame[7]);
    break;

  default:
    Serial.printf("[%s] function %02X — this module only implements FC03 and FC16\n",
                  node->name, function);
    SendException(node->address, function, kExcIllegalFunction);
    break;
  }
}

// Collects bytes until the pair falls silent for a frame gap.
void PollBus()
{
  if (Serial2.available() <= 0)
  {
    return;
  }

  g_frame_length = 0;
  uint32_t last_byte_ms = millis();

  while (millis() - last_byte_ms < kFrameGapMs)
  {
    if (Serial2.available() > 0)
    {
      if (g_frame_length < kBufferSize)
      {
        g_frame[g_frame_length++] = static_cast<uint8_t>(Serial2.read());
      }
      else
      {
        Serial2.read(); // keep draining so the next frame starts clean
      }
      last_byte_ms = millis();
    }
  }

  HandleFrame();
}

// ---------------------------------------------------------------------------
// Posting a command

// Writes the mailbox in the order HARDWARE.md tells module authors to use: the
// sequence **last**, so the dryer can never read a new sequence paired with the
// previous command's argument. The two Picos have no lock between them; the
// ordering is the whole protection.
void PostCommand(uint16_t opcode, float argument, uint16_t version, const char *label)
{
  g_sequence++;
  if (g_sequence == 0)
  {
    g_sequence = 1; // 0 means "the mailbox is empty"
  }

  g_ext_mailbox[kExtCmdRegOpcode]   = opcode;
  g_ext_mailbox[kExtCmdRegArgument] = static_cast<uint16_t>(ExtEncodeValue(argument));
  g_ext_mailbox[kExtCmdRegVersion]  = version;
  g_ext_mailbox[kExtCmdRegSequence] = g_sequence;

  g_pending_sequence = g_sequence;
  g_pending_label    = label;

  Serial.printf("\nPosted %s as sequence %u. The dryer reads the mailbox once per\n",
                label, g_sequence);
  Serial.println("cycle, so the answer is due within about two seconds.");
}

void ClearMailbox()
{
  for (uint8_t i = 0; i < EXT_COMMAND_COUNT; i++)
  {
    g_ext_mailbox[i] = 0;
  }
  g_pending_sequence = 0;
  Serial.println("\nMailbox cleared. Sequence 0 is how a module says it wants nothing.\n");
}

// ---------------------------------------------------------------------------
// What the hydraulic module reports

void SetHydraulicValue(uint8_t index, float celsius, const char *label)
{
  g_hydro_telemetry[index] = static_cast<uint16_t>(lroundf(celsius * 10.0f));
  Serial.printf("\n[hydraulic] %s now %.1f C. It reaches the dryer on its next poll,\n",
                label, celsius);
  Serial.println("and the extension's telemetry a cycle after that.\n");
}

void SetHydraulicStatus(uint16_t bits)
{
  g_hydro_telemetry[2] = bits;
  Serial.printf("\n[hydraulic] status word now 0x%04X.\n", bits);
  Serial.println("The dryer stores it and reports nothing about it: HARDWARE.md declares");
  Serial.println("the register but no bit meanings yet, so this is where that gets");
  Serial.println("decided once the real module exists.\n");
}

void PrintHydraulicState()
{
  Serial.println("\n--- hydraulic module ---");
  Serial.printf("  %-14s %s\n", "on the bus", g_hydraulic_online ? "yes" : "NO — answering nothing");
  Serial.printf("  %-14s %s\n", "circulator", g_hydro_command[0] ? "ON" : "off");
  Serial.printf("  %-14s %6.1f C   (asked by the dryer)\n", "setpoint",
                static_cast<int16_t>(g_hydro_command[1]) / 10.0f);
  Serial.printf("  %-14s %6.1f C   (reported by us)\n", "water",
                static_cast<int16_t>(g_hydro_telemetry[0]) / 10.0f);
  Serial.printf("  %-14s %6.1f C   (reported by us)\n", "tank",
                static_cast<int16_t>(g_hydro_telemetry[1]) / 10.0f);
  Serial.printf("  %-14s 0x%04X\n", "status", g_hydro_telemetry[2]);
  Serial.println();
}

void PrintCounters()
{
  uint32_t now = millis();
  Serial.println("\n--- bus counters ---");
  Serial.printf("  frames seen        %lu\n", (unsigned long)g_frames_seen);
  Serial.printf("  for another node   %lu   (the probe @%d — proof the pair is right)\n",
                (unsigned long)g_not_for_us, MODBUS_INLET_ADDRESS);
  Serial.printf("  CRC errors         %lu   (counted only on frames addressed to us:\n",
                (unsigned long)g_crc_errors);
  Serial.println("                          a listener cannot judge a request glued to");
  Serial.println("                          the reply that follows it)");
  Serial.printf("  extension writes   %lu", (unsigned long)g_ext_writes);
  if (g_ext_writes > 0)
  {
    Serial.printf("   last %lu ms ago", (unsigned long)(now - g_last_ext_write_ms));
  }
  Serial.println();
  Serial.printf("  extension reads    %lu", (unsigned long)g_ext_reads);
  if (g_ext_reads > 0)
  {
    Serial.printf("   last %lu ms ago", (unsigned long)(now - g_last_ext_read_ms));
  }
  Serial.println();
  Serial.printf("  hydraulic exchange %lu write / %lu read", (unsigned long)g_hydro_writes,
                (unsigned long)g_hydro_reads);
  if (g_hydro_writes > 0)
  {
    Serial.printf("   last %lu ms ago", (unsigned long)(now - g_last_hydro_ms));
  }
  Serial.println();
  Serial.printf("  exceptions sent    %lu\n", (unsigned long)g_exceptions_sent);

  if (g_frames_seen == 0)
  {
    Serial.println("\n  Nothing at all on the pair. A and B swapped, no termination,");
    Serial.println("  the dryer not running, or its DE never rising.");
  }
  else if (g_ext_writes == 0 && g_not_for_us > 0)
  {
    Serial.printf("\n  Traffic for other nodes but none for @%d: the dryer is not\n",
                  MODBUS_EXTENSION_ADDRESS);
    Serial.println("  addressing this module. Check MODBUS_EXTENSION_ADDRESS on both sides.");
  }
  else if (g_ext_writes > 0 && g_ext_reads == 0)
  {
    Serial.println("\n  The write lands and the read never comes. The dryer skips the");
    Serial.println("  mailbox read when the write fails — but this one did not fail,");
    Serial.println("  so check EXT_REG_COMMAND agrees on both sides.");
  }
  Serial.println();
}

void PrintWiring()
{
  Serial.println("This is the *module* side. Expected wiring, identical to the dryer's:");
  Serial.printf("  RO  / TXD  receiver out -> GP%-2d   UART1 RX\n", RS485_RX_PIN);
  Serial.printf("  DI  / RXD  driver in    <- GP%-2d   UART1 TX\n", RS485_TX_PIN);
  Serial.printf("  DE+RE / EN direction    <- GP%-2d   HIGH = transmit\n", RS485_DE_PIN);
  Serial.println("  VCC                     -> 3V3(OUT)   3.3 V only");
  Serial.println("  GND                     -> GND        and to the dryer's GND");
  Serial.println();
  Serial.println("A to A, B to B, no crossover. 120 ohm at both ends of the segment and");
  Serial.println("nowhere in between — a third resistor in the middle is the same fault");
  Serial.println("as none at all. The two boards must share a ground reference: RS485 is");
  Serial.println("differential, not isolated.");
  Serial.println();
}

void PrintHelp()
{
  Serial.println("Extension port @2 — a value may follow, e.g. `t 38.5`:");
  Serial.println("  s        post STOP");
  Serial.println("  t <C>    post a temperature setpoint");
  Serial.println("  h <%>    post a humidity setpoint");
  Serial.println();
  Serial.println("  The four refusals, each of which should come back named:");
  Serial.println("  r        post START      -> refused by policy, always");
  Serial.println("  o        post 90 C       -> out of range");
  Serial.println("  x        post opcode 99  -> unknown opcode");
  Serial.println("  v        post STOP at the wrong version -> protocol version");
  Serial.println();
  Serial.println("  c        clear the mailbox");
  Serial.println("  d        dump the last telemetry again");
  Serial.println();
  Serial.println("Hydraulic module @10 — what it reports back to the dryer:");
  Serial.println("  w <C>    water temperature");
  Serial.println("  k <C>    tank temperature");
  Serial.println("  b <n>    status word, decimal or 0x hex");
  Serial.println("  g        show the module's state and what the dryer asks of it");
  Serial.println();
  Serial.println("Both:");
  Serial.println("  e        take the extension off the bus, or put it back");
  Serial.println("  u        take the hydraulic module off, or put it back");
  Serial.println("           — silence is how the dryer's 30 s timeout and its backoff");
  Serial.println("             get tested without unplugging anything");
  Serial.println("  n        bus counters");
  Serial.println("  ?        this help");
  Serial.println();
}

// Reads one console line: a command letter and an optional number.
void PollConsole()
{
  static char    line[32];
  static uint8_t length = 0;

  while (Serial.available() > 0)
  {
    char c = static_cast<char>(Serial.read());

    if (c != '\n' && c != '\r')
    {
      if (length < sizeof(line) - 1)
      {
        line[length++] = c;
      }
      continue;
    }

    if (length == 0)
    {
      continue;
    }
    line[length] = '\0';
    length = 0;

    char  key = line[0];
    float argument = atof(&line[1]);                     // 0 when nothing follows
    long  integer = strtol(&line[1], nullptr, 0);        // base 0: 0x.. understood

    switch (key)
    {
    case 's': PostCommand(kExtCmdStop, 0.0f, EXT_PROTOCOL_VERSION, "STOP"); break;
    case 't': PostCommand(kExtCmdSetTemp, argument, EXT_PROTOCOL_VERSION, "set temperature"); break;
    case 'h': PostCommand(kExtCmdSetHumidity, argument, EXT_PROTOCOL_VERSION, "set humidity"); break;

    case 'r': PostCommand(kExtCmdStart, 0.0f, EXT_PROTOCOL_VERSION, "START"); break;
    case 'o': PostCommand(kExtCmdSetTemp, 90.0f, EXT_PROTOCOL_VERSION, "set temperature 90 C"); break;
    case 'x': PostCommand(99, 0.0f, EXT_PROTOCOL_VERSION, "opcode 99"); break;
    case 'v': PostCommand(kExtCmdStop, 0.0f, EXT_PROTOCOL_VERSION + 1, "STOP at a wrong version"); break;

    case 'c': ClearMailbox(); break;
    case 'd': PrintTelemetry(); break;

    case 'w': SetHydraulicValue(0, argument, "water temperature"); break;
    case 'k': SetHydraulicValue(1, argument, "tank temperature"); break;
    case 'b': SetHydraulicStatus(static_cast<uint16_t>(integer)); break;
    case 'g': PrintHydraulicState(); break;

    case 'e':
      g_extension_online = !g_extension_online;
      Serial.printf("\nExtension @%d is now %s.\n\n", MODBUS_EXTENSION_ADDRESS,
                    g_extension_online ? "answering again" : "silent — expect the dryer to back off");
      break;

    case 'u':
      g_hydraulic_online = !g_hydraulic_online;
      Serial.printf("\nHydraulic @%d is now %s.\n", MODBUS_HYDRAULIC_ADDRESS,
                    g_hydraulic_online ? "answering again" : "silent");
      if (!g_hydraulic_online)
      {
        Serial.println("The dryer should raise hydraulic-offline within 30 s and fall back");
        Serial.println("to electric-only — losing it degrades the dryer, it does not stop it.");
      }
      Serial.println();
      break;

    case 'n': PrintCounters(); break;
    case '?': PrintWiring(); PrintHelp(); break;
    default:
      Serial.printf("Unknown key '%c'. Press ? for the list.\n", key);
      break;
    }
  }
}

} // namespace

void setup()
{
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("========================================");
  Serial.println("  Remote module stand-in");
  Serial.println("========================================");
  Serial.println();

  PrintWiring();

  for (uint8_t i = 0; i < EXT_TELEMETRY_COUNT; i++)
  {
    g_ext_telemetry[i] = 0;
  }
  ClearMailbox();

  Serial2.setTX(RS485_TX_PIN);
  Serial2.setRX(RS485_RX_PIN);
  Serial2.begin(MODBUS_BAUDRATE, SERIAL_8N1);

  pinMode(RS485_DE_PIN, OUTPUT);
  digitalWrite(RS485_DE_PIN, LOW); // receive until we have something to say

  Serial.printf("Answering for two slaves at %d baud 8N1.\n", MODBUS_BAUDRATE);
  Serial.printf("  @%-3d extension   0x%04X x%-2d telemetry in, 0x%04X x%d mailbox out\n",
                MODBUS_EXTENSION_ADDRESS, EXT_REG_TELEMETRY, EXT_TELEMETRY_COUNT,
                EXT_REG_COMMAND, EXT_COMMAND_COUNT);
  Serial.printf("  @%-3d hydraulic   0x%04X x2  command in, 0x%04X x3 measurements out\n",
                MODBUS_HYDRAULIC_ADDRESS, HYDRO_REG_STATE, HYDRO_REG_WATER_TEMP);
  Serial.println();
  Serial.printf("Water starts at %.1f C and the tank at %.1f C — figures this sketch\n",
                static_cast<int16_t>(g_hydro_telemetry[0]) / 10.0f,
                static_cast<int16_t>(g_hydro_telemetry[1]) / 10.0f);
  Serial.println("invented, not measured. The hydraulic map has no sentinel for a missing");
  Serial.println("reading, so zero would look like ice water rather than like no answer.");
  Serial.println();
  Serial.println("A telemetry block should land every two seconds. If none does, press n");
  Serial.println("before touching the wiring — it says which half is missing.");
  Serial.println();
  PrintHelp();
}

void loop()
{
  PollBus();
  PollConsole();
}
