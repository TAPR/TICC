# SPI Timing Test Branch

This branch adds performance measurement instrumentation using pin A0 as a timing output.

## Purpose

Test whether implementing TDC7200 auto-increment SPI mode would provide worthwhile
performance improvements. This branch allows precise measurement of current SPI read
timing before implementing optimizations.

## How It Works

Since the TICC only measures rising edges (not pulse widths or falling edges), the
instrumentation uses a simple sampling approach:
1. Generate **one brief pulse** (~2 µs) every N iterations (N = TIMING_SAMPLE_INTERVAL)
2. Measurement TICC timestamps each pulse
3. **Time between consecutive timestamps** = time for N iterations
4. **Divide by N** to get average time per iteration

### Two Measurement Modes

**TIMING_TEST_SYNTHETIC_SPI (Recommended):**
- Runs SPI reads in tight loop, **independent of input signal**
- No waiting for measurements - pure processing throughput
- Isolates SPI read time from input rate

**Other modes (TIMING_TEST_SPI_READS, etc):**
- Measure during normal operation with real input signals
- Includes wait time for next measurement + processing time
- Limited by input signal rate (e.g., 1 kHz = minimum 1000 µs per cycle)

## Hardware Setup

### Two-TICC Test Configuration

**Test TICC (running this instrumented firmware):**
- Compile and upload this branch
- Connect pin A0 to Channel A input of measurement TICC
- Feed ~1 kHz test signal to one channel

**Measurement TICC (running normal firmware):**
- Run normal release firmware
- Configure for **Timestamp mode** (single channel)
- Channel A input connected to test TICC pin A0
- Will capture pairs of timestamps (start pulse, end pulse)

### Test Signal

- **Frequency:** ~1 kHz recommended (well below max measurement rate)
- **Duration:** 10-60 seconds (collect 10,000 to 60,000 samples)
- **Amplitude:** Standard TICC input levels

## Configuration

### Sample Rate Control

Edit `TICC/timing_test.h` to set the sample interval:

```cpp
#define TIMING_SAMPLE_INTERVAL 1000  // Generate one pulse per 1000 measurements
```

This controls how often timing pulses are generated:
- **1000** (default): One pulse per 1000 measurements (~1 per second @ 1 kHz input)
- **100**: More frequent sampling for quicker data collection
- **10000**: Less frequent for very long test runs
- **1**: Every measurement (generates maximum data rate - use with caution!)

At 1 kHz input with `TIMING_SAMPLE_INTERVAL = 1000`, you'll get one timing measurement per second, which is very manageable for data collection.

### Test Selection

Edit `TICC/timing_test.h` and uncomment ONE test at a time:

### 1. TIMING_TEST_SYNTHETIC_SPI (Recommended - Default)
**Synthetic tight-loop test of just SPI reads, independent of input rate**
- Bypasses normal measurement processing entirely
- Runs `read_spi_timing_only()` in tight loop
- **NOT dependent on input signal rate** - runs as fast as possible
- Measures pure SPI read throughput
- **Expected:** ~80-120 µs for 5 SPI reads
- **This is the test you want for measuring SPI optimization impact**

**Usage:**
- No input signal needed (or will be ignored)
- Generates timing pulses every TIMING_SAMPLE_INTERVAL iterations
- Each interval = time for N SPI read cycles
- Divide by N to get time per SPI read cycle

**Example:**
```
Interval: 0.100 seconds for 1000 iterations
0.100 / 1000 = 0.0001 seconds = 100 µs per SPI read cycle
```

### 2. TIMING_TEST_SPI_READS
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

With `TIMING_SAMPLE_INTERVAL = 1000` and 1 kHz input:
- Test TICC processes 1000 measurements per second
- One timing pulse generated every 1000 measurements (~once per second)
- Measurement TICC outputs **one timestamp per second**
- Very manageable data rate for long-term collection

**Timestamp Format:**
You'll see regularly-spaced timestamps like this:
```
534.70612449223 chA  ← Pulse after 1000 measurements
536.71234567890 chA  ← Pulse after next 1000 measurements
538.71856789012 chA  ← Pulse after next 1000 measurements
540.72478901234 chA  ← Pulse after next 1000 measurements
```

**Calculate Execution Time:**
```
time_for_N_measurements = timestamp[i+1] - timestamp[i]
average_time_per_measurement = time_for_N_measurements / TIMING_SAMPLE_INTERVAL
```

Example with your data:
```
959.71672889707 - 957.71673443291 = 1.99999446416 seconds
1.99999446416 / 1000 = 0.001999994 seconds = 1999.99 µs per measurement
```

This includes ALL processing time (SPI reads, arithmetic, overhead), not just the
instrumented section. For `TIMING_TEST_SPI_READS`, the pulse is generated just before
the SPI reads, so the interval represents the time from one set of SPI reads to the next.

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

# Configuration
TIMING_SAMPLE_INTERVAL = 1000  # Must match value in timing_test.h

# Load timestamps from file (first column only)
timestamps = np.loadtxt('timing_data.txt', usecols=0)

# Calculate intervals between consecutive timestamps
intervals = np.diff(timestamps)

# Convert to average time per measurement
# Each interval represents TIMING_SAMPLE_INTERVAL measurements
time_per_measurement = intervals / TIMING_SAMPLE_INTERVAL

# Convert to microseconds
time_per_measurement_us = time_per_measurement * 1e6

# Statistics
print(f"Samples: {len(time_per_measurement_us)}")
print(f"Mean:    {np.mean(time_per_measurement_us):.2f} µs per measurement")
print(f"Median:  {np.median(time_per_measurement_us):.2f} µs")
print(f"Min:     {np.min(time_per_measurement_us):.2f} µs")
print(f"Max:     {np.max(time_per_measurement_us):.2f} µs")
print(f"Std Dev: {np.std(time_per_measurement_us):.2f} µs")

# If measuring just SPI reads (TIMING_TEST_SPI_READS), this is the overhead per measurement
# Note: This includes time from one SPI read cycle to the next, not just the SPI time itself
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

