// timestamps.cpp -- timestamp utility functions for TICC

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016-2025
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

#include <Arduino.h>
#include "tdc7200.h"
#include "timestamps.h"
#include "config.h"  // For PS_PER_SEC constant

// Calculate timestamp from channel data (pure calculation, no I/O)
// Updates channel.last_picstop, channel.timestamp, channel.new_ts_ready, channel.totalize
void calculate_timestamp(tdc7200Channel* channel, int64_t pictick_ps) {
  if (!channel) return;
  
  // Preserve last values
  channel->last_tof = channel->tof;
  channel->last_timestamp = channel->timestamp;
  
  // Read new TDC data
  channel->tof = channel->read();
  
  // Clear TDC INTB immediately to prevent duplicate processing
  channel->ready_next();
  
  // Calculate delta ticks since previous event on this channel
  // Protect PICstop reads from ISR trips
  int64_t picstop_now64, last_picstop64;
  noInterrupts();
  picstop_now64 = channel->PICstop;
  last_picstop64 = channel->last_picstop;
  channel->last_picstop = picstop_now64; // Update last_picstop for next calculation
  interrupts();
  int64_t dcount = picstop_now64 - last_picstop64;

  // Calculate delta in picoseconds
  int64_t delta_ps = dcount * pictick_ps
                   - (int64_t)channel->tof
                   + (int64_t)channel->last_tof;

  // Mixed-radix accumulation with automatic carry/borrow
  if (delta_ps >= 0) {
    channel->timestamp.picos += (uint64_t)delta_ps;
    if (channel->timestamp.picos >= PS_PER_SEC) {
      channel->timestamp.picos -= PS_PER_SEC;
      channel->timestamp.seconds += 1u;
    }
  } else {
    uint64_t m = (uint64_t)(-delta_ps);
    if (m <= channel->timestamp.picos) {
      channel->timestamp.picos -= m;
    } else {
      m -= channel->timestamp.picos;
      uint64_t borrow_sec = 1 + (m / PS_PER_SEC);
      uint64_t rem = m % PS_PER_SEC;
      // changed below from (uint32_t) cast to see if that solves
      // issue at around 8000 seconds
      channel->timestamp.seconds -= (int32_t)borrow_sec;
      channel->timestamp.picos = PS_PER_SEC - rem;
    }
  }

  // Mark timestamp as ready and increment counter
  channel->new_ts_ready = 1;
  channel->totalize++;
}

// Process binary mode: read TDC data and output binary format
// Updates channel.last_picstop, channel.totalize
// Returns true if binary mode was processed, false if not in binary mode
bool process_binary_mode(tdc7200Channel* channel) {
  if (!channel) return false;
  
  // Read TDC data
  channel->tof = channel->read();
  uint32_t tof_raw = (uint32_t)channel->tof;
  
  // Clear TDC INTB immediately to prevent duplicate processing
  channel->ready_next();
  
  // Build binary output buffer: 2 byte header + 4 bytes PICstop + 4 bytes tof + 1 byte channel + CRC
  uint8_t buf[12];
  buf[0] = 0x55;
  buf[1] = 0xAA;
  buf[2] = channel->name;
  
  // Protect PICstop read from ISR trips
  uint32_t picstop_low32;
  noInterrupts();
  picstop_low32 = (uint32_t)channel->PICstop;
  interrupts();
  
  buf[3] = (uint8_t)(picstop_low32 & 0xFF);
  buf[4] = (uint8_t)((picstop_low32 >> 8) & 0xFF);
  buf[5] = (uint8_t)((picstop_low32 >> 16) & 0xFF);
  buf[6] = (uint8_t)((picstop_low32 >> 24) & 0xFF);
  buf[7] = (uint8_t)(tof_raw & 0xFF);
  buf[8] = (uint8_t)((tof_raw >> 8) & 0xFF);
  buf[9] = (uint8_t)((tof_raw >> 16) & 0xFF);
  buf[10] = (uint8_t)((tof_raw >> 24) & 0xFF);
  buf[11] = crc8_maxim(&buf[2], 9); // CRC over payload only
  
  // Single Serial.write call
  while (Serial.availableForWrite() < 12) {}
  Serial.write(buf, 12);
  
  // Update channel state
  channel->last_picstop = channel->PICstop;
  channel->totalize++;
  
  return true;
}

// Compare two Timestamp64 structs: returns true if a >= b
bool timestamp_ge(const Timestamp64* a, const Timestamp64* b) {
  if (!a || !b) return false;
  if (a->seconds != b->seconds) return a->seconds > b->seconds;
  return a->picos >= b->picos;
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
  
  if (hi->picos >= lo->picos) {
    pico = hi->picos - lo->picos;
  } else {
    pico = (hi->picos + PS_PER_SEC) - lo->picos; // borrow 1 second
    sec -= 1;
  }
  
  if (positive) {
    result.seconds = sec;
    result.picos = pico; // already normalized
  } else {
    // negate (sec, pico) in canonical form
    if (pico == 0) {
      result.seconds = (uint32_t)(-(int32_t)sec);
      result.picos = 0;
    } else {
      result.seconds = (uint32_t)(-(int32_t)sec - 1);
      result.picos = PS_PER_SEC - pico;
    }
  }
  
  return result;
}

// Calculate timestamp difference in picoseconds: a - b
int64_t timestamp_difference_ps(const Timestamp64* a, const Timestamp64* b) {
  if (!a || !b) return 0;
  
  int64_t sec_diff = (int64_t)a->seconds - (int64_t)b->seconds;
  int64_t pico_diff = (int64_t)a->picos - (int64_t)b->picos;
  
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
  uint64_t frac = diff->picos;
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

// CRC-8 Dallas/Maxim (poly 0x31, reflected => 0x8C, init 0x00)
uint8_t crc8_maxim(const uint8_t *data, size_t len) {
  uint8_t crc = 0x00;
  while (len--) {
    uint8_t in = *data++;
    crc ^= in;
    for (uint8_t i = 0; i < 8; i++) {
      if (crc & 0x01) {
        crc = (crc >> 1) ^ 0x8C;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}