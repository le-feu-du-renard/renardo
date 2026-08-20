#include "InputHandler.h"
#include "config.h"
#include "Logger.h"

InputHandler::InputHandler()
    : start_button_(BTN_START_PIN, "start"),
      stop_button_(BTN_STOP_PIN, "stop"),
      encoder_(ENCODER_A_PIN, ENCODER_B_PIN, ENCODER_SW_PIN, ENCODER_REVERSED) {}

void InputHandler::Begin()
{
  start_button_.Begin();
  stop_button_.Begin();
  encoder_.Begin();

  Logger::Info("InputHandler: initialized");
}

void InputHandler::Update()
{
  uint32_t now = millis();

  encoder_.Update();
  start_button_.Update(now);
  stop_button_.Update(now);
}
