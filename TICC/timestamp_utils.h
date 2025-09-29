// timestamp_utils.h -- timestamp utility functions for TICC

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016-2025
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

#ifndef TIMESTAMP_UTILS_H
#define TIMESTAMP_UTILS_H

#include "tdc7200.h"  // For Timestamp64 definition

// Compare two Timestamp64 structs: returns true if a >= b
bool timestamp_ge(const Timestamp64* a, const Timestamp64* b);

// Calculate timestamp difference: a - b
// Handles negative results and borrow/carry correctly
Timestamp64 timestamp_difference(const Timestamp64* a, const Timestamp64* b);

// Calculate timestamp difference in picoseconds: a - b
int64_t timestamp_difference_ps(const Timestamp64* a, const Timestamp64* b);

// Format time difference for display
int format_time_difference(
  char* out,
  size_t out_size,
  const Timestamp64* diff,
  int places,
  char ch_name
);

// Generate CRC-8 Dallas/Maxim (poly 0x31, reflected => 0x8C, init 0x00)
uint8_t crc8_maxim(const uint8_t *data, size_t len);

#endif // TIMESTAMP_UTILS_H
