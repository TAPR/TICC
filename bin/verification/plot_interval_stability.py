#!/usr/bin/env python3
"""
Plot interval stability of TICC timestamp data.
Shows chA(n) - chA(n-1) and chB(n) - chB(n-1) over time,
similar to the TimeLab plot.
"""

import sys
import numpy as np
import matplotlib.pyplot as plt

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
    
    print(f"# Loaded {len(timestamps['A'])} timestamps for channel A")
    print(f"# Loaded {len(timestamps['B'])} timestamps for channel B")
    
    return timestamps

def calculate_intervals(timestamps):
    """Calculate intervals with wrap correction."""
    data = np.array(timestamps)
    
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
    
    print(f"# Detected {wrap_count} timestamp wraps")
    
    return intervals

def plot_interval_stability(timestamps_a, timestamps_b, output_file=None, avg_window=100, 
                           plot_raw=True, ylim=None, plot_phase=False):
    """Plot interval stability for both channels."""
    
    # Calculate intervals
    print("\n# Channel A:")
    intervals_a = calculate_intervals(timestamps_a)
    
    print("\n# Channel B:")
    intervals_b = calculate_intervals(timestamps_b)
    
    # Expected interval is 1 second = 1e12 picoseconds
    expected_interval = 1e12
    
    # Calculate deviations from expected interval (frequency/interval error)
    deviations_a = intervals_a - expected_interval
    deviations_b = intervals_b - expected_interval
    
    # If plotting phase, calculate cumulative timestamp differences
    if plot_phase:
        # Calculate cumulative timestamp differences (what TimeLab does)
        # Need to handle wraps properly for cumulative calculation
        
        # Reconstruct unwrapped timestamps
        unwrapped_a = []
        unwrapped_b = []
        wrap_count_a = 0
        wrap_count_b = 0
        
        for i in range(len(timestamps_a)):
            # Handle wraps for channel A
            if i > 0 and timestamps_a[i] < timestamps_a[i-1]:
                wrap_count_a += 1
            unwrapped_a.append(timestamps_a[i] + wrap_count_a * 100e12)  # Add 100 seconds for each wrap
            
            # Handle wraps for channel B  
            if i > 0 and timestamps_b[i] < timestamps_b[i-1]:
                wrap_count_b += 1
            unwrapped_b.append(timestamps_b[i] + wrap_count_b * 100e12)
        
        unwrapped_a = np.array(unwrapped_a)
        unwrapped_b = np.array(unwrapped_b)
        
        # Calculate expected timestamps from first timestamp
        expected_timestamps_a = unwrapped_a[0] + np.arange(len(unwrapped_a)) * expected_interval
        expected_timestamps_b = unwrapped_b[0] + np.arange(len(unwrapped_b)) * expected_interval
        
        # Calculate cumulative phase as difference from expected timestamps
        phase_a = unwrapped_a - expected_timestamps_a
        phase_b = unwrapped_b - expected_timestamps_b
        
        # Use phase for plotting
        plot_data_a = phase_a
        plot_data_b = phase_b
        ylabel = 'Cumulative Phase Deviation (ns)'
        title_suffix = ' [Cumulative Timestamp Diff]'
    else:
        # Use frequency deviations for plotting
        plot_data_a = deviations_a
        plot_data_b = deviations_b
        ylabel = 'Interval Deviation (ns)'
        title_suffix = ''
    
    # Time axis (in seconds) - use length of plot data
    time_a = np.arange(len(plot_data_a))
    time_b = np.arange(len(plot_data_b))
    
    # Convert to nanoseconds for plotting (easier to read)
    # 1000 ps = 1 ns
    plot_data_a_ns = plot_data_a / 1000.0
    plot_data_b_ns = plot_data_b / 1000.0
    
    # Create the plot
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8), sharex=True)
    
    # Plot Channel A
    if plot_raw:
        ax1.plot(time_a, plot_data_a_ns, 'r-', linewidth=0.3, label='Raw data', alpha=0.3)
    
    ax1.axhline(y=0, color='k', linestyle='--', linewidth=0.5, alpha=0.5)
    ax1.axvline(x=11000, color='gray', linestyle=':', linewidth=1, alpha=0.5, label='11000s mark')
    ax1.set_ylabel(ylabel, fontsize=11)
    
    # Print statistics for Channel A
    std_a = np.std(deviations_a)
    print(f"\n# Channel A Statistics:")
    print(f"#   Mean deviation: {np.mean(deviations_a):.2f} ps")
    print(f"#   Std deviation:  {std_a:.2f} ps")
    print(f"#   Range: [{np.min(deviations_a):.1f}, {np.max(deviations_a):.1f}] ps")
    
    # Plot Channel B
    if plot_raw:
        ax2.plot(time_b, plot_data_b_ns, 'g-', linewidth=0.3, label='Raw data', alpha=0.3)
    
    ax2.axhline(y=0, color='k', linestyle='--', linewidth=0.5, alpha=0.5)
    ax2.axvline(x=11000, color='gray', linestyle=':', linewidth=1, alpha=0.5, label='11000s mark')
    ax2.set_xlabel('Time (seconds)', fontsize=11)
    ax2.set_ylabel(ylabel, fontsize=11)
    
    # Print statistics for Channel B
    std_b = np.std(deviations_b)
    print(f"\n# Channel B Statistics:")
    print(f"#   Mean deviation: {np.mean(deviations_b):.2f} ps")
    print(f"#   Std deviation:  {std_b:.2f} ps")
    print(f"#   Range: [{np.min(deviations_b):.1f}, {np.max(deviations_b):.1f}] ps")
    
    # Add running average to see trends better
    if avg_window > 0 and len(plot_data_a) > avg_window:
        print(f"\n# Calculating {avg_window}-second moving average...")
        running_avg_a = np.convolve(plot_data_a_ns, np.ones(avg_window)/avg_window, mode='valid')
        running_avg_b = np.convolve(plot_data_b_ns, np.ones(avg_window)/avg_window, mode='valid')
        time_avg = np.arange(avg_window//2, len(plot_data_a) - avg_window//2 + 1)
        
        ax1.plot(time_avg, running_avg_a, 'b-', linewidth=2, label=f'{avg_window}s avg', alpha=0.9)
        ax2.plot(time_avg, running_avg_b, 'r-', linewidth=2, label=f'{avg_window}s avg', alpha=0.9)
        
        # Print averaged statistics
        print(f"#   Averaged A - Range: [{np.min(running_avg_a):.3f}, {np.max(running_avg_a):.3f}] ns")
        print(f"#   Averaged B - Range: [{np.min(running_avg_b):.3f}, {np.max(running_avg_b):.3f}] ns")
        
        # Check for drift in averaged data
        before_11k_a = running_avg_a[time_avg < 11000]
        after_11k_a = running_avg_a[time_avg >= 11000]
        before_11k_b = running_avg_b[time_avg < 11000]
        after_11k_b = running_avg_b[time_avg >= 11000]
        
        if len(before_11k_a) > 0 and len(after_11k_a) > 0:
            drift_a = np.mean(after_11k_a[-1000:]) - np.mean(before_11k_a[:1000])
            drift_b = np.mean(after_11k_b[-1000:]) - np.mean(before_11k_b[:1000])
            print(f"\n# Drift Analysis (averaged data):")
            print(f"#   Channel A: {drift_a*1000:.1f} ps from start to end")
            print(f"#   Channel B: {drift_b*1000:.1f} ps from start to end")
    
    # Set titles
    avg_text = f' [{avg_window}s avg]' if avg_window > 0 else ''
    ax1.set_title(f'TICC Interval Stability: chA(n) - chA(n-1){title_suffix}{avg_text}', fontsize=12)
    ax2.set_title(f'TICC Interval Stability: chB(n) - chB(n-1){title_suffix}{avg_text}', fontsize=12)
    
    ax1.grid(True, alpha=0.3)
    ax2.grid(True, alpha=0.3)
    ax1.legend(loc='upper right')
    ax2.legend(loc='upper right')
    
    # Set Y-axis limits if specified
    if ylim:
        ax1.set_ylim(ylim)
        ax2.set_ylim(ylim)
    
    plt.tight_layout()
    
    if output_file:
        plt.savefig(output_file, dpi=150, bbox_inches='tight')
        print(f"\n# Plot saved to: {output_file}")
    else:
        plt.show()

if __name__ == '__main__':
    import argparse
    
    parser = argparse.ArgumentParser(
        description='Plot TICC interval stability: chA(n) - chA(n-1) and chB(n) - chB(n-1)',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Basic plot with 100-second averaging
  python3 plot_interval_stability.py ticc_data.dat output.png
  
  # Show only averaged data with 100-second window
  python3 plot_interval_stability.py ticc_data.dat output.png --avg 100 --no-raw
  
  # Use 500-second averaging and limit Y-axis to ±0.1 ns
  python3 plot_interval_stability.py ticc_data.dat output.png --avg 500 --ylim 0.1
        """)
    
    parser.add_argument('input_file', help='TICC data file to analyze')
    parser.add_argument('output_file', nargs='?', default=None, 
                       help='Output plot file (if omitted, display interactively)')
    parser.add_argument('--avg', type=int, default=100, metavar='N',
                       help='Moving average window size in seconds (default: 100, 0=disable)')
    parser.add_argument('--no-raw', dest='plot_raw', action='store_false',
                       help='Do not plot raw data, only averaged')
    parser.add_argument('--ylim', type=float, default=None, metavar='Y',
                       help='Y-axis limit in nanoseconds (e.g., 0.1 for ±0.1 ns)')
    parser.add_argument('--phase', dest='plot_phase', action='store_true',
                       help='Plot cumulative phase (integrated frequency error) instead of frequency deviation')
    
    args = parser.parse_args()
    
    # Convert ylim to tuple if specified
    ylim = (-args.ylim, args.ylim) if args.ylim else None
    
    print(f"# Analyzing TICC data file: {args.input_file}")
    print("#" + "="*60)
    
    timestamps = load_ticc_data(args.input_file)
    
    if timestamps['A'] and timestamps['B']:
        plot_interval_stability(timestamps['A'], timestamps['B'], 
                              output_file=args.output_file,
                              avg_window=args.avg,
                              plot_raw=args.plot_raw,
                              ylim=ylim,
                              plot_phase=args.plot_phase)
    else:
        print("# Error: Need both channels A and B data")
        sys.exit(1)

