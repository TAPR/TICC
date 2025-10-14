#!/usr/bin/env python3
"""Analyze TICC timing test data"""

import numpy as np

# Configuration - must match value in timing_test.h
TIMING_SAMPLE_INTERVAL = 10000  # Adjust based on test mode

# Your timing data - FULL LOOP TEST (Test 5)
timestamps = np.array([
    2412.49445879718,
    2414.93492320700,
    2417.37721890457,
    2419.81953254351,
    2422.26377992403,
    2424.70608803295,
    2427.15033463204,
    2429.59458427901,
    2432.04122993659,
    2434.48354077453,
    2436.92778888914,
    2439.37204061654,
    2441.81872806129,
    2444.26297125652,
    2446.70961773267,
    2449.15623677579,
    2451.60465530104,
    2454.04512745786,
    2456.48739455718,
    2458.92975723749,
    2461.37399524813,
    2463.81628985824,
    2466.26053778169,
    2468.70479347243,
    2471.15146297564,
    2473.59375579145,
    2476.03799112591,
    2478.48227345533,
    2480.92893568352,
    2483.37321310225,
    2485.81987793073,
    2488.26653651961,
    2490.71496198010,
    2493.15726699873,
    2495.60148723312,
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

