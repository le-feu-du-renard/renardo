#include "StatusLed.h"
#include "Logger.h"

StatusLed::StatusLed(uint8_t green_pin, uint8_t red_pin)
    : green_pin_(green_pin),
      red_pin_(red_pin),
      green_on_(false),
      red_on_(false) {}

void StatusLed::Begin()
{
  digitalWrite(green_pin_, LOW);
  digitalWrite(red_pin_, LOW);
  pinMode(green_pin_, OUTPUT);
  pinMode(red_pin_, OUTPUT);
  digitalWrite(green_pin_, LOW); // re-assert now that the pins actually drive
  digitalWrite(red_pin_, LOW);

  green_on_ = false;
  red_on_   = false;

  Logger::Info("StatusLed: green GP%d, red GP%d, active HIGH", green_pin_, red_pin_);
}

void StatusLed::Apply(const LedPattern &pattern)
{
  if (pattern.green != green_on_)
  {
    green_on_ = pattern.green;
    digitalWrite(green_pin_, green_on_ ? HIGH : LOW);
  }
  if (pattern.red != red_on_)
  {
    red_on_ = pattern.red;
    digitalWrite(red_pin_, red_on_ ? HIGH : LOW);
  }
}
