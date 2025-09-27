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
    # Supports:
    #   "<float> <channel>"            e.g., "1.234456 chA"
    #   "<id> <value> <channel>"       e.g., "000... 60617700 A"
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
    """
    raw is expected to include the entire frame ending with b'\\r\\n'.
    Take the byte just before CRLF as the channel if it's A–Z/a–z.
    Return str channel or None.
    """
    if len(raw) < 3:
        return None
    if not raw.endswith(b"\r\n"):
        return None
    ch_byte = raw[-3]  # byte right before the CRLF pair
    if (65 <= ch_byte <= 90) or (97 <= ch_byte <= 122):  # A-Z or a-z
        return chr(ch_byte)
    return None


def drain_for_seconds(ser, seconds: float):
    if seconds <= 0:
        return
    end = time.monotonic() + seconds
    ser.reset_input_buffer()
    # Actively read and discard to clear kernel/driver buffers
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
    p.add_argument(
        "--debug-drop",
        type=int,
        default=0,
        help="Print up to N dropped frames after start",
    )
    p.add_argument(
        "--warmup",
        type=float,
        default=None,
        help="Seconds to suppress printing after stream becomes active "
        "(default: 2×window; use 0 to disable)",
    )
    p.add_argument(
        "--flush-seconds",
        type=float,
        default=1.0,
        help="Seconds to actively drain/discard input after opening port (default: 1.0)",
    )
    # Active stream detection
    p.add_argument(
        "--active-window",
        type=float,
        default=1.0,
        help="Seconds used to detect active stream (default: 1.0)",
    )
    p.add_argument(
        "--min-events-total",
        type=int,
        default=150,
        help="Total lines within active-window to mark stream active (default: 150)",
    )
    # Channel whitelist
    p.add_argument(
        "--channels",
        nargs="+",
        default=["A"],
        help="Allowed channel labels (default: A). Example: --channels A B C",
    )
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
            timeout=args.timeout,  # used by read_until
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

    # Per-channel timestamp queues for windowed rates (monotonic times)
    stamps = defaultdict(deque)  # ch -> deque[float]
    known_channels = set()

    # Cumulative counts for whole-run averages (start when stream is active)
    total_counts = defaultdict(int)
    run_start = None  # monotonic time when stream becomes active

    # Startup/printing state
    stream_seen = False      # saw first data-format line
    active_seen = False      # stream declared active
    t_first_data = 0.0       # time of first data-format line
    next_report = 0.0
    dropped_preview = 0
    first_print_done = False

    # Active stream detector: keep times of recent events
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
        # Drop events older than active_window
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
            # Read exactly CRLF-terminated frame
            raw = ser.read_until(b"\r\n")
            if not raw:
                continue

            # Fast binary path: channel is the byte before CRLF
            ch = parse_channel_from_crlf_frame(raw)

            if ch is None:
                # If not binary letter-ending, try text format on the content before CRLF
                content = raw[:-2] if raw.endswith(b"\r\n") else raw.rstrip(b"\r\n")
                text = content.decode(args.encoding, errors="ignore")
                ch = parse_text_line(text)

            # Apply channel whitelist
            if ch is not None and ch not in allowed_channels:
                ch = None

            if ch is not None:
                now = time.monotonic()
                if not stream_seen:
                    stream_seen = True
                    t_first_data = now
                    print("First data-format line received.")

                # Record timestamp for windowed rate calculations
                stamps[ch].append(now)

                # Active detection and whole-run counting
                recent_events.append(now)
                update_active(now)
                if active_seen:
                    total_counts[ch] += 1
                    # Only consider channel "known" once it has contributed to totals
                    known_channels.add(ch)
            else:
                # Optional debug for malformed lines after we started seeing data
                if stream_seen and args.debug_drop and dropped_preview < args.debug_drop:
                    # Show tail including CRLF presence check
                    tail = raw[-8:]
                    hex_tail = tail.hex(" ")
                    ascii_tail = "".join(chr(b) if 32 <= b <= 126 else "." for b in tail)
                    ends_crlf = raw.endswith(b"\r\n")
                    try:
                        content = raw[:-2] if raw.endswith(b"\r\n") else raw.rstrip(b"\r\n")
                        text_preview = content.decode(args.encoding, errors="replace")
                    except Exception:
                        text_preview = "<decode error>"
                    print(
                        f"[drop] len={len(raw)} ends_crlf={ends_crlf} "
                        f"tail_hex={hex_tail} tail_ascii={ascii_tail!r} text={text_preview!r}"
                    )
                    dropped_preview += 1

            if active_seen:
                now = time.monotonic()
                if now >= next_report:
                    intervals = int((now - next_report) // args.report_every) + 1
                    next_report += intervals * args.report_every

                    rates, counts, duration, elapsed = compute_window(now)

                    # Gate the very first print until we've warmed up
                    if not first_print_done:
                        if elapsed < warmup:
                            continue
                        # Proceed as soon as there's any event in the window
                        if sum(counts.values()) == 0:
                            continue
                        first_print_done = True

                    # Build list of channels that actually have samples in this window
                    ch_list = sorted([ch for ch, cnt in counts.items() if cnt > 0])
                    if not ch_list:
                        continue

                    print(
                        " | ".join(
                            f"{ch}: {rates.get(ch, 0.0):.2f} Hz "
                            f"(count={counts.get(ch, 0)} over {duration:.2f}s)"
                            for ch in ch_list
                        )
                    )

    except KeyboardInterrupt:
        print("\nExiting...")
        try:
            now = time.monotonic()
            # Last-window snapshot
            rates, counts, duration, elapsed = compute_window(now)

            if active_seen and run_start is not None:
                run_seconds = max(1e-6, now - run_start)
                # Only show channels with totals > 0
                ch_list = sorted([ch for ch, tot in total_counts.items() if tot > 0])
                if ch_list:
                    print("Final whole-run averages (from active stream start):")
                    for ch in ch_list:
                        avg_hz = total_counts[ch] / run_seconds
                        print(
                            f"  {ch}: {avg_hz:.4f} Hz over {run_seconds:.2f}s "
                            f"(total={total_counts[ch]})"
                        )
                else:
                    print("No channel events recorded during active run.")
            elif stream_seen:
                print("Data lines were seen, but the stream never reached the active threshold.")
            else:
                print("No data lines received.")

            # Also print the last-window snapshot for context (only non-empty)
            window_chs = sorted([ch for ch, cnt in counts.items() if cnt > 0])
            if window_chs:
                print("Last window snapshot:")
                print(
                    " | ".join(
                        f"{ch}: {rates.get(ch, 0.0):.2f} Hz "
                        f"(count={counts.get(ch, 0)} over {duration:.2f}s)"
                        for ch in window_chs
                    )
                )
        finally:
            try:
                ser.close()
            except Exception:
                pass

if __name__ == "__main__":
    main()

