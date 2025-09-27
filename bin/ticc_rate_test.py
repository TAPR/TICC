#!/usr/bin/env python3
import sys
import time
import argparse
from collections import deque, defaultdict

try:
    import serial
except ImportError:
    print("pyserial is required. Install with: pip install pyserial", file=sys.stderr)
    sys.exit(1)


def is_comment_or_blank(s: str) -> bool:
    if not s:
        return True
    i = 0
    n = len(s)
    while i < n and s[i].isspace():
        i += 1
    return i >= n or s[i] == "#"


def parse_text_line(s: str):
    # Accept:
    #   "<float> <channel>"          e.g., "1.234456 chA"
    #   "<id> <value> <channel>"     e.g., "0000... 60617700 A"
    if is_comment_or_blank(s):
        return None
    parts = s.split()
    if len(parts) == 2:
        val_str, ch = parts
        try:
            float(val_str)
        except ValueError:
            return None
        return ch
    if len(parts) == 3:
        return parts[2]
    return None


def parse_channel_from_crlf_frame(raw: bytes):
    # raw must end with CRLF; take the byte before CRLF if it's a letter
    if len(raw) < 3 or not raw.endswith(b"\r\n"):
        return None
    ch_byte = raw[-3]
    if (65 <= ch_byte <= 90) or (97 <= ch_byte <= 122):
        return chr(ch_byte)
    return None


def normalize_channel(ch: str, normalize_ch_prefix: bool) -> str:
    # If requested, map "chA" -> "A" (case-insensitive "ch")
    if normalize_ch_prefix and len(ch) == 3 and ch[:2].lower() == "ch":
        last = ch[2:]
        if len(last) == 1 and last.isalpha():
            return last
    return ch


def drain_for_seconds(ser, seconds: float):
    if seconds <= 0:
        return
    end = time.monotonic() + seconds
    ser.reset_input_buffer()
    while time.monotonic() < end:
        _ = ser.read(4096)


def main():
    p = argparse.ArgumentParser(
        description="Measure per-channel rates from a CRLF serial stream."
    )
    p.add_argument("--port", required=True, help="Serial port (e.g. COM3, /dev/ttyACM0)")
    p.add_argument("--baud", type=int, default=115200, help="Baud rate")
    p.add_argument("--window", type=float, default=5.0, help="Averaging window seconds")
    p.add_argument("--report-every", type=float, default=1.0, help="Report cadence seconds")
    p.add_argument("--timeout", type=float, default=1.0, help="Serial read timeout (s)")
    p.add_argument("--encoding", default="utf-8", help="Text encoding")
    p.add_argument("--debug-drop", type=int, default=0, help="Print up to N dropped frames after start")
    p.add_argument("--warmup", type=float, default=None,
                   help="Seconds to suppress printing after stream becomes active "
                        "(default: 2×window; use 0 to disable)")
    p.add_argument("--flush-seconds", type=float, default=1.0,
                   help="Seconds to actively drain/discard input after opening port (default: 1.0)")
    # Active stream detection
    p.add_argument("--active-window", type=float, default=1.0,
                   help="Seconds used to detect active stream (default: 1.0)")
    p.add_argument("--min-events-total", type=int, default=150,
                   help="Total lines within active-window to mark stream active (default: 150)")
    # Channel handling
    p.add_argument("--channels", nargs="+", default=["A"],
                   help="Allowed channel labels after normalization (default: A). "
                        "Example: --channels chA or --normalize-ch-prefix --channels A")
    p.add_argument("--normalize-ch-prefix", action="store_true",
                   help="Normalize 'chX' -> 'X' for text lines")
    args = p.parse_args()

    allowed_channels = set(args.channels)
    warmup = 2.0 * args.window if args.warmup is None else max(0.0, args.warmup)

    try:
        ser = serial.Serial(
            port=args.port,
            baudrate=args.baud,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=args.timeout,
        )
    except serial.SerialException as e:
        print(f"Failed to open {args.port}: {e}", file=sys.stderr)
        sys.exit(2)

    print(f"Opened {args.port} @ {args.baud} baud. Averaging window: {args.window}s")
    if args.flush_seconds > 0:
        print(f"Draining input for {args.flush_seconds:.2f}s to avoid stale buffered data...")
    drain_for_seconds(ser, args.flush_seconds)
    ser.reset_input_buffer()
    print("Waiting for data lines (ignoring lines beginning with '#')...")

    stamps = defaultdict(deque)   # ch -> deque[float]
    known_channels = set()

    total_counts = defaultdict(int)
    run_start = None

    stream_seen = False
    active_seen = False
    t_first_data = 0.0
    next_report = 0.0
    dropped_preview = 0
    first_print_done = False

    recent_events = deque()

    def prune_window(now):
        cutoff = now - args.window
        for ch in list(known_channels):
            dq = stamps[ch]
            while dq and dq[0] < cutoff:
                dq.popleft()

    def compute_window(now):
        prune_window(now)
        counts = {ch: len(stamps[ch]) for ch in stamps.keys()}
        if not active_seen:
            return {}, {}, 0.0, 0.0
        elapsed = now - run_start
        duration = args.window if elapsed >= args.window else max(elapsed, 1e-6)
        rates = {ch: counts.get(ch, 0) / duration for ch in counts.keys()}
        return rates, counts, duration, elapsed

    def update_active(now):
        nonlocal active_seen, run_start, next_report
        cutoff = now - args.active_window
        while recent_events and recent_events[0] < cutoff:
            recent_events.popleft()
        if not active_seen and len(recent_events) >= args.min_events_total:
            active_seen = True
            run_start = now
            next_report = run_start + args.report_every
            print("Active measurement stream detected. Whole-run averaging starts now.")
            if warmup > 0:
                print(f"Warming up for {warmup:.2f}s before first on-screen report...")

    try:
        while True:
            # Read a full CRLF-terminated frame
            raw = ser.read_until(b"\r\n")
            if not raw:
                continue

            # Text-first: decode and try text formats
            content = raw[:-2] if raw.endswith(b"\r\n") else raw.rstrip(b"\r\n")
            text = content.decode(args.encoding, errors="ignore")
            ch = parse_text_line(text)

            # Fallback to binary channel (byte before CRLF) if text didn't match
            if ch is None:
                ch = parse_channel_from_crlf_frame(raw)

            # Normalize and apply whitelist
            if ch is not None:
                ch = normalize_channel(ch, args.normalize_ch_prefix)
                if ch not in allowed_channels:
                    ch = None

            if ch is not None:
                now = time.monotonic()
                if not stream_seen:
                    stream_seen = True
                    t_first_data = now
                    print("First data-format line received.")

                # Record timestamp
                stamps[ch].append(now)

                # Active detection and counts
                recent_events.append(now)
                update_active(now)
                if active_seen:
                    total_counts[ch] += 1
                    known_channels.add(ch)
            else:
                if stream_seen and args.debug_drop and dropped_preview < args.debug_drop:
                    tail = raw[-12:]
                    hex_tail = tail.hex(" ")
                    ascii_tail = "".join(chr(b) if 32 <= b <= 126 else "." for b in tail)
                    print(
                        f"[drop] len={len(raw)} ends_crlf={raw.endswith(b'\\r\\n')} "
                        f"tail_hex={hex_tail} tail_ascii={ascii_tail!r} text={text!r}"
                    )
                    dropped_preview += 1

            if active_seen:
                now = time.monotonic()
                if now >= next_report:
                    intervals = int((now - next_report) // args.report_every) + 1
                    next_report += intervals * args.report_every

                    rates, counts, duration, elapsed = compute_window(now)

                    # First print gate: warmup elapsed and at least one event in window
                    if not first_print_done:
                        if elapsed < warmup:
                            continue
                        if sum(counts.values()) == 0:
                            continue
                        first_print_done = True

                    # Only print channels present in this window
                    ch_list = sorted([c for c, cnt in counts.items() if cnt > 0])
                    if not ch_list:
                        continue

                    print(
                        " | ".join(
                            f"{c}: {rates.get(c, 0.0):.2f} Hz "
                            f"(count={counts.get(c, 0)} over {duration:.2f}s)"
                            for c in ch_list
                        )
                    )

    except KeyboardInterrupt:
        print("\nExiting...")
        try:
            now = time.monotonic()
            rates, counts, duration, elapsed = compute_window(now)

            if active_seen and run_start is not None:
                run_seconds = max(1e-6, now - run_start)
                ch_list = sorted([c for c, tot in total_counts.items() if tot > 0])
                if ch_list:
                    print("Final whole-run averages (from active stream start):")
                    for c in ch_list:
                        avg_hz = total_counts[c] / run_seconds
                        print(f"  {c}: {avg_hz:.4f} Hz over {run_seconds:.2f}s (total={total_counts[c]})")
                else:
                    print("No channel events recorded during active run.")
            elif stream_seen:
                print("Data lines were seen, but the stream never reached the active threshold.")
            else:
                print("No data lines received.")

            window_chs = sorted([c for c, cnt in counts.items() if cnt > 0])
            if window_chs:
                print("Last window snapshot:")
                print(
                    " | ".join(
                        f"{c}: {rates.get(c, 0.0):.2f} Hz "
                        f"(count={counts.get(c, 0)} over {duration:.2f}s)"
                        for c in window_chs
                    )
                )
        finally:
            try:
                ser.close()
            except Exception:
                pass


if __name__ == "__main__":
    main()
