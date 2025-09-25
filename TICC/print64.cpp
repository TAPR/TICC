// print64.cpp -- optimized 64-bit printing routines for TICC
// Based on 64-bit_print.txt but adapted for TICC environment
//
// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016-2025
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

// CRITICAL: Include printf BEFORE Arduino.h to override Arduino's printf functions
#include "printf.h"           // mpaland printf library for 64-bit support

#include <Arduino.h>

// Include TICC headers for constants and external variables
#include "config.h"
#include "misc.h"
#include "tdc7200.h"          // For Timestamp64 definition

// mpaland printf should be linked so snprintf here supports 64-bit integers.

// External references to global config
extern config_t config;


// Constants - using TICC's actual constants
static const uint64_t ONE_T_PS   = PS_PER_SEC;  // 1e12 picoseconds per second
static const uint32_t POW10_U32[10] = {
  1u, 10u, 100u, 1000u, 10000u,
  100000u, 1000000u, 10000000u, 100000000u, 1000000000u
};
static const uint64_t POW10_U64[13] = {
  1ULL,
  10ULL,
  100ULL,
  1000ULL,
  10000ULL,
  100000ULL,
  1000000ULL,
  10000000ULL,
  100000000ULL,
  1000000000ULL,
  10000000000ULL,
  100000000000ULL,
  1000000000000ULL
};

// Helper: clamp PLACES to [0,12]
static inline uint8_t clamp_places(uint8_t p) {
  return (p > 12) ? 12 : p;
}

// Helper: returns 10^k as uint32_t for k<=9, else 0
static inline uint32_t pow10_u32_or0(uint8_t k) {
  if (k <= 9) return POW10_U32[k];
  return 0u;
}

// Format function using global config
// out: output buffer
// out_size: size of output buffer
// t: timestamp (seconds, sub_ps normalized 0..1e12-1)
// ch_name: channel name character (e.g., 'A', 'B', '0', '1') - will be formatted as "chA", "chB", etc.
// Returns number of bytes written (without terminating NUL) or -1 on error.
int format_timestamp_line(
  char* out,
  size_t out_size,
  const Timestamp64* t,
  char ch_name
) {
  if (!out || out_size == 0 || !t) return -1;

  // Use global config values
  uint8_t PLACES = (uint8_t)config.PLACES;
  uint8_t WRAP_SECONDS = (uint8_t)config.WRAP;
  
  // Format channel name
  char ch_label[8];
  snprintf(ch_label, sizeof(ch_label), "ch%c", ch_name);

  // Clamp PLACES and WRAP_SECONDS
  PLACES = clamp_places(PLACES);
  if (WRAP_SECONDS > 12) WRAP_SECONDS = 12;

  // Prepare seconds for printing (wrapped or full)
  // For performance, prefer WRAP_SECONDS <= 9 (32-bit).
  uint32_t wrap_mod_u32 = pow10_u32_or0(WRAP_SECONDS);
  uint64_t wrap_mod_u64 = (WRAP_SECONDS > 9) ? POW10_U64[WRAP_SECONDS] : 0ULL;

  // Compute displayed seconds
  uint32_t disp_seconds_u32 = t->seconds; // default (no wrap)
  uint64_t disp_seconds_u64 = t->seconds; // for WRAP >= 10

  if (WRAP_SECONDS != 0) {
    if (wrap_mod_u32) {
      disp_seconds_u32 = t->seconds % wrap_mod_u32;
    } else {
      // WRAP 10..12: modulo by up to 1e12; seconds is 32-bit so this is cheap enough
      disp_seconds_u64 = ((uint64_t)t->seconds) % wrap_mod_u64;
    }
  }

  // Prepare fractional part with rounding to PLACES digits
  // We need to print exactly PLACES digits after the dot, rounded.
  // sub_ps has 12 digits max. To round to PLACES:
  //   if PLACES == 12: use sub_ps as-is.
  //   else:
  //     base = 10^(12-PLACES)
  //     frac_rounded = (sub_ps + base/2) / base
  //     if frac_rounded == 10^PLACES, carry 1 second and set frac to 0.
  uint64_t frac_ps = t->sub_ps; // 0..999,999,999,999
  uint64_t frac_out = 0ULL;
  uint32_t carry_to_seconds = 0u;

  if (PLACES == 0) {
    // Round to integer seconds
    if (frac_ps >= 500000000000ULL) carry_to_seconds = 1u;
    frac_out = 0ULL;
  } else if (PLACES == 12) {
    frac_out = frac_ps;
  } else {
    uint8_t drop_digits = (uint8_t)(12 - PLACES);
    uint64_t base = POW10_U64[drop_digits];     // 10^(12-PLACES)
    uint64_t half = base >> 1;                   // base/2
    uint64_t rounded = (frac_ps + half) / base;  // rounded PLACES digits

    uint64_t full_scale = POW10_U64[PLACES];
    if (rounded >= full_scale) {
      // e.g., 0.999999999999 rounded to fewer places overflowed
      rounded = 0ULL;
      carry_to_seconds = 1u;
    }
    frac_out = rounded;
  }

  // Apply carry to seconds (after wrap decision)
  // For WRAP_SECONDS == 0, we can safely add carry to full seconds.
  // For WRAP_SECONDS > 0:
  //  - If <= 9: do wrap on disp_seconds_u32 only for displayed seconds (no need to alter t->seconds).
  //  - If > 9: do wrap on the disp_seconds_u64 similarly.
  uint32_t print_sec_u32 = 0;
  uint64_t print_sec_u64 = 0;

  if (WRAP_SECONDS == 0) {
    uint32_t s = t->seconds + carry_to_seconds;
    print_sec_u32 = s;
  } else if (wrap_mod_u32) {
    uint32_t s = disp_seconds_u32 + carry_to_seconds;
    if (s >= wrap_mod_u32) s -= wrap_mod_u32;
    print_sec_u32 = s;
  } else {
    uint64_t s = disp_seconds_u64 + (uint64_t)carry_to_seconds;
    if (s >= wrap_mod_u64) s -= wrap_mod_u64;
    print_sec_u64 = s;
  }

  // Build output with CRLF. Ensure the buffer is large enough.
  // Worst-case sizes:
  // - Seconds: up to 10 digits (uint32_t) or up to 12 digits when wrapped with WRAP_SECONDS=12
  // - Dot + fractional: 1 + PLACES (<=12)
  // - Space + channel (e.g., " chA"): up to, say, 6 chars (depends on your labels)
  // - CRLF: 2
  // - NUL: 1
  // Use a safe line length like 48 in your caller, as you already do.
  int n = 0;
  if (WRAP_SECONDS == 0) {
    // No wrap: seconds as %lu, fractional as %.PLACES with zero-padding via width
    if (PLACES == 0) {
      n = snprintf(out, out_size, "%lu %s", (unsigned long)print_sec_u32, ch_label);
    } else if (PLACES <= 9) {
      // For PLACES <= 9, frac_out fits in uint32_t
      n = snprintf(out, out_size, "%lu.%0*u %s",
                   (unsigned long)print_sec_u32,
                   (int)PLACES, (unsigned int)frac_out,
                   ch_label);
    } else {
      // PLACES 10..12: use 64-bit for fractional
      // Build width via fixed formats to avoid %.* with 64-bit if you prefer;
      // mpaland supports %0*llu, so we can use width as int.
      n = snprintf(out, out_size, "%lu.%0*llu %s",
                   (unsigned long)print_sec_u32,
                   (int)PLACES, (unsigned long long)frac_out,
                   ch_label);
    }
  } else {
    // With wrap: seconds printed zero-padded to WRAP_SECONDS digits
    if (wrap_mod_u32) {
      if (PLACES == 0) {
        n = snprintf(out, out_size, "%0*u %s",
                     (int)WRAP_SECONDS, (unsigned int)print_sec_u32, ch_label);
      } else if (PLACES <= 9) {
        n = snprintf(out, out_size, "%0*u.%0*u %s",
                     (int)WRAP_SECONDS, (unsigned int)print_sec_u32,
                     (int)PLACES,       (unsigned int)frac_out,
                     ch_label);
      } else {
        n = snprintf(out, out_size, "%0*u.%0*llu %s",
                     (int)WRAP_SECONDS, (unsigned int)print_sec_u32,
                     (int)PLACES,       (unsigned long long)frac_out,
                     ch_label);
      }
    } else {
      // WRAP 10..12: seconds modulo up to 1e12 and zero-padded to WRAP_SECONDS
      // Use 64-bit printing for seconds
      if (PLACES == 0) {
        // e.g., %012llu for WRAP_SECONDS=12
        if (WRAP_SECONDS == 10)
          n = snprintf(out, out_size, "%010llu %s",
                       (unsigned long long)print_sec_u64, ch_label);
        else if (WRAP_SECONDS == 11)
          n = snprintf(out, out_size, "%011llu %s",
                       (unsigned long long)print_sec_u64, ch_label);
        else
          n = snprintf(out, out_size, "%012llu %s",
                       (unsigned long long)print_sec_u64, ch_label);
      } else {
        if (WRAP_SECONDS == 10)
          n = snprintf(out, out_size, "%010llu.%0*llu %s",
                       (unsigned long long)print_sec_u64,
                       (int)PLACES, (unsigned long long)frac_out, ch_label);
        else if (WRAP_SECONDS == 11)
          n = snprintf(out, out_size, "%011llu.%0*llu %s",
                       (unsigned long long)print_sec_u64,
                       (int)PLACES, (unsigned long long)frac_out, ch_label);
        else
          n = snprintf(out, out_size, "%012llu.%0*llu %s",
                       (unsigned long long)print_sec_u64,
                       (int)PLACES, (unsigned long long)frac_out, ch_label);
      }
    }
  }

  if (n < 0) return -1;

  // Append CRLF
  if ((size_t)n + 2 >= out_size) return -1;
  out[n++] = '\r';
  out[n++] = '\n';
  out[n] = '\0';
  return n;
}

// Test function for print64.cpp - call this at startup to verify functionality
void test_print64_function() {
  Serial.println("# ");
  Serial.println("# Testing print64.cpp timestamp formatting...");
  Serial.println("# ");
  
  char test_buffer[64];
  Timestamp64 test_ts;
  
  // Test cases with various time ranges and decimal places
  struct TestCase {
    uint32_t seconds;
    uint64_t sub_ps;
    char ch_name;
    uint8_t places;
    uint8_t wrap;
    const char* description;
  };
  
  TestCase test_cases[] = {
    // Basic tests - 1 second with various fractional parts
    {1, 0, 'A', 6, 0, "1 second, no fractional"},
    {1, 500000000000ULL, 'B', 6, 0, "1.5 seconds"},
    {1, 123456789012ULL, 'C', 12, 0, "1 second + 123456789012 ps"},
    
    // Various decimal places (0-12)
    {42, 999999999999ULL, 'D', 0, 0, "42 seconds, 0 places (round to integer)"},
    {42, 999999999999ULL, 'E', 3, 0, "42 seconds, 3 places"},
    {42, 999999999999ULL, 'F', 6, 0, "42 seconds, 6 places"},
    {42, 999999999999ULL, 'G', 9, 0, "42 seconds, 9 places"},
    {42, 999999999999ULL, 'H', 12, 0, "42 seconds, 12 places (full precision)"},
    
    // Wrap tests - various wrap values
    {12345, 500000000000ULL, 'I', 6, 2, "12345 seconds, wrap=2 (should show 45)"},
    {123456, 250000000000ULL, 'J', 6, 3, "123456 seconds, wrap=3 (should show 456)"},
    {1234567, 750000000000ULL, 'K', 6, 4, "1234567 seconds, wrap=4 (should show 4567)"},
    {12345678, 100000000000ULL, 'L', 6, 5, "12345678 seconds, wrap=5 (should show 45678)"},
    {123456789, 900000000000ULL, 'M', 6, 6, "123456789 seconds, wrap=6 (should show 456789)"},
    
    // Large time values (years worth of seconds)
    {31536000, 0, 'N', 6, 0, "1 year (31536000 seconds)"},
    {315360000, 500000000000ULL, 'O', 6, 0, "10 years (315360000 seconds)"},
    {3153600000, 123456789012ULL, 'P', 12, 0, "100 years (3153600000 seconds)"},
    
    // Edge cases
    {0, 1, 'Q', 12, 0, "1 picosecond"},
    {0, 1000000000000ULL - 1, 'R', 12, 0, "Just under 1 second"},
    {4294967295U, 0, 'S', 6, 0, "Max uint32_t seconds"},
    {4294967295U, 999999999999ULL, 'T', 12, 0, "Max uint32_t seconds + max fractional"},
  };
  
  // Save original config values
  int16_t orig_places = config.PLACES;
  int16_t orig_wrap = config.WRAP;
  
  for (int i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
    TestCase* tc = &test_cases[i];
    
    // Set config for this test
    config.PLACES = tc->places;
    config.WRAP = tc->wrap;
    
    // Set up test timestamp
    test_ts.seconds = tc->seconds;
    test_ts.sub_ps = tc->sub_ps;
    
    // Format the timestamp
    int result = format_timestamp_line(test_buffer, sizeof(test_buffer), &test_ts, tc->ch_name);
    
    if (result > 0) {
      Serial.print("# Test ");
      Serial.print(i + 1);
      Serial.print(" (");
      Serial.print(tc->description);
      Serial.print("): ");
      Serial.write((const uint8_t*)test_buffer, result);
    } else {
      Serial.print("# Test ");
      Serial.print(i + 1);
      Serial.print(" FAILED: format_timestamp_line returned ");
      Serial.println(result);
    }
  }
  
  // Restore original config values
  config.PLACES = orig_places;
  config.WRAP = orig_wrap;
  
  Serial.println("# ");
  Serial.println("# print64.cpp test complete.");
  Serial.println("# ");
}
