// Routine to calculate TDC7200 results using integer math.  Many thanks
// to "BigBobby" of the Programming Questions board at forum.arduino.cc
// Initial version -- 27 March 2016

#include <stdint.h>               // define unint16_t, uint32_t

#define PS_PER_SEC        (1e12)  // ps/s
#define CLOCK_FREQ        (1e8)   // Hz
#define CLOCK_PERIOD_PS   (uint32_t)(PS_PER_SEC/CLOCK_FREQ)  // ps
#define CALIBRATION2_PERIODS 10   // Can only be 2, 10, 20, or 40.

uint16_t CALIBRATION2 = 1;
uint16_t CALIBRATION1 = 2;
uint16_t TIME1 = 3;
uint16_t TIME2 = 4;
uint32_t CLOCK_COUNT1 = 5;

void dummy_function_name()  {
    // Calculation from 8.4.2.2.1 of datasheet.

    // These registers are 23 bits, but in example never
    // got larger than 16bit when CALIBRATION_PERIODS = 10.
    CALIBRATION2 = 23133;
    CALIBRATION1 = 2315;

    // These registers are 23 bits, but in example never
    // got larger than 16bit.
    // If I understand their example correctly, 
    // they should never be larger than that in your 
    // application either.
    TIME1 = 2147;
    TIME2 = 201;

    // This register is 23 bits, and but in example neveri
    //  got larger than 16bit.
    // If I understand their example correctly,
    // I think it will be >16 bits in your case.
    // CLOCK_COUNT1 = 3814; // Datasheet said 39.855us
    // results in 3814 count. If 39.855us results in 3814,
    // then 1ms should result in proportionally more.
    CLOCK_COUNT1 = (uint32_t)(1.0F * 3814*1000000/39855 + 0.5);

    uint32_t calc_time;
    uint16_t tempu16;
    uint32_t tempu32;

    // Perform calculation while measuring the calculation time.
    calc_time = micros();
    // since TIME1 will always be > TIME2, this is still 16 bit.
    tempu16 = (TIME1 - TIME2); 
    // After multiplying by the constants, you will now be a 32 bit number.
    tempu32 = tempu16 * (CLOCK_PERIOD_PS * (CALIBRATION2_PERIODS-1));
    // This division sort of sucks, but since I assume these must be 
    // variables there's no way around it.
    tempu32 = (tempu32 + ((CALIBRATION2 - CALIBRATION1 + 1) >> 1)) /
      (CALIBRATION2 - CALIBRATION1);
    // Add in another 32bit variable.  Given the limitations on
    // inputs, these two 32 bits still won't overflow.
    tempu32 += CLOCK_COUNT1 * CLOCK_PERIOD_PS;
    // Calculate the time it took for function.
    calc_time = micros() - calc_time;

    LT_printf(F("calculated %lu in %lu us\r\n"), tempu32, calc_time);
  }

