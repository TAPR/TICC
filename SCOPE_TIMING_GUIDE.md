# Oscilloscope-Based Timing Measurement Guide

## Overview

This guide describes how to use oscilloscope pulse-width measurements to accurately characterize TICC performance. Unlike the TICC-to-TICC measurement method, oscilloscope measurements provide direct pulse width readings that are easier to interpret.

## Hardware Setup

### Required Equipment
- **Oscilloscope:** 2-channel recommended (can use 1-channel for single measurements)
- **Test TICC:** Running instrumented firmware from `spi-timing-test` branch
- **Function Generator:** For tests requiring input signals (~1 kHz square wave)

### Connections
```
Scope CH1 → Test TICC Pin A0 (Primary timing channel)
Scope CH2 → Test TICC Pin A7 (Secondary timing channel - optional)
Scope GND → Test TICC GND

For live signal tests:
Function Gen → Test TICC Channel A input
```

### Scope Settings
- **Trigger:** CH1 rising edge
- **Time base:** Appropriate for measurement (10-100 µs/div for SPI tests)
- **Voltage:** 5V scale (Arduino logic levels)
- **Statistics:** Enable min/max/mean/std dev if available
- **Cursors:** For manual pulse width measurement

## Test Configurations

All configurations are in `TICC/timing_scope.h`. Three easy ways to configure:

### Method 1: Use Presets (Recommended)

Uncomment ONE preset at the bottom of `timing_scope.h`:

#### SCOPE_PRESET_SPI_COMPARISON (Most Useful)
Simultaneously compares baseline vs optimized SPI reads on 2 scope channels:
- **CH1 (A0):** Baseline - 5 × readReg24 with digitalWrite
- **CH2 (A1):** Optimized - auto-increment + direct CSB
- **No input signal needed** (synthetic test)
- **Expected CH1:** ~236 µs pulse width
- **Expected CH2:** ~132 µs pulse width
- **Visual comparison** of 44% speedup

#### SCOPE_PRESET_BREAKDOWN
Measures SPI reads + timestamp calculation:
- **CH1 (A0):** Optimized SPI reads
- **CH2 (A1):** Timestamp accumulation arithmetic
- **No input signal needed**
- Shows relative time of different components

#### SCOPE_PRESET_LIVE_PROCESSING  
Measures complete processing with real measurements:
- **CH1 (A0):** Full calculate_timestamp() function
- **Requires 1 kHz input signal** on test TICC Channel A
- Shows total processing time per measurement
- **Expected:** ~200-400 µs depending on optimizations

### Method 2: Manual Configuration

For more control, enable `TIMING_USE_SCOPE_MODE` and select individual tests:

**Channel 1 (A0) Options:**
- `SCOPE_CH1_SPI_READS` - Baseline 5-transaction SPI (~236 µs)
- `SCOPE_CH1_SPI_OPTIMIZED` - Optimized 2-transaction SPI (~132 µs)
- `SCOPE_CH1_FULL_PROCESSING` - Complete measurement processing (~200-400 µs, needs input)
- `SCOPE_CH1_READY_NEXT` - Just ready_next() call (~25-30 µs)
- `SCOPE_CH1_TOF_CALC` - Just TOF arithmetic (~20 µs)

**Channel 2 (A1) Options:**
- `SCOPE_CH2_SPI_READS` - Baseline SPI
- `SCOPE_CH2_SPI_OPTIMIZED` - Optimized SPI
- `SCOPE_CH2_TIMESTAMP_CALC` - Timestamp accumulation (~30 µs)
- `SCOPE_CH2_LED_OPS` - LED on/off operations (~10 µs)

### Method 3: Edit SCOPE_SAMPLE_EVERY_N

Control how often timing pulses are generated:
- **1** (default for synthetic): Every iteration
- **10**: Every 10th measurement (for high-rate inputs)
- **100**: Every 100th measurement (for sustained testing)

## Running Tests

### Test Procedure

1. **Edit** `TICC/timing_scope.h` 
   - Uncomment desired preset OR manually select tests
   - Adjust `SCOPE_SAMPLE_EVERY_N` if needed

2. **Compile and upload** to test TICC

3. **Connect scope** probes to pins A0 and A1

4. **For live tests:** Connect function generator (1 kHz) to test TICC Channel A

5. **Trigger scope** on CH1 rising edge

6. **Measure pulse widths**
   - Use scope cursor measurements
   - Or enable scope statistics (mean/min/max)
   - Collect data for 30+ samples

7. **Record results** in `TIMING_TEST_RESULTS.md`

### Interpreting Results

**Pulse Width = Execution Time**
- CH1 pulse width directly shows CH1 test execution time
- CH2 pulse width directly shows CH2 test execution time
- No calculation needed - scope shows the time!

**Gaps Between Pulses:**
- For synthetic tests: Gap shows loop overhead
- For live signal tests: Gap shows wait time + loop overhead

## Recommended Test Sequence

### Phase 1: Verify Previous Results (No input needed)

**Test A: SPI Comparison**
```cpp
#define SCOPE_PRESET_SPI_COMPARISON
```
- Verify CH1 ~236 µs, CH2 ~132 µs
- Confirms 44% improvement seen in TICC-to-TICC tests

**Test B: Components Breakdown**
```cpp
#define SCOPE_PRESET_BREAKDOWN  
```
- Measure individual component times
- Verify estimates of SPI vs calculation time

### Phase 2: Live Processing (1 kHz input required)

**Test C: Baseline Processing**
```cpp
#define TIMING_USE_SCOPE_MODE
#define SCOPE_CH1_FULL_PROCESSING
// All optimizations OFF
```
- Measure complete processing time with baseline SPI
- Expected: ~200-250 µs pulse width

**Test D: Optimized Processing**
```cpp
#define TIMING_USE_SCOPE_MODE
#define SCOPE_CH1_FULL_PROCESSING
#define USE_AUTOINCREMENT_SPI
#define USE_DIRECT_CSB
```
- Measure complete processing time with optimizations
- Expected: ~150-180 µs pulse width (savings visible on scope!)

### Phase 3: Maximum Rate Testing

**Test E: Maximum Throughput**
- Use highest input frequency test TICC can handle
- Measure processing pulse width
- If pulses start overlapping, system is at max rate
- Gap between pulses shows available headroom

## Expected Results

### Verification of Previous Tests

| Measurement | Scope CH | Expected Pulse Width | Compares to Test |
|-------------|----------|----------------------|------------------|
| Baseline SPI | CH1 | ~236 µs | Test 1 |
| Optimized SPI | CH1 | ~132 µs | Test 3 |
| TOF Calculation | CH1 | ~20 µs | (new) |
| ready_next() | CH1 | ~25-30 µs | (new) |
| Timestamp calc | CH2 | ~30 µs | (new) |
| LED operations | CH2 | ~10 µs | (new) |
| Full processing (baseline) | CH1 | ~250 µs | (new) |
| Full processing (optimized) | CH1 | ~175 µs | (new) |

### Advantages Over TICC-to-TICC Method

✅ **Direct measurement** - pulse width = execution time (no calculation)  
✅ **Visual confirmation** - see the optimization impact on screen  
✅ **Easier interpretation** - no ambiguity about what's being measured  
✅ **Better for live tests** - can measure actual processing time with real signals  
✅ **Simultaneous measurements** - 2-channel scope measures multiple things at once  
✅ **No sample rate limitations** - scope can capture every iteration  

### Disadvantages

❌ Requires oscilloscope equipment  
❌ Slightly less precision than TICC (but sufficient for µs-scale measurements)  

## Data Recording

Record results in table format:

```
Test: SPI Comparison
Date: [date]
Config: SCOPE_PRESET_SPI_COMPARISON

CH1 (Baseline SPI):
  Mean: _____ µs
  Min:  _____ µs  
  Max:  _____ µs
  Std:  _____ µs

CH2 (Optimized SPI):
  Mean: _____ µs
  Min:  _____ µs
  Max:  _____ µs
  Std:  _____ µs

Improvement: _____ µs (___%)
```

## Troubleshooting

**No pulses on scope:**
- Check TIMING_USE_SCOPE_MODE is defined
- Check at least one SCOPE_CH1_* or SCOPE_CH2_* test is enabled
- Verify pin connections (A0=CH1, A1=CH2)
- For live tests: verify input signal is connected and working

**Pulses too fast/slow:**
- Adjust SCOPE_SAMPLE_EVERY_N in timing_scope.h
- Higher N = less frequent pulses
- Lower N = more frequent pulses

**Pulses overlapping:**
- Increase SCOPE_SAMPLE_EVERY_N to reduce sample rate
- Or system is at maximum throughput (interesting result!)

**Different results than TICC-to-TICC method:**
- Small differences expected due to measurement method
- Large differences indicate issue - check configuration

## Notes

- Scope measurements have ~1-10 ns resolution (sufficient for µs-scale timing)
- Statistics functions on modern scopes provide excellent data
- Can use scope screenshots for documentation
- Single-shot mode useful for capturing timing anomalies
- Persistence display shows timing variation visually

---

**Ready to measure!** Start with `SCOPE_PRESET_SPI_COMPARISON` to verify the 44% improvement.

