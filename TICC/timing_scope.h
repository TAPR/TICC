#ifndef TIMING_SCOPE_H
#define TIMING_SCOPE_H

// OSCILLOSCOPE TIMING TEST CONFIGURATION
// =======================================
// This file configures pulse-width timing tests for oscilloscope measurement.
// Pulses go HIGH at start and LOW at end of code sections, allowing direct
// measurement of execution time via scope pulse width measurement.
//
// PINS USED:
//   Pin A0 (PF0) - Primary timing channel (Scope CH1)
//   Pin A7 (PF7) - Secondary timing channel (Scope CH2) - for simultaneous measurements
//
// USAGE WITH 2-CHANNEL SCOPE:
//   Connect both pins to scope channels for simultaneous measurements
//   Set trigger on CH1 rising edge
//   Measure pulse widths, analyze statistics
//
// To enable scope timing mode:
//   #define TIMING_USE_SCOPE_MODE  (uncomment below)
//
// Then select which tests to run by uncommenting the desired measurements

// ============================================================================
// ENABLE SCOPE MODE
// ============================================================================
//#define TIMING_USE_SCOPE_MODE     // Enable oscilloscope pulse-width timing mode

// ============================================================================
// TEST SELECTION (Choose tests to run)
// ============================================================================
// You can enable MULTIPLE tests if using 2-channel scope:
//   CH1 (A0): Enable one test here
//   CH2 (A1): Enable one test in CH2 section

// CHANNEL 1 (Pin A0) Tests - uncomment ONE:
//#define SCOPE_CH1_SPI_READS           // 5 × readReg24 baseline
//#define SCOPE_CH1_SPI_OPTIMIZED       // Auto-increment + direct CSB optimized
//#define SCOPE_CH1_FULL_PROCESSING     // Complete calculate_timestamp() with real signal
//#define SCOPE_CH1_READY_NEXT          // Just the ready_next() call
//#define SCOPE_CH1_TOF_CALC            // Just TOF arithmetic (inside read())

// CHANNEL 2 (Pin A1) Tests - uncomment ONE (optional):
//#define SCOPE_CH2_SPI_READS           // 5 × readReg24 baseline
//#define SCOPE_CH2_SPI_OPTIMIZED       // Auto-increment + direct CSB optimized
//#define SCOPE_CH2_TIMESTAMP_CALC      // Timestamp accumulation arithmetic
//#define SCOPE_CH2_LED_OPS             // LED on/off operations

// ============================================================================
// CONVENIENCE CONFIGURATIONS (Uncomment ONE preset to auto-configure both channels)
// ============================================================================

// Compare baseline vs optimized SPI (most useful initial test)
//#define SCOPE_PRESET_SPI_COMPARISON
#ifdef SCOPE_PRESET_SPI_COMPARISON
  #define TIMING_USE_SCOPE_MODE
  #define SCOPE_CH1_SPI_READS          // CH1: Baseline
  #define SCOPE_CH2_SPI_OPTIMIZED      // CH2: Optimized (auto-incr + direct CSB)
#endif

// Measure SPI reads + other components
//#define SCOPE_PRESET_BREAKDOWN
#ifdef SCOPE_PRESET_BREAKDOWN
  #define TIMING_USE_SCOPE_MODE
  #define SCOPE_CH1_SPI_OPTIMIZED      // CH1: Optimized SPI reads
  #define SCOPE_CH2_TIMESTAMP_CALC     // CH2: Timestamp accumulation
#endif

// Full processing time with real signal (no output)
//#define SCOPE_PRESET_LIVE_PROCESSING
#ifdef SCOPE_PRESET_LIVE_PROCESSING
  #define TIMING_USE_SCOPE_MODE
  #define SCOPE_CH1_FULL_PROCESSING    // CH1: Complete processing cycle
  // CH2 unused - could add another measurement
#endif

// Full cycle time with output (includes printing)
// NOTE: This does NOT use TIMING_USE_SCOPE_MODE - it runs normal code path with markers
#define SCOPE_PRESET_LIVE_WITH_OUTPUT
#ifdef SCOPE_PRESET_LIVE_WITH_OUTPUT
  #define SCOPE_CH1_FULL_CYCLE_WITH_OUTPUT  // CH1: From INTB to after print
#endif

// ============================================================================
// MEASUREMENT CONTROL
// ============================================================================

// For synthetic tests (no input signal): measure every iteration
// For real signal tests: can also measure every measurement with scope
#ifdef SCOPE_CH1_FULL_PROCESSING
  #define SCOPE_SAMPLE_EVERY_N  1     // Measure every measurement (scope handles it fine)
#else
  #define SCOPE_SAMPLE_EVERY_N  1     // Sample every iteration for synthetic tests
#endif

// Counter for sample rate control
extern volatile uint32_t scope_counter;

// ============================================================================
// INSTRUMENTATION MACROS
// ============================================================================
// These macros insert timing signals based on the enabled tests
// Used throughout the codebase to mark start/end of code sections

// Check if we should generate timing signal on this iteration
#ifdef TIMING_USE_SCOPE_MODE
  #define SCOPE_SHOULD_SAMPLE() ((scope_counter % SCOPE_SAMPLE_EVERY_N) == 0)
#else
  #define SCOPE_SHOULD_SAMPLE() (0)  // Never sample if scope mode disabled
#endif

// Channel 1 (A0) macros
#ifdef SCOPE_CH1_SPI_READS
  #define SCOPE_CH1_START_SPI_BASELINE() if(SCOPE_SHOULD_SAMPLE()) TIMING_PIN_HIGH
  #define SCOPE_CH1_END_SPI_BASELINE()   if(SCOPE_SHOULD_SAMPLE()) TIMING_PIN_LOW
#else
  #define SCOPE_CH1_START_SPI_BASELINE()
  #define SCOPE_CH1_END_SPI_BASELINE()
#endif

#ifdef SCOPE_CH1_SPI_OPTIMIZED
  #define SCOPE_CH1_START_SPI_OPTIMIZED() if(SCOPE_SHOULD_SAMPLE()) TIMING_PIN_HIGH
  #define SCOPE_CH1_END_SPI_OPTIMIZED()   if(SCOPE_SHOULD_SAMPLE()) TIMING_PIN_LOW
#else
  #define SCOPE_CH1_START_SPI_OPTIMIZED()
  #define SCOPE_CH1_END_SPI_OPTIMIZED()
#endif

#ifdef SCOPE_CH1_FULL_PROCESSING
  #define SCOPE_CH1_START_FULL_PROCESSING() if(SCOPE_SHOULD_SAMPLE()) TIMING_PIN_HIGH
  #define SCOPE_CH1_END_FULL_PROCESSING()   if(SCOPE_SHOULD_SAMPLE()) TIMING_PIN_LOW
#else
  #define SCOPE_CH1_START_FULL_PROCESSING()
  #define SCOPE_CH1_END_FULL_PROCESSING()
#endif

#ifdef SCOPE_CH1_READY_NEXT
  #define SCOPE_CH1_START_READY_NEXT() if(SCOPE_SHOULD_SAMPLE()) TIMING_PIN_HIGH
  #define SCOPE_CH1_END_READY_NEXT()   if(SCOPE_SHOULD_SAMPLE()) TIMING_PIN_LOW
#else
  #define SCOPE_CH1_START_READY_NEXT()
  #define SCOPE_CH1_END_READY_NEXT()
#endif

#ifdef SCOPE_CH1_TOF_CALC
  #define SCOPE_CH1_START_TOF_CALC() if(SCOPE_SHOULD_SAMPLE()) TIMING_PIN_HIGH
  #define SCOPE_CH1_END_TOF_CALC()   if(SCOPE_SHOULD_SAMPLE()) TIMING_PIN_LOW
#else
  #define SCOPE_CH1_START_TOF_CALC()
  #define SCOPE_CH1_END_TOF_CALC()
#endif

// Channel 2 (A1) macros
#ifdef SCOPE_CH2_SPI_READS
  #define SCOPE_CH2_START_SPI_BASELINE() if(SCOPE_SHOULD_SAMPLE()) TIMING_PIN2_HIGH
  #define SCOPE_CH2_END_SPI_BASELINE()   if(SCOPE_SHOULD_SAMPLE()) TIMING_PIN2_LOW
#else
  #define SCOPE_CH2_START_SPI_BASELINE()
  #define SCOPE_CH2_END_SPI_BASELINE()
#endif

#ifdef SCOPE_CH2_SPI_OPTIMIZED
  #define SCOPE_CH2_START_SPI_OPTIMIZED() if(SCOPE_SHOULD_SAMPLE()) TIMING_PIN2_HIGH
  #define SCOPE_CH2_END_SPI_OPTIMIZED()   if(SCOPE_SHOULD_SAMPLE()) TIMING_PIN2_LOW
#else
  #define SCOPE_CH2_START_SPI_OPTIMIZED()
  #define SCOPE_CH2_END_SPI_OPTIMIZED()
#endif

#ifdef SCOPE_CH2_TIMESTAMP_CALC
  #define SCOPE_CH2_START_TIMESTAMP_CALC() if(SCOPE_SHOULD_SAMPLE()) TIMING_PIN2_HIGH
  #define SCOPE_CH2_END_TIMESTAMP_CALC()   if(SCOPE_SHOULD_SAMPLE()) TIMING_PIN2_LOW
#else
  #define SCOPE_CH2_START_TIMESTAMP_CALC()
  #define SCOPE_CH2_END_TIMESTAMP_CALC()
#endif

#ifdef SCOPE_CH2_LED_OPS
  #define SCOPE_CH2_START_LED_OPS() if(SCOPE_SHOULD_SAMPLE()) TIMING_PIN2_HIGH
  #define SCOPE_CH2_END_LED_OPS()   if(SCOPE_SHOULD_SAMPLE()) TIMING_PIN2_LOW
#else
  #define SCOPE_CH2_START_LED_OPS()
  #define SCOPE_CH2_END_LED_OPS()
#endif

// Full cycle with output macros
#ifdef SCOPE_CH1_FULL_CYCLE_WITH_OUTPUT
  #define SCOPE_START_FULL_CYCLE() TIMING_PULSE()
  #define SCOPE_END_FULL_CYCLE()   TIMING_PULSE2()
#else
  #define SCOPE_START_FULL_CYCLE()
  #define SCOPE_END_FULL_CYCLE()
#endif

#endif /* TIMING_SCOPE_H */

