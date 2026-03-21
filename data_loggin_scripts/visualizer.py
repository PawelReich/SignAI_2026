#!/usr/bin/env python3
"""
tof_visualizer.py
Wizualizator VL53L8CX 8x8 — Qt5Agg z poprawnym pełnym odświeżaniem.
"""

import argparse
import serial
import serial.tools.list_ports
import numpy as np
import matplotlib
matplotlib.use('Qt5Agg')
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import csv
import sys
import re
import datetime
import threading
from collections import deque

# ── Konfiguracja ─────────────────────────────────────────────
DEFAULT_BAUD    = 2000000
DIST_MAX        = 2000
MIN_VALID_DIST  = 30
SERIAL_QUEUE    = deque(maxlen=5)
CSV_FLUSH_EVERY = 20


def find_port():
    for p in serial.tools.list_ports.comports():
        if p.vid in {0x0483, 0x1366}:
            print(f"[auto] {p.device} ({p.description})")
            return p.device
    return None


def parse_line(line):
    try:
        vals = [int(x) for x in re.split(r'[,\s]+', line) if x]
        if len(vals) >= 128:
            vals = [vals[i] for i in range(0, 128, 2)]
        if len(vals) < 64:
            return None
        return np.array(vals[:64], dtype=float).reshape(8, 8)
    except (ValueError, IndexError):
        return None


def uart_reader(ser, queue, stop_event):
    while not stop_event.is_set():
        try:
            raw = ser.readline()
            if not raw:
                continue
            line = raw.decode('ascii', errors='ignore').strip()
            if not line or line[0] in ('#', 'W', 'T', 'V', 'O', 'N'):
                continue
            grid = parse_line(line)
            if grid is not None:
                queue.append(grid)
        except Exception:
            continue


def run(port, baud, save_file, max_rows):
    try:
        ser = serial.Serial(port, baud, timeout=1)
        ser.reset_input_buffer()
    except serial.SerialException as e:
        sys.exit(f"[BŁĄD] {e}")

    print(f"Połączono: {port} @ {baud} baud")

    # ── CSV ──────────────────────────────────────────────────
    csv_file   = None
    csv_writer = None
    rows_saved = [0]

    if save_file:
        csv_file   = open(save_file, 'w', newline='', buffering=65536)
        csv_writer = csv.writer(csv_file)
        print(f"CSV: {save_file}  (max {max_rows if max_rows else '∞'} wierszy)\n")

    # ── Wątek UART ───────────────────────────────────────────
    stop_event = threading.Event()
    t = threading.Thread(target=uart_reader,
                         args=(ser, SERIAL_QUEUE, stop_event), daemon=True)
    t.start()

    # ── Matplotlib ───────────────────────────────────────────
    # WAŻNE: interactive mode OFF — FuncAnimation sam kontroluje pętlę
    plt.ioff()

    fig, ax = plt.subplots(figsize=(7, 7))
    fig.patch.set_facecolor('#1a1a2e')
    plt.subplots_adjust(left=0.05, right=0.95, bottom=0.08, top=0.92)

    ax.set_facecolor('#1a1a2e')
    init_data = np.full((8, 8), float(DIST_MAX))

    # animated=False + blit=False = matplotlib rysuje wszystko od zera
    im = ax.imshow(init_data, cmap='plasma',
                   vmin=0, vmax=DIST_MAX,
                   interpolation='nearest', aspect='equal')

    cbar = fig.colorbar(im, ax=ax)
    cbar.set_label('mm', color='white', fontsize=10)
    cbar.ax.yaxis.set_tick_params(color='white', labelsize=8)
    plt.setp(cbar.ax.yaxis.get_ticklabels(), color='white')

    ax.set_title('VL53L8CX 8×8', color='white', fontsize=11, pad=6)
    ax.set_xticks(range(8))
    ax.set_yticks(range(8))
    ax.tick_params(colors='white', labelsize=8)

    cell_texts = [[
        ax.text(c, r, '', ha='center', va='center',
                fontsize=7, color='white', fontweight='bold')
        for c in range(8)] for r in range(8)]

    status_text = fig.text(0.5, 0.02, 'Oczekiwanie...',
                           ha='center', color='#aaaaaa', fontsize=9)

    frame_count = [0]

    def update_frame(_):
        if not SERIAL_QUEUE:
            return

        # Zawsze najnowsza klatka
        grid = SERIAL_QUEUE[-1]
        SERIAL_QUEUE.clear()

        grid_display = grid.copy()
        grid_display[grid_display < MIN_VALID_DIST] = DIST_MAX

        frame_count[0] += 1

        im.set_data(grid_display)

        for r in range(8):
            for c in range(8):
                v = int(grid_display[r, c])
                if v >= DIST_MAX:
                    cell_texts[r][c].set_text('')
                else:
                    cell_texts[r][c].set_text(str(v))
                    cell_texts[r][c].set_color(
                        '#111' if v > DIST_MAX * 0.6 else 'white')

        flat = grid_display.flatten()
        valid = flat[flat < DIST_MAX - 100]
        stats = (f"n={len(valid)}/64  min={int(valid.min())}  śr={int(valid.mean())} mm"
                 if len(valid) > 0 else "Brak obiektów")

        if csv_writer and (max_rows == 0 or rows_saved[0] < max_rows):
            csv_writer.writerow([int(v) for v in flat])
            rows_saved[0] += 1
            if rows_saved[0] % CSV_FLUSH_EVERY == 0:
                csv_file.flush()

        csv_info = f" | CSV:{rows_saved[0]}" if save_file else ""
        status_text.set_text(f"#{frame_count[0]}{csv_info}  {stats}")

    ani = animation.FuncAnimation(
        fig, update_frame,
        interval=50,       # 20 FPS — wystarczy dla ODR=15Hz
        blit=False,        # pełne odświeżanie całej figury
        cache_frame_data=False
    )

    try:
        plt.show()
    finally:
        stop_event.set()
        ser.close()
        if csv_file and not csv_file.closed:
            csv_file.flush()
            csv_file.close()
        print(f"\nZapisano {rows_saved[0]} wierszy")


def main():
    parser = argparse.ArgumentParser(
        description='VL53L8CX — wizualizator + CSV logger')
    parser.add_argument('--port',  default=None)
    parser.add_argument('--baud',  type=int, default=DEFAULT_BAUD)
    parser.add_argument('--save',  default=None)
    parser.add_argument('--rows',  type=int, default=0)
    args = parser.parse_args()

    port = args.port or find_port()
    if not port:
        sys.exit('[BŁĄD] Nie znaleziono portu. Podaj --port /dev/ttyACM0')

    run(port, args.baud, args.save, args.rows)


if __name__ == '__main__':
    main()