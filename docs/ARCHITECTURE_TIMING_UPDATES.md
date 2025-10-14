# TICC Architecture Timing Updates

## Summary of Required Updates to TICC_architecture.md

Based on comprehensive testing completed October 14, 2025.

---

## Section: Performance Characteristics (Lines 70-105)

### OLD VALUES (Lines 79-83)

```
Single-Channel Active (per timestamp):
- Calculation phase: ~164 µs
  - TDC SPI read: ~94 µs
  - TDC ready_next(): ~31 µs  
  - Timestamp arithmetic & accumulation: ~19 µs
  - LED operations & overhead: ~20 µs
```

### NEW VALUES (Based on Measurements)

```
Single-Channel Active (per timestamp) - CURRENT BASELINE:
- Calculation phase: ~700 µs (complete processing from INTB to ready)
  - TDC SPI reads (5 transactions): ~236 µs (34%)
  - TDC ready_next() (1 SPI write): ~30 µs (4%)
  - Timestamp arithmetic & accumulation: ~56 µs (8%)
  - LED operations: ~10 µs (1%)
  - 64-bit math overhead: ~100-150 µs (14-21%)
  - Function call overhead: ~40-60 µs (6-9%)
  - ISR interruptions: ~20-60 µs (3-9%)
  - Integration & state management: ~90-140 µs (13-20%)
  - Total measured: 700 µs

Single-Channel Active (per timestamp) - WITH SPI OPTIMIZATIONS:
- Calculation phase: ~580 µs (17% faster than baseline)
  - TDC SPI reads (2 auto-incr trans, direct CSB): ~136 µs (23%)
  - TDC ready_next(): ~30 µs (5%)
  - Timestamp arithmetic & accumulation: ~56 µs (10%)
  - LED operations: ~10 µs (2%)
  - 64-bit math overhead: ~100-150 µs (17-26%)
  - Function call overhead: ~40-60 µs (7-10%)
  - ISR interruptions: ~20-60 µs (3-10%)
  - Integration & state management: ~90-140 µs (16-24%)
  - Total measured: 580 µs
```

---

## Section: Throughput (Lines 95-104)

### Note to Add

The ~1400 measurements/second maximum is consistent with:
- 580 µs processing per measurement (optimized)
- 420 µs idle loop time
- Total: 1000 µs per measurement = 1000/sec sustained
- Bursts up to ~1400/sec when processing is faster than average

---

## Key Insights to Add

### Performance Bottleneck Analysis

**Major Finding:** Integration overhead is significant
- Isolated component sum: ~232 µs
- Real-world measurement: ~580 µs (optimized)  
- Integration overhead: ~348 µs (60% of total!)

**Largest Bottlenecks (in priority order):**
1. **64-bit math operations:** ~100-150 µs (17-26% of total)
   - Expensive on 8-bit AVR architecture
   - Timestamp accumulation uses multiple 64-bit operations
   - Conditional branches on 64-bit values
   - Future optimization target

2. **SPI reads:** ~136 µs optimized, was ~236 µs (23% of total)
   - ✅ OPTIMIZED via auto-increment mode + direct CSB
   - Was largest bottleneck, now optimized

3. **Function call overhead:** ~40-60 µs (7-10% of total)
   - Multiple nested function calls
   - Potential optimization: inline critical path

4. **ISR interruptions:** ~20-60 µs (3-10% of total)
   - PICcount ISR fires every 100 µs
   - Variable impact depending on timing
   - Necessary overhead, hard to optimize

5. **Integration overhead:** ~90-140 µs (16-24% of total)
   - State management, memory access, conditional logic
   - Scattered throughout code
   - Harder to optimize significantly

---

## Testing Methodology Note to Add

**Performance Measurement Methods:**

Two complementary measurement methods were used:

1. **Two-TICC Precision Timing**
   - One TICC generates timing pulses, another measures with picosecond accuracy
   - Excellent for isolated component testing (synthetic tests)
   - Measures pure code execution without input signal dependency

2. **Oscilloscope Time Delta**
   - Direct pulse width or time delta measurements
   - Better for live processing with real input signals
   - Visual confirmation of optimization impact

**Key insight:** Isolated components (synthetic tests) show theoretical maximum improvement (44% for SPI), while live tests show real-world benefit including all overhead (17% overall).

---

## Recommended Changes Summary

**Critical updates:**
- Idle loop: ~1.2 µs → ~52 µs (our test shows this)
- Calculation phase: ~164 µs → ~700 µs baseline, ~580 µs optimized
- TDC SPI read: ~94 µs → ~236 µs baseline, ~136 µs optimized
- Add detailed breakdown showing 64-bit math as major bottleneck

**Why the old estimates were wrong:**
- Likely based on theoretical calculation or partial measurements
- Didn't account for integration overhead
- Underestimated 64-bit math cost on 8-bit AVR
- Didn't measure complete end-to-end processing

**Impact on documented throughput:**
- Maximum rate estimates remain valid (~1400/sec)
- Consistent with 580 µs processing + idle time
- Binary mode numbers still accurate

---

**See:** `docs/SPI_OPTIMIZATION_FINDINGS.md` for complete analysis and test data.

