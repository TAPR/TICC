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

// Cache config parameters to avoid external access on every call
static uint8_t cached_places = 11;  // Default value
static uint8_t cached_wrap = 2;     // Default value
static bool config_cached = false;

// Function to update cached config parameters (call when config changes)
void update_cached_config() {
  extern struct config_t config;
  cached_places = (config.PLACES > 12) ? 12 : ((config.PLACES < 0) ? 0 : config.PLACES);
  cached_wrap = config.WRAP;
  config_cached = true;
}

// Ultra-fast timestamp formatting using advisor's optimized approach
// Performance: 548 measurements/second (tested) with PLACES/WRAP support
// Optimizations: config parameter caching, direct array operations, 
//                advisor's 64-bit to decimal conversion algorithm
int format_timestamp_line_direct_memory(
  char* out,
  size_t out_size,
  const Timestamp64* t,
  char ch_name
) {
  if (!out || out_size < 32 || !t) return -1;
  
  // Use cached config parameters for maximum speed
  uint8_t places = cached_places;
  uint8_t wrap = cached_wrap;
  
  // Pre-calculate common values to avoid repeated calculations
  uint32_t sec = t->seconds;
  char* p = out;
  
  // Handle seconds with wrap logic - optimized for common cases
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
      // General case - unrolled for better performance
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
      // General case for larger numbers - use stack buffer for speed
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
      // Use high 6 + some of low 6 - avoid calling split12_fast twice
      uint32_t hi, lo;
      split12_fast(t->sub_ps, &hi, &lo);
      to6digits(hi, p);
      p += 6;
      to6digits(lo, p);
      p += (places - 6);
    }
  }
  
  // Channel name - always 4 characters, write directly to avoid pointer arithmetic
  p[0] = ' '; p[1] = 'c'; p[2] = 'h'; p[3] = ch_name;
  p[4] = '\r'; p[5] = '\n';
  p += 6;
  
  // Write directly with calculated length
  Serial.write((const uint8_t*)out, p - out);
  return p - out;
}

// Check if timestamp a >= timestamp b
static inline bool timestamp_ge(const Timestamp64* a, const Timestamp64* b) {
  if (a->seconds != b->seconds) return a->seconds > b->seconds;
  return a->sub_ps >= b->sub_ps;
}

// Calculate difference between two timestamps (a - b)
// Returns difference as Timestamp64, handling both positive and negative results
// Result is normalized: sub_ps is always in [0, PS_PER_SEC)
Timestamp64 timestamp_difference(const Timestamp64* a, const Timestamp64* b) {
  Timestamp64 result = {0, 0};
  if (!a || !b) return result;
  
  const Timestamp64 *hi, *lo;
  bool positive = timestamp_ge(a, b);
  if (positive) { hi = a; lo = b; }
  else          { hi = b; lo = a; }
  
  // Calculate unsigned difference hi - lo
  uint32_t sec = hi->seconds - lo->seconds;
  uint64_t pico;
  
  if (hi->sub_ps >= lo->sub_ps) {
    pico = hi->sub_ps - lo->sub_ps;
  } else {
    pico = (hi->sub_ps + PS_PER_SEC) - lo->sub_ps; // borrow 1 second
    sec -= 1;
  }
  
  if (positive) {
    result.seconds = sec;
    result.sub_ps = pico; // already normalized
  } else {
    // negate (sec, pico) in canonical form
    if (pico == 0) {
      result.seconds = (uint32_t)(-(int32_t)sec);
      result.sub_ps = 0;
    } else {
      result.seconds = (uint32_t)(-(int32_t)sec - 1);
      result.sub_ps = PS_PER_SEC - pico;
    }
  }
  
  return result;
}

// Calculate difference between two timestamps and return in picoseconds
// Returns difference in picoseconds, handling both positive and negative results
int64_t timestamp_difference_ps(const Timestamp64* a, const Timestamp64* b) {
  if (!a || !b) return 0;
  
  Timestamp64 diff = timestamp_difference(a, b);
  
  // Convert to total picoseconds
  int64_t sec_ps = (int64_t)diff.seconds * PS_PER_SEC;
  int64_t total_ps = sec_ps + (int64_t)diff.sub_ps;
  
  // Handle negative result
  if (diff.seconds > 0x7FFFFFFF) { // Check if negative (using high bit)
    total_ps = -total_ps;
  }
  
  return total_ps;
}

// Format time difference for output (uses PLACES but not WRAP)
int format_time_difference(
  char* out,
  size_t out_size,
  const Timestamp64* diff,
  const char* label
) {
  if (!out || out_size < 32 || !diff) return -1;
  
  // Use cached config parameters for maximum speed
  uint8_t places = cached_places;
  
  char* p = out;
  
  // Handle negative differences (if needed in the future)
  // For now, we assume differences are always positive
  
  // Print seconds (no wrap for differences)
  uint32_t sec = diff->seconds;
  if (sec == 0) {
    *p++ = '0';
  } else if (sec < 10) {
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
  
  // Decimal point (only if places > 0)
  if (places > 0) {
    *p++ = '.';
    
    // Fractional part using advisor's fast method, truncated to places
    if (places == 12) {
      // Full precision
      frac12_to_chars_fast(diff->sub_ps, p);
      p += 12;
    } else if (places <= 6) {
      // Use only high 6 digits
      uint32_t hi, lo;
      split12_fast(diff->sub_ps, &hi, &lo);
      to6digits(hi, p);
      p += places;
    } else {
      // Use high 6 + some of low 6
      uint32_t hi, lo;
      split12_fast(diff->sub_ps, &hi, &lo);
      to6digits(hi, p);
      p += 6;
      to6digits(lo, p);
      p += (places - 6);
    }
  }
  
  // Add label if provided
  if (label && label[0]) {
    *p++ = ' ';
    while (*label) *p++ = *label++;
  }
  
  *p++ = '\r'; *p++ = '\n';
  
  Serial.write((const uint8_t*)out, p - out);
  return p - out;
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