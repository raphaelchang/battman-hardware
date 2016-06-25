#ifndef LTC2480_H
#define LTC2480_H
#include <Arduino.h>

class LTC2480
{
public:
float Read(uint8_t command);

private:
int transferBit(int bit);
};

#endif

