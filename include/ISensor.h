#ifndef ISENSOR_H
#define ISENSOR_H

class ISensor
{
public:
  virtual bool Begin() = 0;
  virtual bool Read(float &temperature, float &humidity) = 0;
  virtual ~ISensor() = default;
};

#endif // ISENSOR_H
