// misc.h -- miscellaneous TICC functions

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016-2025
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

// See misc.cpp for printing rationale: signed vs unsigned and formatting approach.

// Old SplitTime struct and functions removed - now using Timestamp64

void print_int64(int64_t x);
size_t format_int64_to_buffer(char *buf, size_t cap, int64_t num);

// Legacy helpers retained for compatibility
void print_timestamp_sec_frac(int64_t sec, int64_t frac_ps, int places, int32_t wrap);
void print_signed_sec_frac(int64_t sec, int64_t frac_ps, int places);

// Old SplitTime formatting functions removed - now using print_timestamp from timestamp_print.cpp
