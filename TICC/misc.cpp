// misc.cpp -- miscellaneous TICC functions

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016-2025
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

/*
 * Printing rationale (summary):
 * - Timestamps and intervals are represented as SplitTime: 32-bit seconds and
 *   fractional picoseconds split into two 6-digit fields (frac_hi, frac_lo).
 *   This avoids 64-bit operations in formatting and simple math.
 * - We only assemble 64-bit ps values when legacy interfaces require it.
 */


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

// ------------------------------------------------------------
// Lightweight 32-bit printing helpers (no sprintf/printf)
// ------------------------------------------------------------
static const uint32_t POW10[10] = {
  1UL, 10UL, 100UL, 1000UL, 10000UL,
  100000UL, 1000000UL, 10000000UL, 100000000UL, 1000000000UL
};

static inline uint8_t numDigitsU32(uint32_t v) {
  if (v >= 1000000000UL) return 10;
  if (v >= 100000000UL) return 9;
  if (v >= 10000000UL) return 8;
  if (v >= 1000000UL) return 7;
  if (v >= 100000UL) return 6;
  if (v >= 10000UL) return 5;
  if (v >= 1000UL) return 4;
  if (v >= 100UL) return 3;
  if (v >= 10UL) return 2;
  return 1;
}

static inline void serialPrintU32Padded(uint32_t v, uint8_t width) {
  uint8_t d = numDigitsU32(v);
  for (uint8_t i = d; i < width; ++i) Serial.write('0');
  Serial.print(v);
}

// Print leftmost 'digits' of a zero-padded 6-digit number (n in [0,999999])
static inline void serialPrintFirstDigitsOf6(uint32_t n, uint8_t digits) {
  if (digits == 0) return;
  uint32_t divisor = POW10[6 - digits];
  uint32_t m = n / divisor;               // keep upper 'digits'
  serialPrintU32Padded(m, digits);
}

static inline void serialPrintFrac(uint32_t frac_hi, uint32_t frac_lo, uint8_t places) {
  if (places <= 6) {
    serialPrintFirstDigitsOf6(frac_hi, places);
  } else {
    serialPrintFirstDigitsOf6(frac_hi, 6);
    serialPrintFirstDigitsOf6(frac_lo, (uint8_t)(places - 6));
  }
}

static inline void serialPrintSecondsWrapped(int32_t sec, int32_t wrap) {
  if (wrap <= 0) {
    Serial.print(sec);
    return;
  }
  bool neg = (sec < 0);
  uint32_t s = (uint32_t)(neg ? -sec : sec);
  uint32_t mod = POW10[(uint8_t)wrap];
  uint32_t tail = s % mod;
  if (neg) Serial.write('-');
  serialPrintU32Padded(tail, (uint8_t)wrap);
}

static inline char * bufAppendChar(char *p, const char *end, char c) {
  if (p < end) *p++ = c;
  return p;
}
static inline char * bufAppendU32Padded(char *p, const char *end, uint32_t v, uint8_t width) {
  uint8_t d = numDigitsU32(v);
  for (uint8_t i = d; i < width; ++i) { if (p < end) *p++ = '0'; }
  // print value
  char tmp[10];
  uint8_t n = 0;
  do { tmp[n++] = (char)('0' + (v % 10)); v /= 10; } while (v);
  while (n--) { if (p < end) *p++ = tmp[n]; }
  return p;
}
static inline char * bufAppendSecondsWrapped(char *p, const char *end, int32_t sec, int32_t wrap) {
  if (wrap <= 0) {
    // simple itoa
    if (sec < 0) { if (p < end) *p++ = '-'; sec = -sec; }
    char tmp[12]; uint8_t n=0; uint32_t s=(uint32_t)sec; do { tmp[n++] = (char)('0'+(s%10)); s/=10; } while (s);
    while (n--) { if (p < end) *p++ = tmp[n]; }
    return p;
  }
  bool neg = (sec < 0);
  uint32_t s = (uint32_t)(neg ? -sec : sec);
  uint32_t mod = POW10[(uint8_t)wrap];
  uint32_t tail = s % mod;
  if (neg) { if (p < end) *p++ = '-'; }
  return bufAppendU32Padded(p, end, tail, (uint8_t)wrap);
}
static inline char * bufAppendFirstDigitsOf6(char *p, const char *end, uint32_t n, uint8_t digits) {
  if (digits == 0) return p;
  uint32_t divisor = POW10[6 - digits];
  uint32_t m = n / divisor;
  return bufAppendU32Padded(p, end, m, digits);
}
static inline char * bufAppendFrac(char *p, const char *end, uint32_t frac_hi, uint32_t frac_lo, uint8_t places) {
  if (places <= 6) {
    return bufAppendFirstDigitsOf6(p, end, frac_hi, places);
  } else {
    p = bufAppendFirstDigitsOf6(p, end, frac_hi, 6);
    return bufAppendFirstDigitsOf6(p, end, frac_lo, (uint8_t)(places - 6));
  }
}
size_t formatTimestampSplitTo(char *buf, size_t cap, const SplitTime &t, int places, int32_t wrap) {
  char *p = buf; const char *end = buf + cap;
  int32_t sec = t.sec;
  p = bufAppendSecondsWrapped(p, end, sec, wrap);
  p = bufAppendChar(p, end, '.');
  p = bufAppendFrac(p, end, t.frac_hi, t.frac_lo, (uint8_t)places);
  if (p < end) *p = '\0';
  return (size_t)(p - buf);
}
size_t formatSignedSplitTo(char *buf, size_t cap, const SplitTime &t, int places) {
  char *p = buf; const char *end = buf + cap;
  int32_t sec = t.sec;
  uint32_t hi = t.frac_hi, lo = t.frac_lo;
  if (sec < 0) {
    if (p < end) *p++ = '-';
    if (hi != 0 || lo != 0) {
      sec = -(sec + 1);
      if (lo == 0) { lo = 0; hi = 1000000UL - hi; }
      else { lo = 1000000UL - lo; hi = (hi == 0) ? 999999UL : (1000000UL - hi); }
    } else {
      sec = -sec;
    }
  }
  // seconds
  {
    char tmp[12]; uint8_t n=0; uint32_t s=(uint32_t)sec; do { tmp[n++] = (char)('0'+(s%10)); s/=10; } while (s);
    while (n--) { if (p < end) *p++ = tmp[n]; }
  }
  p = bufAppendChar(p, end, '.');
  p = bufAppendFrac(p, end, hi, lo, (uint8_t)places);
  if (p < end) *p = '\0';
  return (size_t)(p - buf);
}


void print_timestamp_sec_frac(int64_t sec, int64_t frac_ps, int places, int32_t wrap) {
  bool neg = (sec < 0);
  if (neg) {
    Serial.write('-');
    sec = -sec;
  }
  // Integer seconds (optionally wrapped)
  serialPrintSecondsWrapped((int32_t)sec, wrap);
  Serial.write('.');
  // Convert frac_ps to two chunks
  uint32_t frac_hi = (uint32_t)(frac_ps / 1000000LL);
  uint32_t frac_lo = (uint32_t)(frac_ps % 1000000LL);
  serialPrintFrac(frac_hi, frac_lo, (uint8_t)places);
}

void print_signed_sec_frac(int64_t sec, int64_t frac_ps, int places) {
  // Signed formatting with borrow handling
  bool neg = (sec < 0);
  if (neg) {
    Serial.write('-');
    if (frac_ps > 0) {
      sec = -(sec + 1);
      frac_ps = PS_PER_SEC - frac_ps;
    } else {
      sec = -sec;
    }
  }
  Serial.print((int32_t)sec);
  Serial.write('.');
  uint32_t frac_hi = (uint32_t)(frac_ps / 1000000LL);
  uint32_t frac_lo = (uint32_t)(frac_ps % 1000000LL);
  serialPrintFrac(frac_hi, frac_lo, (uint8_t)places);
}

// SplitTime helpers

void normalizeSplit(struct SplitTime *t) {
  if (!t) return;
  // Bring frac_lo, frac_hi into range [0, 1e6)
  if ((int32_t)t->frac_lo < 0) {
    // Borrow from frac_hi
    uint32_t borrow = ((uint32_t)(-(int32_t)t->frac_lo) + 999999UL) / 1000000UL;
    if (borrow == 0) borrow = 1;
    if (t->frac_hi >= borrow) {
      t->frac_hi -= borrow;
      t->frac_lo += borrow * 1000000UL;
    } else {
      uint32_t need = borrow - t->frac_hi;
      t->frac_hi = 1000000UL - need; // after borrowing 1 second
      t->sec -= 1;
      t->frac_lo += borrow * 1000000UL;
    }
  } else if (t->frac_lo >= 1000000UL) {
    uint32_t carry = t->frac_lo / 1000000UL;
    t->frac_lo -= carry * 1000000UL;
    t->frac_hi += carry;
  }

  if ((int32_t)t->frac_hi < 0) {
    // borrow from seconds
    t->frac_hi += 1000000UL;
    t->sec -= 1;
  } else if (t->frac_hi >= 1000000UL) {
    uint32_t carry = t->frac_hi / 1000000UL;
    t->frac_hi -= carry * 1000000UL;
    t->sec += (int32_t)carry;
  }
}

SplitTime diffSplit(const SplitTime &b, const SplitTime &a) {
  SplitTime r;
  r.sec = (int32_t)(b.sec - a.sec);
  int32_t lo = (int32_t)b.frac_lo - (int32_t)a.frac_lo;
  int32_t hi = (int32_t)b.frac_hi - (int32_t)a.frac_hi;
  if (lo < 0) { lo += 1000000; hi -= 1; }
  if (hi < 0) { hi += 1000000; r.sec -= 1; }
  r.frac_lo = (uint32_t)lo;
  r.frac_hi = (uint32_t)hi;
  return r;
}

SplitTime absDeltaSplit(const SplitTime &b, const SplitTime &a) {
  SplitTime d = diffSplit(b, a);
  // If negative, convert to magnitude
  if (d.sec < 0) {
    if (d.frac_hi != 0 || d.frac_lo != 0) {
      d.sec = -(d.sec + 1);
      d.frac_hi = 1000000UL - d.frac_hi - (d.frac_lo ? 1UL : 0UL);
      d.frac_lo = (d.frac_lo ? 1000000UL - d.frac_lo : 0UL);
    } else {
      d.sec = -d.sec;
    }
  }
  return d;
}

void printTimestampSplit(const SplitTime &t, int places, int32_t wrap) {
  // integer seconds
  int32_t sec = t.sec;
  bool neg = (sec < 0);
  if (neg) sec = -sec;

  if (neg) Serial.write('-');
  serialPrintSecondsWrapped(sec, wrap);
  Serial.write('.');

  // fractional
  serialPrintFrac(t.frac_hi, t.frac_lo, (uint8_t)places);
}

void printSignedSplit(const SplitTime &t, int places) {
  int32_t sec = t.sec;
  uint32_t hi = t.frac_hi;
  uint32_t lo = t.frac_lo;

  if (sec < 0) {
    Serial.write('-');
    if (hi != 0 || lo != 0) {
      sec = -(sec + 1);
      if (lo == 0) {
        lo = 0;
        hi = 1000000UL - hi;
      } else {
        lo = 1000000UL - lo;
        hi = (hi == 0) ? 999999UL : (1000000UL - hi);
      }
    } else {
      sec = -sec;
    }
  }

  Serial.print(sec);
  Serial.write('.');
  serialPrintFrac(hi, lo, (uint8_t)places);
}

// Append CRLF and write out buffer in a single Serial.write().
// Assumes buf has at least 64 bytes capacity; caps total length at 64.
void writeln64(char *buf, size_t n) {
  if (!buf) return;
  if (n > 62) n = 62; // leave space for CRLF
  buf[n++] = '\r';
  buf[n++] = '\n';
  Serial.write((const uint8_t*)buf, n);
}
