#!/usr/bin/env python3
"""
Apply TICC-style timestamp wrapping to unwrapped timestamp data.

This tool reads a file with unwrapped timestamps and applies the TICC wrap=2
algorithm (wrap at 100 seconds) to generate output that looks exactly like
what the TICC would produce with wrap=2 enabled.

Input format: TICC timestamp file with unwrapped data
Output format: Same format but with seconds wrapped modulo 100
"""

import sys
import argparse

def parse_timestamp_line(line):
    """Parse a TICC timestamp line, return (timestamp_str, channel, rest_of_line)."""
    parts = line.split()
    if len(parts) < 2:
        return None, None, None
    
    timestamp_str = parts[0]
    channel = parts[1]
    rest = ' '.join(parts[2:]) if len(parts) > 2 else ''
    
    return timestamp_str, channel, rest

def wrap_timestamp(timestamp_str, wrap_digits=2):
    """
    Apply TICC wrap algorithm to a timestamp string.
    
    Args:
        timestamp_str: Timestamp in format "SSS.FFFFFFFFFFF" (any number of second digits)
        wrap_digits: Number of digits to display (2 = wrap at 100 seconds)
    
    Returns:
        Wrapped timestamp string in format "SS.FFFFFFFFFFF"
    """
    # Split into seconds and fractional parts
    parts = timestamp_str.split('.')
    if len(parts) != 2:
        # Invalid format, return as-is
        return timestamp_str
    
    seconds_str = parts[0]
    fraction_str = parts[1]
    
    # Handle negative sign
    is_negative = seconds_str.startswith('-')
    if is_negative:
        seconds_str = seconds_str[1:]
    
    # Convert to integer and apply modulo
    try:
        seconds = int(seconds_str)
    except ValueError:
        # Can't parse, return as-is
        return timestamp_str
    
    # Apply wrap modulo (e.g., 10^2 = 100 for wrap_digits=2)
    wrap_modulo = 10 ** wrap_digits
    wrapped_seconds = seconds % wrap_modulo
    
    # Format with zero-padding to wrap_digits width
    # This matches what TICC does in print_timestamp()
    wrapped_str = str(wrapped_seconds).zfill(wrap_digits)
    
    # Add back negative sign if needed (though TICC typically doesn't have negative timestamps)
    if is_negative:
        wrapped_str = '-' + wrapped_str
    
    # Reconstruct timestamp
    return f"{wrapped_str}.{fraction_str}"

def process_file(input_file, output_file, wrap_digits=2):
    """Process TICC timestamp file and apply wrapping."""
    lines_processed = 0
    timestamps_wrapped = 0
    
    with open(input_file, 'r') as inf:
        with open(output_file, 'w') as outf:
            for line in inf:
                line = line.rstrip('\n\r')
                
                # Pass through comment lines and empty lines unchanged
                if line.startswith('#') or not line.strip():
                    outf.write(line + '\n')
                    continue
                
                # Parse timestamp line
                timestamp_str, channel, rest = parse_timestamp_line(line)
                
                if timestamp_str is None:
                    # Not a valid timestamp line, pass through unchanged
                    outf.write(line + '\n')
                    continue
                
                # Apply wrapping
                wrapped_timestamp = wrap_timestamp(timestamp_str, wrap_digits)
                
                # Reconstruct line
                if rest:
                    wrapped_line = f"{wrapped_timestamp} {channel} {rest}"
                else:
                    wrapped_line = f"{wrapped_timestamp} {channel}"
                
                outf.write(wrapped_line + '\n')
                
                lines_processed += 1
                if wrapped_timestamp != timestamp_str:
                    timestamps_wrapped += 1
    
    return lines_processed, timestamps_wrapped

def main():
    parser = argparse.ArgumentParser(
        description='Apply TICC-style timestamp wrapping to unwrapped data',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Apply 100-second wrap (wrap=2) to unwrapped data
  python3 apply_timestamp_wrap.py unwrapped.dat wrapped.dat

  # Apply 1000-second wrap (wrap=3)
  python3 apply_timestamp_wrap.py unwrapped.dat wrapped.dat --wrap 3

This tool simulates exactly what the TICC does with its wrap setting,
allowing you to test how analysis software handles wrapped timestamps
from known-good unwrapped data.
        """)
    
    parser.add_argument('input_file', 
                       help='Input TICC data file with unwrapped timestamps')
    parser.add_argument('output_file',
                       help='Output file with wrapped timestamps')
    parser.add_argument('--wrap', type=int, default=2, metavar='N',
                       help='Number of wrap digits (2=100s, 3=1000s, etc.). Default: 2')
    
    args = parser.parse_args()
    
    # Validate wrap value
    if args.wrap < 0 or args.wrap > 9:
        print(f"# Error: wrap must be between 0 and 9", file=sys.stderr)
        sys.exit(1)
    
    if args.wrap == 0:
        print(f"# Warning: wrap=0 means no wrapping (same as input)", file=sys.stderr)
    
    print(f"# Processing: {args.input_file}")
    print(f"# Output to: {args.output_file}")
    print(f"# Wrap digits: {args.wrap} (wraps at {10**args.wrap} seconds)")
    print("#" + "="*60)
    
    try:
        lines, wrapped = process_file(args.input_file, args.output_file, args.wrap)
        print(f"# Processed {lines} timestamp lines")
        print(f"# Applied wrapping to {wrapped} timestamps")
        print(f"# Output written to: {args.output_file}")
    except FileNotFoundError:
        print(f"# Error: Input file '{args.input_file}' not found", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"# Error: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == '__main__':
    main()

