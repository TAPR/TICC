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

/* in issue 28 (https://github.com/TAPR/TICC/issues/28)
 * robots suggests the following change because the original
 * can procude incorrect results in some cases
 *
 * sec =  ABS(x / 100 000 000 000 0);
 * frac = ABS(x % 100 000 000 000 0);
 * // break fractional part of seconds into two 6 digit numbers 
 * frach = frac / 100 000 0;
 * fracl = frac % 100 000 0;
 *
 * to replace:
 * 
 * sec = abs(x / 100 000 000 000 0);
 * secx = sec * 1000 000 000 000;
 * frac = x - secx;
 * // break fractional part of seconds into two 6 digit numbers
 * frach = frac / 100 000 0;
 * fracx = frach * 100 000 0;
 * fracl = frac - fracx;
 * 
 * 31 October 2022 -- that change has been made in all the print functions below
 */

/*
 * Printing rationale (summary):
 * - Timestamps and intervals are represented as int64_t picoseconds elsewhere to safely
 *   handle subtraction and potential negative values; these helpers accept signed values
 *   where needed and format without floating point.
 * - We split x into seconds and fractional picoseconds via division/modulo by 1e12 to
 *   avoid large intermediates and rounding issues. The fractional component is then
 *   split into two 6-digit fields for efficient zero-padded printing and truncated to
 *   the requested number of places.
 * - For unsigned inputs (purely non-negative values), use print_unsigned_picos_as_seconds;
 *   for possibly negative values, use print_signed_picos_as_seconds or print_timestamp.
 */

void print_unsigned_picos_as_seconds (uint64_t x, int places) {
  uint64_t sec, frac, frach, fracl;    
   char str[24],str1[8],str2[8];
  
  sec = abs(x / 1000000000000);
  frac = abs(x % 1000000000000ULL);

  // break fractional part of seconds into two 6 digit numbers

  frach = frac / 1000000ULL;
  fracl = frac % 1000000ULL;

  sprintf(str,"%lu.",sec);
  Serial.print(str);
  sprintf(str1, "%06lu", frach);
  sprintf(str2, "%06lu", fracl);
  sprintf(str, "%s%s", str1, str2);
  str[places] = '\0';  // chop off to PLACES resolution   
  Serial.print(str);
  
} 

void print_signed_picos_as_seconds (int64_t x, int places) {
  int64_t sec, frac, frach, fracl;    
  char str[24],str1[8],str2[8];
  
  sec = abs(x / 1000000000000);  // hopefully avoid double negative sign.  Thanks, Curt!
  frac = abs(x % 1000000000000LL);
  
  // break fractional part of seconds into two 6 digit numbers

  frach = frac / 1000000LL;
  fracl = frac % 1000000LL;

  if (x < 0) {
    Serial.print("-");
  }
  
  sprintf(str,"%ld.",sec);
  Serial.print(str);
 
  sprintf(str1, "%06ld", frach);
  sprintf(str2, "%06ld", fracl);
  sprintf(str, "%s%s", str1, str2);
  str[places] = '\0';  // chop off to PLACES resolution 
  Serial.print(str);
}

void print_timestamp(int64_t x, int places, int32_t wrap) {
// places is the number of places to the right of the decimal point
// wrap is the number of integer digits to output
  
  int64_t sec, frac, frach, fracl;   
  char str[24],str1[24],str2[24],trunc_sec[24];

  // deal with integer part and wrap value if specified
  sec = abs(x / 1000000000000);   // hopefully avoid double negative sign.  Thanks, Curt!
  if (wrap == 0) {
    sprintf(str,"%ld.",sec);
    Serial.print(str);
  } else {
    sprintf(str,"%0.10ld.",sec);    // pad with zeros to 10 characters long
    Serial.print(&str[10 - wrap]); // now print keeping only lowest order integer places
  }
 
  // now handle fractional part
  frac = abs(x % 1000000000000LL);
  
  // break fractional into two 6 digit numbers 
  frach = frac / 1000000LL;
  fracl = frac % 1000000LL;

  if (x < 0) {
    Serial.print("-");
  }
  
  sprintf(str1, "%06ld", frach);
  sprintf(str2, "%06ld", fracl);
  sprintf(str, "%s%s", str1, str2);
  str[places] = '\0';  // chop off to PLACES resolution 
  Serial.print(str);  ;
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
