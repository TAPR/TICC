#!/usr/bin/env python3
"""Analyze TICC timing test data"""

import numpy as np

# Configuration - must match value in timing_test.h
TIMING_SAMPLE_INTERVAL = 10000

# Your timing data - SYNTHETIC MODE
timestamps = np.array([
    2121.37557062473,
    2123.73763452081,
    2126.09645278065,
    2128.45858031387,
    2130.82067806617,
    2133.18488953481,
    2135.54371476495,
    2137.90576675449,
    2140.26775111420,
    2142.63189525420,
    2144.99409580379,
    2147.35818102071,
    2149.72238599201,
    2152.08873589173,
    2154.44756848782,
    2156.80971888941,
    2159.17193067054,
    2161.53601661464,
    2163.89814502235,
    2166.26231995078,
    2168.62648254781,
    2170.99292975735,
    2173.35502770099,
    2175.71915954546,
    2178.08326482909,
    2180.44972256649,
    2182.81392617631,
    2185.18035175205,
    2187.54682295010,
    2189.91621818688,
    2192.27504252986,
    2194.63716778688,
    2196.99924316470,
    2199.36339079410,
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

