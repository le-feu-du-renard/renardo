#ifndef AIR_DAMPER_H
#define AIR_DAMPER_H

#include <Arduino.h>

// Binary air damper state tracking.
// Hardware output is written by UpdateOutputs() in main.cpp.

class AirDamper
{
public:
  AirDamper();

  void Open();
  void Close();
  bool IsOpen() const { return is_open_; }

private:
  bool is_open_;
};

#endif // AIR_DAMPER_H
