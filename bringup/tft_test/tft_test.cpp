// Display bring-up test — built by `pio run -e tft_test -t upload -t monitor`.
//
// The 7-pin module has no MISO line, so there is normally no way to tell a
// working panel from a dead one: TFT_eSPI writes blind. The ST7789 will however
// answer on the SDA line itself, which is bidirectional in 4-wire SPI, and
// TFT_eSPI can turn the pin around for a read when TFT_SDA_READ is set.
//
// Reading the ID register settles the question the main firmware cannot:
//   plausible ID  -> the panel is powered, out of reset, and both directions of
//                    the wiring work; anything still wrong is in the driver
//                    configuration
//   0x00 or 0xFF  -> nothing is answering, so it is power, reset, CS or the
//                    clock/data pair
//
// Reads need a much slower clock than writes, hence SPI_READ_FREQUENCY in the
// environment.

#include <Arduino.h>
#include <TFT_eSPI.h>

namespace
{

TFT_eSPI tft;

// ST7789 identification and status registers.
constexpr uint8_t kCmdReadDisplayId     = 0x04; // RDDID: 3 bytes
constexpr uint8_t kCmdReadDisplayStatus = 0x09; // RDDST: 4 bytes
constexpr uint8_t kCmdReadPowerMode     = 0x0A; // RDDPM
constexpr uint8_t kCmdReadMadctl        = 0x0B; // RDDMADCTL

void ReportRegister(const char *label, uint8_t command, uint8_t count)
{
  Serial.printf("  %-16s (0x%02X): ", label, command);
  bool all_zero = true;
  bool all_ones = true;

  for (uint8_t index = 0; index < count; index++)
  {
    uint8_t value = tft.readcommand8(command, index);
    Serial.printf("0x%02X ", value);
    if (value != 0x00) all_zero = false;
    if (value != 0xFF) all_ones = false;
  }

  if (all_zero)      Serial.print(" <- all zero, nothing answering");
  else if (all_ones) Serial.print(" <- all ones, line idle high");
  Serial.println();
}

// Ask the library what it actually ended up configured with, rather than
// trusting that the -D flags reached it. If USER_SETUP_LOADED were not honoured
// the library would silently fall back to User_Setup.h and drive a completely
// different set of pins, which looks exactly like a dead panel.
void ReportEffectiveSetup()
{
  setup_t setup;
  tft.getSetup(setup);

  Serial.println("Configuration the library is actually using:");
  Serial.printf("  driver code : 0x%04X   (ST7789 expected)\n", setup.tft_driver);
  Serial.printf("  panel size  : %u x %u\n", setup.tft_width, setup.tft_height);
  Serial.printf("  MOSI        : GP%d\n", setup.pin_tft_mosi);
  Serial.printf("  SCLK        : GP%d\n", setup.pin_tft_clk);
  Serial.printf("  CS          : GP%d\n", setup.pin_tft_cs);
  Serial.printf("  DC          : GP%d\n", setup.pin_tft_dc);
  Serial.printf("  RST         : GP%d\n", setup.pin_tft_rst);
  Serial.printf("  MISO        : GP%d   (-1 = not wired, expected)\n",
                setup.pin_tft_miso);
  Serial.println();
  Serial.println("  Expected: MOSI 19, SCLK 18, CS 16, DC 17, RST 20.");
  Serial.println("  Anything else means the build flags never reached the");
  Serial.println("  library and it fell back to its own defaults.");
  Serial.println();
}

void ShowPattern(uint16_t colour, const char *name)
{
  Serial.printf("  filling %s\n", name);
  tft.fillScreen(colour);
  delay(700);
}

} // namespace

void setup()
{
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("========================================");
  Serial.println("  ST7789 bring-up test");
  Serial.println("========================================");

  tft.init();
  tft.setRotation(1);

  ReportEffectiveSetup();

  // Note: with CS on GP16, which is also SPI0's RX pin, TFT_SDA_READ is not
  // trustworthy — it calls spi.end()/spi.begin() around the read, which
  // reclaims the pin as MISO and drops CS mid-transaction. Treat a silent
  // answer here as inconclusive; the pattern sweep is the real test.
  Serial.println("Reading back over SDA (TFT_SDA_READ, unreliable with CS on GP16):");
  ReportRegister("display id", kCmdReadDisplayId, 3);
  ReportRegister("display status", kCmdReadDisplayStatus, 4);
  ReportRegister("power mode", kCmdReadPowerMode, 1);
  ReportRegister("madctl", kCmdReadMadctl, 1);
  Serial.println();
  Serial.println("An ST7789V usually answers 0x85 0x85 0x52 on the id register.");
  Serial.println("Some modules tie SDA through a series resistor and cannot be");
  Serial.println("read at all, so all-zero is suggestive rather than conclusive.");
  Serial.println();
}

void loop()
{
  Serial.println("Pattern sweep:");
  ShowPattern(TFT_RED, "red");
  ShowPattern(TFT_GREEN, "green");
  ShowPattern(TFT_BLUE, "blue");
  ShowPattern(TFT_WHITE, "white");
  ShowPattern(TFT_BLACK, "black");

  // Corner markers reveal orientation and any row/column offset: the boxes must
  // sit flush in the corners, and the numbers must read the right way up.
  Serial.println("  corner markers and axes");
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, 40, 40, TFT_RED);
  tft.fillRect(tft.width() - 40, 0, 40, 40, TFT_GREEN);
  tft.fillRect(0, tft.height() - 40, 40, 40, TFT_BLUE);
  tft.fillRect(tft.width() - 40, tft.height() - 40, 40, 40, TFT_YELLOW);
  tft.drawRect(0, 0, tft.width(), tft.height(), TFT_WHITE);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("ST7789", tft.width() / 2, tft.height() / 2 - 20, 4);

  char size[32];
  snprintf(size, sizeof(size), "%dx%d", tft.width(), tft.height());
  tft.drawString(size, tft.width() / 2, tft.height() / 2 + 10, 4);
  delay(4000);
}
