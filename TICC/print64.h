// print64.h -- optimized 64-bit printing routines for TICC

#ifndef PRINT64_H
#define PRINT64_H

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

// Test function for verifying the optimized print routine
void test_optimized_print();

// Function to update cached config parameters (call when config changes)
void update_cached_config();

// Timestamp difference functions
Timestamp64 timestamp_difference(const Timestamp64* a, const Timestamp64* b);
int64_t timestamp_difference_ps(const Timestamp64* a, const Timestamp64* b);

// Format time difference for output (uses PLACES but not WRAP)
int format_time_difference(
  char* out,
  size_t out_size,
  const Timestamp64* diff,
  const char* label
);

#endif // PRINT64_H