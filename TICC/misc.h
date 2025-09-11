// misc.h -- miscellaneous TICC functions

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016-2025
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

// See misc.cpp for printing rationale: signed vs unsigned and formatting approach.

// Split timestamp representation used across the codebase
// sec: whole seconds (can be negative); frac_hi/frac_lo are decimal 6-digit chunks [0, 1e6)
struct SplitTime {
  int32_t  sec;
  uint32_t frac_hi;  // upper 6 digits of picoseconds (ps / 1e6)
  uint32_t frac_lo;  // lower 6 digits of picoseconds (ps % 1e6)
};

// Normalize so that 0 <= frac_lo < 1e6 and 0 <= frac_hi < 1e6; adjust sec accordingly
void normalizeSplit(struct SplitTime *t);

// Return b - a using signed math with proper borrow across frac_lo -> frac_hi -> sec
SplitTime diffSplit(const SplitTime &b, const SplitTime &a);

// Return |b - a| as a non-negative SplitTime
SplitTime absDeltaSplit(const SplitTime &b, const SplitTime &a);

// Printing helpers for SplitTime
void printTimestampSplit(const SplitTime &t, int places, int32_t wrap);
void printSignedSplit(const SplitTime &t, int places);

void print_int64(int64_t x);
size_t format_int64_to_buffer(char *buf, size_t cap, int64_t num);

// Legacy helpers retained for compatibility
void print_timestamp_sec_frac(int64_t sec, int64_t frac_ps, int places, int32_t wrap);
void print_signed_sec_frac(int64_t sec, int64_t frac_ps, int places);

// Buffer-formatting helpers (no Serial; return bytes written)
size_t formatTimestampSplitTo(char *buf, size_t cap, const SplitTime &t, int places, int32_t wrap);
size_t formatSignedSplitTo(char *buf, size_t cap, const SplitTime &t, int places);

// Append CRLF to a buffer (capped at 64 total) and write via Serial.write()
void writeln64(char *buf, size_t n);
