#!/usr/bin/env python3
"""
tof_visualizer.py
Wizualizator VL53L8CX 8x8 — Qt5Agg z poprawnym pełnym odświeżaniem.
Wyświetla aktualną klasę pod animacją (tylko linie "# -> class_xxx").
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
import threading
from collections import deque

# ── Konfiguracja ─────────────────────────────────────────────
DEFAULT_BAUD    = 115200    
DIST_MAX        = 2000
MIN_VALID_DIST  = 30
SERIAL_QUEUE    = deque(maxlen=5)
CLASS_QUEUE     = deque(maxlen=1)
CSV_FLUSH_EVERY = 20

# Kolory dla klas — dodaj swoje nazwy klas jeśli inne
CLASS_COLORS = {
    'CLASS_FREE':   '#00ff99',
    'CLASS_WALL':   '#ff4444',
    'CLASS_CENTER': '#44aaff',
    'CLASS_LEFT':   '#ffaa00',
    'CLASS_RIGHT':  '#ffaa00',
    'CLASS_TOP':    '#cc44ff',
    'CLASS_BOTTOM': '#cc44ff',
    'FREE':         '#00ff99',
    'WALL':         '#ff4444',
    'CENTER':       '#44aaff',
    'LEFT':         '#ffaa00',
    'RIGHT':        '#ffaa00',
    'TOP':          '#cc44ff',
    'BOTTOM':       '#cc44ff',
}
DEFAULT_CLASS_COLOR = '#ffffff'


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
            if not line:
                continue

            # Tylko linie "# -> class_xxx" lub "# -> FREE" itp.
            if line.startswith('#'):
                m = re.search(r'->\s*(\S+)', line)
                if m:
                    CLASS_QUEUE.append(m.group(1).upper())
                continue  # wszystkie inne linie z # pomijamy

            # Pomijaj linie startowe/statusowe
            if line[0] in ('W', 'T', 'V', 'O', 'N'):
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
    plt.ioff()

    fig, ax = plt.subplots(figsize=(7, 8))
    fig.patch.set_facecolor('#1a1a2e')
    plt.subplots_adjust(left=0.05, right=0.95, bottom=0.15, top=0.92)

    ax.set_facecolor('#1a1a2e')
    init_data = np.full((8, 8), float(DIST_MAX))

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

    # Pasek tła dla etykiety klasy
    class_bg = fig.add_axes([0.1, 0.04, 0.8, 0.07])
    class_bg.set_facecolor('#2a2a4a')
    class_bg.set_xticks([])
    class_bg.set_yticks([])
    for spine in class_bg.spines.values():
        spine.set_edgecolor('#444466')

    # Duża etykieta klasy
    class_text = fig.text(0.5, 0.075, '—',
                          ha='center', va='center',
                          color='white', fontsize=22, fontweight='bold',
                          family='monospace')

    status_text = fig.text(0.5, 0.01, 'Oczekiwanie...',
                           ha='center', color='#aaaaaa', fontsize=8)

    frame_count = [0]

    def update_frame(_):
        # Nowa klasa?
        if CLASS_QUEUE:
            name = CLASS_QUEUE[-1]
            CLASS_QUEUE.clear()
            color = CLASS_COLORS.get(name, DEFAULT_CLASS_COLOR)
            class_text.set_text(name)
            class_text.set_color(color)
            # Tło paska w kolorze klasy (bardzo przezroczyste)
            r = int(color[1:3], 16) / 255
            g = int(color[3:5], 16) / 255
            b = int(color[5:7], 16) / 255
            class_bg.set_facecolor((r, g, b, 0.15))

        # Nowe dane z czujnika?
        if not SERIAL_QUEUE:
            return

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
        stats = (f"#{frame_count[0]}  n={len(valid)}/64  "
                 f"min={int(valid.min())}  śr={int(valid.mean())} mm"
                 if len(valid) > 0 else f"#{frame_count[0]}  Brak obiektów")

        if csv_writer and (max_rows == 0 or rows_saved[0] < max_rows):
            csv_writer.writerow([int(v) for v in flat])
            rows_saved[0] += 1
            if rows_saved[0] % CSV_FLUSH_EVERY == 0:
                csv_file.flush()

        csv_info = f" | CSV:{rows_saved[0]}" if save_file else ""
        status_text.set_text(stats + csv_info)

    ani = animation.FuncAnimation(
        fig, update_frame,
        interval=50,
        blit=False,
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