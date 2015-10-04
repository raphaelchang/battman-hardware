#include "LTC2480.h"
#include "Pins.h"

float LTC2480::Read(uint8_t command)
{
  int result = 0;
  pinMode(scki, OUTPUT);
  digitalWrite(scki, LOW);

  pinMode(sdi, OUTPUT);
  digitalWrite(sdi, LOW);

  pinMode(sdo, INPUT_PULLUP);
  delayMicroseconds(1);
  
  pinMode(ltc2480_cs, OUTPUT);
  digitalWrite(ltc2480_cs, LOW);
  delayMicroseconds(1);
  
  while(digitalRead(sdo) == HIGH);
  delayMicroseconds(1);
  for (int i = 0; i < 24; i++)
  {
    int bit = transferBit(0);
    result = (result << 1) | bit;
  }
  pinMode(scki, OUTPUT);
  digitalWrite(scki, LOW);
  //result = result << 8;
  result &= 0xFFFFFFF0;
  result -= 0x00200000;
  float voltage = (float) result;
  voltage /= 1024576.0;
  voltage *= 1.65;
  pinMode(ltc2480_cs, INPUT);
  return voltage;
}

int LTC2480::transferBit(int bit)
{
  pinMode(scki, OUTPUT);
  digitalWrite(scki, LOW);
  delayMicroseconds(1);
  pinMode(scki, INPUT);
  int result = digitalRead(sdo);
  delayMicroseconds(1);
  return result;
}


