// print.cpp -- optimized 64-bit printing routines for TICC

#include <Arduino.h>
#include "config.h"
#include "tdc7200.h"
#include "timestamp_utils.h"
#include "print.h"

// External config variable (defined in TICC.ino)
extern config_t config;

// Fast 64-bit to 12-digit conversion
#define M6 1000000
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

// Ultra-fast timestamp formatting
// Performance: 565 measurements/second (tested) with PLACES/WRAP support
int print_timestamp(
  char* out,
  size_t out_size,
  const Timestamp64* t,
  char ch_name,
  bool use_wrap = true
) {
  if (!out || out_size < 32 || !t) return -1;
  
  // Use cached config parameters for maximum speed
  uint8_t places = cached_places;
  uint8_t wrap = use_wrap ? cached_wrap : 0;  // Only use wrap if requested
  
  // Pre-calculate common values to avoid repeated calculations
  int32_t sec = t->seconds;
  char* p = out;
  
  // Handle negative sign if needed
  bool is_negative = (sec < 0);
  if (is_negative) {
    *p++ = '-';
    sec = -sec;  // Make positive for processing
  }
  
  // Handle seconds with wrap logic - optimized for common cases
  if (wrap > 0 && wrap <= 9) {
    // Apply wrap: show only last 'wrap' digits using lookup table
    uint32_t mod = POW10_TABLE[wrap];
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
    // No wrap: print full seconds - optimized for common cases
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
    
    // Fractional part using advisor's fast method, truncated to places
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
    print_timestamp(line, sizeof(line), &test_cases[i], test_names[i % 2]);
  }
  
  Serial.println("# Optimized print test complete.");
}

// Comprehensive test function for PLACES and WRAP options
void test_places_and_wrap() {
  Serial.println("# Testing PLACES and WRAP options...");
  
  // Test timestamp with known fractional part
  Timestamp64 test_ts;
  test_ts.seconds = 1234567;  // Large seconds to test wrapping
  test_ts.picos = 123456789012ULL;  // Known fractional part
  
  // Test all PLACES values (0-12)
  for (uint8_t places = 0; places <= 12; places++) {
    // Temporarily set PLACES
    uint8_t original_places = config.PLACES;
    config.PLACES = places;
    update_cached_config();
    
    Serial.print("# PLACES=");
    Serial.print(places);
    Serial.print(": ");
    
    char line[64];
    print_timestamp(line, sizeof(line), &test_ts, 'A', true);
    Serial.println(line);
    
    // Restore original PLACES
    config.PLACES = original_places;
    update_cached_config();
  }
  
  Serial.println();
  
  // Test all WRAP values (0-6)
  for (uint8_t wrap = 0; wrap <= 6; wrap++) {
    // Temporarily set WRAP
    uint8_t original_wrap = config.WRAP;
    config.WRAP = wrap;
    update_cached_config();
    
    Serial.print("# WRAP=");
    Serial.print(wrap);
    Serial.print(" (wrap at ");
    if (wrap == 0) {
      Serial.print("none");
    } else {
      Serial.print("10^");
      Serial.print(wrap);
    }
    Serial.print("): ");
    
    char line[64];
    print_timestamp(line, sizeof(line), &test_ts, 'B', true);
    Serial.println(line);
    
    // Restore original WRAP
    config.WRAP = original_wrap;
    update_cached_config();
  }
  
  // Test negative values with PLACES and WRAP
  Serial.println();
  Serial.println("# Testing negative values...");
  
  test_ts.seconds = -1234567;
  test_ts.picos = 123456789012ULL;
  
  // Test with PLACES=6 and WRAP=3
  config.PLACES = 6;
  config.WRAP = 3;
  update_cached_config();
  
  char line[64];
  print_timestamp(line, sizeof(line), &test_ts, 'C', true);
  Serial.print("# Negative with PLACES=6, WRAP=3: ");
  Serial.println(line);
  
  // Test with WRAP disabled
  config.WRAP = 0;
  update_cached_config();
  
  print_timestamp(line, sizeof(line), &test_ts, 'C', false);
  Serial.print("# Negative with PLACES=6, WRAP=0: ");
  Serial.println(line);
  
  Serial.println("# PLACES and WRAP test complete.");
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
