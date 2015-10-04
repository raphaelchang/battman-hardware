#include <SPI.h>
#include "Pins.h"
#include "LTC68031.h"
#include "LTC2480.h"
//#define BALANCE

LTC6803* ltc6803;
LTC2480* ltc2480;

double cell_voltages[12];
float current;
boolean balance[12];
IntervalTimer timer;
int precharge_timer = 0;
int precharge_time = 0;
int charge_ticks = 0;
boolean balancing = false;
boolean power_button_released = false;
boolean power_button_pressed = false;

double balance_low_threshold = 3300;
double balance_high_threshold = 3500;
double balance_delta_threshold = 15;
double balance_goal = 0;

void setup() {
  pinMode(power_button, INPUT_PULLUP);
  pinMode(charger_detect, INPUT);
  if (digitalRead(power_button) == LOW || digitalRead(charger_detect) == HIGH)
  {
    pinMode(power_switch, OUTPUT);
    digitalWrite(power_switch, HIGH);
    Serial.begin(115200);
    digitalWrite(ltc2480_cs, LOW);
    pinMode(ltc2480_cs, INPUT);
    pinMode(ltc6803_csbi, OUTPUT);
    pinMode(sdi, OUTPUT);
    pinMode(sdo, INPUT);
    pinMode(scki, OUTPUT);
    pinMode(load_switch, OUTPUT);
    pinMode(power_button, INPUT);
    pinMode(power_led, OUTPUT);
    pinMode(precharge_switch, OUTPUT);
    pinMode(charge_stat1, INPUT_PULLUP);
    pinMode(charge_stat2, INPUT_PULLUP);
    pinMode(charge_shdn, OUTPUT);
    digitalWrite(charge_shdn, LOW);
    LTC6803* ltc6803 = new LTC6803(ltc6803_csbi);
    LTC2480* ltc2480 = new LTC2480();
    CORE_PIN13_CONFIG = PORT_PCR_MUX(0);
    CORE_PIN14_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);
    //*portConfigRegister(scki) |= PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_ODE;
    //*portConfigRegister(sdi) |= PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_ODE;
  //  *portModeRegister(ltc2480_cs) = 1;
  //  *portConfigRegister(ltc2480_cs) &= ~PORT_PCR_MUX_MASK;
  //  *portConfigRegister(ltc2480_cs) |= (PORT_PCR_SRE | PORT_PCR_DSE | PORT_PCR_MUX(1));
  //  *portConfigRegister(ltc2480_cs) |= PORT_PCR_ODE;
  //  digitalWrite(ltc2480_cs, HIGH);
    *portConfigRegister(sdo) |= (PORT_PCR_PE | PORT_PCR_PS);
    analogReadResolution(16);
    analogReadAveraging(1000);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    power();
    timer.begin(main_loop, 150000);
    for (int i = 0; i < 12; i++)
      balance[i] = false;
  }
}

void power()
{
  digitalWrite(power_switch, HIGH);
  delay(10);
  digitalWrite(power_led, HIGH);
  digitalWrite(load_switch, HIGH);
}

void read_voltages()
{
  pinMode(sdi, OUTPUT);
  pinMode(sdo, INPUT);
  pinMode(scki, OUTPUT);
  SPI.begin();
  CORE_PIN13_CONFIG = PORT_PCR_MUX(0);
  CORE_PIN14_CONFIG = PORT_PCR_DSE | PORT_PCR_MUX(2);
  SPI.setClockDivider(SPI_CLOCK_DIV64);
  SPI.setDataMode(SPI_MODE3);
  *portConfigRegister(sdo) |= (PORT_PCR_PE | PORT_PCR_PS);
  uint8_t config[1][6];
  config[0][0] = 0b01100001;
  config[0][1] = 0b00000000;
  config[0][2] = 0b00000000;
  config[0][3] = 0b11111100;
  config[0][4] = 0b00000000;
  config[0][5] = 0b00000000;
  for (int i = 1; i <= 12; i++)
  {
    if (balance[i - 1])
    {
      if (i <= 8)
      {
        config[0][1] |= 1 << (i - 1);
      }
      else
      {
        config[0][2] |= 1 << (i - 9);
      }
      Serial.print(i);
      Serial.print(" ");
    }
  }
  Serial.println();
  ltc6803->LTC6803_wrcfg(1, config);
  ltc6803->LTC6803_stcvad();
  delay(13);
  uint16_t cell_codes[1][12];
  ltc6803->LTC6803_rdcv(1, cell_codes);
  double sum = 0;
  for(int i = 0; i < 12; i++)
  {
    cell_voltages[i] = cell_codes[0][i] * 1.5;
    Serial.println(cell_voltages[i]);
    sum += cell_voltages[i];
  }
  Serial.println(sum);
  SPI.end();
}

void measure_current()
{
    float vdiff = ltc2480->Read(0);
    //Serial.println(vdiff, 16);
    float current = (vdiff / 0.0003) / 50;
    Serial.println(current, 16);
}

void main_loop()
{
  digitalWrite(power_switch, HIGH);
  if (digitalRead(power_button) == HIGH)
    power_button_released = true;
  read_voltages();
  measure_current();
  if (digitalRead(charger_detect) == HIGH)
  {
    charge_ticks++;
    boolean stat1 = digitalRead(charge_stat1);
    boolean stat2 = digitalRead(charge_stat2);
    Serial.println(stat1);
    Serial.println(stat2);
    digitalWrite(load_switch, LOW);
    if (charge_ticks % 6 < 3 || (stat1 && stat2 && charge_ticks % 6 < 5))
      digitalWrite(power_led, HIGH);
    else
      digitalWrite(power_led, LOW);
//    if (charge_ticks <= 50 && !balancing)
//    {
    if (!balancing)
    {
          digitalWrite(charge_shdn, HIGH);
          digitalWrite(LED_BUILTIN, HIGH);
    }
    else
    {
          digitalWrite(charge_shdn, LOW);
          digitalWrite(LED_BUILTIN, LOW);
    }
//    }
//    else
//    {
//      if (balancing && charge_ticks > 50)
//      {
//        for (int i = 0; i < 12; i++)
//          balance[i] = false;
//      }
//      digitalWrite(charge_shdn, LOW);
//      digitalWrite(LED_BUILTIN, LOW);
//    }
//    if (charge_ticks >= 60)
//    {
//      charge_ticks = 0;
      int highest = 0;
      int lowest = 0;
      for (int i = 0; i < 12; i++)
      {
        if (cell_voltages[i] > cell_voltages[highest])
        {
          highest = i;
        }
        if (cell_voltages[i] < cell_voltages[lowest])
        {
          lowest = i;
        }
      }
#ifdef BALANCE
      if (cell_voltages[highest] >= balance_high_threshold || balancing)
      {
        if (cell_voltages[highest] >= balance_high_threshold && !balancing) // First time
        {
          balance_goal = cell_voltages[lowest];
          for (int i = 0; i < 12; i++)
          {
            balance[i] = false;
            if (cell_voltages[i] - balance_goal >= balance_delta_threshold)
            {
              if (cell_voltages[i] > balance_low_threshold)
              {
                balancing = true;
                balance[i] = true;
              }
            }
          }
        }
        else
        {
          balancing = false;
          for (int i = 0; i < 12; i++)
          {
            if (balance[i])
            {
              balance[i] = false;
              if (cell_voltages[i] - balance_goal >= balance_delta_threshold)
              {
                if (cell_voltages[i] > balance_low_threshold)
                {
                  balancing = true;
                  balance[i] = true;
                }
              }
            }
          }
        }
      }
#endif
//    }
  }
  else
  {
    if (digitalRead(power_button) == LOW && power_button_released)
    {
      power_button_pressed = true;
    }
    if (digitalRead(power_button) == HIGH && power_button_pressed)
    {
      digitalWrite(load_switch, LOW);
      digitalWrite(charge_shdn, LOW);
      digitalWrite(power_led, LOW);
      delay(200);
      digitalWrite(power_switch, LOW);
      delay(600);
    }
    charge_ticks = 0;
    digitalWrite(charge_shdn, LOW);
    for (int i = 0; i < 12; i++)
      balance[i] = false;
    int highest = 0;
    int lowest = 0;
    for (int i  = 0; i < 12; i++)
    {
      if (cell_voltages[i] > cell_voltages[highest])
      {
        highest = i;
      }
      if (cell_voltages[i] < cell_voltages[lowest])
      {
        lowest = i;
      }
    }
    if (cell_voltages[lowest] <= 2800) // Faults
    {
      digitalWrite(load_switch, LOW);
      digitalWrite(power_led, LOW);
    }
    else
    {
      digitalWrite(load_switch, HIGH);
      digitalWrite(power_led, HIGH);
    }
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

void loop() {
}


