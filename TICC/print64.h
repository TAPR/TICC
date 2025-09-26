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

// Format timestamp using global config settings (complex version with places/wrap)
int format_timestamp_line_complex(
  char* out,
  size_t out_size,
  const Timestamp64* t,
  char ch_name
);

// Minimal baseline function - just seconds.12_decimal_places chX
int format_timestamp_line(
  char* out,
  size_t out_size,
  const Timestamp64* t,
  char ch_name
);

// Alternative formatting functions for benchmarking
int format_timestamp_line_manual32(char* out, size_t out_size, const Timestamp64* t, char ch_name);
int format_timestamp_line_pure_manual(char* out, size_t out_size, const Timestamp64* t, char ch_name);
int format_timestamp_line_splittime_style(char* out, size_t out_size, const Timestamp64* t, char ch_name);
int format_timestamp_line_simple64(char* out, size_t out_size, const Timestamp64* t, char ch_name);
int format_timestamp_line_buffer_reuse(char* out, size_t out_size, const Timestamp64* t, char ch_name);

// Ultra-fast alternatives (no sprintf)
int format_timestamp_line_ultra_fast(char* out, size_t out_size, const Timestamp64* t, char ch_name);
int format_timestamp_line_fixed_width(char* out, size_t out_size, const Timestamp64* t, char ch_name);
int format_timestamp_line_minimal_sprintf(char* out, size_t out_size, const Timestamp64* t, char ch_name);

// Maximum performance alternatives
int format_timestamp_line_lookup_optimized(char* out, size_t out_size, const Timestamp64* t, char ch_name);
int format_timestamp_line_unrolled(char* out, size_t out_size, const Timestamp64* t, char ch_name);
int format_timestamp_line_direct_memory(char* out, size_t out_size, const Timestamp64* t, char ch_name);

// Ultra-high performance alternatives (targeting 500+ measurements/second)
int format_timestamp_line_ultra_minimal(char* out, size_t out_size, const Timestamp64* t, char ch_name);
int format_timestamp_line_static_buffer(char* out, size_t out_size, const Timestamp64* t, char ch_name);
void format_timestamp_line_direct_serial(char* out, size_t out_size, const Timestamp64* t, char ch_name);

// Test function for print64.cpp - call this at startup to verify functionality
void test_print64_function();

#endif  // PRINT64_H
