#!/usr/bin/env python3
"""Analyze TICC timing test data"""

import numpy as np

# Configuration - must match value in timing_test.h
TIMING_SAMPLE_INTERVAL = 10000  # Adjust based on test mode

# Your timing data - FULL LOOP TEST (Test 5)
timestamps = np.array([
    # Paste timestamp data here
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

