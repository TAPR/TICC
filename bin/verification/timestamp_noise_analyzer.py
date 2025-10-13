#!/usr/bin/env python3
"""
Timestamp Noise Analyzer for TICC Data

Analyzes timestamp data files to detect noise patterns, particularly focusing
on the least significant digits and looking for noise increases over time.

Usage: python3 timestamp_noise_analyzer.py <input_file>
"""

import sys
import re
import numpy as np
import matplotlib.pyplot as plt
from collections import defaultdict
import argparse

def parse_timestamp_line(line):
    """Parse a timestamp line and return (timestamp, channel) or None if invalid"""
    # Pattern: "01.01608335621 chA" or "01.01608334960 chB"
    pattern = r'^(\d+)\.(\d{11})\s+ch([AB])$'
    match = re.match(pattern, line.strip())
    
    if not match:
        return None
    
    seconds = int(match.group(1))
    fractional_part = match.group(2)
    channel = match.group(3)
    
    # Convert fractional part to picoseconds (11 digits = picoseconds)
    picoseconds = int(fractional_part)
    
    return (seconds, picoseconds, channel)

def analyze_timestamp_noise(filename):
    """Analyze timestamp noise in the data file"""
    
    # Read and parse the file
    timestamps = {'A': [], 'B': []}
    
    with open(filename, 'r') as f:
        for line_num, line in enumerate(f, 1):
            # Skip header lines (starting with #)
            if line.startswith('#'):
                continue
            
            # Parse timestamp line
            parsed = parse_timestamp_line(line)
            if parsed:
                seconds, picoseconds, channel = parsed
                timestamps[channel].append((seconds, picoseconds, line_num))
    
    print(f"Loaded {len(timestamps['A'])} timestamps for channel A")
    print(f"Loaded {len(timestamps['B'])} timestamps for channel B")
    
    # Analyze each channel
    for channel in ['A', 'B']:
        if len(timestamps[channel]) < 2:
            print(f"Channel {channel}: insufficient data")
            continue
            
        print(f"\n=== Channel {channel} Analysis ===")
        
        # Convert to numpy arrays for easier analysis
        data = timestamps[channel]
        seconds = np.array([x[0] for x in data])
        picoseconds = np.array([x[1] for x in data])  # Use actual picoseconds, not line number
        
        # For analysis, we need to use the actual time, not line numbers
        # The timestamp data shows seconds from 1 to 1 (due to wrap), so we need to reconstruct actual time
        # Since this is a wrap=2 system, seconds wrap at 100, so we need to track the full time
        actual_seconds = []
        current_full_seconds = 0
        
        for i, sec in enumerate(seconds):
            if i > 0 and sec < seconds[i-1]:  # Wrap occurred
                current_full_seconds += 100  # Add 100 for wrap=2
            actual_seconds.append(current_full_seconds + sec)
        
        actual_seconds = np.array(actual_seconds)
        
        # Calculate differences in picoseconds between consecutive measurements
        pico_diffs = np.diff(picoseconds)
        
        # For wrap=2, we expect measurements every ~1 second, so differences should be ~1e12 ps
        # Look for deviations from this expected interval
        expected_interval_ps = 1e12  # 1 second in picoseconds
        interval_errors = pico_diffs - expected_interval_ps
        
        # Analyze the data for phase drift around 11000 seconds
        print(f"Total measurements: {len(picoseconds)}")
        print(f"Time span: {actual_seconds[0]:.1f} to {actual_seconds[-1]:.1f} seconds ({actual_seconds[-1] - actual_seconds[0]:.1f} seconds)")
        
        # Look for phase drift around 11000 seconds
        # The issue is phase drift from ~30 ps to ~100 ps starting around 11000 seconds
        # This should show up as a systematic change in the fractional precision
        
        # For 11 decimal places, each digit represents 10 picoseconds
        # Look at the last few digits that would show 10 ps precision
        fractional_precision = picoseconds % 100000  # Last 5 digits (10 ps precision)
        
        drift_analysis = []
        window_size = 100  # Analyze in windows of 100 measurements
        
        for i in range(0, len(fractional_precision) - window_size, window_size//2):
            window_data = fractional_precision[i:i+window_size]
            window_seconds = actual_seconds[i:i+window_size]
            
            # Calculate mean fractional value in this window
            mean_val = np.mean(window_data)
            std_dev = np.std(window_data)
            time_midpoint = np.mean(window_seconds)
            
            drift_analysis.append({
                'time': time_midpoint,
                'mean': mean_val,
                'std_dev': std_dev,
                'measurements': len(window_data)
            })
        
        # Print drift analysis
        print(f"\nPhase Drift Analysis (10 ps precision, window size: {window_size}):")
        print("Time(s)    Mean(ps)    StdDev(ps)  Measurements")
        print("-" * 50)
        
        for analysis in drift_analysis:
            print(f"{analysis['time']:8.1f}   {analysis['mean']:8.1f}   {analysis['std_dev']:8.1f}   {analysis['measurements']:8d}")
        
        # Look for the specific phase drift pattern around 11000 seconds
        if len(drift_analysis) > 4:
            times = [a['time'] for a in drift_analysis]
            means = [a['mean'] for a in drift_analysis]
            std_devs = [a['std_dev'] for a in drift_analysis]
            
            # Look for step change around 11000 seconds
            before_11k = [(t, m) for t, m in zip(times, means) if t < 11000]
            after_11k = [(t, m) for t, m in zip(times, means) if t >= 11000]
            
            if before_11k and after_11k:
                before_mean = np.mean([m for t, m in before_11k])
                after_mean = np.mean([m for t, m in after_11k])
                phase_shift = after_mean - before_mean
                
                print(f"\nPhase Shift Analysis around 11000 seconds:")
                print(f"Mean phase before 11000s: {before_mean:.1f} ps")
                print(f"Mean phase after 11000s:  {after_mean:.1f} ps")
                print(f"Phase shift: {phase_shift:.1f} ps")
                
                if abs(phase_shift) > 20:  # Significant shift (> 20 ps)
                    print(f"*** SIGNIFICANT PHASE SHIFT DETECTED! ({phase_shift:.1f} ps) ***")
                else:
                    print("No significant phase shift detected")
            
            # Calculate overall drift trend
            z_mean = np.polyfit(times, means, 1)
            drift_slope = z_mean[0]
            drift_intercept = z_mean[1]
            
            print(f"\nOverall Drift Trend:")
            print(f"Slope: {drift_slope:.2e} ps/second")
            print(f"Intercept: {drift_intercept:.1f} ps")
            
            if abs(drift_slope) > 1e-6:  # Significant drift
                if drift_slope > 0:
                    print("*** POSITIVE drift detected! ***")
                else:
                    print("*** NEGATIVE drift detected! ***")
            else:
                print("No significant drift trend detected")
                
            # Also check noise trend
            z_noise = np.polyfit(times, std_devs, 1)
            noise_slope = z_noise[0]
            noise_intercept = z_noise[1]
            
            print(f"\nNoise Analysis:")
            print(f"Slope: {noise_slope:.2e} ps/second")
            print(f"Intercept: {noise_intercept:.1f} ps")
            
            if abs(noise_slope) > 1e-6:  # Significant trend
                if noise_slope > 0:
                    print("*** INCREASING noise trend detected! ***")
                else:
                    print("*** DECREASING noise trend detected! ***")
            else:
                print("No significant noise trend detected")
        
        # Look for wrap-related anomalies
        print(f"\nWrap Analysis (looking for anomalies around wrap points):")
        wrap_points = []
        for i in range(len(seconds) - 1):
            if seconds[i+1] < seconds[i]:  # Wrap occurred
                wrap_points.append((i, seconds[i], seconds[i+1]))
        
        if wrap_points:
            print(f"Found {len(wrap_points)} wrap events:")
            for idx, old_sec, new_sec in wrap_points:
                print(f"  Line {data[idx][2]}: {old_sec} -> {new_sec}")
                
                # Check if noise increases after this wrap
                if idx > 10 and idx < len(fractional_precision) - 10:
                    before_wrap = np.std(fractional_precision[max(0, idx-10):idx])
                    after_wrap = np.std(fractional_precision[idx:min(len(fractional_precision), idx+10)])
                    print(f"    Noise before wrap: {before_wrap:.1f} ps")
                    print(f"    Noise after wrap:  {after_wrap:.1f} ps")
                    if after_wrap > before_wrap * 1.5:
                        print(f"    *** Increased noise after wrap! ***")
        else:
            print("No wrap events detected in this data")
        
        # Overall statistics
        overall_std = np.std(fractional_precision)
        overall_mean = np.mean(fractional_precision)
        
        print(f"\nOverall Statistics (fractional precision):")
        print(f"Mean: {overall_mean:.1f} ps")
        print(f"Std Dev: {overall_std:.1f} ps")
        print(f"Min: {np.min(fractional_precision):.1f} ps")
        print(f"Max: {np.max(fractional_precision):.1f} ps")
        
        # Check for systematic patterns
        print(f"\nPattern Analysis:")
        # Look for periodic patterns in the fractional precision
        fft = np.fft.fft(fractional_precision - np.mean(fractional_precision))
        freqs = np.fft.fftfreq(len(fractional_precision))
        
        # Find dominant frequency (excluding DC component)
        power = np.abs(fft[1:len(fft)//2])**2
        dominant_freq_idx = np.argmax(power) + 1
        dominant_freq = freqs[dominant_freq_idx]
        
        if abs(dominant_freq) > 1e-6:  # Significant periodic component
            period = 1.0 / abs(dominant_freq)
            print(f"Dominant periodic component: {period:.1f} measurement periods")
            if period < 1000:  # Short period might indicate systematic issue
                print(f"*** Short-period pattern detected - possible systematic issue! ***")

def main():
    parser = argparse.ArgumentParser(description='Analyze timestamp noise in TICC data files')
    parser.add_argument('input_file', help='Input timestamp data file')
    parser.add_argument('--plot', action='store_true', help='Generate plots (requires matplotlib)')
    
    args = parser.parse_args()
    
    try:
        analyze_timestamp_noise(args.input_file)
        
        if args.plot:
            print("\nPlot generation not implemented in this version")
            
    except FileNotFoundError:
        print(f"Error: File '{args.input_file}' not found")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()
