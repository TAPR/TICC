# SPI Timing Test Branch

This branch adds performance measurement instrumentation using pin A0 as a timing output.

## Purpose

Test whether implementing TDC7200 auto-increment SPI mode would provide worthwhile
performance improvements. This branch allows precise measurement of current SPI read
timing before implementing optimizations.

## Hardware Setup

### Two-TICC Test Configuration

**Test TICC (running this instrumented firmware):**
- Compile and upload this branch
- Connect pin A0 to Channel A input of measurement TICC
- Feed ~1 kHz test signal to one channel

**Measurement TICC (running normal firmware):**
- Run normal release firmware
- Configure for Interval mode (measures pulse widths on Channel A)
- OR use Timestamp mode and calculate intervals offline
- Channel A input connected to test TICC pin A0

### Test Signal

- **Frequency:** ~1 kHz recommended (well below max measurement rate)
- **Duration:** 10-60 seconds (collect 10,000 to 60,000 samples)
- **Amplitude:** Standard TICC input levels

## Available Timing Measurements

Edit `TICC/timing_test.h` and uncomment ONE test at a time:

### 1. TIMING_TEST_SPI_READS (Default)
Measures just the 5 × readReg24 SPI transactions in the read() function.
- **Expected:** ~80-120 µs
- **Location:** tdc7200.cpp, read() function
- **What it measures:** Pure SPI transaction overhead

### 2. TIMING_TEST_FULL_READ
Measures entire read() function including:
- 5 × readReg24 calls
- TOF calculation arithmetic
- tdc_ack_int() (2 more SPI transactions)
- **Expected:** ~150-180 µs
- **Location:** tdc7200.cpp, read() function

### 3. TIMING_TEST_READY_NEXT
Measures the ready_next() call that re-enables the TDC for next measurement.
- **Expected:** ~25-35 µs
- **Location:** tdc7200.cpp, ready_next() function
- **What it measures:** Single SPI write transaction

### 4. TIMING_TEST_FULL_CALC
Measures the complete calculate_timestamp() function:
- read() (with all its components)
- ready_next()
- Timestamp arithmetic and accumulation
- **Expected:** ~160-200 µs
- **Location:** timestamps.cpp, calculate_timestamp() function

### 5. TIMING_TEST_OVERHEAD
To measure just the timing pin toggle overhead:
1. Manually add to loop in TICC.ino:
   ```cpp
   TIMING_PIN_HIGH;
   TIMING_PIN_LOW;
   ```
2. **Expected:** ~250 ns (2 CPU cycles @ 16 MHz)

## Running Tests

### Procedure

1. Edit `TICC/timing_test.h` - uncomment desired test
2. Verify ONLY ONE test is enabled
3. Compile and upload to test TICC
4. Connect hardware as described above
5. Feed test signal to test TICC (~1 kHz)
6. Collect data from measurement TICC
7. Analyze timing statistics

### Data Collection

**Option A: Use Interval Mode on Measurement TICC**
- Direct pulse width measurements
- Each line = one pulse width = one execution time
- Collect 10,000+ samples

**Option B: Use Timestamp Mode on Measurement TICC**
- Calculate intervals offline: `interval[n] = timestamp[n+1] - timestamp[n]`
- Alternate intervals are pulse widths (HIGH time) vs gaps (LOW time)

### Statistical Analysis

Calculate from collected data:
- **Mean:** Average execution time
- **Min:** Best case (typical of optimal path)
- **Max:** Worst case (may include interrupt delays)
- **Std Dev:** Timing jitter
- **Distribution:** Look for modes, outliers

Example Python analysis:
```python
import numpy as np
data = np.loadtxt('timing_data.txt')  # Load interval measurements
print(f"Mean: {np.mean(data)*1e6:.2f} µs")
print(f"Min:  {np.min(data)*1e6:.2f} µs")
print(f"Max:  {np.max(data)*1e6:.2f} µs")
print(f"Std:  {np.std(data)*1e6:.2f} µs")
```

## Timing Pin Details

- **Pin:** Arduino Mega 2560 pin A0
- **Port:** PORTF bit 0
- **Toggle time:** ~125 ns (2 CPU cycles @ 16 MHz)
- **Macro:** `TIMING_PIN_HIGH` / `TIMING_PIN_LOW`
- **Method:** Direct port manipulation (not digitalWrite)

## Expected Results

Based on preliminary analysis:

| Measurement | Expected Time | Components |
|-------------|---------------|------------|
| SPI Reads (5×) | 80-120 µs | 5 × readReg24 @ 20 MHz |
| Full read() | 150-180 µs | SPI + arithmetic + tdc_ack_int |
| ready_next() | 25-35 µs | 1 SPI write |
| Full calc | 160-200 µs | read + ready_next + timestamp math |
| Pin toggle | ~0.25 µs | 2 port operations |

## Future Optimization

After collecting baseline timing data, the next step is to implement TDC7200
auto-increment mode and measure the improvement:

**Proposed optimization:**
- Replace 5 × readReg24 with 2 auto-increment reads
- Read TIME1→TIME2 (9 bytes continuous)
- Read CALIBRATION1→CALIBRATION2 (6 bytes continuous)
- **Expected savings:** ~20-40 µs per measurement

## Files Modified

- `TICC/board.h` - Added TIMING_PIN definitions and fast macros
- `TICC/setup.cpp` - Initialize timing pin as OUTPUT
- `TICC/tdc7200.cpp` - Instrumentation in read() and ready_next()
- `TICC/timestamps.cpp` - Instrumentation in calculate_timestamp()
- `TICC/timing_test.h` - Configuration for selecting tests (NEW FILE)
- `TIMING_TEST_README.md` - This file (NEW FILE)

## Notes

- Only one test should be enabled at a time
- Timing pin overhead (~250 ns) is negligible compared to µs-scale measurements
- Use sustained test rates to get realistic performance under load
- External interrupts may cause occasional timing outliers (this is normal)
- The measurement TICC provides picosecond-accurate timing - far better than
  software timing methods

## Returning to Normal Operation

```bash
git checkout main
```

This removes all instrumentation and returns to production firmware.

