#!/usr/bin/env python3
"""
Analyze interval stability of TICC timestamp data.
Calculate chA(n) - chA(n-1) and chB(n) - chB(n-1) to see
if there's a 75 ps drift starting around 11,000 seconds.
"""

import sys
import numpy as np

def parse_timestamp(ts_str):
    """Convert timestamp string like '01.01608335621' to total picoseconds."""
    parts = ts_str.split('.')
    seconds = int(parts[0])
    # The fractional part represents picoseconds (with 11 decimal places = 10 ps resolution)
    frac_str = parts[1]
    # Convert fractional part to picoseconds
    # 11 decimal places means 1e-11 seconds = 10 picoseconds per digit
    picoseconds = int(frac_str)
    
    # Total picoseconds = seconds * 1e12 + fractional picoseconds
    total_ps = seconds * 1000000000000 + picoseconds * 10
    
    return total_ps

def load_ticc_data(filename):
    """Load TICC data file and return timestamps for each channel."""
    timestamps = {'A': [], 'B': []}
    measurement_count = 0
    
    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            # Skip comments and empty lines
            if line.startswith('#') or not line:
                continue
            
            parts = line.split()
            if len(parts) >= 2:
                timestamp_str = parts[0]
                channel = parts[1].replace('ch', '')
                
                if channel in ['A', 'B']:
                    total_ps = parse_timestamp(timestamp_str)
                    timestamps[channel].append(total_ps)
                    
                    if channel == 'B':
                        measurement_count += 1
    
    print(f"Loaded {len(timestamps['A'])} timestamps for channel A")
    print(f"Loaded {len(timestamps['B'])} timestamps for channel B")
    
    return timestamps

def analyze_intervals(timestamps, channel_name):
    """Analyze interval stability for a channel."""
    print(f"\n{'='*60}")
    print(f"=== Channel {channel_name} Interval Stability Analysis ===")
    print('='*60)
    
    data = np.array(timestamps)
    
    # Calculate intervals (differences between consecutive measurements)
    # Need to handle wrap-around at 100 seconds (for wrap=2 mode)
    intervals = []
    wrap_count = 0
    
    for i in range(1, len(data)):
        interval = data[i] - data[i-1]
        
        # If interval is hugely negative, a wrap occurred
        # Wrap happens at 100 seconds = 100e12 ps
        if interval < -90e12:  # Threshold for detecting wrap
            wrap_count += 1
            # Add 100 seconds to correct for the wrap
            interval += 100e12
        
        intervals.append(interval)
    
    intervals = np.array(intervals)
    
    print(f"Detected {wrap_count} timestamp wraps")
    
    # Expected interval is 1 second = 1e12 picoseconds
    expected_interval = 1e12
    
    # Calculate deviations from expected interval
    deviations = intervals - expected_interval
    
    print(f"\nTotal measurements: {len(data)}")
    print(f"Total intervals: {len(intervals)}")
    
    # Analyze deviations in windows
    window_size = 500  # 500 seconds per window
    
    print(f"\nInterval Deviation Analysis (window size: {window_size} measurements):")
    print("Time(s)      Mean Dev(ps)  StdDev(ps)  Min(ps)    Max(ps)")
    print("-" * 70)
    
    means = []
    times = []
    
    for i in range(0, len(deviations) - window_size, window_size // 2):
        window = deviations[i:i+window_size]
        time_midpoint = i + window_size // 2
        
        mean_dev = np.mean(window)
        std_dev = np.std(window)
        min_dev = np.min(window)
        max_dev = np.max(window)
        
        means.append(mean_dev)
        times.append(time_midpoint)
        
        print(f"{time_midpoint:7d}      {mean_dev:8.1f}      {std_dev:8.1f}    {min_dev:8.1f}   {max_dev:8.1f}")
    
    # Look for drift around 11000 seconds
    print(f"\n{'='*60}")
    print("Analysis around 11,000 seconds:")
    print('='*60)
    
    # Find indices for before and after 11000 seconds
    before_11k_idx = [i for i, t in enumerate(times) if t < 11000]
    after_11k_idx = [i for i, t in enumerate(times) if t >= 11000]
    
    if before_11k_idx and after_11k_idx:
        before_11k_mean = np.mean([means[i] for i in before_11k_idx])
        after_11k_mean = np.mean([means[i] for i in after_11k_idx])
        
        # Also look at the last 1000 seconds
        late_idx = [i for i, t in enumerate(times) if t >= len(deviations) - 1000]
        if late_idx:
            late_mean = np.mean([means[i] for i in late_idx])
        else:
            late_mean = after_11k_mean
        
        drift_from_11k = late_mean - after_11k_mean
        total_drift = late_mean - before_11k_mean
        
        print(f"Mean deviation before 11,000s: {before_11k_mean:8.1f} ps")
        print(f"Mean deviation at 11,000s:     {after_11k_mean:8.1f} ps")
        print(f"Mean deviation at end of run:  {late_mean:8.1f} ps")
        print(f"\nDrift from start to 11,000s:   {after_11k_mean - before_11k_mean:8.1f} ps")
        print(f"Drift from 11,000s to end:     {drift_from_11k:8.1f} ps")
        print(f"Total drift over entire run:   {total_drift:8.1f} ps")
        
        if abs(drift_from_11k) > 30:
            print(f"\n*** SIGNIFICANT DRIFT DETECTED after 11,000s: {drift_from_11k:.1f} ps ***")
    
    # Overall statistics
    print(f"\n{'='*60}")
    print("Overall Interval Statistics:")
    print('='*60)
    print(f"Mean deviation:   {np.mean(deviations):8.1f} ps")
    print(f"Std deviation:    {np.std(deviations):8.1f} ps")
    print(f"Min deviation:    {np.min(deviations):8.1f} ps")
    print(f"Max deviation:    {np.max(deviations):8.1f} ps")
    print(f"Range:            {np.max(deviations) - np.min(deviations):8.1f} ps")

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python3 interval_stability.py <ticc_data_file>")
        sys.exit(1)
    
    filename = sys.argv[1]
    
    print(f"Analyzing TICC data file: {filename}")
    print("="*60)
    
    timestamps = load_ticc_data(filename)
    
    if timestamps['A']:
        analyze_intervals(timestamps['A'], 'A')
    
    if timestamps['B']:
        analyze_intervals(timestamps['B'], 'B')

