#ifndef TIMING_TEST_H
#define TIMING_TEST_H

// TIMING TEST CONFIGURATION
// ========================
// This file controls which code sections are instrumented for performance measurement
// using pin A0 as a timing output. Connect A0 to another TICC running normal firmware
// to get picosecond-accurate timing measurements.
//
// USAGE:
// 1. Uncomment ONE timing test section below
// 2. Compile and upload to test TICC
// 3. Connect pin A0 on test TICC to Channel A input on measurement TICC
// 4. Feed ~1kHz test signal to test TICC
// 5. Run measurement TICC in Interval or Timestamp mode
// 6. Analyze the timing data from measurement TICC
//
// TIMING PIN: Arduino Mega A0 (Port F, bit 0)
//
// Available measurements:

// Uncomment ONE of these:
#define TIMING_TEST_SPI_READS        // Measure just the 5 × readReg24 calls (~100 µs expected)
// #define TIMING_TEST_FULL_READ        // Measure entire read() function including arithmetic and tdc_ack_int()
// #define TIMING_TEST_READY_NEXT       // Measure the ready_next() call
// #define TIMING_TEST_FULL_CALC        // Measure calculate_timestamp() function (read + ready_next + arithmetic)
// #define TIMING_TEST_OVERHEAD         // Measure just the timing pin toggle overhead (should be ~250ns)

// WARNING: Only define ONE test at a time! Multiple definitions will produce confusing results.

#endif /* TIMING_TEST_H */

