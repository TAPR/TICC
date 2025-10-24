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

def extract_time_dilation_values(filename):
    """Extract time_dilation values from startup banner"""
    chA_dilation = "?"
    chB_dilation = "?"
    
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
                        chA_dilation = ch0_part.split('(')[0].strip()
                        chB_dilation = ch1_part.split('(')[0].strip()
                    else:
                        # Single value format
                        chA_dilation = parts.split('(')[0].strip()
                        chB_dilation = chA_dilation
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

def detect_sawtooth_pattern(data, phase_diffs):
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
    
    # Calculate correlation as the difference in jump magnitudes
    # If sawtooth exists, jumps near CLOCK1 should be larger
    mean_near = np.mean(np.abs(phase_jumps_near_clock1))
    mean_elsewhere = np.mean(np.abs(phase_jumps_elsewhere))
    
    # Correlation is the relative difference (0 = no difference, 1 = all jumps near CLOCK1)
    if mean_elsewhere > 0:
        correlation = (mean_near - mean_elsewhere) / mean_elsewhere
    else:
        correlation = 0.0
    
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

def analyze_channel(data, channel_name, time_dilation, nominal_rate_hz, show_histograms=False, detrend_method='linear'):
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
    correlation, sawtooth_amplitude, rollover_count, recommended_correction = detect_sawtooth_pattern(data_sorted, phase_diffs)
    
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
    
    return correlation, sawtooth_amplitude, recommended_correction

def analyze_file(filename, nominal_rate_hz, show_histograms=False, show_plot=False, detrend_method='linear'):
    """Main analysis function"""
    
    if not Path(filename).exists():
        print(f"Error: File '{filename}' not found")
        sys.exit(1)
    
    # Extract time_dilation values from startup banner
    chA_dilation, chB_dilation = extract_time_dilation_values(filename)
    
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
    
    # Analyze each channel
    results = {}
    
    if chA_data:
        results['A'] = analyze_channel(chA_data, "Channel A", chA_dilation, nominal_rate_hz, show_histograms, detrend_method)
    
    if chB_data:
        results['B'] = analyze_channel(chB_data, "Channel B", chB_dilation, nominal_rate_hz, show_histograms, detrend_method)
    
    # Summary comparison if both channels present
    if len(results) == 2:
        print(f"\nChannel Comparison Summary:")
        print(f"=" * 60)
        
        chA_corr, chA_amp, chA_rec = results['A']
        chB_corr, chB_amp, chB_rec = results['B']
        
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
