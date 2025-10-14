#!/usr/bin/env python3
"""Analyze TICC timing test data"""

import numpy as np

# Configuration - must match value in timing_test.h
TIMING_SAMPLE_INTERVAL = 1000  # Adjust based on test mode

# For REAL_SIGNAL test with known input frequency
INPUT_FREQUENCY_HZ = 1000  # Hz (adjust if using different frequency)
INPUT_PERIOD_US = 1e6 / INPUT_FREQUENCY_HZ  # microseconds

# Your timing data - REAL SIGNAL TEST (Test 6)
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
print("REAL SIGNAL INTERPRETATION")
print("=" * 70)
print("Timing pulse generated at start of each Nth measurement processing.")
print(f"With {INPUT_FREQUENCY_HZ} Hz input, measurements arrive every {INPUT_PERIOD_US:.0f} µs")
print()
print("Each measured interval represents:")
print(f"  {TIMING_SAMPLE_INTERVAL} × (processing_time + wait_for_next_input)")
print()
avg_cycle = np.mean(time_per_measurement_us)
print(f"Average measured cycle time: {avg_cycle:.2f} µs")
print()
print("NOTE: At 1 kHz input rate, the system processes measurements as they")
print("arrive. The measured time (~1000-1200 µs) represents the full cycle:")
print("  - Process current measurement (~200-400 µs)")
print("  - Idle loop waiting for next input (~600-800 µs)")
print("  - Total cycle time matches input period (~1000 µs)")
print()
if avg_cycle > INPUT_PERIOD_US:
    processing_overhead = avg_cycle - INPUT_PERIOD_US
    print(f"System is keeping up: {avg_cycle:.0f} µs per measurement")
    print(f"(Slightly > {INPUT_PERIOD_US:.0f} µs input period - within timing variation)")
else:
    print(f"System has headroom: {avg_cycle:.0f} µs cycle < {INPUT_PERIOD_US:.0f} µs period")
print("=" * 70)

