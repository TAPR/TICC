// misc.h -- miscellaneous TICC functions

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016-2020
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

// See misc.cpp for printing rationale: signed vs unsigned and formatting approach.

// Split timestamp representation used across the codebase
// sec: whole seconds (can be negative); frac_ps: [0, PS_PER_SEC)
struct SplitTime {
  int64_t sec;
  int64_t frac_ps;
};

// Normalize so that 0 <= frac_ps < PS_PER_SEC; adjust sec accordingly
void normalizeSplit(struct SplitTime *t);

// Return b - a using signed math and proper borrow into seconds
SplitTime diffSplit(const SplitTime &b, const SplitTime &a);

// Return |b - a| as a non-negative SplitTime
SplitTime absDeltaSplit(const SplitTime &b, const SplitTime &a);

// Printing helpers for SplitTime
void printTimestampSplit(const SplitTime &t, int places, int32_t wrap);
void printSignedSplit(const SplitTime &t, int places);

void print_int64(int64_t x);

// Print from split seconds and fractional picoseconds to avoid 64-bit ps overflow
void print_timestamp_sec_frac(int64_t sec, int64_t frac_ps, int places, int32_t wrap);

// Print signed value from split seconds and fractional picoseconds (no wrap)
void print_signed_sec_frac(int64_t sec, int64_t frac_ps, int places);
