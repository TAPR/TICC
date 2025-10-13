#!/usr/bin/env python3
"""
TICC Timestamp Analyzer

Analyzes timestamp streams from TICC for ordering compliance.
Reads from stdin, ignoring comment lines starting with '#'.

Usage:
    python3 timestamp_analyzer.py --strict-order    # Test for strictly increasing timestamps
    python3 timestamp_analyzer.py --strict-pairing  # Test for strict chA/chB pairing
    python3 timestamp_analyzer.py --strict-order --wrap 2  # Handle wrapping at 100 seconds

Options:
    --wrap N   Handle timestamp wrapping at 10^N seconds (1-8)
               Example: --wrap 2 means timestamps wrap at 100 seconds

Input format:
    1.23434455 chA
    1.23443445 chB
    2.23434t45 chA
    2.34434566 chB
"""

import sys
import argparse
import re
from typing import List, Tuple, Optional

class TimestampEntry:
    def __init__(self, timestamp: float, channel: str, line_num: int):
        self.timestamp = timestamp
        self.channel = channel
        self.line_num = line_num
    
    def __repr__(self):
        return f"TimestampEntry({self.timestamp}, '{self.channel}', line {self.line_num})"

def parse_timestamp_line(line: str, line_num: int) -> Optional[TimestampEntry]:
    """Parse a timestamp line, return None if it's a comment or invalid."""
    line = line.strip()
    
    # Skip empty lines and comments
    if not line or line.startswith('#'):
        return None
    
    # Match pattern: number followed by channel (chA, chB, etc.)
    match = re.match(r'^([0-9]+\.?[0-9]*)\s+(ch[A-Z]?)$', line)
    if not match:
        print(f"Warning: Invalid format at line {line_num}: {line}")
        return None
    
    try:
        timestamp = float(match.group(1))
        channel = match.group(2)
        return TimestampEntry(timestamp, channel, line_num)
    except ValueError:
        print(f"Warning: Invalid timestamp at line {line_num}: {line}")
        return None

def test_strict_order(entries: List[TimestampEntry], wrap_value: Optional[int] = None) -> bool:
    """Test if timestamps are strictly increasing regardless of channel.
    
    Args:
        entries: List of timestamp entries to check
        wrap_value: If set, handle wrapping at 10^wrap_value seconds
    """
    if wrap_value:
        wrap_period = 10 ** wrap_value
        print(f"Testing for strictly increasing timestamps with wrapping at {wrap_period} seconds...")
    else:
        print("Testing for strictly increasing timestamps...")
    
    violations = 0
    wrap_count = 0
    
    for i in range(1, len(entries)):
        curr_ts = entries[i].timestamp
        prev_ts = entries[i-1].timestamp
        
        # Check if this is a valid sequence
        is_violation = False
        is_wrap = False
        
        if curr_ts <= prev_ts:
            if wrap_value:
                # Check if this looks like a legitimate wrap
                # A wrap occurs when current timestamp is near 0 and previous was near wrap_period
                # We use half the wrap period as the threshold
                wrap_period = 10 ** wrap_value
                threshold = wrap_period / 2.0
                
                # If the backward jump is more than half the period, assume it's a wrap
                if (prev_ts - curr_ts) > threshold:
                    is_wrap = True
                    wrap_count += 1
                else:
                    is_violation = True
            else:
                is_violation = True
        
        if is_violation:
            print(f"VIOLATION: Line {entries[i].line_num}: {curr_ts} <= {prev_ts} (line {entries[i-1].line_num})")
            violations += 1
    
    if wrap_value and wrap_count > 0:
        print(f"Detected {wrap_count} timestamp wrap(s)")
    
    if violations == 0:
        print("✓ PASS: All timestamps are strictly increasing")
        return True
    else:
        print(f"✗ FAIL: {violations} ordering violations found")
        return False

def test_strict_pairing(entries: List[TimestampEntry]) -> bool:
    """Test if channels are strictly paired in chA/chB order."""
    print("Testing for strict chA/chB pairing...")
    
    violations = 0
    expected_channel = 'chA'
    
    for entry in entries:
        if entry.channel != expected_channel:
            print(f"VIOLATION: Line {entry.line_num}: Expected {expected_channel}, got {entry.channel}")
            violations += 1
        
        # Toggle expected channel
        expected_channel = 'chB' if expected_channel == 'chA' else 'chA'
    
    if violations == 0:
        print("✓ PASS: All channels follow strict chA/chB pairing")
        return True
    else:
        print(f"✗ FAIL: {violations} pairing violations found")
        return False

def main():
    parser = argparse.ArgumentParser(description='Analyze TICC timestamp ordering')
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--strict-order', action='store_true', 
                      help='Test for strictly increasing timestamps regardless of channel')
    group.add_argument('--strict-pairing', action='store_true',
                      help='Test for strict chA/chB pairing order')
    
    parser.add_argument('--wrap', type=int, choices=range(1, 9), metavar='N',
                       help='Handle timestamp wrapping at 10^N seconds (1-8). '
                            'Example: --wrap 2 means timestamps wrap at 100 seconds')
    
    args = parser.parse_args()
    
    # Read and parse all timestamp entries
    entries = []
    line_num = 0
    
    print("Reading timestamp stream from stdin...")
    print("(Press Ctrl+D to end input)")
    
    try:
        for line in sys.stdin:
            line_num += 1
            entry = parse_timestamp_line(line, line_num)
            if entry:
                entries.append(entry)
    except KeyboardInterrupt:
        print("\nInterrupted by user")
        sys.exit(1)
    except EOFError:
        pass
    
    if not entries:
        print("No valid timestamp entries found")
        sys.exit(1)
    
    print(f"Parsed {len(entries)} timestamp entries")
    
    # Run the appropriate test
    if args.strict_order:
        success = test_strict_order(entries, wrap_value=args.wrap)
    elif args.strict_pairing:
        success = test_strict_pairing(entries)
    
    # Print summary
    print(f"\nSummary:")
    print(f"Total entries: {len(entries)}")
    if entries:
        print(f"First timestamp: {entries[0].timestamp} ({entries[0].channel})")
        print(f"Last timestamp: {entries[-1].timestamp} ({entries[-1].channel})")
    
    sys.exit(0 if success else 1)

if __name__ == '__main__':
    main()
