// misc.cpp -- miscellaneous TICC functions

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016-2020
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

#include <stdint.h>           // define unint16_t, uint32_t
#include <SPI.h>              // SPI support
#include <string.h>

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

// legacy printing helpers removed after migration to SplitTime printers

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

void print_timestamp_sec_frac(int64_t sec, int64_t frac_ps, int places, int32_t wrap) {
  // sec: whole seconds, frac_ps: [0, 1e12) picoseconds
  // places: digits to the right of the decimal (0..12)
  // wrap: integer digits to display (0 means no wrapping)
  char str[24], str1[24], str2[24];

  if (sec < 0) {
    Serial.print("-");
    sec = -sec;
  }

  // convert sec to string manually for 64-bit safety
  char ibuf[32];
  char *p = &ibuf[31];
  *p = '\0';
  if (sec == 0) { *(--p) = '0'; }
  while (sec > 0) { *(--p) = '0' + (char)(sec % 10); sec /= 10; }

  int len = (int)strlen(p);
  if (wrap == 0) {
    Serial.print(p);
    Serial.print('.');
  } else {
    if (len < wrap) {
      for (int i = 0; i < wrap - len; ++i) Serial.print('0');
      Serial.print(p);
      Serial.print('.');
    } else {
      Serial.print(p + (len - wrap));
      Serial.print('.');
    }
  }

  int64_t frach = frac_ps / 1000000LL;   // upper 6 digits
  int64_t fracl = frac_ps % 1000000LL;   // lower 6 digits

  sprintf(str1, "%06ld", (long)frach);
  sprintf(str2, "%06ld", (long)fracl);
  sprintf(str, "%s%s", str1, str2);
  str[places] = '\0';
  Serial.print(str);
}

void print_signed_sec_frac(int64_t sec, int64_t frac_ps, int places) {
  // sec may be negative; frac_ps must be in [0, 1e12)
  char str[24], str1[8], str2[8];

  bool neg = (sec < 0);
  if (neg) {
    Serial.print("-");
    if (frac_ps > 0) {
      // Convert from borrowed form (e.g., -1, PS-δ) to standard (-0, δ)
      sec = -(sec + 1);           // e.g., -1 -> 0
      frac_ps = PS_PER_SEC - frac_ps;
    } else {
      // Exact integer seconds negative
      sec = -sec;
    }
  }

  // integer part
  sprintf(str, "%ld.", (long)sec);
  Serial.print(str);

  int64_t frach = frac_ps / 1000000LL;
  int64_t fracl = frac_ps % 1000000LL;

  sprintf(str1, "%06ld", (long)frach);
  sprintf(str2, "%06ld", (long)fracl);
  sprintf(str, "%s%s", str1, str2);
  str[places] = '\0';
  Serial.print(str);
}

// SplitTime helpers

void normalizeSplit(struct SplitTime *t) {
  if (!t) return;
  // bring frac_ps into [0, PS_PER_SEC)
  if (t->frac_ps >= PS_PER_SEC) {
    int64_t carry = t->frac_ps / PS_PER_SEC;
    t->sec += carry;
    t->frac_ps -= carry * PS_PER_SEC;
  } else if (t->frac_ps < 0) {
    int64_t borrow = (-t->frac_ps + PS_PER_SEC - 1) / PS_PER_SEC;
    t->sec -= borrow;
    t->frac_ps += borrow * PS_PER_SEC;
  }
}

SplitTime diffSplit(const SplitTime &b, const SplitTime &a) {
  SplitTime r;
  r.sec = b.sec - a.sec;
  r.frac_ps = b.frac_ps - a.frac_ps;
  if (r.frac_ps < 0) { r.frac_ps += PS_PER_SEC; r.sec -= 1; }
  return r;
}

SplitTime absDeltaSplit(const SplitTime &b, const SplitTime &a) {
  SplitTime d = diffSplit(b, a);
  if (d.sec < 0 || (d.sec == 0 && d.frac_ps < 0)) {
    // Negate: convert signed split to magnitude
    if (d.frac_ps > 0) {
      d.frac_ps = PS_PER_SEC - d.frac_ps;
      d.sec = -(d.sec + 1);
    } else {
      d.sec = -d.sec;
    }
  }
  return d;
}

void printTimestampSplit(const SplitTime &t, int places, int32_t wrap) {
  print_timestamp_sec_frac(t.sec, t.frac_ps, places, wrap);
}

void printSignedSplit(const SplitTime &t, int places) {
  // Use sign of seconds and fractional handled inside print_signed_sec_frac
  print_signed_sec_frac(t.sec, t.frac_ps, places);
}
