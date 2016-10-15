#ifndef USERCONFIG_H
#define USERCONFIG_H

// User Configuration routines for TICC Time interval Counter
// based on TICC Shield using TDC7200
// version 0.01 26 August 2016

enum MeasureMode : unsigned char {timestamp, period, interval, timelab};

MeasureMode UserConfig();

#endif /* UserConfig.h */

