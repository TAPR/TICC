#ifndef TICC_H
#define TICC_H

// TICC Time interval Counter based on TICC Shield using TDC7200
// version 0.79 14 October 2016

// Copyright John Ackermann N8UR 2016
// Portions Copyright George Byrkit K9TRV 2016
// Portions Copyright Jeremy McDermond NH6Z 2016
// Licensed under BSD 2-clause license

#include <Arduino.h>  // for type definitions

#define PS_PER_SEC        (int64_t)  1000000000000  // ps/s
#define PRINT_REG_RESULTS // if defined, prints time1, time2, clock1, cal1, cal2, tof before timestamp
//#define DETAIL_TIMING     // if defined, prints execution time

#endif	/* TICC_H */
