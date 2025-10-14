#!/usr/bin/env python3
"""Analyze TICC timing test data"""

import numpy as np

# Configuration - must match value in timing_test.h
TIMING_SAMPLE_INTERVAL = 1000

# Your timing data
timestamps = np.array([
    1250.29625521531,
    1252.29624818595,
    1254.29625207835,
    1256.29625308864,
    1258.29624652495,
    1260.29624327922,
    1262.29625017718,
    1264.29625366996,
    1266.29625577818,
    1268.29624877282,
    1270.29625034611,
    1272.29624694358,
    1274.29625818500,
    1276.29624454621,
    1278.29625017725,
    1280.29624812853,
    1282.29623894860,
    1284.29625719801,
    1286.29624895338,
    1288.29624872287,
    1290.29625396809,
    1292.29625882810,
    1294.29624890495,
    1296.29625619052,
    1298.29623969867,
    1300.29625808919,
    1302.29625426914,
    1304.29624419036,
    1306.29624096870,
    1308.29624336870,
    1310.29624153228,
    1312.29624534144,
    1314.29624102682,
    1316.29625002358,
    1318.29624384734,
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

