// misc.h -- miscellaneous TICC functions

// TICC Time interval Counter based on TICC Shield using TDC7200
//
// Copyright John Ackermann N8UR 2016-2020
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

void print_unsigned_picos_as_seconds (uint64_t x, int places);

void print_signed_picos_as_seconds (int64_t x, int places);

void print_timestamp(int64_t x, int places, int32_t wrap);

void print_int64(int64_t x);
