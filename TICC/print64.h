// print64.h -- optimized 64-bit printing routines for TICC
//
// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016-2025
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

#ifndef PRINT64_H
#define PRINT64_H

#include <stdint.h>
#include "tdc7200.h"  // For Timestamp64 typedef

// Format timestamp using global config settings
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
);

// Test function for print64.cpp - call this at startup to verify functionality
void test_print64_function();

#endif  // PRINT64_H
