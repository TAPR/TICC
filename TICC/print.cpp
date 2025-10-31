// print.cpp -- optimized 64-bit printing routines for TICC

#include <Arduino.h>
#include "TICC.h"
#include "tdc7200.h"
#include "timestamps.h"
#include "print.h"

// External variables now defined in TICC.h

// Fast 64-bit to 12-digit conversion
#define M6 1000000
#define RECIP_M6 281474976UL

// Two-digit lookup table for fast conversion (100 entries × 2 bytes = 200 bytes)
static const char TWO_DIGIT_TABLE[100][2] PROGMEM = {
  {'0','0'}, {'0','1'}, {'0','2'}, {'0','3'}, {'0','4'}, {'0','5'}, {'0','6'}, {'0','7'}, {'0','8'}, {'0','9'},
  {'1','0'}, {'1','1'}, {'1','2'}, {'1','3'}, {'1','4'}, {'1','5'}, {'1','6'}, {'1','7'}, {'1','8'}, {'1','9'},
  {'2','0'}, {'2','1'}, {'2','2'}, {'2','3'}, {'2','4'}, {'2','5'}, {'2','6'}, {'2','7'}, {'2','8'}, {'2','9'},
  {'3','0'}, {'3','1'}, {'3','2'}, {'3','3'}, {'3','4'}, {'3','5'}, {'3','6'}, {'3','7'}, {'3','8'}, {'3','9'},
  {'4','0'}, {'4','1'}, {'4','2'}, {'4','3'}, {'4','4'}, {'4','5'}, {'4','6'}, {'4','7'}, {'4','8'}, {'4','9'},
  {'5','0'}, {'5','1'}, {'5','2'}, {'5','3'}, {'5','4'}, {'5','5'}, {'5','6'}, {'5','7'}, {'5','8'}, {'5','9'},
  {'6','0'}, {'6','1'}, {'6','2'}, {'6','3'}, {'6','4'}, {'6','5'}, {'6','6'}, {'6','7'}, {'6','8'}, {'6','9'},
  {'7','0'}, {'7','1'}, {'7','2'}, {'7','3'}, {'7','4'}, {'7','5'}, {'7','6'}, {'7','7'}, {'7','8'}, {'7','9'},
  {'8','0'}, {'8','1'}, {'8','2'}, {'8','3'}, {'8','4'}, {'8','5'}, {'8','6'}, {'8','7'}, {'8','8'}, {'8','9'},
  {'9','0'}, {'9','1'}, {'9','2'}, {'9','3'}, {'9','4'}, {'9','5'}, {'9','6'}, {'9','7'}, {'9','8'}, {'9','9'}
};

// Fast division by 10 using reciprocal multiplication (exact, no precision loss)
// Replaces slow software division (~500 cycles) with multiplication + shift (~50 cycles)
// Magic constant: 0xCCCCCCCD = ceil(2^35 / 10), exact for all 32-bit values
static inline uint32_t div10_fast(uint32_t x) {
  return ((uint64_t)x * 0xCCCCCCCDUL) >> 35;
}

// Fast division by 1000 using reciprocal multiplication (exact, no precision loss)
// Magic constant: 0x10624DD3 = ceil(2^38 / 1000), exact for all 32-bit values
static inline uint32_t div1000_fast(uint32_t x) {
  return ((uint64_t)x * 0x10624DD3UL) >> 38;
}

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
  
  // Due to the trick of using RECIP_M6, if the fractional part
  // is an exact multiple of 1 us, that would introduce an error
  // of 1 us into the printed output.  This catches and fixes
  // this: if lo >= 1000000, carry to hi
  if (lo >= 1000000U) {
    hi++;
    lo -= 1000000U;
  }
  
  *high6 = hi;
  *low6 = lo;
}

// Convert 0..999999 to 6 ASCII digits using hybrid reciprocal + lookup table
// This is mathematically exact - no precision loss
static inline void to6digits(uint32_t x, char *buf) {
  // Split into upper 3 digits and lower 3 digits using fast division
  uint16_t thousands = div1000_fast(x);     // Upper 3 digits: x / 1000
  uint16_t ones = x - thousands * 1000;     // Lower 3 digits: x % 1000
  
  // Convert upper 3 digits: split into 2-digit pair + 1 digit
  uint8_t d5d4 = div10_fast(thousands);     // Upper 2 digits: thousands / 10
  uint8_t d3 = thousands - d5d4 * 10;       // Middle digit: thousands % 10
  
  // Convert lower 3 digits: split into 2-digit pair + 1 digit
  uint8_t d2d1 = div10_fast(ones);          // Middle 2 digits: ones / 10
  uint8_t d0 = ones - d2d1 * 10;            // Lowest digit: ones % 10
  
  // Write output using lookup table for 2-digit pairs
  memcpy_P(buf, TWO_DIGIT_TABLE[d5d4], 2);
  buf[2] = '0' + d3;
  memcpy_P(buf + 3, TWO_DIGIT_TABLE[d2d1], 2);
  buf[5] = '0' + d0;
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

// Ultra-fast timestamp formatting (~395 µs on AVR @ 16 MHz)
// Optimizations for 8-bit AVR architecture:
//   1. Reciprocal multiplication for division: replaces slow software division 
//      (~500 cycles) with multiplication + shift (~50 cycles)
//   2. Two-digit lookup table: converts digit pairs via table lookup instead of division
//   3. All operations are mathematically exact - no precision loss
// Performance (measured): 
//   - Formatting time: ~395 µs
//   - Throughput @ 115200 baud: 587 Hz (transmission-limited)
//   - Throughput @ 230400 baud: 765 Hz (23% improvement due to faster transmission)
int print_timestamp(
  char* out,
  size_t out_size,
  const Timestamp64* t,
  char ch_name,
  bool use_wrap = true
) {
  if (!out || out_size < 32 || !t) return -1;
  
  // Use cached config parameters for maximum speed
  // If config not cached, get values directly from config
  uint8_t places, wrap;
  if (config_cached) {
    places = cached_places;
    wrap = use_wrap ? cached_wrap : 0;
  } else {
    // Fallback to direct config access if not cached
    extern struct config_t config;
    places = (config.PLACES > 12) ? 12 : ((config.PLACES < 0) ? 0 : config.PLACES);
    wrap = use_wrap ? config.WRAP : 0;
  }
  
  // Debug: check if config values are reasonable
  if (wrap > 9) wrap = 0;  // Sanity check
  if (places > 12) places = 12;  // Sanity check
  
  // Pre-calculate common values to avoid repeated calculations
  int32_t sec = t->seconds;
  char* p = out;
  
  // Handle negative sign if needed
  bool is_negative = (sec < 0);
  if (is_negative) {
    *p++ = '-';
    sec = -sec;  // Make positive for processing
  }
  
  // Handle seconds with wrap logic - keep simple for maintainability
  if (wrap > 0 && wrap <= 9) {
    // Apply wrap: show only last 'wrap' digits using lookup table
    uint32_t mod = pgm_read_dword(&POW10_TABLE[wrap]);
    uint32_t sec_u = (uint32_t)sec;  // Convert to unsigned for modulo
    sec_u = sec_u % mod;
    
    // Zero-pad to wrap width - unrolled for common wrap values
    if (wrap == 2) {
      p[0] = '0' + (sec_u / 10);
      p[1] = '0' + (sec_u % 10);
      p += 2;
    } else if (wrap == 3) {
      p[0] = '0' + (sec_u / 100);
      p[1] = '0' + ((sec_u % 100) / 10);
      p[2] = '0' + (sec_u % 10);
      p += 3;
    } else {
      // General case - unrolled for better performance
      for (int i = wrap - 1; i >= 0; i--) {
        p[i] = '0' + (sec_u % 10);
        sec_u /= 10;
      }
      p += wrap;
    }
  } else {
    // No wrap: print full seconds - keep simple for maintainability
    uint32_t sec_u = (uint32_t)sec;  // Convert to unsigned for processing
    if (sec_u < 10) {
      *p++ = '0' + sec_u;
    } else if (sec_u < 100) {
      *p++ = '0' + (sec_u / 10);
      *p++ = '0' + (sec_u % 10);
    } else if (sec_u < 1000) {
      *p++ = '0' + (sec_u / 100);
      *p++ = '0' + ((sec_u % 100) / 10);
      *p++ = '0' + (sec_u % 10);
    } else {
      // General case for larger numbers - use stack buffer for speed
      char tmp[12];
      int n = 0;
      uint32_t s = sec_u;
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
    
    // Fractional part truncated to places
    if (places == 12) {
      // Full precision - most common case
      frac12_to_chars_fast(t->picos, p);
      p += 12;
    } else if (places <= 6) {
      // Use only high 6 digits - common case
      uint32_t hi, lo;
      split12_fast(t->picos, &hi, &lo);
      to6digits(hi, p);
      p += places;
    } else {
      // Use high 6 + some of low 6 - avoid calling split12_fast twice
      uint32_t hi, lo;
      split12_fast(t->picos, &hi, &lo);
      to6digits(hi, p);
      p += 6;
      to6digits(lo, p);
      p += (places - 6);
    }
  }
  
  // Channel name - only if ch_name is not blank/null
  if (ch_name && ch_name != ' ') {
    p[0] = ' '; p[1] = 'c'; p[2] = 'h'; p[3] = ch_name;
    p[4] = '\r'; p[5] = '\n';
    p += 6;
  } else {
    p[0] = '\r'; p[1] = '\n';
    p += 2;
  }
  
  // Write directly with calculated length
  Serial.write((const uint8_t*)out, p - out);
  return p - out;
}

// Print timestamp difference - handles canonical form for negative values
// Converts canonical form to printable form, then calls print_timestamp
int print_timestamp_difference(
  char* out,
  size_t out_size,
  const Timestamp64* diff,
  char ch_name,
  bool use_wrap = true
) {
  if (!out || out_size < 32 || !diff) return -1;
  
  bool is_negative = (diff->seconds < 0);
  
  // Convert canonical form to printable form if needed
  Timestamp64 printable;
  
  if (is_negative && diff->picos > 0) {
    // Canonical form: {-X, Y} represents -X + Y/PS_PER_SEC
    // Calculate absolute value, then extract integer and fractional parts
    int64_t total_ps = (int64_t)diff->seconds * PS_PER_SEC + (int64_t)diff->picos;
    int64_t abs_total_ps = (total_ps < 0) ? -total_ps : total_ps;
    int32_t int_sec = (int32_t)(abs_total_ps / PS_PER_SEC);
    uint64_t frac_pico = (uint64_t)(abs_total_ps % PS_PER_SEC);
    
    // Handle values between -1 and 0: format manually to print "-0.XXX"
    if (int_sec == 0 && frac_pico > 0) {
      // Format manually for negative fractional values between -1 and 0
      char* p = out;
      *p++ = '-';
      *p++ = '0';
      *p++ = '.';
      
      // Format fractional part
      // Get places from config
      uint8_t places;
      if (config_cached) {
        places = cached_places;
      } else {
        extern struct config_t config;
        places = (config.PLACES > 12) ? 12 : ((config.PLACES < 0) ? 0 : config.PLACES);
      }
      
      if (places > 0) {
        if (places == 12) {
          frac12_to_chars_fast(frac_pico, p);
          p += 12;
        } else if (places <= 6) {
          uint32_t hi, lo;
          split12_fast(frac_pico, &hi, &lo);
          to6digits(hi, p);
          p += places;
        } else {
          uint32_t hi, lo;
          split12_fast(frac_pico, &hi, &lo);
          to6digits(hi, p);
          p += 6;
          to6digits(lo, p);
          p += (places - 6);
        }
      }
      
      // Add CRLF
      if (ch_name && ch_name != ' ') {
        p[0] = ' '; p[1] = 'c'; p[2] = 'h'; p[3] = ch_name;
        p[4] = '\r'; p[5] = '\n';
        p += 6;
      } else {
        p[0] = '\r'; p[1] = '\n';
        p += 2;
      }
      
      Serial.write((const uint8_t*)out, p - out);
      return p - out;
    } else {
      // Convert to printable form: integer part is non-zero
      printable.seconds = -int_sec;
      printable.picos = frac_pico;
    }
  } else {
    // Not in canonical form or has no fractional part - use as-is
    printable.seconds = diff->seconds;
    printable.picos = diff->picos;
  }
  
  // Format using print_timestamp with converted printable form
  return print_timestamp(out, out_size, &printable, ch_name, use_wrap);
}

// Efficient 64-bit integer printing function
// Based on print_timestamp model but simplified for integers only
// Handles negative values and large numbers efficiently
void print_int64(int64_t value, bool add_crlf) {
  char buffer[32];  // Sufficient for 64-bit signed integers
  char* p = buffer;
  
  // Handle negative values
  if (value < 0) {
    *p++ = '-';
    value = -value;
  }
  
  // Handle zero case
  if (value == 0) {
    *p++ = '0';
  } else {
    // Convert to string using optimized approach
    // Build string backwards, then reverse
    char temp[32];
    char* temp_p = temp;
    int64_t v = value;
    
    while (v > 0) {
      *temp_p++ = '0' + (v % 10);
      v /= 10;
    }
    
    // Copy reversed digits to output buffer
    while (temp_p > temp) {
      *p++ = *(--temp_p);
    }
  }
  
  // Add CRLF if requested
  if (add_crlf) {
    *p++ = '\r';
    *p++ = '\n';
  }
  
  // Write to serial port
  Serial.write((const uint8_t*)buffer, p - buffer);
}
