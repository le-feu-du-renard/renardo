// RS485 bring-up for the "probe" bus (UART0) — built by
// `pio run -e rs485_test -t upload -t monitor`. This is the inlet probe's own
// segment, separate from the "ext" bus the hydraulic module and the extension
// port share; extension_test covers that one.
//
// Answers, in the order the faults actually happen:
//   RX stuck LOW at rest        -> A and B are swapped, or the pair is unbiased
//   every read times out (0xE2) -> nobody is answering: address, power, A/B,
//                                  baud, or DE never rising
//   0xE0 / 0xE1                 -> we are hearing our own frame: RE is not
//                                  following DE, so the receiver stays enabled
//                                  while we transmit
//   0xE3 (bad CRC)              -> bytes arrive but mangled: wrong baud, noise,
//                                  missing termination
//   0x02 (illegal address)      -> the probe is alive and correctly addressed,
//                                  but its register map is not the one in
//                                  config.h
//
// It drives the *production* Rs485Bus, so a reading printed here is a reading
// the firmware would get. What it adds is everything Rs485Bus deliberately
// hides: the idle level of the receiver before a single frame is sent, the raw
// bytes on the wire, and a scan that does not need to know the address or the
// baud rate beforehand.
//
// The scan uses its own frame builder rather than ModbusMaster, whose response
// timeout is a fixed two seconds — that would turn a 32-address sweep into a
// minute of waiting. Hand-built frames also mean the raw answer can be shown
// even when it is too corrupt for a library to accept.

#include <Arduino.h>

#include "config.h"
#include "Logger.h"
#include "Rs485Bus.h"

namespace
{

Rs485Bus g_bus(Serial1, RS485_PROBE_TX_PIN, RS485_PROBE_RX_PIN, RS485_PROBE_DE_PIN, "rs485");

// Baud rates worth trying on a probe of unknown configuration. 9600 first:
// it is both the factory default of these probes and what config.h expects.
constexpr uint32_t kBaudRates[] = {9600, 4800, 19200, 38400, 115200};
constexpr uint8_t  kBaudCount   = sizeof(kBaudRates) / sizeof(kBaudRates[0]);

// Addresses swept when nobody answers at the expected one. Modbus allows 1-247,
// but a probe out of its box is always low, and a full sweep is four minutes.
constexpr uint8_t kScanFirstAddress = 1;
constexpr uint8_t kScanLastAddress  = 32;

// A silent slave is silent within a few milliseconds at any of these bauds; the
// generous value is for a probe that samples on demand before answering.
constexpr uint32_t kRawTimeoutMs   = 120;
// End of frame: Modbus RTU says 3.5 character times, 4 ms at 9600 baud.
constexpr uint32_t kRawInterByteMs = 8;

uint32_t g_baudrate = MODBUS_BAUDRATE;

uint32_t g_polls = 0;
uint32_t g_ok    = 0;
uint8_t  g_last_error = 0;

// A bus that never worked and a bus that stopped working are different faults
// with the same error code, so the run of successes before the first failure is
// worth keeping.
uint32_t g_consecutive_failures = 0;
uint32_t g_successes_before_failure = 0;
bool     g_pair_rechecked = false;

uint32_t g_last_poll_ms = 0;
constexpr uint32_t kPollIntervalMs = 2000;

// The UART framing the probe is asked to use. 8N1 is what config.h and
// Rs485Bus assume; the sweep exists because a probe configured for even parity
// answers perfectly and yet never parses.
uint16_t    g_serial_config = SERIAL_8N1;
const char *g_config_name   = "8N1";

// The last frame put on the wire, kept so the answer can be compared with it.
uint8_t g_last_request[8];

// ---------------------------------------------------------------------------
// Raw Modbus, used by the scans

uint16_t Crc16(const uint8_t *frame, uint8_t length)
{
  uint16_t crc = 0xFFFF;
  for (uint8_t i = 0; i < length; i++)
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

// Send one FC03 request and return whatever comes back, corrupt or not.
// Returns the number of bytes received.
uint8_t RawRead(uint8_t address, uint16_t start_register, uint8_t count,
                uint8_t *response, uint8_t response_size)
{
  uint8_t frame[8];
  frame[0] = address;
  frame[1] = 0x03;
  frame[2] = static_cast<uint8_t>(start_register >> 8);
  frame[3] = static_cast<uint8_t>(start_register & 0xFF);
  frame[4] = 0x00;
  frame[5] = count;
  uint16_t crc = Crc16(frame, 6);
  frame[6] = static_cast<uint8_t>(crc & 0xFF);   // CRC goes out low byte first
  frame[7] = static_cast<uint8_t>(crc >> 8);
  memcpy(g_last_request, frame, sizeof(frame));

  while (Serial1.available() > 0)
  {
    Serial1.read();  // anything still pending belongs to the previous frame
  }

  digitalWrite(RS485_PROBE_DE_PIN, HIGH);
  Serial1.write(frame, sizeof(frame));
  Serial1.flush();               // the last bit must leave before DE drops
  digitalWrite(RS485_PROBE_DE_PIN, LOW);

  uint8_t  received = 0;
  uint32_t deadline = millis() + kRawTimeoutMs;
  while (millis() < deadline && received < response_size)
  {
    if (Serial1.available() > 0)
    {
      response[received++] = static_cast<uint8_t>(Serial1.read());
      deadline = millis() + kRawInterByteMs;  // frame ends on silence, not length
    }
  }
  return received;
}

// A Modbus frame carries its own CRC in the last two bytes, low byte first.
bool FrameCrcOk(const uint8_t *frame, uint8_t length)
{
  if (length < 4)
  {
    return false;
  }
  uint16_t expected = Crc16(frame, static_cast<uint8_t>(length - 2));
  uint16_t received = static_cast<uint16_t>(frame[length - 2] |
                                            (frame[length - 1] << 8));
  return expected == received;
}

// Where in the frame we just sent the bytes that came back can be found, or -1
// if they are not part of it.
//
// A whole echo is the easy case. The common one is a fragment: RE left floating
// sits near its threshold and the receiver flickers on and off through the
// transmission, so what returns is some contiguous slice of the request — its
// first two bytes, or its last four, rather than all eight. Those slices are
// what a swept bus is full of, and none of them is a slave answering.
//
// A genuine reply cannot be mistaken for one: it carries a byte count where the
// request carries the high half of a register number, so it diverges by the
// third byte. Single bytes are excluded because any one value can coincide.
int EchoOffset(const uint8_t *response, uint8_t length)
{
  if (length < 2 || length > sizeof(g_last_request))
  {
    return -1;
  }
  for (uint8_t offset = 0; offset + length <= sizeof(g_last_request); offset++)
  {
    if (memcmp(response, g_last_request + offset, length) == 0)
    {
      return offset;
    }
  }
  return -1;
}

bool IsEcho(const uint8_t *response, uint8_t length)
{
  return EchoOffset(response, length) >= 0;
}

void PrintBytes(const uint8_t *bytes, uint8_t length)
{
  for (uint8_t i = 0; i < length; i++)
  {
    Serial.printf("%02X ", bytes[i]);
  }
}

void ReopenPort(uint32_t baudrate, uint16_t config)
{
  Serial1.end();
  delay(20);
  Serial1.setTX(RS485_PROBE_TX_PIN);
  Serial1.setRX(RS485_PROBE_RX_PIN);
  Serial1.begin(baudrate, config);
  delay(20);
}

// ---------------------------------------------------------------------------
// Diagnosis

const char *ErrorName(uint8_t error)
{
  switch (error)
  {
    case 0x00: return "success";
    case 0x01: return "illegal function";
    case 0x02: return "illegal data address";
    case 0x03: return "illegal data value";
    case 0x04: return "slave device failure";
    case 0xE0: return "invalid slave ID in the answer";
    case 0xE1: return "invalid function in the answer";
    case 0xE2: return "response timed out";
    case 0xE3: return "invalid CRC";
    default:   return "unknown";
  }
}

// The part that matters: which wire to go and look at.
void PrintDiagnosis(uint8_t error)
{
  switch (error)
  {
    case 0xE2:
      Serial.println("        Nothing answered at all. In the order worth checking:");
      Serial.println("        the probe has no power; A and B are swapped; the address");
      Serial.println("        is not the one asked for; the baud rate differs; DE never");
      Serial.println("        rises, so the frame never reached the pair.");
      Serial.println("        Press 's' to sweep addresses, 'b' to sweep baud rates.");
      break;
    case 0xE0:
    case 0xE1:
      Serial.println("        Something answered, but not as the slave we addressed —");
      Serial.println("        this is the shape of hearing our own frame back. RE must");
      Serial.println("        follow DE: tied together on GP3, both HIGH while sending.");
      Serial.println("        RE left on GND keeps the receiver on during transmission.");
      break;
    case 0xE3:
      Serial.println("        Bytes arrived but the CRC did not match. Most often this is");
      Serial.println("        our own frame coming back: RE is not following DE, so the");
      Serial.println("        receiver stayed on while we transmitted. The echo begins");
      Serial.println("        with the address and function code we just sent, which the");
      Serial.println("        library accepts, and only then chokes on the byte count —");
      Serial.println("        which is why an echo reads as a bad CRC and not as a wrong");
      Serial.println("        slave ID. Press 'e' to settle it in one frame.");
      Serial.println("        Otherwise the framing is wrong rather than the wiring:");
      Serial.println("        wrong baud rate ('b'), wrong parity ('p'), or an unbiased");
      Serial.println("        pair picking up noise. Press 's' to see the raw bytes.");
      break;
    case 0x02:
      Serial.println("        The probe is alive and correctly addressed, but has no");
      Serial.println("        register at 0x0000. Its map differs from config.h —");
      Serial.println("        press 'm' to walk the first registers and find the real one.");
      break;
    default:
      break;
  }
}

// ---------------------------------------------------------------------------
// Phases

// Before a single frame is sent: on an idle RS485 pair with A above B, the
// receiver output sits HIGH, which is the UART's mark state. Sampling GP5 as a
// plain input therefore catches a swapped pair before Modbus is even involved
// — the fault that otherwise looks exactly like a dead probe.
void CheckIdleLevel()
{
  // The UART owns RX once the bus is open; take the pin back for the sample.
  Serial1.end();
  delay(5);

  pinMode(RS485_PROBE_DE_PIN, OUTPUT);
  digitalWrite(RS485_PROBE_DE_PIN, LOW);  // receive, so RO reflects the pair
  pinMode(RS485_PROBE_RX_PIN, INPUT);
  delay(5);

  uint32_t high = 0;
  uint32_t total = 0;
  for (uint32_t i = 0; i < 5000; i++)
  {
    if (digitalRead(RS485_PROBE_RX_PIN) == HIGH)
    {
      high++;
    }
    total++;
    delayMicroseconds(10);
  }

  uint32_t percent = (high * 100) / total;
  Serial.printf("Idle receiver level: HIGH %lu%% of the time\n", (unsigned long)percent);

  if (percent >= 99)
  {
    Serial.println("  Good — the pair is idle and the right way round.");
  }
  else if (percent <= 1)
  {
    Serial.println("  STUCK LOW. The receiver reads a permanent break, which is what a");
    Serial.println("  reversed pair looks like: swap A and B at the probe. If the probe");
    Serial.println("  is not connected yet, this instead means A and B are floating with");
    Serial.println("  no bias — normal on a bare transceiver, retest once the probe is on.");
  }
  else
  {
    Serial.println("  Toggling. Either another master is talking on the pair, or the pair");
    Serial.println("  is floating and picking up noise. Noise here becomes CRC errors");
    Serial.println("  later: bias the pair, 680 ohm from A to 3V3 and from B to GND.");
  }
  Serial.println();

  ReopenPort(g_baudrate, g_serial_config);
}

// Asks a question of an address nothing on this bus owns. Nobody can answer,
// so any byte coming back is our own frame returning — the one fault that
// leaves the wiring looking perfectly plausible.
void CheckEcho()
{
  // Legal Modbus address, and not one this dryer ever assigns.
  constexpr uint8_t kUnusedAddress = 247;

  uint8_t response[24];
  uint8_t length = RawRead(kUnusedAddress, MODBUS_REG_HUMIDITY, 2, response, sizeof(response));

  Serial.printf("Echo check (asking @%d, which nothing owns): ", kUnusedAddress);
  if (length == 0)
  {
    Serial.println("silent.");
    Serial.println("  Good — the receiver is off while we transmit, so an answer that");
    Serial.println("  does arrive comes from a slave and not from us.");
  }
  else if (IsEcho(response, length))
  {
    if (length == sizeof(g_last_request))
    {
      Serial.println("the whole frame we just sent, back again.");
    }
    else
    {
      Serial.printf("%d bytes, and they are a slice of the frame we just sent: ", length);
      PrintBytes(response, length);
      Serial.println();
    }
    Serial.println("  ECHO. RE is not following DE, so the receiver stayed on while the");
    Serial.println("  driver was talking. Tie RE and DE together and run one wire to GP3:");
    Serial.println("  RE is active LOW, so leaving it on GND — or leaving it unconnected,");
    Serial.println("  which floats near the threshold — keeps the receiver listening to us.");
    Serial.println("  A slice rather than the whole frame is the floating case: the pin");
    Serial.println("  wanders across the threshold and the receiver flickers mid-frame.");
    Serial.println("  This is what reads as 'invalid CRC' once a probe is actually polled.");
    Serial.println();
    Serial.println("  It is also proof the local chain is sound: the bytes travelled");
    Serial.println("  GP4 -> DI -> the pair -> RO -> GP5 and came back intact, so TX and RX");
    Serial.println("  are on the right pins, the transceiver is powered, and DE does work.");
  }
  else
  {
    Serial.printf("%d unexpected bytes: ", length);
    PrintBytes(response, length);
    Serial.println();
    Serial.println("  Not an echo, and no slave should have spoken. Either another master");
    Serial.println("  shares the pair, or the pair is floating and the receiver is reading");
    Serial.println("  noise as characters.");
  }
  Serial.println();
}

void PollProbe(uint8_t address)
{
  uint16_t raw[2] = {0, 0};
  g_polls++;

  if (g_bus.ReadHoldingRegisters(address, MODBUS_REG_HUMIDITY, 2, raw))
  {
    g_ok++;
    g_last_error = 0;
    g_consecutive_failures = 0;
    g_pair_rechecked = false;
    Serial.printf("@%-3d  %5.1f %%RH   %5.1f C     raw %04X %04X   (%lu/%lu ok)\n",
                  address,
                  static_cast<double>(raw[0]) / MODBUS_RAW_SCALE,
                  static_cast<double>(raw[1]) / MODBUS_RAW_SCALE,
                  raw[0], raw[1],
                  (unsigned long)g_ok, (unsigned long)g_polls);
    return;
  }

  uint8_t error = g_bus.GetLastError();
  Serial.printf("@%-3d  FAILED  error %02X (%s)   (%lu/%lu ok)\n",
                address, error, ErrorName(error),
                (unsigned long)g_ok, (unsigned long)g_polls);

  g_consecutive_failures++;
  if (g_consecutive_failures == 1)
  {
    g_successes_before_failure = g_ok;
  }

  // The advice is long, so print it only when the failure mode changes.
  if (error != g_last_error)
  {
    PrintDiagnosis(error);
  }
  g_last_error = error;

  // A bus that answered and then stopped is a different animal from one that
  // never answered, and the error code is the same for both. Three failures in
  // a row after a working run: go and look at the pair itself, which says
  // whether the probe is still there at all.
  if (g_consecutive_failures == 3 && g_successes_before_failure > 0 && !g_pair_rechecked)
  {
    g_pair_rechecked = true;
    Serial.printf("\n%lu reads succeeded, then the bus went quiet. Looking at the pair:\n",
                  (unsigned long)g_successes_before_failure);
    CheckIdleLevel();
    Serial.println("Read that line as follows:");
    Serial.println("  HIGH 100%  the probe is still powering and biasing the pair, so it");
    Serial.println("             is alive and simply not answering us. Suspect the EN or");
    Serial.println("             the TX wire — a lead that has worked loose stops the");
    Serial.println("             driver dead while leaving the pair perfectly idle.");
    Serial.println("  toggling   nothing is holding the pair any more: the probe has lost");
    Serial.println("             its supply, or an A/B wire is off. Measure its V+ now,");
    Serial.println("             while it is failing — not after wiggling anything.");
    Serial.println("Then press 'e': an echo proves the driver still transmits.");
    Serial.println();
  }
}

void ScanAddresses()
{
  Serial.printf("\nSweeping addresses %d..%d at %lu baud...\n",
                kScanFirstAddress, kScanLastAddress, (unsigned long)g_baudrate);

  uint8_t found = 0;
  for (uint8_t address = kScanFirstAddress; address <= kScanLastAddress; address++)
  {
    uint8_t response[24];
    uint8_t length = RawRead(address, MODBUS_REG_HUMIDITY, 2, response, sizeof(response));
    if (length == 0)
    {
      continue;
    }

    // Every address appears to answer when we are hearing ourselves, so say so
    // once and stop rather than printing the same echo thirty-two times.
    if (IsEcho(response, length))
    {
      Serial.printf("  @%-3d gave back %d bytes of the frame we sent — an echo, not an answer.\n",
                    address, length);
      Serial.println("  Stopping the sweep: with the receiver on during transmission,");
      Serial.println("  every address would look alive. Press 'e' for the full diagnosis.");
      Serial.println();
      return;
    }

    found++;
    Serial.printf("  @%-3d answered %d bytes: ", address, length);
    PrintBytes(response, length);

    // A well-formed FC03 answer to a two-register read is 9 bytes:
    // addr, 03, byte count, four data bytes, two CRC bytes.
    if (length >= 9 && response[0] == address && response[1] == 0x03 &&
        FrameCrcOk(response, 9))
    {
      uint16_t humidity    = static_cast<uint16_t>((response[3] << 8) | response[4]);
      uint16_t temperature = static_cast<uint16_t>((response[5] << 8) | response[6]);
      Serial.printf("  ->  %.1f %%RH  %.1f C",
                    static_cast<double>(humidity) / MODBUS_RAW_SCALE,
                    static_cast<double>(temperature) / MODBUS_RAW_SCALE);
    }
    else if (length >= 3 && (response[1] & 0x80) != 0)
    {
      Serial.printf("  ->  alive, but rejected the request (exception %02X)", response[2]);
    }
    else
    {
      Serial.print("  ->  no valid frame in there: wrong baud ('b') or wrong parity ('p')");
    }
    Serial.println();
    delay(20);
  }

  if (found == 0)
  {
    Serial.println("  Nobody answered. If the idle level above was good, try 'b' to");
    Serial.println("  sweep baud rates before suspecting the wiring again.");
  }
  Serial.println();
}

void ScanBaudRates()
{
  Serial.println("\nSweeping baud rates at every address in the scan range...");
  Serial.println("Only a frame whose CRC holds counts: at the wrong baud rate bytes still");
  Serial.println("arrive, they simply mean nothing.");

  for (uint8_t i = 0; i < kBaudCount; i++)
  {
    ReopenPort(kBaudRates[i], g_serial_config);

    Serial.printf("  %6lu baud: ", (unsigned long)kBaudRates[i]);
    uint8_t valid = 0;
    uint8_t noise = 0;
    for (uint8_t address = kScanFirstAddress; address <= kScanLastAddress; address++)
    {
      uint8_t response[24];
      uint8_t length = RawRead(address, MODBUS_REG_HUMIDITY, 2, response, sizeof(response));
      if (length == 0 || IsEcho(response, length))
      {
        continue;
      }
      if (length >= 9 && FrameCrcOk(response, 9))
      {
        Serial.printf("@%d ", address);
        valid++;
      }
      else
      {
        noise++;
      }
    }

    if (valid > 0)
    {
      Serial.println();
      g_baudrate = kBaudRates[i];
      Serial.printf("  ^ a valid frame came back here. Set MODBUS_BAUDRATE to %lu in\n",
                    (unsigned long)g_baudrate);
      Serial.println("    config.h, or reconfigure the probe to 9600 — the rest of the bus");
      Serial.println("    is 9600, and one slave out of step stalls every transaction.");
      ReopenPort(g_baudrate, g_serial_config);
      return;
    }
    Serial.printf("%s\n", noise > 0 ? "bytes, none of them a valid frame" : "silent");
  }

  Serial.println("  No valid frame at any baud rate. If bytes came back throughout, the");
  Serial.println("  framing is wrong in another way — try 'p' for parity. If it was silent");
  Serial.println("  throughout, the fault is upstream: power, the pair, or DE.");
  Serial.println();

  // Leave the bus where the firmware expects it.
  ReopenPort(g_baudrate, g_serial_config);
}

// A probe set to even parity answers every request and parses as nothing: the
// parity bit is read as the first stop bit, so every byte lands shifted. It is
// indistinguishable from noise until it is tried.
void ScanSerialConfigs()
{
  struct Framing
  {
    uint16_t    config;
    const char *name;
  };
  static const Framing kFramings[] = {
      {SERIAL_8N1, "8N1"},
      {SERIAL_8E1, "8E1"},
      {SERIAL_8O1, "8O1"},
      {SERIAL_8N2, "8N2"},
  };

  Serial.printf("\nSweeping framings at %lu baud, address %d...\n",
                (unsigned long)g_baudrate, MODBUS_INLET_ADDRESS);

  for (const Framing &framing : kFramings)
  {
    ReopenPort(g_baudrate, framing.config);
    Serial.printf("  %s: ", framing.name);

    uint8_t response[24];
    uint8_t length = RawRead(MODBUS_INLET_ADDRESS, MODBUS_REG_HUMIDITY, 2,
                             response, sizeof(response));
    if (length == 0)
    {
      Serial.println("silent");
    }
    else if (IsEcho(response, length))
    {
      Serial.println("our own frame back — fix the echo first, press 'e'");
      break;
    }
    else if (length >= 9 && FrameCrcOk(response, 9))
    {
      Serial.print("valid frame: ");
      PrintBytes(response, 9);
      Serial.println();
      g_serial_config = framing.config;
      g_config_name   = framing.name;
      Serial.printf("  ^ the probe speaks %s. Rs485Bus opens the port as 8N1, so either\n",
                    framing.name);
      Serial.println("    reconfigure the probe or change the SERIAL_8N1 in Rs485Bus::Begin");
      Serial.println("    — and remember every other slave on the pair must match.");
      ReopenPort(g_baudrate, g_serial_config);
      return;
    }
    else
    {
      Serial.printf("%d bytes, CRC bad: ", length);
      PrintBytes(response, length);
      Serial.println();
    }
    delay(20);
  }

  Serial.println("  No framing produced a valid frame at this baud rate.");
  Serial.println();
  ReopenPort(g_baudrate, g_serial_config);
}

// Some probes number their registers from 1, some put temperature first, some
// answer only single-register reads. Walking the first sixteen one at a time
// tells which, and costs two seconds.
void WalkRegisters(uint8_t address)
{
  Serial.printf("\nReading registers 0x0000..0x000F one by one at @%d...\n", address);
  for (uint16_t reg = 0; reg < 16; reg++)
  {
    uint8_t response[16];
    uint8_t length = RawRead(address, reg, 1, response, sizeof(response));
    if (length >= 5 && response[1] == 0x03)
    {
      uint16_t value = static_cast<uint16_t>((response[3] << 8) | response[4]);
      Serial.printf("  0x%04X = %5u   (/10 = %6.1f)\n", reg, value,
                    static_cast<double>(value) / 10.0);
    }
    else if (length >= 3 && (response[1] & 0x80) != 0)
    {
      Serial.printf("  0x%04X   exception %02X\n", reg, response[2]);
    }
    else
    {
      Serial.printf("  0x%04X   no answer\n", reg);
    }
    delay(20);
  }
  Serial.println();
}

// Everything the pair carries, decoded by nobody. Useful when frames come back
// mangled: a wrong baud rate has a signature, noise has another.
void Listen(uint32_t duration_ms)
{
  Serial.printf("\nListening on the pair for %lu s (no frame is sent)...\n",
                (unsigned long)(duration_ms / 1000));
  while (Serial1.available() > 0)
  {
    Serial1.read();
  }

  uint32_t deadline = millis() + duration_ms;
  uint32_t count = 0;
  while (millis() < deadline)
  {
    if (Serial1.available() > 0)
    {
      Serial.printf("%02X ", Serial1.read());
      if (++count % 16 == 0)
      {
        Serial.println();
      }
    }
  }
  Serial.println();
  if (count == 0)
  {
    Serial.println("  Silent, which is correct for an idle bus with one master.");
  }
  else
  {
    Serial.printf("  %lu bytes with nobody asking. The pair is unbiased and picking up\n",
                  (unsigned long)count);
    Serial.println("  noise, or another master shares the segment.");
  }
  Serial.println();
}

void PrintWiring()
{
  Serial.println("Expected wiring — MAX3485, Pico header pins in brackets. This is the");
  Serial.println("'probe' bus (UART0), not the 'ext' bus the hydraulic module and the");
  Serial.println("extension port share.");
  Serial.println("Silkscreens vary; the second column is what the pin really is:");
  Serial.printf("  RO  / TXD  receiver out -> GP%-2d [22]  UART0 RX\n", RS485_PROBE_RX_PIN);
  Serial.printf("  DI  / RXD  driver in    <- GP%-2d [21]  UART0 TX\n", RS485_PROBE_TX_PIN);
  Serial.printf("  DE+RE / EN direction    <- GP%-2d [34]  HIGH = transmit\n", RS485_PROBE_DE_PIN);
  Serial.println("  VCC                     -> 3V3(OUT) [36]   3.3 V only");
  Serial.println("  GND                     -> GND      [23]  shared with the fan/electric commands");
  Serial.println("  A / B                   -> the probe's A / B, no crossover");
  Serial.println();
  Serial.println("RXD and TXD are named from the microcontroller's point of view: the pin");
  Serial.println("marked RXD is the transceiver's DI and takes the Pico's TX. A board");
  Serial.println("marked DI/RO is named from its own, and wires the other way round.");
  Serial.println();
}

void PrintHelp()
{
  Serial.println("Keys:  e = echo check, the one to run first on a CRC error");
  Serial.println("       i = idle level of the pair, the one to run when a working");
  Serial.println("           bus goes quiet");
  Serial.println("       s = sweep addresses    b = sweep baud rates");
  Serial.println("       p = sweep parity and stop bits");
  Serial.println("       m = walk the register map at the expected address");
  Serial.println("       l = listen to the pair for 5 s");
  Serial.println("       h = this help");
  Serial.println();
}

} // namespace

void setup()
{
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("========================================");
  Serial.println("  RS485 / Modbus — bring-up");
  Serial.println("========================================");
  Serial.println();

  PrintWiring();

  Logger::Init();
  g_bus.Begin(g_baudrate);
  Serial.println();

  // Reads the receiver's resting level as a plain logic level, with the UART
  // briefly handed the pin back. Repeatable on 'i', which is what makes it
  // useful when a bus that was working stops.
  CheckIdleLevel();

  CheckEcho();

  Serial.printf("\nPolling the inlet probe @%d every %lu ms, registers 0x%04X..0x%04X.\n",
                MODBUS_INLET_ADDRESS, (unsigned long)kPollIntervalMs,
                MODBUS_REG_HUMIDITY, MODBUS_REG_TEMPERATURE);
  Serial.println("Breathe on the probe: the humidity must climb within a few seconds.");
  Serial.println("A value that never moves is a probe answering from a cached register.");
  Serial.println();
  PrintHelp();
}

void loop()
{
  if (Serial.available() > 0)
  {
    int key = Serial.read();
    while (Serial.available() > 0)
    {
      Serial.read();
    }

    switch (key)
    {
      case 'e': CheckEcho(); break;
      case 'i': CheckIdleLevel(); break;
      case 's': ScanAddresses(); break;
      case 'b': ScanBaudRates(); break;
      case 'p': ScanSerialConfigs(); break;
      case 'm': WalkRegisters(MODBUS_INLET_ADDRESS); break;
      case 'l': Listen(5000); break;
      case 'h': PrintWiring(); PrintHelp(); break;
      default: break;
    }
    g_last_poll_ms = millis();
    return;
  }

  uint32_t now = millis();
  if (now - g_last_poll_ms >= kPollIntervalMs)
  {
    g_last_poll_ms = now;
    PollProbe(MODBUS_INLET_ADDRESS);
  }
}
