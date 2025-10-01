#!/usr/bin/env python3
# ID: ticc_rate_verify.py v2025-09-29.1

# Verify timestamped throughput/continuity per channel from a decoded ASCII file.
# Input lines: "<timestamp_seconds> chX"
#
# Reports per-channel:
# - total lines
# - elapsed time (last - first), whole-run average Hz
# - Δt stats: min / avg / max / stddev
# - count of intervals off expected by > tolerance
# - count of "gaps" (Δt > expected + tolerance)
#
# Optional: small histogram for Δt

import argparse
import math
import statistics
import sys

def parse_args():
    p = argparse.ArgumentParser(description="Verify TICC timestamp throughput/continuity per channel.")
    p.add_argument("--infile", required=True, help="Decoder output file (e.g., interleaved ts.txt)")
    p.add_argument("--channels", nargs="+", default=None, help="Channels to include (e.g., A B). If omitted, auto-detect.")
    p.add_argument("--expected-interval", type=float, required=True,
                   help="Expected time between samples in seconds (e.g., 0.001 for 1 kHz)")
    p.add_argument("--tol", type=float, default=0.0002,
                   help="Tolerance (seconds) for 'off-interval' and 'gap' checks (default 200 microseconds)")
    p.add_argument("--hist-bins", type=int, default=0,
                   help="Bins for Δt histogram (0 to disable). If >0, bin over [expected - 3*tol, expected + 3*tol]")
    return p.parse_args()

def parse_line(line: str):
    s = line.strip()
    if not s or s.startswith("#"):
        return None
    parts = s.split()
    if len(parts) != 2:
        return None
    try:
        ts = float(parts[0])
    except ValueError:
        return None
    ch = parts[1]
    if ch.startswith("ch") and len(ch) == 3 and ch[2].isalpha():
        ch = ch[2]
    return ts, ch

def main():
    a = parse_args()

    # Read and bucket by channel
    per_ch = {}
    with open(a.infile, "r", encoding="utf-8") as f:
        for line in f:
            parsed = parse_line(line)
            if not parsed:
                continue
            ts, ch = parsed
            if a.channels and ch not in a.channels:
                continue
            per_ch.setdefault(ch, []).append(ts)

    if not per_ch:
        print("No data for requested channels.", file=sys.stderr)
        sys.exit(2)

    for ch in sorted(per_ch.keys()):
        ts_list = per_ch[ch]
        ts_list.sort()
        n = len(ts_list)
        if n < 2:
            print(f"{ch}: only {n} sample(s); not enough data.")
            continue

        elapsed = ts_list[-1] - ts_list[0]
        avg_hz = n / max(1e-12, elapsed)  # inclusive count; for long runs, near true rate

        # Δt
        deltas = [ts_list[i] - ts_list[i-1] for i in range(1, n)]
        exp = a.expected_interval
        tol = a.tol
        off = sum(1 for dt in deltas if abs(dt - exp) > tol)
        gaps = sum(1 for dt in deltas if dt > exp + tol)

        d_min = min(deltas)
        d_max = max(deltas)
        d_avg = sum(deltas) / len(deltas)
        d_std = statistics.pstdev(deltas) if len(deltas) > 1 else 0.0

        print(f"Channel {ch}:")
        print(f"  samples={n}  elapsed={elapsed:.6f}s  whole-run avg={avg_hz:.3f} Hz")
        print(f"  Δt: min={d_min:.6f}s  avg={d_avg:.6f}s  max={d_max:.6f}s  std={d_std*1e6:.2f} µs")
        print(f"  intervals off>tol: {off}  gaps: {gaps}  (expected={exp:.6f}s tol={tol:.6f}s)")

        if a.hist_bins > 0:
            lo = exp - 3*tol
            hi = exp + 3*tol
            if hi <= lo:
                hi = lo + 1e-6
            bins = [0]*(a.hist_bins)
            width = (hi - lo) / a.hist_bins
            for dt in deltas:
                idx = int((dt - lo) / max(1e-12, width))
                if 0 <= idx < a.hist_bins:
                    bins[idx] += 1
            # Print a small histogram
            print("  Δt histogram:")
            for i, count in enumerate(bins):
                bin_lo = lo + i*width
                bin_hi = bin_lo + width
                bar = "#" * min(60, count*60 // max(1, len(deltas)))
                print(f"    [{bin_lo:.6f}, {bin_hi:.6f}): {count:6d} {bar}")

if __name__ == "__main__":
    main()
