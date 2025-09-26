// timestamp_utils.cpp -- timestamp utility functions for TICC

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016-2025
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

#include "printf.h"  // Must be before Arduino.h to override printf functions
#include <Arduino.h>
#include "tdc7200.h"
#include "timestamp_utils.h"

// Compare two Timestamp64 structs: returns true if a >= b
bool timestamp_ge(const Timestamp64* a, const Timestamp64* b) {
  if (!a || !b) return false;
  if (a->seconds != b->seconds) return a->seconds > b->seconds;
  return a->sub_ps >= b->sub_ps;
}

// Calculate timestamp difference: a - b
// Handles negative results and borrow/carry correctly
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

// Calculate timestamp difference in picoseconds: a - b
int64_t timestamp_difference_ps(const Timestamp64* a, const Timestamp64* b) {
  if (!a || !b) return 0;
  
  int64_t sec_diff = (int64_t)a->seconds - (int64_t)b->seconds;
  int64_t pico_diff = (int64_t)a->sub_ps - (int64_t)b->sub_ps;
  
  return sec_diff * PS_PER_SEC + pico_diff;
}

// Format time difference for display
int format_time_difference(
  char* out,
  size_t out_size,
  const Timestamp64* diff,
  int places,
  char ch_name
) {
  if (!out || out_size < 32) return 0;
  
  char* p = out;
  const char* end = out + out_size;
  
  // Handle negative sign
  bool is_negative = (diff->seconds < 0);
  if (is_negative) {
    *p++ = '-';
  }
  
  // Print seconds (absolute value)
  uint32_t sec = (is_negative) ? (uint32_t)(-diff->seconds) : (uint32_t)diff->seconds;
  
  // Convert seconds to string
  char sec_buf[12];
  int sec_len = 0;
  if (sec == 0) {
    sec_buf[sec_len++] = '0';
  } else {
    while (sec > 0) {
      sec_buf[sec_len++] = '0' + (sec % 10);
      sec /= 10;
    }
  }
  
  // Reverse the string
  for (int i = 0; i < sec_len; i++) {
    if (p < end) *p++ = sec_buf[sec_len - 1 - i];
  }
  
  // Add decimal point
  if (p < end) *p++ = '.';
  
  // Add fractional part
  uint64_t frac = diff->sub_ps;
  for (int i = 0; i < places && p < end; i++) {
    frac *= 10;
    *p++ = '0' + (char)(frac / PS_PER_SEC);
    frac %= PS_PER_SEC;
  }
  
  // Add channel name if specified
  if (ch_name != '\0') {
    if (p < end) *p++ = ' ';
    if (p < end) *p++ = 'c';
    if (p < end) *p++ = 'h';
    if (p < end) *p++ = ch_name;
  }
  
  // Null terminate
  if (p < end) *p = '\0';
  
  return (int)(p - out);
}
