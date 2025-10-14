#!/usr/bin/env python3
"""Analyze TICC timing test data"""

import numpy as np

# Configuration - must match value in timing_test.h
TIMING_SAMPLE_INTERVAL = 10000

# Your timing data - AUTO-INCREMENT + DIRECT CSB (Test 3)
timestamps = np.array([
    3298.26369460997,
    3299.58256705562,
    3300.90388955071,
    3302.21497066281,
    3303.52777295012,
    3304.84058177201,
    3306.15595540528,
    3307.46875408468,
    3308.78412273063,
    3310.09946763742,
    3311.41624934324,
    3312.72905510286,
    3314.04437157554,
    3315.35972084293,
    3316.67656342720,
    3317.99193480661,
    3319.30877076720,
    3320.62552835391,
    3321.94443108369,
    3323.25721978985,
    3324.57255946421,
    3325.88793270495,
    3327.20472475262,
    3328.52007428639,
    3329.83687266104,
    3331.15362707927,
    3332.47248116000,
    3333.78785348495,
    3335.10473255346,
    3336.42152100363,
    3337.74035463558,
    3339.05717514737,
    3340.37605462690,
    3341.69496799669,
    3343.01629516783,
])

# Calculate intervals between consecutive timestamps
intervals = np.diff(timestamps)

# Convert to average time per measurement
# Each interval represents TIMING_SAMPLE_INTERVAL measurements
time_per_measurement = intervals / TIMING_SAMPLE_INTERVAL

# Convert to microseconds
time_per_measurement_us = time_per_measurement * 1e6

# Statistics
print("=" * 70)
print("TICC TIMING ANALYSIS")
print("=" * 70)
print(f"Test configuration: TIMING_SAMPLE_INTERVAL = {TIMING_SAMPLE_INTERVAL}")
print(f"Samples collected: {len(time_per_measurement_us)}")
print()
print("Time per measurement cycle:")
print(f"  Mean:    {np.mean(time_per_measurement_us):8.2f} µs")
print(f"  Median:  {np.median(time_per_measurement_us):8.2f} µs")
print(f"  Min:     {np.min(time_per_measurement_us):8.2f} µs")
print(f"  Max:     {np.max(time_per_measurement_us):8.2f} µs")
print(f"  Std Dev: {np.std(time_per_measurement_us):8.2f} µs")
print(f"  Range:   {np.max(time_per_measurement_us) - np.min(time_per_measurement_us):8.2f} µs")
print()

# Show all individual measurements
print("Individual interval measurements:")
for i, t_us in enumerate(time_per_measurement_us):
    print(f"  Sample {i+1:2d}: {t_us:8.2f} µs")

print()
print("=" * 70)
print("INTERPRETATION")
print("=" * 70)
print("With TIMING_TEST_SPI_READS enabled, the pulse is generated just before")
print("the 5 × readReg24 SPI reads. The measured time includes:")
print("  - The 5 SPI read transactions (target of optimization)")
print("  - TOF calculation arithmetic")  
print("  - ready_next() SPI write")
print("  - Timestamp accumulation")
print("  - LED operations")
print("  - Wait time for next 1 kHz input pulse (~1000 µs)")
print()
print("For 1 kHz input signal, expect ~1000 µs wait + ~200 µs processing")
print(f"= ~1200 µs total, but measured average is {np.mean(time_per_measurement_us):.0f} µs")
print()
print("The processing overhead (excluding wait time) is approximately:")
print(f"  {np.mean(time_per_measurement_us) - 1000:.0f} µs per measurement")
print("=" * 70)

