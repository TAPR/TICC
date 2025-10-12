// utils.h -- TICC utility functions
//
// Copyright John Ackermann N8UR 2016-2025
// Licensed under BSD 2-clause license

#ifndef UTILS_H
#define UTILS_H

// Serial input buffer
extern char serial_char;  // Last character read from serial (for config and poll gating)

// Reference clock monitoring
bool check_reference_clock();

// Channel pairing helpers
bool both_channels_ready();
void consume_both_flags();

// Poll gating functions
bool poll_gating_ok();
bool check_poll_gating();

// Config menu handling
bool handle_config_request();

#endif // UTILS_H
