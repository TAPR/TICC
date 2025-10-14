# Oscilloscope Timing Tests - Quick Start

## Quick Setup for Verification Tests

### Test 1: Compare Baseline vs Optimized SPI (Easiest)

**Hardware:**
- Scope CH1 → Pin A0
- Scope CH2 → Pin A1  
- No input signal needed

**Configuration** in `TICC/timing_scope.h`:
```cpp
#define SCOPE_PRESET_SPI_COMPARISON
```

**Compile, upload, measure:**
- Scope shows two pulse widths simultaneously
- **CH1 (A0):** ~236 µs (baseline SPI)
- **CH2 (A1):** ~132 µs (optimized SPI)
- **44% improvement** visible directly on scope!

---

### Test 2: Full Processing with Real Signal

**Hardware:**
- Scope CH1 → Pin A0
- Function gen (1 kHz) → Test TICC Channel A input
- Input signal required

**Configuration** in `TICC/timing_scope.h`:
```cpp
#define SCOPE_PRESET_LIVE_PROCESSING
```

**Compile, upload, measure:**
- Scope shows processing time per measurement
- Pulse width = actual processing time
- Gap between pulses = idle/wait time
- **Baseline:** ~250 µs pulse width
- **Optimized:** ~175 µs pulse width (enable USE_AUTOINCREMENT_SPI + USE_DIRECT_CSB)

---

## All Available Presets

Uncomment ONE in `TICC/timing_scope.h`:

```cpp
//#define SCOPE_PRESET_SPI_COMPARISON    // CH1:baseline, CH2:optimized (no signal)
//#define SCOPE_PRESET_BREAKDOWN         // CH1:SPI, CH2:timestamp calc (no signal)
//#define SCOPE_PRESET_LIVE_PROCESSING   // CH1:full processing (needs 1 kHz input)
```

## Sample Rate Control

Adjust in `timing_scope.h`:
```cpp
#define SCOPE_SAMPLE_EVERY_N  1    // Every iteration (default for synthetic)
// #define SCOPE_SAMPLE_EVERY_N  100   // Every 100th (for live tests)
```

## Quick Results Template

```
Test: [name]
Date: [date]

CH1 Pulse Width: _____ µs (mean)
CH2 Pulse Width: _____ µs (mean)

Improvement: _____ %
Matches TICC test: [Yes/No]
```

---

**For detailed information, see `SCOPE_TIMING_GUIDE.md`**

