// misc.h -- miscellaneous TICC functions

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016-2020
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

// See misc.cpp for printing rationale: signed vs unsigned and formatting approach.
void print_unsigned_picos_as_seconds (uint64_t x, int places);

void print_signed_picos_as_seconds (int64_t x, int places);

void print_timestamp(int64_t x, int places, int32_t wrap);

void print_int64(int64_t x);

// Print from split seconds and fractional picoseconds to avoid 64-bit ps overflow
void print_timestamp_sec_frac(int64_t sec, int64_t frac_ps, int places, int32_t wrap);
