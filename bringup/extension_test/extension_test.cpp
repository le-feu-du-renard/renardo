// Extension module stand-in — built by
// `pio run -e extension_test -t upload -t monitor`.
//
// Runs on a *second* Pico with its own MAX3485, landed on the same A/B pair as
// the dryer. It is the module the dryer thinks it is talking to: a Modbus RTU
// slave on MODBUS_EXTENSION_ADDRESS that accepts the telemetry block, decodes
// it, and hands back whatever command you type on the USB console.
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

#include "config.h"
#include "ExtensionProtocol.h"

namespace
{

// ---------------------------------------------------------------------------
// The register file
//
// Two blocks, exactly the ones in HARDWARE.md. The dryer writes the first with
// FC16 and reads the second with FC03; nothing else exists on this slave, and
// anything addressed outside them gets an illegal-data-address exception rather
// than a polite zero — a silent zero is how a register map disagreement hides.

uint16_t g_telemetry[EXT_TELEMETRY_COUNT];
uint16_t g_mailbox[EXT_COMMAND_COUNT];

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

constexpr uint8_t kFcReadHolding    = 0x03;
constexpr uint8_t kFcWriteMultiple  = 0x10;

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
uint32_t g_writes_served = 0;
uint32_t g_reads_served = 0;
uint32_t g_exceptions_sent = 0;
uint32_t g_last_write_ms = 0;
uint32_t g_last_read_ms = 0;

// ---------------------------------------------------------------------------
// The pending command, and what became of it

uint16_t g_sequence = 0;          // last sequence we posted
uint16_t g_pending_sequence = 0;  // still waiting for an acknowledgement
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

void SendFrame(uint8_t *frame, uint16_t payload_length)
{
  uint16_t crc = Crc16(frame, payload_length);
  frame[payload_length]     = static_cast<uint8_t>(crc & 0xFF); // low byte first
  frame[payload_length + 1] = static_cast<uint8_t>(crc >> 8);

  digitalWrite(RS485_DE_PIN, HIGH);
  Serial2.write(frame, payload_length + 2);
  Serial2.flush();
  delayMicroseconds(kDriveHoldUs);
  digitalWrite(RS485_DE_PIN, LOW);
}

void SendException(uint8_t function, uint8_t code)
{
  uint8_t reply[5];
  reply[0] = MODBUS_EXTENSION_ADDRESS;
  reply[1] = function | 0x80;
  reply[2] = code;
  SendFrame(reply, 3);

  g_exceptions_sent++;
  Serial.printf("  -> exception %02X on function %02X\n", code, function);
}

// ---------------------------------------------------------------------------
// Decoding what the dryer sent

const char *ResultName(uint16_t result)
{
  switch (result)
  {
  case kExtResultOk:             return "ok";
  case kExtResultUnknownOpcode:  return "unknown opcode";
  case kExtResultRefused:        return "refused by policy";
  case kExtResultOutOfRange:     return "out of range";
  case kExtResultBadVersion:     return "protocol version";
  default:                       return "unrecognised result";
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
  uint32_t elapsed = (static_cast<uint32_t>(g_telemetry[kExtRegElapsedHigh]) << 16) |
                     g_telemetry[kExtRegElapsedLow];
  uint32_t uptime = (static_cast<uint32_t>(g_telemetry[kExtRegUptimeHigh]) << 16) |
                    g_telemetry[kExtRegUptimeLow];

  Serial.printf("\n--- telemetry, protocol v%d ---\n", g_telemetry[kExtRegVersion]);

  if (g_telemetry[kExtRegVersion] != EXT_PROTOCOL_VERSION)
  {
    Serial.printf("  ** the dryer speaks v%d, this sketch v%d — everything below\n",
                  g_telemetry[kExtRegVersion], EXT_PROTOCOL_VERSION);
    Serial.println("     is being read against the wrong map **");
  }

  Serial.printf("  %-14s %s\n", "phase", PhaseName(g_telemetry[kExtRegPhase]));
  PrintFlags(g_telemetry[kExtRegFlags]);

  PrintValue("inlet", g_telemetry[kExtRegInletTemp], "C");
  PrintValue("inlet RH", g_telemetry[kExtRegInletHumidity], "%");
  PrintValue("water", g_telemetry[kExtRegWaterTemp], "C");
  PrintValue("tank", g_telemetry[kExtRegTankTemp], "C");
  PrintValue("target", g_telemetry[kExtRegTargetTemp], "C");
  PrintValue("target RH", g_telemetry[kExtRegTargetHumidity], "%");

  PrintPosition("extraction", g_telemetry[kExtRegExtractionPos]);
  PrintPosition("recycling", g_telemetry[kExtRegRecyclingPos]);

  Serial.printf("  %-14s %lus  (uptime %lus)\n", "session",
                (unsigned long)elapsed, (unsigned long)uptime);

  uint16_t ack_sequence = g_telemetry[kExtRegAckSequence];
  uint16_t ack_result   = g_telemetry[kExtRegAckResult];
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

// ---------------------------------------------------------------------------
// Request handling

void HandleReadHolding(uint16_t start, uint16_t count)
{
  const uint16_t *source = nullptr;
  uint16_t offset = 0;

  if (start >= EXT_REG_COMMAND &&
      start + count <= EXT_REG_COMMAND + EXT_COMMAND_COUNT)
  {
    source = g_mailbox;
    offset = start - EXT_REG_COMMAND;
  }
  // Reading the telemetry back is not something the dryer does, but it is the
  // quickest way to confirm from a Modbus tool that a write landed.
  else if (start >= EXT_REG_TELEMETRY &&
           start + count <= EXT_REG_TELEMETRY + EXT_TELEMETRY_COUNT)
  {
    source = g_telemetry;
    offset = start - EXT_REG_TELEMETRY;
  }
  else
  {
    Serial.printf("FC03 for 0x%04X x%u — outside both blocks\n", start, count);
    SendException(kFcReadHolding, kExcIllegalAddress);
    return;
  }

  uint8_t reply[5 + 2 * EXT_TELEMETRY_COUNT];
  reply[0] = MODBUS_EXTENSION_ADDRESS;
  reply[1] = kFcReadHolding;
  reply[2] = static_cast<uint8_t>(count * 2);
  for (uint16_t i = 0; i < count; i++)
  {
    reply[3 + i * 2] = static_cast<uint8_t>(source[offset + i] >> 8);
    reply[4 + i * 2] = static_cast<uint8_t>(source[offset + i] & 0xFF);
  }
  SendFrame(reply, static_cast<uint16_t>(3 + count * 2));

  g_reads_served++;
  g_last_read_ms = millis();
}

void HandleWriteMultiple(uint16_t start, uint16_t count, const uint8_t *values)
{
  // The mailbox is ours to write and the dryer's to read. A master writing here
  // has the direction of the port backwards, and saying so is more use than
  // accepting it.
  if (start >= EXT_REG_COMMAND &&
      start < EXT_REG_COMMAND + EXT_COMMAND_COUNT)
  {
    Serial.printf("FC16 for 0x%04X — that is the command mailbox, read-only to the dryer\n",
                  start);
    SendException(kFcWriteMultiple, kExcIllegalAddress);
    return;
  }

  if (start < EXT_REG_TELEMETRY ||
      start + count > EXT_REG_TELEMETRY + EXT_TELEMETRY_COUNT)
  {
    Serial.printf("FC16 for 0x%04X x%u — outside the telemetry block\n", start, count);
    SendException(kFcWriteMultiple, kExcIllegalAddress);
    return;
  }

  uint16_t offset = start - EXT_REG_TELEMETRY;
  for (uint16_t i = 0; i < count; i++)
  {
    g_telemetry[offset + i] =
        static_cast<uint16_t>((values[i * 2] << 8) | values[i * 2 + 1]);
  }

  uint8_t reply[6];
  reply[0] = MODBUS_EXTENSION_ADDRESS;
  reply[1] = kFcWriteMultiple;
  reply[2] = static_cast<uint8_t>(start >> 8);
  reply[3] = static_cast<uint8_t>(start & 0xFF);
  reply[4] = static_cast<uint8_t>(count >> 8);
  reply[5] = static_cast<uint8_t>(count & 0xFF);
  SendFrame(reply, 6);

  g_writes_served++;
  g_last_write_ms = millis();

  PrintTelemetry();
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

  uint16_t received_crc = static_cast<uint16_t>(g_frame[g_frame_length - 2]) |
                          static_cast<uint16_t>(g_frame[g_frame_length - 1] << 8);
  if (received_crc != Crc16(g_frame, g_frame_length - 2))
  {
    g_crc_errors++;
    Serial.printf("CRC error on a %u byte frame (%lu so far) — baud, noise, or\n",
                  g_frame_length, (unsigned long)g_crc_errors);
    Serial.println("  a segment with termination at only one end");
    return;
  }

  uint8_t address = g_frame[0];
  if (address != MODBUS_EXTENSION_ADDRESS)
  {
    // The probe @1 and the hydraulic module @10 share this pair. Hearing their
    // traffic is proof the wiring is right, so it is counted, not complained
    // about.
    g_not_for_us++;
    return;
  }

  uint8_t function = g_frame[1];
  uint16_t start = static_cast<uint16_t>((g_frame[2] << 8) | g_frame[3]);
  uint16_t count = static_cast<uint16_t>((g_frame[4] << 8) | g_frame[5]);

  switch (function)
  {
  case kFcReadHolding:
    if (count == 0 || count > EXT_TELEMETRY_COUNT)
    {
      SendException(function, kExcIllegalValue);
      return;
    }
    HandleReadHolding(start, count);
    break;

  case kFcWriteMultiple:
    // addr fc start(2) count(2) bytecount(1) payload crc(2)
    if (g_frame_length < 9 + count * 2 || g_frame[6] != count * 2)
    {
      Serial.printf("FC16 byte count disagrees with the register count (%u vs %u)\n",
                    g_frame[6], count * 2);
      SendException(function, kExcIllegalValue);
      return;
    }
    HandleWriteMultiple(start, count, &g_frame[7]);
    break;

  default:
    Serial.printf("Function %02X — this module only implements FC03 and FC16\n",
                  function);
    SendException(function, kExcIllegalFunction);
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

  g_mailbox[kExtCmdRegOpcode]   = opcode;
  g_mailbox[kExtCmdRegArgument] = static_cast<uint16_t>(ExtEncodeValue(argument));
  g_mailbox[kExtCmdRegVersion]  = version;
  g_mailbox[kExtCmdRegSequence] = g_sequence;

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
    g_mailbox[i] = 0;
  }
  g_pending_sequence = 0;
  Serial.println("\nMailbox cleared. Sequence 0 is how a module says it wants nothing.\n");
}

void PrintCounters()
{
  uint32_t now = millis();
  Serial.println("\n--- bus counters ---");
  Serial.printf("  frames seen        %lu\n", (unsigned long)g_frames_seen);
  Serial.printf("  for another slave  %lu   (proof the pair is right)\n",
                (unsigned long)g_not_for_us);
  Serial.printf("  CRC errors         %lu\n", (unsigned long)g_crc_errors);
  Serial.printf("  telemetry writes   %lu", (unsigned long)g_writes_served);
  if (g_writes_served > 0)
  {
    Serial.printf("   last %lu ms ago", (unsigned long)(now - g_last_write_ms));
  }
  Serial.println();
  Serial.printf("  mailbox reads      %lu", (unsigned long)g_reads_served);
  if (g_reads_served > 0)
  {
    Serial.printf("   last %lu ms ago", (unsigned long)(now - g_last_read_ms));
  }
  Serial.println();
  Serial.printf("  exceptions sent    %lu\n", (unsigned long)g_exceptions_sent);

  if (g_frames_seen == 0)
  {
    Serial.println("\n  Nothing at all on the pair. A and B swapped, no termination,");
    Serial.println("  the dryer not running, or its DE never rising.");
  }
  else if (g_writes_served == 0 && g_not_for_us > 0)
  {
    Serial.printf("\n  Traffic for other slaves but none for @%d: the dryer is not\n",
                  MODBUS_EXTENSION_ADDRESS);
    Serial.println("  addressing this module. Check MODBUS_EXTENSION_ADDRESS on both sides.");
  }
  else if (g_writes_served > 0 && g_reads_served == 0)
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
  Serial.println("Keys — a value may follow, e.g. `t 38.5`:");
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
  Serial.println("  n        bus counters");
  Serial.println("  ?        this help");
  Serial.println();
}

// Reads one console line: a command letter and an optional number.
void PollConsole()
{
  static char line[32];
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
    float argument = atof(&line[1]); // 0 when nothing follows

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
  Serial.println("  Extension module stand-in");
  Serial.println("========================================");
  Serial.println();

  PrintWiring();

  for (uint8_t i = 0; i < EXT_TELEMETRY_COUNT; i++)
  {
    g_telemetry[i] = 0;
  }
  ClearMailbox();

  Serial2.setTX(RS485_TX_PIN);
  Serial2.setRX(RS485_RX_PIN);
  Serial2.begin(MODBUS_BAUDRATE, SERIAL_8N1);

  pinMode(RS485_DE_PIN, OUTPUT);
  digitalWrite(RS485_DE_PIN, LOW); // receive until we have something to say

  Serial.printf("Listening as slave @%d at %d baud 8N1.\n",
                MODBUS_EXTENSION_ADDRESS, MODBUS_BAUDRATE);
  Serial.printf("  telemetry  0x%04X x%d   written by the dryer, FC16\n",
                EXT_REG_TELEMETRY, EXT_TELEMETRY_COUNT);
  Serial.printf("  mailbox    0x%04X x%d    read by the dryer, FC03\n",
                EXT_REG_COMMAND, EXT_COMMAND_COUNT);
  Serial.println();
  Serial.println("A telemetry block should land every two seconds. If none does,");
  Serial.println("press n before touching the wiring — it says which half is missing.");
  Serial.println();
  PrintHelp();
}

void loop()
{
  PollBus();
  PollConsole();
}
