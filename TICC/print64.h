// print64.h -- optimized 64-bit printing routines for TICC

#ifndef PRINT64_H
#define PRINT64_H

#include "tdc7200.h"  // For Timestamp64 definition

// Ultra-fast timestamp formatting using advisor's optimized approach
// Performance: 490 measurements/second
int format_timestamp_line_direct_memory(
  char* out,
  size_t out_size,
  const Timestamp64* t,
  char ch_name
);

// Test function for verifying the optimized print routine
void test_optimized_print();

#endif // PRINT64_H