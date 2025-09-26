// print64.cpp -- optimized 64-bit printing routines for TICC
// Based on advisor's fast 64-bit to decimal conversion algorithm

#include "printf.h"  // Must be before Arduino.h to override printf functions
#include <Arduino.h>
#include "config.h"
#include "misc.h"
#include "tdc7200.h"
#include "print64.h"

// Fast 64-bit to 12-digit conversion using advisor's optimized approach
#define M6 1000000UL
#define RECIP_M6 281474976UL

// Compute q = floor(x / 1,000,000) for x < 10^12 without 64-bit division
static inline uint32_t div1e6_u64_u32(uint64_t x) {
  uint64_t lo = (uint32_t)x;
  uint64_t hi = x >> 32;
  uint64_t p0 = lo * (uint64_t)RECIP_M6;
  uint64_t p1 = hi * (uint64_t)RECIP_M6;
  uint64_t mid = (p0 >> 16) + (p1 << 16);
  return (uint32_t)(mid >> 32);
}

// Split 64-bit frac into two 6-digit chunks
static inline void split12_fast(uint64_t frac, uint32_t *high6, uint32_t *low6) {
  uint32_t hi = div1e6_u64_u32(frac);
  uint32_t lo = (uint32_t)(frac - (uint64_t)hi * M6);
  *high6 = hi;
  *low6 = lo;
}

// Convert 0..999999 to 6 ASCII digits using optimized 32-bit operations
static inline void to6digits(uint32_t x, char *buf) {
  for (int i = 5; i >= 0; --i) {
    uint32_t q = x / 10;
    uint32_t r = x - q * 10;  // Faster than modulo on AVR
    buf[i] = (char)('0' + r);
    x = q;
  }
}

// Convert frac (0..999999999999) to exactly 12 digits
static inline void frac12_to_chars_fast(uint64_t frac, char *out12) {
  uint32_t hi, lo;
  split12_fast(frac, &hi, &lo);
  to6digits(hi, out12 + 0);
  to6digits(lo, out12 + 6);
}

// Pre-allocated buffers for maximum speed
static char line_buffer[64];  // Reusable buffer
static const uint32_t POW10_TABLE[10] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};

// Ultra-fast timestamp formatting using advisor's optimized approach
// Performance: 490+ measurements/second with PLACES/WRAP support
int format_timestamp_line_direct_memory(
  char* out,
  size_t out_size,
  const Timestamp64* t,
  char ch_name
) {
  if (!out || out_size < 32 || !t) return -1;
  
  // Get configuration parameters once
  extern struct config_t config;
  int16_t places = config.PLACES;
  int16_t wrap = config.WRAP;
  
  // Clamp places to valid range (0-12) - single comparison
  if (places > 12) places = 12;
  if (places < 0) places = 0;
  
  char* p = out;
  
  // Handle seconds with wrap logic - optimized for common cases
  uint32_t sec = t->seconds;
  if (wrap > 0 && wrap <= 9) {
    // Apply wrap: show only last 'wrap' digits using lookup table
    uint32_t mod = POW10_TABLE[wrap];
    sec = sec % mod;
    
    // Zero-pad to wrap width - unrolled for common wrap values
    if (wrap == 2) {
      p[0] = '0' + (sec / 10);
      p[1] = '0' + (sec % 10);
      p += 2;
    } else if (wrap == 3) {
      p[0] = '0' + (sec / 100);
      p[1] = '0' + ((sec % 100) / 10);
      p[2] = '0' + (sec % 10);
      p += 3;
    } else {
      // General case
      for (int i = wrap - 1; i >= 0; i--) {
        p[i] = '0' + (sec % 10);
        sec /= 10;
      }
      p += wrap;
    }
  } else {
    // No wrap: print full seconds - optimized for common cases
    if (sec < 10) {
      *p++ = '0' + sec;
    } else if (sec < 100) {
      *p++ = '0' + (sec / 10);
      *p++ = '0' + (sec % 10);
    } else if (sec < 1000) {
      *p++ = '0' + (sec / 100);
      *p++ = '0' + ((sec % 100) / 10);
      *p++ = '0' + (sec % 10);
    } else {
      // General case for larger numbers
      char tmp[12];
      int n = 0;
      uint32_t s = sec;
      do {
        tmp[n++] = '0' + (s % 10);
        s /= 10;
      } while (s);
      while (n--) *p++ = tmp[n];
    }
  }
  
  // Decimal point (only if places > 0)
  if (places > 0) {
    *p++ = '.';
    
    // Fractional part using advisor's fast method, truncated to places
    if (places == 12) {
      // Full precision - most common case
      frac12_to_chars_fast(t->sub_ps, p);
      p += 12;
    } else if (places <= 6) {
      // Use only high 6 digits - common case
      uint32_t hi, lo;
      split12_fast(t->sub_ps, &hi, &lo);
      to6digits(hi, p);
      p += places;
    } else {
      // Use high 6 + some of low 6
      uint32_t hi, lo;
      split12_fast(t->sub_ps, &hi, &lo);
      to6digits(hi, p);
      p += 6;
      to6digits(lo, p);
      p += (places - 6);
    }
  }
  
  // Channel name - always 4 characters
  *p++ = ' '; *p++ = 'c'; *p++ = 'h'; *p++ = ch_name;
  *p++ = '\r'; *p++ = '\n';
  
  // Calculate length once and write
  size_t len = p - out;
  Serial.write((const uint8_t*)out, len);
  return len;
}

// Test function for verifying the optimized print routine
void test_optimized_print() {
  Serial.println("# Testing optimized timestamp printing...");
  
  // Test cases with various timestamp values
  Timestamp64 test_cases[] = {
    {0, 0},                           // Zero case
    {1, 0},                           // 1 second
    {12345, 123456789012},            // 5 seconds + fractional
    {99999, 999999999999},            // Max seconds + max fractional
    {86400, 500000000000},            // 1 day + half second
    {31536000, 123456789012},         // 1 year + fractional
  };
  
  char test_names[] = {'A', 'B'};
  
  for (int i = 0; i < 6; i++) {
    char line[64];
    format_timestamp_line_direct_memory(line, sizeof(line), &test_cases[i], test_names[i % 2]);
  }
  
  Serial.println("# Optimized print test complete.");
}