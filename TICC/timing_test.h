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
// #define TIMING_TEST_SYNTHETIC_SPI    // Synthetic test: tight loop of SPI reads, independent of input rate
// #define TIMING_TEST_IDLE_LOOP        // Measure idle loop speed (no INTB activity)
#define TIMING_TEST_FULL_LOOP        // Measure full loop with one channel active (all calculations)
// #define TIMING_TEST_SPI_READS        // Measure just the 5 × readReg24 calls (~100 µs expected)
// #define TIMING_TEST_FULL_READ        // Measure entire read() function including arithmetic and tdc_ack_int()
// #define TIMING_TEST_READY_NEXT       // Measure the ready_next() call
// #define TIMING_TEST_FULL_CALC        // Measure calculate_timestamp() function (read + ready_next + arithmetic)
// #define TIMING_TEST_OVERHEAD         // Measure just the timing pin toggle overhead (should be ~250ns)

// WARNING: Only define ONE test at a time! Multiple definitions will produce confusing results.

// TIMING OUTPUT RATE CONTROL
// Only generate timing pulse every N iterations (reduces data volume)
// Recommended values by test type:
//   IDLE_LOOP: 100000+ (very fast, ~52 µs per iteration)
//   FULL_LOOP: 10000 (moderate, ~150 µs avg per iteration - alternates idle/process)
//   SYNTHETIC_SPI: 10000 (fast, ~130-240 µs per iteration)
//   Signal-based tests: 1000 (depends on input rate)
#define TIMING_SAMPLE_INTERVAL 10000  // Generate one pulse per N iterations

// OPTIMIZATION MODE (for TIMING_TEST_SYNTHETIC_SPI only)
// Can enable multiple optimizations to test combinations:
//#define USE_AUTOINCREMENT_SPI     // Use auto-increment mode (2 transactions instead of 5)
//#define USE_DIRECT_CSB            // Use direct port manipulation for CSB (faster than digitalWrite)

// Global counter for timing sample rate control (defined in tdc7200.cpp)
extern volatile uint32_t timing_sample_counter;

#endif /* TIMING_TEST_H */

