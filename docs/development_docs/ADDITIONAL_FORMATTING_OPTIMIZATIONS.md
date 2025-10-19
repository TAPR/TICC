# Additional Formatting Optimization Opportunities

**Date:** October 18, 2025  
**Current Performance:** 395 µs formatting time (after reciprocal division optimization)

## Remaining Bottlenecks

### 1. Seconds Formatting (Wrap Mode)

**Lines 169-190:** Wrap logic still uses expensive operations

**Wrap = 2 case (most common):**
```cpp
uint32_t mod = pgm_read_dword(&POW10_TABLE[wrap]);  // ~100-150 cycles
uint32_t sec_u = (uint32_t)sec;
sec_u = sec_u % mod;                                // ~500 cycles (modulo by 100)

p[0] = '0' + (sec_u / 10);                          // ~500 cycles
p[1] = '0' + (sec_u % 10);                          // ~500 cycles
```

**Cost:** ~1,600-1,650 cycles (~100-103 µs) for wrap=2

**Optimization opportunity:**
```cpp
if (wrap == 2) {
  uint32_t sec_u = (uint32_t)sec;
  // Fast modulo by 100 using reciprocal
  uint32_t q100 = div10_fast(div10_fast(sec_u));  // sec / 100
  uint32_t r100 = sec_u - q100 * 100;             // sec % 100
  
  // Use lookup table for 2-digit result
  memcpy_P(p, TWO_DIGIT_TABLE[r100], 2);
  p += 2;
}
```

**Expected savings:** ~80-90 µs (replaces 1 PROGMEM read + 3 divisions with 2 fast divisions + 1 lookup)

### 2. Seconds Formatting (No Wrap, Small Values)

**Lines 194-202:** Cases for 1, 2, 3 digit seconds still use division

```cpp
} else if (sec_u < 100) {
  *p++ = '0' + (sec_u / 10);      // ~500 cycles
  *p++ = '0' + (sec_u % 10);      // ~500 cycles
}
```

**Optimization:**
```cpp
} else if (sec_u < 100) {
  memcpy_P(p, TWO_DIGIT_TABLE[sec_u], 2);
  p += 2;
}
```

**Expected savings:** ~5-10 µs for common 2-digit case

### 3. PROGMEM Read for POW10_TABLE

**Line 169:** PROGMEM read happens on every call in wrap mode

```cpp
uint32_t mod = pgm_read_dword(&POW10_TABLE[wrap]);  // ~100-150 cycles
```

**Optimization:** Pre-compute common wrap values
```cpp
// At start of function or cached
static const uint32_t COMMON_WRAPS[4] = {0, 10, 100, 1000};
uint32_t mod = (wrap <= 3) ? COMMON_WRAPS[wrap] : pgm_read_dword(&POW10_TABLE[wrap]);
```

**Expected savings:** ~6-9 µs for common wrap values

### 4. Three-Digit Seconds Formatting

**Lines 199-202:** Three divisions for 3-digit case

```cpp
} else if (sec_u < 1000) {
  *p++ = '0' + (sec_u / 100);             // ~500 cycles
  *p++ = '0' + ((sec_u % 100) / 10);      // ~500 + 500 cycles
  *p++ = '0' + (sec_u % 10);              // ~500 cycles
}
```

**Optimization:** Use div10_fast + lookup
```cpp
} else if (sec_u < 1000) {
  uint8_t hundreds = sec_u / 100;  // Could optimize but 3-digit uncommon
  uint8_t tens_ones = sec_u - hundreds * 100;
  *p++ = '0' + hundreds;
  memcpy_P(p, TWO_DIGIT_TABLE[tens_ones], 2);
  p += 2;
}
```

**Expected savings:** ~10-15 µs (but 3-digit seconds are less common)

## Estimated Total Savings

**If all optimizations applied:**
- Wrap=2 optimization: ~80-90 µs (most impactful, very common case)
- Small no-wrap optimizations: ~10-20 µs
- PROGMEM cache: ~6-9 µs
- **Total potential: ~96-119 µs additional savings**

**New formatting time:** 395 µs → ~276-299 µs (30% additional improvement)  
**New total print time:** 571 µs → ~452-475 µs

## Implementation Priority

### High Priority: Wrap=2 Optimization

**Why:** Most users will use wrap=2 for typical timestamp display  
**Impact:** ~80-90 µs savings (20% of current formatting time)  
**Risk:** Low (uses same techniques as already proven to6digits optimization)  
**Complexity:** Low

### Medium Priority: Small No-Wrap Cases

**Why:** Common for short-running tests  
**Impact:** ~10-20 µs  
**Risk:** Low  
**Complexity:** Low

### Lower Priority: Other Cases

**Why:** Less frequently used  
**Impact:** ~6-15 µs  
**Risk:** Low  
**Complexity:** Low

## Recommended Next Step

Implement wrap=2 optimization first and measure:
- If formatting drops to ~310-315 µs, the optimization works as predicted
- This would bring total print time to ~477-482 µs
- Maximum sustainable rate would increase to ~1000-1050 Hz

Then evaluate whether additional optimizations are worthwhile based on user requirements.

## Additional Considerations

### memcpy_P Performance

Current implementation uses `memcpy_P()` for PROGMEM reads. This is efficient for 2-byte reads (~100-150 cycles total), but could potentially be optimized further with direct pointer access:

```cpp
// Current: memcpy_P(p, TWO_DIGIT_TABLE[idx], 2);
// Alternative:
const char* digits = &TWO_DIGIT_TABLE[idx][0];
p[0] = pgm_read_byte(digits);
p[1] = pgm_read_byte(digits + 1);
```

However, the compiler likely already optimizes memcpy_P for small constant sizes, so this may not provide measurable improvement.

### Measurement Validation

Any optimization should be validated with GPIO instrumentation:
- A0→A7 pulse width should decrease proportionally
- Output must remain mathematically identical
- Test with various wrap/places configurations


