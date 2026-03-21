#!/usr/bin/env python3
"""
uart_csv_logger.py
──────────────────
Captures VL53L8CX distance data from an STM32 Nucleo over UART (USB-CDC)
and saves it as a CSV file ready for NanoEdge AI Studio.

Usage
-----
  python uart_csv_logger.py                         # auto-detect port
  python uart_csv_logger.py --port COM5             # Windows
  python uart_csv_logger.py --port /dev/ttyACM0     # Linux
  python uart_csv_logger.py --port /dev/tty.usbmodem14101  # macOS
  python uart_csv_logger.py --rows 500 --out data.csv

NanoEdge AI Studio expects:
  • Plain CSV, no header row  (header comment lines starting with '#' are stripped)
  • One sample per row
  • Consistent number of columns
  • All values numeric (integers or floats)
"""

import argparse
import csv
import datetime
import glob
import sys
import time
import serial
import serial.tools.list_ports

# ── Default settings ─────────────────────────────────────────
DEFAULT_BAUD    = 115200
DEFAULT_ROWS    = 1000          # how many data rows to capture (0 = infinite)
DEFAULT_TIMEOUT = 2.0           # serial read timeout in seconds
EXPECTED_COLS   = 64            # 8×8 ToF zones


# ── Port auto-detection ──────────────────────────────────────
def find_nucleo_port() -> str | None:
    """Return first ST-Link / Nucleo virtual COM port found."""
    known_vids = {0x0483, 0x1366}   # STMicroelectronics, SEGGER
    for p in serial.tools.list_ports.comports():
        if p.vid in known_vids:
            print(f"[auto] Found Nucleo/ST-Link on {p.device}  ({p.description})")
            return p.device
    # Fallback: first ACM or COM port
    patterns = ["/dev/ttyACM*", "/dev/tty.usbmodem*", "/dev/ttyUSB*"]
    for pat in patterns:
        matches = glob.glob(pat)
        if matches:
            print(f"[auto] Guessing {matches[0]}")
            return matches[0]
    return None


# ── CSV output filename ──────────────────────────────────────
def default_filename() -> str:
    ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    return f"tof_data_{ts}.csv"


# ── Main logger ──────────────────────────────────────────────
def run_logger(port: str, baud: int, max_rows: int, outfile: str):
    print(f"\n{'='*55}")
    print(f"  VL53L8CX → NanoEdge AI CSV Logger")
    print(f"  Port     : {port}  @{baud} baud")
    print(f"  Max rows : {'unlimited' if max_rows == 0 else max_rows}")
    print(f"  Output   : {outfile}")
    print(f"{'='*55}\n")

    try:
        ser = serial.Serial(port, baud, timeout=DEFAULT_TIMEOUT)
    except serial.SerialException as e:
        sys.exit(f"[ERROR] Cannot open {port}: {e}")

    time.sleep(0.5)   # let Nucleo boot / reset
    ser.reset_input_buffer()

    rows_written  = 0
    rows_skipped  = 0
    col_count     = None

    with open(outfile, "w", newline="") as csvfile:
        writer = csv.writer(csvfile)

        print("Logging… press Ctrl+C to stop early.\n")
        try:
            while max_rows == 0 or rows_written < max_rows:
                raw = ser.readline()

                # Decode; skip if garbled
                try:
                    line = raw.decode("ascii").strip()
                except UnicodeDecodeError:
                    rows_skipped += 1
                    continue

                # Skip empty lines and comment lines (header info from firmware)
                if not line or line.startswith("#"):
                    if line:
                        print(f"  [info] {line}")
                    continue

                # Skip the text header row (d00,d01,...)
                if line.startswith("d"):
                    print(f"  [header] {line}")
                    continue

                # Parse numeric CSV row
                import re
                values_raw = re.split(r'[,\s]+', line.strip())
                values_raw = [v for v in values_raw if v]  # usuń puste
                try:
                    values = [int(float(v)) for v in values_raw]
                except ValueError:
                    rows_skipped += 1
                    continue

                # Validate column count
                if col_count is None:
                    col_count = len(values)
                    print(f"  [info] {col_count} columns per row detected")
                    if col_count != EXPECTED_COLS:
                        print(f"  [warn] Expected {EXPECTED_COLS} cols, got {col_count}")

                if len(values) != col_count:
                    rows_skipped += 1
                    continue

                writer.writerow(values)
                rows_written += 1

                # Progress indicator every 50 rows
                if rows_written % 50 == 0:
                    print(f"  Rows captured: {rows_written}"
                          + (f" / {max_rows}" if max_rows else "")
                          + f"   (skipped: {rows_skipped})")

        except KeyboardInterrupt:
            print("\n[stopped by user]")
        finally:
            ser.close()

    print(f"\n{'='*55}")
    print(f"  Done!  {rows_written} rows saved to '{outfile}'")
    print(f"  Skipped / corrupt lines: {rows_skipped}")
    print(f"{'='*55}\n")
    print("NanoEdge AI Studio import tips:")
    print("  1. Open NanoEdge AI Studio → New project → Anomaly / Classification")
    print("  2. Import this CSV as your dataset (no header row required)")
    print("  3. Each row = 1 sample, all 64 ToF zone distances in mm")
    print("  4. Recommended: capture ≥500 rows per class for classification")


# ── CLI ──────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(
        description="Log VL53L8CX UART stream to NanoEdge AI-ready CSV")
    parser.add_argument("--port", default=None,
                        help="Serial port (auto-detected if omitted)")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD,
                        help=f"Baud rate (default {DEFAULT_BAUD})")
    parser.add_argument("--rows", type=int, default=DEFAULT_ROWS,
                        help="Number of data rows to capture (0 = unlimited)")
    parser.add_argument("--out", default=None,
                        help="Output CSV filename (timestamped by default)")
    args = parser.parse_args()

    port = args.port or find_nucleo_port()
    if port is None:
        sys.exit("[ERROR] No serial port found. Specify with --port")

    outfile = args.out or default_filename()
    run_logger(port, args.baud, args.rows, outfile)


if __name__ == "__main__":
    main()