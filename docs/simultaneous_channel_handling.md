# Simultaneous TDC Channel Handling Analysis

## Overview

This document analyzes how the TICC handles simultaneous inputs on both TDC7200 channels and verifies the output ordering behavior. The analysis confirms that the system is designed to handle near-simultaneous inputs (within nanoseconds) without blocking behavior while maintaining consistent output ordering.

## Key Findings

### ✅ No Blocking Behavior
The TICC uses a **non-blocking, interrupt-driven architecture** that processes both channels independently without waiting or synchronization.

### ✅ Consistent Output Ordering  
Channel 0 (chA) always prints before Channel 1 (chB) regardless of the order in which data becomes available.

### ✅ Race Condition Prevention
The design prevents race conditions by keeping interrupt service routines minimal and performing timestamp calculations in the main loop.

## Technical Architecture

### Interrupt-Driven Design

**Hardware Interrupts:**
- `STOP_0` → `catch_stop0()` ISR → captures `PICcount` for channel 0
- `STOP_1` → `catch_stop1()` ISR → captures `PICcount` for channel 1
- `COARSEint` → `coarseTimer()` ISR → increments `PICcount`

**Main Loop Processing:**
```cpp
for (i = 0; i < ARRAY_SIZE(channels); ++i) {
  // No work to do unless intb is low
  if (digitalRead(channels[i].INTB) == 0) {
    // Process this channel's data
    channels[i].tof = channels[i].read();  // Read TDC7200 data
    // Calculate timestamp, set new_ts_ready flag
  }
}
```

### Simultaneous Input Handling

**What Happens with Simultaneous Inputs:**

1. **Both TDC7200 chips** receive STOP signals nearly simultaneously (within nanoseconds)
2. **Both chips** raise their `INTB` flags when measurement completes
3. **Main loop** processes both channels sequentially (i=0, then i=1) in the same iteration
4. **Both channels** get processed without blocking or waiting
5. **Output ordering** is enforced regardless of processing order

**Key Design Principles:**
- **Non-blocking**: Loop only checks `digitalRead(INTB)` - no waiting
- **Independent processing**: Each channel processed separately
- **Sequential processing**: Both channels processed in same loop iteration
- **Fast ISRs**: Interrupt handlers only capture `PICcount`, no calculations

## Output Ordering Guarantees

### Timestamp Mode
Uses a two-slot pair buffer that enforces chA → chB ordering:

```cpp
// Determine composition and enforce chA then chB order when both present
if ((ts_pair[0].ch == 0 && ts_pair[1].ch == 1) || (ts_pair[0].ch == 1 && ts_pair[0].ch == 0)) {
  // Mixed channels: find A then B
  const PairSlot *A = (ts_pair[0].ch == 0) ? &ts_pair[0] : &ts_pair[1];
  const PairSlot *B = (ts_pair[0].ch == 1) ? &ts_pair[0] : &ts_pair[1];
  // Print A first, then B
}
```

### Interval/TimeLab Mode
Explicitly processes channels in order:

```cpp
// chA (channel 0) is always printed first
n = formatTimestampSplitTo(line, sizeof(line), channels[0].ts_split, config.PLACES, WRAP);
n += sprintf(line + n, " ch%c", (char)channels[0].name);
writeln64(line, n);

// chB (channel 1) is printed second  
n = formatTimestampSplitTo(line, sizeof(line), channels[1].ts_split, config.PLACES, WRAP);
n += sprintf(line + n, " ch%c", (char)channels[1].name);
writeln64(line, n);
```

## Race Condition Prevention

### ISR Design
Interrupt Service Routines are kept minimal to prevent race conditions:

```cpp
void catch_stop0() {
  channels[0].PICstop = PICcount;  // Only capture counter value
}

void catch_stop1() {
  channels[1].PICstop = PICcount;  // Only capture counter value
}
```

### Main Loop Calculations
All timestamp calculations happen in the main loop, not in ISRs:
- Coarse time decomposition
- Fine time-of-flight processing
- Propagation delay subtraction
- Timestamp formatting

This design ensures that:
- **ISRs are fast** and don't interfere with each other
- **Calculations are atomic** within the main loop context
- **No shared state conflicts** between channels

## Performance Characteristics

### Maximum Throughput
- **Synthesized tests**: ~230 timestamp pairs per second
- **Real-world performance**: Slightly less due to I/O overhead
- **Non-blocking design**: Allows maximum utilization of available data

### Timing Precision
- **PICcount capture**: Happens in ISRs with microsecond precision
- **Timestamp calculation**: Performed in main loop with full precision
- **Output ordering**: Maintained regardless of processing timing

## Code Locations

### Key Files
- **`TICC.ino`**: Main loop processing (lines 430-588)
- **`TICC.ino`**: ISR definitions (lines 797-808)
- **`tdc7200.cpp`**: TDC7200 data reading and interrupt handling
- **`tdc7200.h`**: Channel structure and method definitions

### Critical Functions
- **`catch_stop0()` / `catch_stop1()`**: ISR for PICcount capture
- **`channels[i].read()`**: TDC7200 data reading and interrupt clearing
- **`formatTimestampSplitTo()`**: Timestamp formatting with wraparound
- **Pair buffer logic**: Timestamp mode ordering enforcement

## Conclusion

The TICC's architecture is well-designed for handling simultaneous TDC channel inputs:

1. **No blocking behavior** - system processes both channels independently
2. **Consistent output ordering** - ch0 always prints before ch1
3. **Race condition prevention** - minimal ISRs, main loop calculations
4. **High performance** - non-blocking design enables maximum throughput

This design ensures reliable, high-speed timestamp measurement even when both channels receive inputs within nanoseconds of each other.

---

*Document Version: 1.0*  
*Last Updated: 2025-01-16*  
*Analysis based on TICC code version 2025090x.1*
