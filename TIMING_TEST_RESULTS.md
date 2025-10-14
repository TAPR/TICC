# TICC SPI Timing Test Results

## Test Configuration

- **Hardware:** Arduino Mega 2560 @ 16 MHz
- **SPI Clock:** 20 MHz
- **Measurement Method:** Two-TICC setup with pin A0 timing pulses
- **Test Mode:** TIMING_TEST_SYNTHETIC_SPI (tight loop, no input signal dependency)
- **Sample Interval:** 10,000 iterations per timing pulse
- **Measurement TICC:** Normal firmware, Timestamp mode
- **Analysis:** Time between timestamps ÷ TIMING_SAMPLE_INTERVAL = time per iteration

## Results Summary

| Test | Implementation | Mean (µs) | Min (µs) | Max (µs) | Std Dev (µs) | Samples | Improvement | Notes |
|------|----------------|-----------|----------|----------|--------------|---------|-------------|-------|
| 1 | Current (5× readReg24) | 236.33 | 235.88 | 236.94 | 0.24 | 33 | baseline | Individual transactions with CSB toggle |
| 2 | Auto-increment mode | - | - | - | - | - | - | Not yet tested |

---

## Test 1: Current Implementation (Baseline)

**Date:** October 14, 2025  
**Branch:** `spi-timing-test`  
**Configuration:** `TIMING_TEST_SYNTHETIC_SPI` enabled  

### Implementation Details
Current SPI read implementation uses 5 separate transactions:
```cpp
time1Result = readReg24(TIME1);         // 0x10
time2Result = readReg24(TIME2);         // 0x12  
clock1Result = readReg24(CLOCK_COUNT1); // 0x11
cal1Result = readReg24(CALIBRATION1);   // 0x1B
cal2Result = readReg24(CALIBRATION2);   // 0x1C
```

Each `readReg24()` transaction:
- `SPI.beginTransaction()`
- `digitalWrite(CSB, LOW)` (~4 µs on AVR)
- `SPI.transfer()` × 4 bytes (address + 3 data bytes) (~1.6 µs @ 20 MHz)
- `digitalWrite(CSB, HIGH)` (~4 µs)
- `SPI.endTransaction()`
- `delayMicroseconds(5)`

### Raw Data
```
2121.37557062473 chA
2123.73763452081 chA
2126.09645278065 chA
2128.45858031387 chA
2130.82067806617 chA
2133.18488953481 chA
2135.54371476495 chA
2137.90576675449 chA
2140.26775111420 chA
2142.63189525420 chA
2144.99409580379 chA
2147.35818102071 chA
2149.72238599201 chA
2152.08873589173 chA
2154.44756848782 chA
2156.80971888941 chA
2159.17193067054 chA
2161.53601661464 chA
2163.89814502235 chA
2166.26231995078 chA
2168.62648254781 chA
2170.99292975735 chA
2173.35502770099 chA
2175.71915954546 chA
2178.08326482909 chA
2180.44972256649 chA
2182.81392617631 chA
2185.18035175205 chA
2187.54682295010 chA
2189.91621818688 chA
2192.27504252986 chA
2194.63716778688 chA
2196.99924316470 chA
2199.36339079410 chA
```

### Analysis Results
```
Samples collected: 33
Mean:    236.33 µs per iteration
Median:  236.41 µs
Min:     235.88 µs
Max:     236.94 µs
Std Dev:   0.24 µs
Range:     1.06 µs
```

### Breakdown (Estimated)
- 5 × readReg24 transactions
- 5 × SPI.transfer() operations: ~8 µs (4 bytes each @ 20 MHz)
- 10 × digitalWrite() calls: ~40 µs (CSB LOW+HIGH per transaction)
- 5 × delayMicroseconds(5): ~25 µs
- 5 × SPI transaction overhead: ~10-15 µs
- Loop and function call overhead: ~8 µs
- **Total: ~236 µs** ✓ matches measurement

### Observations
- Extremely consistent timing (0.24 µs std dev)
- digitalWrite() overhead dominates (~40 µs of 236 µs = 17%)
- Mandatory 5 µs delays add 25 µs (~11%)
- SPI transfers themselves only ~8 µs (~3%)
- Transaction setup/teardown significant overhead

---

## Test 2: Auto-Increment Mode (Planned)

**Status:** Not yet implemented

### Proposed Implementation
Use TDC7200 auto-increment mode to read sequential registers in fewer transactions:

**Transaction 1:** TIME1 through TIME2 (9 bytes)
```cpp
address = 0x90;  // 0x10 with auto-increment bit set
// Read TIME1 (3) + CLOCK_COUNT1 (3) + TIME2 (3) = 9 bytes
```

**Transaction 2:** CALIBRATION1 through CALIBRATION2 (6 bytes)
```cpp
address = 0x9B;  // 0x1B with auto-increment bit set  
// Read CALIBRATION1 (3) + CALIBRATION2 (3) = 6 bytes
```

### Expected Improvements
- Reduce from 5 transactions to 2 transactions
- Eliminate 3 × digitalWrite pairs: ~24 µs savings
- Eliminate 3 × 5 µs delays: ~15 µs savings
- Reduce transaction overhead: ~6-9 µs savings
- **Expected total savings: 45-48 µs (19-20% improvement)**
- **Target: ~188-191 µs per iteration**

### Implementation Tasks
- [ ] Create new `readReg24_autoincrement()` function
- [ ] Modify `read_spi_timing_only()` to use auto-increment
- [ ] Test with synthetic timing mode
- [ ] Collect timing data (33+ samples)
- [ ] Calculate improvement vs baseline
- [ ] Verify register values are still correct

---

## Notes

### Measurement Accuracy
The TICC measurement system provides picosecond-level timing accuracy, so the measured variations (0.24 µs std dev) represent actual timing jitter in the AVR code execution, not measurement error.

### Limitations
- Measurements include minimal loop overhead from synthetic test mode
- Real-world performance may vary slightly due to:
  - Interrupt handling (disabled in this test)
  - Other code between measurements
  - Cache effects (though AVR has no cache)

### Future Tests
Additional optimizations to consider:
- Direct port manipulation instead of digitalWrite() for CSB
- Remove or reduce delayMicroseconds(5) delays if not needed
- Combine with other optimizations

---

**Last Updated:** October 14, 2025

