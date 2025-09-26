// timestamp_print.h -- optimized 64-bit printing routines for TICC

#ifndef TIMESTAMP_PRINT_H
#define TIMESTAMP_PRINT_H

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
void test_places_and_wrap();

// Function to update cached config parameters (call when config changes)
void update_cached_config();

// Timestamp difference functions moved to timestamp_utils.h

#endif // TIMESTAMP_PRINT_H