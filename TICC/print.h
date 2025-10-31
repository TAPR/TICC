// print.h -- optimized 64-bit printing routines for TICC

#ifndef PRINT_H
#define PRINT_H

#include "tdc7200.h"  // For Timestamp64 definition

// Ultra-fast timestamp formatting using advisor's optimized approach
// Performance: 548 measurements/second (tested) with PLACES/WRAP support
int print_timestamp(
  char* out,
  size_t out_size,
  const Timestamp64* t,
  char ch_name,
  bool use_wrap = true
);

// Print timestamp difference - handles canonical form for negative values
// Converts canonical form to printable form for correct display
// Less performance-critical than print_timestamp but still efficient
int print_timestamp_difference(
  char* out,
  size_t out_size,
  const Timestamp64* diff,
  char ch_name,
  bool use_wrap = true
);

// Function to update cached config parameters (call when config changes)
void update_cached_config();

// Efficient 64-bit integer printing function
void print_int64(int64_t value, bool add_crlf = true);

// Timestamp difference functions moved to timestamps.h

#endif // PRINT_H