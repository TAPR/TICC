// misc.cpp -- miscellaneous TICC functions

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016-2020
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

#include <stdint.h>           // define unint16_t, uint32_t
#include <SPI.h>              // SPI support

#include "misc.h"             // random functions
#include "board.h"            // Arduino pin definitions
#include "config.h"           // config and eeprom
#include "tdc7200.h"          // TDC registers and structures

void print_unsigned_picos_as_seconds (uint64_t x, int places) {
  uint64_t sec, secx, frac, frach, fracx, fracl;    
   char str[24],str1[8],str2[8];
  
  sec = abs(x / 1000000000000);
  secx = sec * 1000000000000;
  frac = x - secx;

  // break fractional part of seconds into two 6 digit numbers

  frach = frac / 1000000;
  fracx = frach * 1000000;
  fracl = frac - fracx;

  sprintf(str,"%lu.",sec);
  Serial.print(str);
  sprintf(str1, "%6lu", frach);
  sprintf(str2, "%6lu", fracl);
  sprintf(str, "%s%s", str1, str2);
  str[places] = '\0';  // chop off to PLACES resolution   
  Serial.print(str);
  
} 

void print_signed_picos_as_seconds (int64_t x, int places) {
  int64_t sec, secx, frac, frach, fracx, fracl;    
  char str[24],str1[8],str2[8];
  
  sec = abs(x / 1000000000000);  // hopefully avoid double negative sign.  Thanks, Curt!
  secx = sec * 1000000000000;
  frac = abs(x - secx);
  
  // break fractional part of seconds into two 6 digit numbers

  frach = frac / 1000000;
  fracx = frach * 1000000;
  fracl = frac - fracx;

  if (x < 0) {
    Serial.print("-");
  }
  
  sprintf(str,"%ld.",sec);
  Serial.print(str);
 
  sprintf(str1, "%6ld", frach);
  sprintf(str2, "%6ld", fracl);
  sprintf(str, "%s%s", str1, str2);
  str[places] = '\0';  // chop off to PLACES resolution 
  Serial.print(str);
}

void print_int64(int64_t num ) {
  const static char toAscii[] = "0123456789ABCDEF";
  int base = 10;
  
  char buffer[65];            //because you might be doing binary
  char* p = &buffer[64];      //this pointer writes into the buffer, starting at the END

  // zero to terminate a C type string
  *p = 0;

  // do digits until the number reaches zero
  do {
    // working on the least significant digit
    //put an ASCII digit at the front of the string
    *(--p) = toAscii[(int)(num % base)];

    //knock the least significant digit off the number
    num = num / base;
    } while (num != 0);

    //print the whole string
    Serial.print(p);
}
