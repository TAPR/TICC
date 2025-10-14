#!/usr/bin/env python3
"""
ticc_split_channels.py - Split TICC data file by channel

Reads a TICC timestamp data file and splits it into separate files,
one per channel, preserving header lines in all output files.

Usage: ticc_split_channels.py <input_file>

Example: ticc_split_channels.py test.dat
  Creates: test_chA.dat, test_chB.dat (if chA and chB are present)
"""

import sys
import os
from pathlib import Path


def split_ticc_file(input_file):
    """
    Split a TICC data file by channel.
    
    Args:
        input_file: Path to the input data file
    
    Returns:
        Dictionary mapping channel names to output file paths
    """
    if not os.path.exists(input_file):
        print(f"Error: Input file '{input_file}' not found")
        sys.exit(1)
    
    # Parse input filename
    input_path = Path(input_file)
    base_name = input_path.stem  # filename without extension
    extension = input_path.suffix  # includes the dot
    
    # First pass: collect header lines and identify channels
    header_lines = []
    channels = set()
    
    print(f"# Reading '{input_file}'...")
    
    with open(input_file, 'r') as f:
        for line in f:
            if line.startswith('#'):
                # Preserve header line exactly as-is
                header_lines.append(line)
            else:
                # Parse data line to extract channel
                line = line.strip()
                if line:  # Skip empty lines
                    parts = line.split()
                    if len(parts) >= 2:
                        # Channel name is the last field
                        channel = parts[-1]
                        channels.add(channel)
    
    if not channels:
        print("# Warning: No channel data found in file")
        return {}
    
    channels = sorted(channels)  # Sort for consistent output
    print(f"# Found {len(channels)} channel(s): {', '.join(channels)}")
    
    # Create output files and write headers
    output_files = {}
    output_paths = {}
    
    for channel in channels:
        output_filename = f"{base_name}_{channel}{extension}"
        output_path = input_path.parent / output_filename
        output_paths[channel] = str(output_path)
        
        # Open file and write header
        output_files[channel] = open(output_path, 'w')
        for header_line in header_lines:
            output_files[channel].write(header_line)
    
    # Second pass: distribute data lines to appropriate channel files
    line_counts = {channel: 0 for channel in channels}
    
    with open(input_file, 'r') as f:
        for line in f:
            if not line.startswith('#'):
                line_stripped = line.strip()
                if line_stripped:  # Skip empty lines
                    parts = line_stripped.split()
                    if len(parts) >= 2:
                        # Channel name is the last field
                        channel = parts[-1]
                        if channel in output_files:
                            output_files[channel].write(line)
                            line_counts[channel] += 1
    
    # Close all output files
    for f in output_files.values():
        f.close()
    
    # Report results
    print(f"#")
    print(f"# Split complete:")
    for channel in channels:
        print(f"#   {output_paths[channel]}: {line_counts[channel]} lines")
    
    return output_paths


def main():
    """Main entry point."""
    if len(sys.argv) != 2:
        print("Usage: ticc_split_channels.py <input_file>")
        print()
        print("Splits a TICC data file by channel, creating separate output files")
        print("for each channel found in the input.")
        print()
        print("Example:")
        print("  ticc_split_channels.py test.dat")
        print("  Creates: test_chA.dat, test_chB.dat (if those channels are present)")
        sys.exit(1)
    
    input_file = sys.argv[1]
    split_ticc_file(input_file)


if __name__ == "__main__":
    main()

