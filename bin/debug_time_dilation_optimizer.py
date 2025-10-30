#!/usr/bin/env python3
"""
TICC Time Dilation Optimizer

Analyzes debug output to determine optimal time_dilation value by:
1. Converting timestamps to phase records
2. Detecting sawtooth patterns from CLOCK1 rollovers
3. Quantifying the correction needed
4. Providing detailed TIME1/TIME2 distribution analysis
5. Handling both channels with different time_dilation settings
"""

import sys
import numpy as np
from pathlib import Path
from scipy import stats
import argparse
import matplotlib.pyplot as plt

def parse_line(line):
    """Parse a single data line"""
    line = line.strip()
    if not line or line.startswith('#'):
        return None
    
    parts = line.split()
    if len(parts) < 9:
        return None
    
    try:
        channel = parts[8].strip()
        if channel not in ['chA', 'chB']:
            # Debug: print what we're getting
            if len(parts) > 8:
                print(f"Unknown channel: '{channel}' in line: {line[:50]}...")
            return None
            
        timestamp = float(parts[7])
        
        # Filter out obviously bad timestamps (negative or extremely large)
        if timestamp < 0 or timestamp > 1000000:  # Allow up to ~11 days
            print(f"Skipping bad timestamp {timestamp} for {channel}")
            return None
        
        result = {
            'time1': int(parts[0]),
            'time2': int(parts[1]),
            'clock1': int(parts[2]),
            'cal1': int(parts[3]),
            'cal2': int(parts[4]),
            'PICstop': int(parts[5]),
            'tof': int(parts[6]),
            'timestamp': timestamp,
            'channel': channel
        }
        
        
        return result
    except (ValueError, IndexError):
        return None

def extract_cal_periods(filename):
    """Extract CAL_PERIODS from startup banner"""
    cal_periods = 20  # default
    
    with open(filename, 'r') as f:
        for line in f:
            if 'Cal Periods:' in line:
                try:
                    # Extract number after "Cal Periods: "
                    parts = line.split('Cal Periods:')
                    if len(parts) > 1:
                        cal_periods = int(parts[1].strip())
                        break
                except (ValueError, IndexError):
                    pass
    
    return cal_periods

def extract_time_dilation_values(filename):
    """Extract time_dilation values from startup banner"""
    chA_dilation = 0  # default
    chB_dilation = 0  # default
    
    with open(filename, 'r') as f:
        for line in f:
            stripped = line.strip()
            if stripped.startswith('#') and 'Time Dilation:' in stripped:
                try:
                    # Look for pattern like "Time Dilation: 0 (ch0), 2500 (ch1)"
                    parts = stripped.split('Time Dilation:')[1]
                    if ',' in parts:
                        ch0_part = parts.split(',')[0].strip()
                        ch1_part = parts.split(',')[1].strip()
                        chA_dilation = int(ch0_part.split('(')[0].strip())
                        chB_dilation = int(ch1_part.split('(')[0].strip())
                    else:
                        # Single value format
                        dilation = int(parts.split('(')[0].strip())
                        chA_dilation = dilation
                        chB_dilation = dilation
                except:
                    pass
                break
    
    return chA_dilation, chB_dilation

def plot_phase_records(chA_data, chB_data, nominal_rate_hz, chA_dilation, chB_dilation, detrend_method='linear'):
    """Plot phase records of each channel in separate subplots with CLOCK1 increment markers"""
    
    if not chA_data and not chB_data:
        return
    
    # Determine number of subplots needed
    num_plots = 0
    if chA_data:
        num_plots += 1
    if chB_data:
        num_plots += 1
    
    if num_plots == 0:
        return
    
    # Create subplots
    if num_plots == 1:
        fig, ax = plt.subplots(1, 1, figsize=(12, 6))
        axes = [ax]
    else:
        fig, axes = plt.subplots(2, 1, figsize=(12, 10))
    
    plot_idx = 0
    
    if chA_data:
        # Calculate phase records for Channel A
        data_sorted, phase_diffs, frequency_offset = calculate_phase_records(chA_data, nominal_rate_hz, detrend_method)
        x_vals = np.arange(len(phase_diffs))
        
        
        axes[plot_idx].plot(x_vals, np.array(phase_diffs) * 1e12, 'b-', alpha=0.7, 
                label=f'Channel A (time_dilation={chA_dilation})', linewidth=1)
        
        # Find CLOCK1 increment points for Channel A
        rollover_indices = []
        for i in range(1, len(data_sorted)):
            if data_sorted[i]['clock1'] != data_sorted[i-1]['clock1']:
                rollover_indices.append(i)
        
        # Mark CLOCK1 increments on Channel A plot
        if rollover_indices:
            clock1_x = [idx for idx in rollover_indices if idx < len(phase_diffs)]
            clock1_y = [phase_diffs[idx] * 1e12 for idx in clock1_x]
            axes[plot_idx].plot(clock1_x, clock1_y, 'bo', markersize=6, alpha=0.8, 
                    label=f'CLOCK1 increments ({len(clock1_x)} points)')
        
        axes[plot_idx].set_xlabel('Sample Number')
        axes[plot_idx].set_ylabel('Phase (picoseconds)')
        axes[plot_idx].set_title(f'Channel A Phase Record (time_dilation={chA_dilation})')
        axes[plot_idx].legend()
        axes[plot_idx].grid(True, alpha=0.3)
        plot_idx += 1
    
    if chB_data:
        # Calculate phase records for Channel B
        data_sorted, phase_diffs, frequency_offset = calculate_phase_records(chB_data, nominal_rate_hz, detrend_method)
        x_vals = np.arange(len(phase_diffs))
        
        
        axes[plot_idx].plot(x_vals, np.array(phase_diffs) * 1e12, 'r-', alpha=0.7, 
                label=f'Channel B (time_dilation={chB_dilation})', linewidth=1)
        
        # Find CLOCK1 increment points for Channel B
        rollover_indices = []
        for i in range(1, len(data_sorted)):
            if data_sorted[i]['clock1'] != data_sorted[i-1]['clock1']:
                rollover_indices.append(i)
        
        # Mark CLOCK1 increments on Channel B plot
        if rollover_indices:
            clock1_x = [idx for idx in rollover_indices if idx < len(phase_diffs)]
            clock1_y = [phase_diffs[idx] * 1e12 for idx in clock1_x]
            axes[plot_idx].plot(clock1_x, clock1_y, 'ro', markersize=6, alpha=0.8, 
                    label=f'CLOCK1 increments ({len(clock1_x)} points)')
        
        axes[plot_idx].set_xlabel('Sample Number')
        axes[plot_idx].set_ylabel('Phase (picoseconds)')
        axes[plot_idx].set_title(f'Channel B Phase Record (time_dilation={chB_dilation})')
        axes[plot_idx].legend()
        axes[plot_idx].grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.show()

def calculate_phase_records(data, nominal_rate_hz, detrend_method='linear'):
    """Convert timestamps to phase records showing cumulative phase difference"""
    
    # Sort by timestamp
    data_sorted = sorted(data, key=lambda x: x['timestamp'])
    
    if len(data_sorted) < 2:
        return data_sorted, []
    
    # Calculate phase differences (actual_interval - expected_interval)
    expected_interval = 1.0 / nominal_rate_hz  # seconds (e.g., 0.1 for 10Hz)
    phase_diffs = []
    for i in range(1, len(data_sorted)):
        actual_interval = data_sorted[i]['timestamp'] - data_sorted[i-1]['timestamp']
        phase_diff = actual_interval - expected_interval
        phase_diffs.append(phase_diff)
    
    # Calculate cumulative phase difference
    cumulative_phase = []
    cumsum = 0.0
    for phase_diff in phase_diffs:
        cumsum += phase_diff
        cumulative_phase.append(cumsum)
    
    # Remove trend using linear or quadratic regression
    if len(cumulative_phase) > 1:
        x_vals = np.arange(len(cumulative_phase))
        
        if detrend_method == 'quadratic':
            # Quadratic regression: y = ax^2 + bx + c
            coeffs = np.polyfit(x_vals, cumulative_phase, 2)
            a, b, c = coeffs
            # Subtract the quadratic trend
            phase_diffs = []
            for i, phase in enumerate(cumulative_phase):
                expected = a * i**2 + b * i + c
                phase_diffs.append(phase - expected)
            frequency_offset = b  # The linear coefficient is the frequency offset
        else:  # linear
            slope, intercept, r_value, p_value, std_err = stats.linregress(x_vals, cumulative_phase)
            # Subtract the linear trend
            phase_diffs = []
            for i, phase in enumerate(cumulative_phase):
                expected = slope * i + intercept
                phase_diffs.append(phase - expected)
            frequency_offset = slope  # The slope is the frequency offset
    else:
        phase_diffs = cumulative_phase
        frequency_offset = 0.0
    
    return data_sorted, phase_diffs, frequency_offset

def detect_sawtooth_pattern(data, phase_diffs, cal_periods=20):
    """Detect sawtooth pattern by looking for correlation between CLOCK1 and phase jumps"""
    
    # Find CLOCK1 increment points (where clock1 changes)
    rollover_indices = []
    for i in range(1, len(data)):
        clock1_diff = data[i]['clock1'] - data[i-1]['clock1']
        # Look for any change in CLOCK1 (increment or decrement)
        if clock1_diff != 0:
            rollover_indices.append(i)
    
    if not rollover_indices:
        return None, None, None, None
    
    # Calculate phase jumps in a window around each CLOCK1 increment point
    phase_jumps_near_clock1 = []
    phase_jumps_elsewhere = []
    
    for idx in rollover_indices:
        # Look in a window around the CLOCK1 increment (jumps can be before or after)
        for offset in range(-2, 3):  # -2, -1, 0, 1, 2 (5-sample window)
            jump_idx = idx + offset
            if jump_idx > 0 and jump_idx < len(phase_diffs):
                # Calculate the phase jump (change in phase difference)
                phase_jump = phase_diffs[jump_idx] - phase_diffs[jump_idx-1]
                phase_jumps_near_clock1.append(phase_jump)
    
    # Calculate phase jumps elsewhere (not near CLOCK1 increments)
    for i in range(1, len(phase_diffs)):
        # Check if this jump is near any CLOCK1 increment
        near_clock1 = False
        for idx in rollover_indices:
            if abs(i - idx) <= 2:  # Within 2 samples of a CLOCK1 increment
                near_clock1 = True
                break
        
        if not near_clock1:
            phase_jump = phase_diffs[i] - phase_diffs[i-1]
            phase_jumps_elsewhere.append(phase_jump)
    
    if not phase_jumps_near_clock1 or not phase_jumps_elsewhere:
        return None, None, None, None
    
    # Calculate sawtooth correlation as the relative difference in phase jump magnitudes
    # If sawtooth exists, phase jumps near CLOCK1 increments should be larger
    mean_near = np.mean(np.abs(phase_jumps_near_clock1))
    mean_elsewhere = np.mean(np.abs(phase_jumps_elsewhere))
    
    # Correlation is the relative difference (0 = no difference, positive = larger jumps near CLOCK1)
    if mean_elsewhere > 0:
        correlation = (mean_near - mean_elsewhere) / mean_elsewhere
    else:
        correlation = 0.0
    
    # Debug: Print some statistics about the sawtooth detection
    print(f"DEBUG: Phase jumps near CLOCK1 - count: {len(phase_jumps_near_clock1)}, mean abs: {mean_near:.2e}")
    print(f"DEBUG: Phase jumps elsewhere - count: {len(phase_jumps_elsewhere)}, mean abs: {mean_elsewhere:.2e}")
    print(f"DEBUG: Correlation: {correlation:.6f}")
    
    # Debug: Print CAL1/CAL2 values for normLSB calculation
    if len(data) > 0:
        CLOCK_PERIOD_SEC = 1e-7  # 100ns in seconds
        cal1_vals = [d['cal1'] for d in data]
        cal2_vals = [d['cal2'] for d in data]
        cal1_mean = sum(cal1_vals) / len(cal1_vals)
        cal2_mean = sum(cal2_vals) / len(cal2_vals)
        cal_diff_mean = cal2_mean - cal1_mean
        print(f"DEBUG CAL: CAL1_mean={cal1_mean:.1f}, CAL2_mean={cal2_mean:.1f}, CAL_diff={cal_diff_mean:.1f}")
        print(f"DEBUG CAL: cal_periods={cal_periods}, calCount={cal_diff_mean/(cal_periods-1):.1f}")
        print(f"DEBUG CAL: normLSB_base={CLOCK_PERIOD_SEC/(cal_diff_mean/(cal_periods-1))*1e12:.6f} ps")
    
    # Calculate sawtooth amplitude (std dev of phase jumps near CLOCK1 in picoseconds)
    # Filter out extreme outliers that might skew the calculation
    phase_jumps_array = np.array(phase_jumps_near_clock1)
    if len(phase_jumps_array) > 0:
        # Remove outliers beyond 3 standard deviations
        mean_jump = np.mean(phase_jumps_array)
        std_jump = np.std(phase_jumps_array)
        filtered_jumps = phase_jumps_array[np.abs(phase_jumps_array - mean_jump) <= 3 * std_jump]
        sawtooth_amplitude = np.std(filtered_jumps) * 1e12  # Convert to picoseconds
    else:
        sawtooth_amplitude = 0.0
    
    # Calculate recommended time_dilation correction based on both amplitude and correlation
    # We need BOTH high amplitude AND high correlation to indicate sawtooth
    if sawtooth_amplitude > 50 and abs(correlation) > 0.1:  # Both amplitude and correlation matter
        # Estimate correction based on correlation strength and amplitude
        recommended_correction = int(abs(correlation) * sawtooth_amplitude / 10)  # Rough scaling
    else:
        recommended_correction = 0
    
    return correlation, sawtooth_amplitude, len(rollover_indices), recommended_correction

def analyze_time1_distribution(data):
    """Analyze TIME1 distribution with enhanced resolution at extremes"""
    
    time1_vals = [d['time1'] for d in data]
    
    # Create histogram with more bins for better resolution
    hist, bin_edges = np.histogram(time1_vals, bins=50)
    
    # Find min/max for scaling
    min_val = min(time1_vals)
    max_val = max(time1_vals)
    
    print(f"\nTIME1 Distribution Analysis:")
    print(f"=" * 60)
    print(f"Range: {min_val} to {max_val} ticks")
    print(f"Span: {max_val - min_val} ticks")
    print(f"Mean: {np.mean(time1_vals):.1f} ticks")
    print(f"Std Dev: {np.std(time1_vals):.1f} ticks")
    
    # Enhanced histogram focusing on extremes
    print(f"\nEnhanced Histogram (50 bins):")
    print(f"-" * 60)
    
    # Group bins for display (every 5th bin)
    for i in range(0, len(hist), 5):
        if i + 4 < len(hist):
            # Combine 5 bins
            combined_count = sum(hist[i:i+5])
            if combined_count > 0:
                start_val = int(bin_edges[i])
                end_val = int(bin_edges[i+5])
                bar_width = int((combined_count / max(hist)) * 50)
                bar = '#' * bar_width
                pct = 100.0 * combined_count / len(time1_vals)
                print(f"  {start_val:>4}-{end_val:<4}: {bar:<50} {combined_count:>5} ({pct:>4.1f}%)")
        else:
            # Handle remaining bins
            remaining_count = sum(hist[i:])
            if remaining_count > 0:
                start_val = int(bin_edges[i])
                end_val = int(bin_edges[-1])
                bar_width = int((remaining_count / max(hist)) * 50)
                bar = '#' * bar_width
                pct = 100.0 * remaining_count / len(time1_vals)
                print(f"  {start_val:>4}-{end_val:<4}: {bar:<50} {remaining_count:>5} ({pct:>4.1f}%)")

def analyze_time1_range_analysis_silent(data_sorted, cal_periods=20, time_dilation=0):
    """Analyze TIME1 range and normLSB silently (no output)"""
    
    if not data_sorted:
        return None
    
    # Extract TIME1, CAL1, CAL2 values
    time1_vals = [d['time1'] for d in data_sorted]
    cal1_vals = [d['cal1'] for d in data_sorted]
    cal2_vals = [d['cal2'] for d in data_sorted]
    
    # Calculate normLSB for each measurement using TDC7200 datasheet formula
    # normLSB = CLOCKperiod / calCount
    # where calCount = (CALIBRATION2 - CALIBRATION1) / (CALIBRATION2_PERIODS - 1)
    # Then apply time_dilation scaling: normLSB_effective = normLSB_base * scale / 1000000
    
    CLOCK_PERIOD_SEC = 1e-7  # 100ns in seconds
    denom = cal_periods - 1
    scale = 1000000 - time_dilation  # Apply time_dilation correction
    
    normLSB_vals = []
    normLSB_base_vals = []  # Store unscaled values too
    
    for i in range(len(data_sorted)):
        cal_diff = cal2_vals[i] - cal1_vals[i]
        calCount = cal_diff / denom  # Simple division as per datasheet
        if calCount < 1:
            calCount = 1
        
        # normLSB = CLOCK_PERIOD / calCount (from TDC7200 datasheet)
        normLSB_base = CLOCK_PERIOD_SEC / calCount
        normLSB_base_vals.append(normLSB_base)
        
        # Apply time_dilation scaling (same as TDC7200 code)
        normLSB_effective = normLSB_base * scale / 1000000
        
        normLSB_vals.append(normLSB_effective)
        
    
    # Calculate TIME1 statistics
    time1_min = min(time1_vals)
    time1_max = max(time1_vals)
    time1_range = time1_max - time1_min
    
    # Calculate normLSB statistics
    normLSB_min = min(normLSB_vals)
    normLSB_max = max(normLSB_vals)
    normLSB_mean = sum(normLSB_vals) / len(normLSB_vals)
    normLSB_std = (sum((x - normLSB_mean)**2 for x in normLSB_vals) / len(normLSB_vals))**0.5
    
    normLSB_base_min = min(normLSB_base_vals)
    normLSB_base_max = max(normLSB_base_vals)
    normLSB_base_mean = sum(normLSB_base_vals) / len(normLSB_base_vals)
    normLSB_base_std = (sum((x - normLSB_base_mean)**2 for x in normLSB_base_vals) / len(normLSB_base_vals))**0.5
    
    # Convert normLSB from seconds to picoseconds for display
    normLSB_mean_ps = normLSB_mean * 1e12
    normLSB_min_ps = normLSB_min * 1e12
    normLSB_max_ps = normLSB_max * 1e12
    normLSB_std_ps = normLSB_std * 1e12
    
    normLSB_base_mean_ps = normLSB_base_mean * 1e12
    normLSB_base_min_ps = normLSB_base_min * 1e12
    normLSB_base_max_ps = normLSB_base_max * 1e12
    normLSB_base_std_ps = normLSB_base_std * 1e12
    
    # Convert TIME1 range to time using mean normLSB (in seconds)
    time1_range_sec = time1_range * normLSB_mean
    time1_range_ns = time1_range_sec * 1e9
    
    # Calculate coverage of 100ns range
    coverage_percent = (time1_range_ns / 100.0) * 100.0
    gap_ns = 100.0 - time1_range_ns
    
    # Convert TIME1 min/max to time using mean normLSB (in seconds)
    time1_min_sec = time1_min * normLSB_mean
    time1_max_sec = time1_max * normLSB_mean
    time1_min_ns = time1_min_sec * 1e9
    time1_max_ns = time1_max_sec * 1e9
    
    # Return data for combined analysis
    return {
        'time1_min': time1_min,
        'time1_max': time1_max,
        'time1_min_ns': time1_min_ns,
        'time1_max_ns': time1_max_ns,
        'coverage_percent': coverage_percent,
        'gap_ns': gap_ns,
        'normLSB_mean': normLSB_mean_ps,
        'normLSB_std': normLSB_std_ps,
        'normLSB_min': normLSB_min_ps,
        'normLSB_max': normLSB_max_ps,
        'normLSB_base_mean': normLSB_base_mean_ps,
        'normLSB_base_std': normLSB_base_std_ps,
        'normLSB_base_min': normLSB_base_min_ps,
        'normLSB_base_max': normLSB_base_max_ps
    }

def analyze_time1_range_analysis(data_sorted, cal_periods=20, time_dilation=0):
    """Analyze TIME1 range and normLSB to see how well it covers 100ns"""
    
    if not data_sorted:
        return None
    
    print(f"\nTIME1 Range Analysis:")
    print(f"-" * 60)
    
    # Extract TIME1, CAL1, CAL2 values
    time1_vals = [d['time1'] for d in data_sorted]
    cal1_vals = [d['cal1'] for d in data_sorted]
    cal2_vals = [d['cal2'] for d in data_sorted]
    
    # Calculate normLSB for each measurement
    # normLSB = (CLOCK_PERIOD * 10^12) / calCount
    # where calCount = ((cal2 - cal1) * scale) / (CAL_PERIODS - 1)
    # and scale = 1000000 - time_dilation
    
    CLOCK_PERIOD = 100000  # 100ns in picoseconds
    scale = 1000000 - time_dilation  # Apply time_dilation correction
    denom = cal_periods - 1
    
    normLSB_vals = []
    for i in range(len(data_sorted)):
        cal_diff = cal2_vals[i] - cal1_vals[i]
        cal_prod = cal_diff * scale
        calCount = int((cal_prod + denom / 2) / denom)  # rounded division
        if calCount < 1:
            calCount = 1
        
        # normLSB = CLOCK_PERIOD / calCount (from TDC7200 datasheet)
        # CLOCK_PERIOD is 100ns = 1e-7 seconds
        CLOCK_PERIOD_SEC = 1e-7  # 100ns in seconds
        normLSB = CLOCK_PERIOD_SEC / calCount
        normLSB_vals.append(normLSB)
    
    # Calculate TIME1 statistics
    time1_min = min(time1_vals)
    time1_max = max(time1_vals)
    time1_range = time1_max - time1_min
    
    # Calculate normLSB statistics
    normLSB_min = min(normLSB_vals)
    normLSB_max = max(normLSB_vals)
    normLSB_mean = sum(normLSB_vals) / len(normLSB_vals)
    normLSB_std = (sum((x - normLSB_mean)**2 for x in normLSB_vals) / len(normLSB_vals))**0.5
    
    normLSB_base_min = min(normLSB_base_vals)
    normLSB_base_max = max(normLSB_base_vals)
    normLSB_base_mean = sum(normLSB_base_vals) / len(normLSB_base_vals)
    normLSB_base_std = (sum((x - normLSB_base_mean)**2 for x in normLSB_base_vals) / len(normLSB_base_vals))**0.5
    
    # Convert normLSB from seconds to picoseconds for display
    normLSB_mean_ps = normLSB_mean * 1e12
    normLSB_min_ps = normLSB_min * 1e12
    normLSB_max_ps = normLSB_max * 1e12
    normLSB_std_ps = normLSB_std * 1e12
    
    normLSB_base_mean_ps = normLSB_base_mean * 1e12
    normLSB_base_min_ps = normLSB_base_min * 1e12
    normLSB_base_max_ps = normLSB_base_max * 1e12
    normLSB_base_std_ps = normLSB_base_std * 1e12
    
    # Convert TIME1 range to time using mean normLSB (in seconds)
    time1_range_sec = time1_range * normLSB_mean
    time1_range_ns = time1_range_sec * 1e9
    
    # Calculate coverage of 100ns range
    coverage_percent = (time1_range_ns / 100.0) * 100.0
    gap_ns = 100.0 - time1_range_ns
    
    # Convert TIME1 min/max to time using mean normLSB (in seconds)
    time1_min_sec = time1_min * normLSB_mean
    time1_max_sec = time1_max * normLSB_mean
    time1_min_ns = time1_min_sec * 1e9
    time1_max_ns = time1_max_sec * 1e9
    
    print(f"TIME1 Statistics:")
    print(f"  Min TIME1: {time1_min} counts ({time1_min_ns:.3f} ns)")
    print(f"  Max TIME1: {time1_max} counts ({time1_max_ns:.3f} ns)")
    print(f"  TIME1 Range: {time1_range} counts")
    print(f"  TIME1 Range: {time1_range_ns:.3f} ns")
    print(f"  Coverage: {coverage_percent:.1f}% of 100ns range")
    print(f"  Gap: {gap_ns:.3f} ns")
    
    print(f"\nnormLSB Statistics:")
    print(f"  Min normLSB: {normLSB_min_ps:.3f} ps")
    print(f"  Max normLSB: {normLSB_max_ps:.3f} ps")
    print(f"  Mean normLSB: {normLSB_mean_ps:.3f} ps")
    print(f"  Std normLSB: {normLSB_std_ps:.3f} ps")
    print(f"  Range: {normLSB_max_ps - normLSB_min_ps:.3f} ps")
    
    # Return data for combined analysis
    return {
        'time1_min': time1_min,
        'time1_max': time1_max,
        'time1_min_ns': time1_min_ns,
        'time1_max_ns': time1_max_ns,
        'coverage_percent': coverage_percent,
        'gap_ns': gap_ns,
        'normLSB_mean': normLSB_mean_ps,
        'normLSB_std': normLSB_std_ps
    }

def analyze_time2_distribution(data):
    """Analyze TIME2 distribution with detailed analysis of narrow range"""
    
    time2_vals = [d['time2'] for d in data]
    
    print(f"\nTIME2 Distribution Analysis:")
    print(f"=" * 60)
    print(f"Min: {min(time2_vals)} ticks")
    print(f"Max: {max(time2_vals)} ticks")
    print(f"Mean: {np.mean(time2_vals):.1f} ticks")
    print(f"Std Dev: {np.std(time2_vals):.1f} ticks")
    print(f"Range: {max(time2_vals) - min(time2_vals)} ticks")
    
    # Detailed histogram for narrow range
    if max(time2_vals) - min(time2_vals) > 0:
        hist, bin_edges = np.histogram(time2_vals, bins=20)
        max_count = max(hist)
        
        print(f"\nDetailed Histogram (20 bins):")
        print(f"-" * 60)
        for i in range(len(hist)):
            if hist[i] > 0:
                bar_width = int((hist[i] / max_count) * 50)
                bar = '#' * bar_width
                pct = 100.0 * hist[i] / len(time2_vals)
                print(f"  {int(bin_edges[i]):>4}-{int(bin_edges[i+1]):<4}: {bar:<50} {hist[i]:>5} ({pct:>4.1f}%)")

def analyze_channel_silent(data, time_dilation, nominal_rate_hz, detrend_method='linear', cal_periods=20):
    """Analyze a single channel silently (no output)"""
    
    # Convert to phase records
    data_sorted, phase_diffs, frequency_offset = calculate_phase_records(data, nominal_rate_hz, detrend_method)
    
    # Detect sawtooth pattern
    correlation, sawtooth_amplitude, rollover_count, recommended_correction = detect_sawtooth_pattern(data_sorted, phase_diffs, cal_periods)
    
    if correlation is None:
        correlation = 0
        sawtooth_amplitude = 0
        recommended_correction = 0
        rollover_count = 0
    
    # TIME1 range analysis
    time1_data = analyze_time1_range_analysis_silent(data_sorted, cal_periods, time_dilation)
    
    # Prepare return data
    result = {
        'correlation': correlation,
        'sawtooth_amplitude': sawtooth_amplitude,
        'recommended_correction': recommended_correction,
        'frequency_offset': frequency_offset,
        'rollover_count': rollover_count
    }
    
    # Add TIME1 range data if available
    if time1_data:
        result.update(time1_data)
    
    return result

def analyze_channel(data, channel_name, time_dilation, nominal_rate_hz, show_histograms=False, detrend_method='linear', cal_periods=20):
    """Analyze a single channel"""
    
    print(f"\n{channel_name} Analysis (time_dilation = {time_dilation}):")
    print(f"=" * 60)
    
    # Convert to phase records
    data_sorted, phase_diffs, frequency_offset = calculate_phase_records(data, nominal_rate_hz, detrend_method)
    
    print(f"Frequency Analysis:")
    print(f"-" * 60)
    print(f"Nominal rate (per channel): {nominal_rate_hz:.9f} Hz")
    print(f"Frequency offset: {frequency_offset:.9f} Hz")
    print(f"Relative frequency offset: {frequency_offset / nominal_rate_hz:.2e}")
    
    # Detect sawtooth pattern
    correlation, sawtooth_amplitude, rollover_count, recommended_correction = detect_sawtooth_pattern(data_sorted, phase_diffs, cal_periods)
    
    if correlation is not None:
        print(f"\nSawtooth Detection:")
        print(f"-" * 60)
        print(f"CLOCK1 rollovers detected: {rollover_count}")
        print(f"Phase jump correlation with CLOCK1: {correlation:.4f}")
        print(f"Sawtooth amplitude (std dev): {sawtooth_amplitude:.2f} ps")
        
        if sawtooth_amplitude > 50 and abs(correlation) > 0.1:
            print(f"Status: STRONG sawtooth pattern detected")
            print(f"Current time_dilation: {time_dilation}")
            print(f"Recommended time_dilation: {recommended_correction}")
        else:
            print(f"Status: Weak or no sawtooth pattern")
            print(f"Current time_dilation: {time_dilation}")
            print(f"Recommendation: time_dilation may not be needed")
    else:
        print(f"\nSawtooth Detection:")
        print(f"-" * 60)
        print(f"No CLOCK1 rollovers detected in this dataset")
        print(f"Current time_dilation: {time_dilation}")
        print(f"Recommendation: Run longer test or increase frequency difference")
    
    # TIME1 and TIME2 distribution analysis (if requested)
    if show_histograms:
        analyze_time1_distribution(data_sorted)
        analyze_time2_distribution(data_sorted)
    
    # Always show TIME1 range analysis
    time1_data = analyze_time1_range_analysis(data_sorted, cal_periods, time_dilation)
    
    # Prepare return data
    result = {
        'correlation': correlation,
        'sawtooth_amplitude': sawtooth_amplitude,
        'recommended_correction': recommended_correction,
        'frequency_offset': frequency_offset,
        'rollover_count': rollover_count if correlation is not None else 0
    }
    
    # Add TIME1 range data if available
    if time1_data:
        result.update(time1_data)
    
    return result

def show_combined_analysis(results, chA_dilation, chB_dilation, nominal_rate_hz, show_histograms, detrend_method, cal_periods):
    """Show combined analysis for both channels side by side"""
    
    print(f"\nCombined Channel Analysis:")
    print(f"=" * 80)
    
    # Frequency Analysis
    print(f"\nFrequency Analysis:")
    print(f"-" * 80)
    print(f"{'Metric':<30} {'Channel A':<20} {'Channel B':<20}")
    print(f"{'-' * 30} {'-' * 20} {'-' * 20}")
    
    chA_freq_offset = results['A'].get('frequency_offset', 0)
    chB_freq_offset = results['B'].get('frequency_offset', 0)
    
    print(f"{'Nominal rate (per channel)':<30} {nominal_rate_hz:.9f} Hz{'':<10} {nominal_rate_hz:.9f} Hz")
    print(f"{'Frequency offset':<30} {chA_freq_offset:.9f} Hz{'':<10} {chB_freq_offset:.9f} Hz")
    print(f"{'Relative frequency offset':<30} {chA_freq_offset/nominal_rate_hz:.2e}{'':<10} {chB_freq_offset/nominal_rate_hz:.2e}")
    
    # Sawtooth Detection
    print(f"\nSawtooth Detection:")
    print(f"-" * 80)
    print(f"{'Metric':<30} {'Channel A':<20} {'Channel B':<20}")
    print(f"{'-' * 30} {'-' * 20} {'-' * 20}")
    
    chA_correlation = results['A'].get('correlation', 0)
    chB_correlation = results['B'].get('correlation', 0)
    chA_amplitude = results['A'].get('sawtooth_amplitude', 0)
    chB_amplitude = results['B'].get('sawtooth_amplitude', 0)
    chA_rollovers = results['A'].get('rollover_count', 0)
    chB_rollovers = results['B'].get('rollover_count', 0)
    
    print(f"{'CLOCK1 rollovers detected':<30} {chA_rollovers:<20} {chB_rollovers}")
    print(f"{'Phase jump correlation':<30} {chA_correlation:.4f}{'':<16} {chB_correlation:.4f}")
    print(f"{'Sawtooth amplitude (std dev)':<30} {chA_amplitude:.2f} ps{'':<13} {chB_amplitude:.2f} ps")
    
    # Status
    chA_status = "STRONG sawtooth pattern detected" if chA_amplitude > 50 and abs(chA_correlation) > 0.1 else "Weak or no sawtooth pattern"
    chB_status = "STRONG sawtooth pattern detected" if chB_amplitude > 50 and abs(chB_correlation) > 0.1 else "Weak or no sawtooth pattern"
    print(f"{'Status':<30} {chA_status:<20} {chB_status}")
    
    # TIME1 Range Analysis (if we have the data)
    if 'time1_min' in results['A'] and 'time1_min' in results['B']:
        print(f"\nTIME1 Range Analysis (with time_dilation applied):")
        print(f"-" * 80)
        print(f"{'Metric':<30} {'Channel A':<20} {'Channel B':<20}")
        print(f"{'-' * 30} {'-' * 20} {'-' * 20}")
        
        chA_time1_min = results['A']['time1_min']
        chB_time1_min = results['B']['time1_min']
        chA_time1_max = results['A']['time1_max']
        chB_time1_max = results['B']['time1_max']
        chA_time1_min_ns = results['A']['time1_min_ns']
        chB_time1_min_ns = results['B']['time1_min_ns']
        chA_time1_max_ns = results['A']['time1_max_ns']
        chB_time1_max_ns = results['B']['time1_max_ns']
        chA_coverage = results['A']['coverage_percent']
        chB_coverage = results['B']['coverage_percent']
        chA_gap = results['A']['gap_ns']
        chB_gap = results['B']['gap_ns']
        
        print(f"{'Min TIME1 (counts)':<30} {chA_time1_min:<20} {chB_time1_min}")
        print(f"{'Max TIME1 (counts)':<30} {chA_time1_max:<20} {chB_time1_max}")
        print(f"{'Raw TIME1 range (counts)':<30} {chA_time1_max - chA_time1_min:<20} {chB_time1_max - chB_time1_min}")
        print(f"{'Min TIME1 (ns)':<30} {chA_time1_min_ns:.3f}{'':<16} {chB_time1_min_ns:.3f}")
        print(f"{'Max TIME1 (ns)':<30} {chA_time1_max_ns:.3f}{'':<16} {chB_time1_max_ns:.3f}")
        print(f"{'TIME1 range (ns)':<30} {chA_time1_max_ns - chA_time1_min_ns:.3f}{'':<16} {chB_time1_max_ns - chB_time1_min_ns:.3f}")
        print(f"{'Coverage of 100ns range':<30} {chA_coverage:.1f}%{'':<17} {chB_coverage:.1f}%")
        print(f"{'Gap (ns)':<30} {chA_gap:.3f}{'':<16} {chB_gap:.3f}")
        
        # normLSB Statistics
        chA_normLSB_mean = results['A']['normLSB_mean']
        chB_normLSB_mean = results['B']['normLSB_mean']
        chA_normLSB_std = results['A']['normLSB_std']
        chB_normLSB_std = results['B']['normLSB_std']
        chA_normLSB_min = results['A']['normLSB_min']
        chB_normLSB_min = results['B']['normLSB_min']
        chA_normLSB_max = results['A']['normLSB_max']
        chB_normLSB_max = results['B']['normLSB_max']
        chA_normLSB_base_mean = results['A'].get('normLSB_base_mean', chA_normLSB_mean)
        chB_normLSB_base_mean = results['B'].get('normLSB_base_mean', chB_normLSB_mean)
        chA_normLSB_base_std = results['A'].get('normLSB_base_std', chA_normLSB_std)
        chB_normLSB_base_std = results['B'].get('normLSB_base_std', chB_normLSB_std)
        chA_normLSB_base_min = results['A'].get('normLSB_base_min', chA_normLSB_min)
        chB_normLSB_base_min = results['B'].get('normLSB_base_min', chB_normLSB_min)
        chA_normLSB_base_max = results['A'].get('normLSB_base_max', chA_normLSB_max)
        chB_normLSB_base_max = results['B'].get('normLSB_base_max', chB_normLSB_max)
        
        print(f"\nnormLSB Statistics:")
        print(f"-" * 80)
        print(f"{'Metric':<30} {'Channel A':<20} {'Channel B':<20}")
        print(f"{'-' * 30} {'-' * 20} {'-' * 20}")
        print(f"{'Raw Values (datasheet formula):':<30}")
        print(f"{'  Min normLSB (ps)':<30} {chA_normLSB_base_min:.6f}{'':<13} {chB_normLSB_base_min:.6f}")
        print(f"{'  Max normLSB (ps)':<30} {chA_normLSB_base_max:.6f}{'':<13} {chB_normLSB_base_max:.6f}")
        print(f"{'  Mean normLSB (ps)':<30} {chA_normLSB_base_mean:.6f}{'':<13} {chB_normLSB_base_mean:.6f}")
        print(f"{'  Std normLSB (ps)':<30} {chA_normLSB_base_std:.6f}{'':<13} {chB_normLSB_base_std:.6f}")
        print(f"{'  Range (ps)':<30} {chA_normLSB_base_max - chA_normLSB_base_min:.6f}{'':<13} {chB_normLSB_base_max - chB_normLSB_base_min:.6f}")
        print(f"{'With time_dilation applied:':<30}")
        print(f"{'  Min normLSB (ps)':<30} {chA_normLSB_min:.6f}{'':<13} {chB_normLSB_min:.6f}")
        print(f"{'  Max normLSB (ps)':<30} {chA_normLSB_max:.6f}{'':<13} {chB_normLSB_max:.6f}")
        print(f"{'  Mean normLSB (ps)':<30} {chA_normLSB_mean:.6f}{'':<13} {chB_normLSB_mean:.6f}")
        print(f"{'  Std normLSB (ps)':<30} {chA_normLSB_std:.6f}{'':<13} {chB_normLSB_std:.6f}")
        print(f"{'  Range (ps)':<30} {chA_normLSB_max - chA_normLSB_min:.6f}{'':<13} {chB_normLSB_max - chB_normLSB_min:.6f}")

def analyze_file(filename, nominal_rate_hz, show_histograms=False, show_plot=False, detrend_method='linear'):
    """Main analysis function"""
    
    if not Path(filename).exists():
        print(f"Error: File '{filename}' not found")
        sys.exit(1)
    
    # Extract time_dilation values and CAL_PERIODS from startup banner
    chA_dilation, chB_dilation = extract_time_dilation_values(filename)
    cal_periods = extract_cal_periods(filename)
    
    # Read data, skip garbage before startup banner
    all_data = []
    in_debug_section = False
    
    with open(filename, 'r') as f:
        for line in f:
            stripped = line.strip()
            if stripped.startswith('#'):
                # Look for the debug header to know when to start collecting data
                if 'TAPR TICC Timestamping Counter' in stripped:
                    in_debug_section = True
                continue
            
            if in_debug_section:
                parsed = parse_line(line)
                if parsed is not None:
                    all_data.append(parsed)
    
    # Separate channels
    chA_data = [d for d in all_data if d['channel'] == 'chA']
    chB_data = [d for d in all_data if d['channel'] == 'chB']
    
    if not chA_data and not chB_data:
        print("Error: No valid data found")
        sys.exit(1)
    
    print(f"\nTICC Time Dilation Optimizer")
    print(f"=" * 60)
    print(f"File: {filename}")
    print(f"Nominal Rate: {nominal_rate_hz} Hz")
    print()
    
    # Analyze each channel (silently, just collect data)
    results = {}
    
    if chA_data:
        results['A'] = analyze_channel_silent(chA_data, chA_dilation, nominal_rate_hz, detrend_method, cal_periods)
    
    if chB_data:
        results['B'] = analyze_channel_silent(chB_data, chB_dilation, nominal_rate_hz, detrend_method, cal_periods)
    
    # Show combined analysis if both channels present
    if len(results) == 2:
        show_combined_analysis(results, chA_dilation, chB_dilation, nominal_rate_hz, show_histograms, detrend_method, cal_periods)
    
    # Summary comparison if both channels present
    if len(results) == 2:
        # Debug: Show the actual time_dilation values being used
        print(f"\nDebug Information:")
        print(f"-" * 80)
        print(f"{'Metric':<30} {'Channel A':<20} {'Channel B':<20}")
        print(f"{'-' * 30} {'-' * 20} {'-' * 20}")
        print(f"{'time_dilation setting':<30} {chA_dilation:<20} {chB_dilation}")
        
        # Get TIME1 range data
        chA_time1_min_ns = results['A'].get('time1_min_ns', 0)
        chA_time1_max_ns = results['A'].get('time1_max_ns', 0)
        chB_time1_min_ns = results['B'].get('time1_min_ns', 0)
        chB_time1_max_ns = results['B'].get('time1_max_ns', 0)
        chA_coverage = results['A'].get('coverage_percent', 0)
        chB_coverage = results['B'].get('coverage_percent', 0)
        chA_correlation = results['A'].get('correlation', 0)
        chB_correlation = results['B'].get('correlation', 0)
        
        print(f"{'TIME1 range (ns)':<30} {chA_time1_max_ns - chA_time1_min_ns:.3f}{'':<16} {chB_time1_max_ns - chB_time1_min_ns:.3f}")
        print(f"{'Expected 100ns coverage':<30} {'100.000':<20} {'100.000'}")
        print(f"{'Coverage %':<30} {chA_coverage:.1f}%{'':<17} {chB_coverage:.1f}%")
        print(f"{'Sawtooth correlation':<30} {chA_correlation:.4f}{'':<16} {chB_correlation:.4f}")
        
        print(f"\nChannel Comparison Summary:")
        print(f"=" * 60)
        
        chA_corr = results['A'].get('correlation', 0)
        chA_amp = results['A'].get('sawtooth_amplitude', 0)
        chA_rec = results['A'].get('recommended_correction', 0)
        chB_corr = results['B'].get('correlation', 0)
        chB_amp = results['B'].get('sawtooth_amplitude', 0)
        chB_rec = results['B'].get('recommended_correction', 0)
        
        chA_corr_str = f"{chA_corr:.3f}" if chA_corr is not None else "N/A"
        chB_corr_str = f"{chB_corr:.3f}" if chB_corr is not None else "N/A"
        print(f"Channel A: time_dilation={chA_dilation}, correlation={chA_corr_str}, recommended={chA_rec}")
        print(f"Channel B: time_dilation={chB_dilation}, correlation={chB_corr_str}, recommended={chB_rec}")
        
        if chA_corr is not None and chB_corr is not None:
            if abs(chA_corr) < abs(chB_corr):
                print(f"\nRecommendation: Channel A has better linearity (lower sawtooth correlation)")
            elif abs(chB_corr) < abs(chA_corr):
                print(f"\nRecommendation: Channel B has better linearity (lower sawtooth correlation)")
            else:
                print(f"\nRecommendation: Both channels show similar sawtooth characteristics")
        
        print(f"\n" + "=" * 60)
    
    # Show plot if requested
    if show_plot:
        plot_phase_records(chA_data, chB_data, nominal_rate_hz, chA_dilation, chB_dilation, detrend_method)

def main():
    parser = argparse.ArgumentParser(description='TICC Time Dilation Optimizer')
    parser.add_argument('data_file', help='Debug output data file')
    parser.add_argument('--rate', type=float, default=10.0, 
                       help='Nominal measurement rate in Hz (default: 10.0)')
    parser.add_argument('--histograms', action='store_true',
                       help='Include detailed histograms in output')
    parser.add_argument('--plot', action='store_true',
                       help='Show phase record plot')
    parser.add_argument('--detrend', choices=['linear', 'quadratic'], default='linear',
                       help='Detrending method: linear (default) or quadratic')
    
    args = parser.parse_args()
    
    analyze_file(args.data_file, args.rate, args.histograms, args.plot, args.detrend)

if __name__ == '__main__':
    main()
