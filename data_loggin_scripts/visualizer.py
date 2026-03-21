#!/usr/bin/env python3
"""
tof_visualizer.py
-----------------
Wizualizacja danych VL53L8CX 8x8 w czasie rzeczywistym przez UART.
Pokazuje siatkę 8x8 z dystansami w mm jako heatmapę.

Użycie:
  python tof_visualizer.py
  python tof_visualizer.py --port /dev/ttyACM1
  python tof_visualizer.py --port /dev/ttyACM1 --save dane.csv --rows 500
"""

import argparse
import serial
import serial.tools.list_ports
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import csv
import sys
import re
from collections import deque

# ── Konfiguracja ─────────────────────────────────────────────
DEFAULT_BAUD   = 115200
GRID_SIZE      = 8
DIST_MIN       = 0
DIST_MAX       = 4000   # mm
HISTORY        = 100    # ile ostatnich FPS do liczenia


def find_port():
    for p in serial.tools.list_ports.comports():
        if p.vid in {0x0483, 0x1366}:
            return p.device
    return None


def parse_line(line):
    """Parsuje linię danych: distance,signal,distance,signal,...
    Zwraca tablicę 64 dystansów (co drugi element)."""
    try:
        vals = [int(x) for x in re.split(r'[,\s]+', line.strip()) if x]
        if len(vals) < 64:
            return None
        # co drugi element to dystans (indeksy 0,2,4,...)
        # co drugi to signal (indeksy 1,3,5,...)
        if len(vals) >= 128:
            distances = [vals[i] for i in range(0, 128, 2)]  # 64 dystansów
        else:
            distances = vals[:64]
        return np.array(distances, dtype=float).reshape(8, 8)
    except (ValueError, IndexError):
        return None


def run(port, baud, save_file, max_rows):
    try:
        ser = serial.Serial(port, baud, timeout=2)
    except serial.SerialException as e:
        sys.exit(f"[BŁĄD] Nie można otworzyć {port}: {e}")

    print(f"Połączono: {port} @ {baud} baud")
    print("Zamknij okno żeby zatrzymać.\n")

    # ── CSV logger ───────────────────────────────────────────
    csv_file   = None
    csv_writer = None
    rows_saved = 0
    if save_file:
        csv_file   = open(save_file, 'w', newline='')
        csv_writer = csv.writer(csv_file)
        print(f"Zapisuję do: {save_file}  (max {max_rows if max_rows else '∞'} wierszy)")

    # ── Matplotlib setup ─────────────────────────────────────
    fig, axes = plt.subplots(1, 2, figsize=(14, 6))
    fig.patch.set_facecolor('#1a1a2e')

    # Lewa: heatmapa
    ax_heat = axes[0]
    ax_heat.set_facecolor('#1a1a2e')
    data_init = np.zeros((8, 8))
    im = ax_heat.imshow(data_init, cmap='plasma',
                        vmin=DIST_MIN, vmax=DIST_MAX,
                        interpolation='nearest', aspect='equal')
    cbar = fig.colorbar(im, ax=ax_heat)
    cbar.set_label('Dystans [mm]', color='white')
    cbar.ax.yaxis.set_tick_params(color='white')
    plt.setp(cbar.ax.yaxis.get_ticklabels(), color='white')

    ax_heat.set_title('VL53L8CX 8×8 — Dystans [mm]',
                       color='white', fontsize=13, pad=10)
    ax_heat.set_xticks(range(8))
    ax_heat.set_yticks(range(8))
    ax_heat.tick_params(colors='white')

    # Tekst z wartościami w każdej komórce
    cell_texts = []
    for row in range(8):
        row_texts = []
        for col in range(8):
            t = ax_heat.text(col, row, '', ha='center', va='center',
                             fontsize=7, color='white', fontweight='bold')
            row_texts.append(t)
        cell_texts.append(row_texts)

    # Prawa: histogram dystansów
    ax_hist = axes[1]
    ax_hist.set_facecolor('#16213e')
    ax_hist.tick_params(colors='white')
    ax_hist.spines['bottom'].set_color('#444')
    ax_hist.spines['left'].set_color('#444')
    ax_hist.spines['top'].set_visible(False)
    ax_hist.spines['right'].set_visible(False)
    ax_hist.set_xlabel('Dystans [mm]', color='white')
    ax_hist.set_ylabel('Liczba stref', color='white')
    ax_hist.set_title('Rozkład dystansów', color='white', fontsize=13)
    ax_hist.set_xlim(0, DIST_MAX)
    ax_hist.set_ylim(0, 64)

    # Status bar
    status_text = fig.text(0.5, 0.02, 'Oczekiwanie na dane...',
                           ha='center', color='#aaaaaa', fontsize=10)

    fps_times = deque(maxlen=HISTORY)
    frame_count = [0]
    current_grid = [None]

    def update_frame(frame):
        try:
            raw = ser.readline()
            line = raw.decode('ascii', errors='ignore').strip()
        except Exception:
            return

        if not line or not line[0].isdigit():
            return

        grid = parse_line(line)
        if grid is None:
            return

        current_grid[0] = grid
        frame_count[0]  += 1

        # Aktualizuj heatmapę
        im.set_data(grid)

        # Aktualizuj tekst w komórkach
        for r in range(8):
            for c in range(8):
                val = int(grid[r, c])
                cell_texts[r][c].set_text(str(val))
                # kolor tekstu zależny od wartości (ciemny na jasnym tle)
                cell_texts[r][c].set_color(
                    'black' if grid[r, c] > DIST_MAX * 0.5 else 'white')

        # Aktualizuj histogram
        ax_hist.cla()
        ax_hist.set_facecolor('#16213e')
        ax_hist.tick_params(colors='white')
        ax_hist.spines['bottom'].set_color('#444')
        ax_hist.spines['left'].set_color('#444')
        ax_hist.spines['top'].set_visible(False)
        ax_hist.spines['right'].set_visible(False)
        ax_hist.set_xlabel('Dystans [mm]', color='white')
        ax_hist.set_ylabel('Liczba stref', color='white')
        ax_hist.set_title('Rozkład dystansów', color='white', fontsize=13)
        flat = grid.flatten()
        ax_hist.hist(flat, bins=20, range=(0, DIST_MAX),
                     color='#e94560', edgecolor='none', alpha=0.85)
        ax_hist.set_xlim(0, DIST_MAX)

        # Min / max / mean overlay
        valid = flat[flat > 0]
        if len(valid):
            stats = f"min={int(valid.min())}  max={int(valid.max())}  śr={int(valid.mean())} mm"
        else:
            stats = ""

        # CSV zapis
        nonlocal rows_saved
        if csv_writer and (max_rows == 0 or rows_saved < max_rows):
            csv_writer.writerow([int(v) for v in flat])
            rows_saved += 1
            if max_rows and rows_saved >= max_rows:
                status_text.set_text(
                    f"✓ Zapisano {rows_saved} wierszy do {save_file}")
                if csv_file:
                    csv_file.close()

        # Status
        saved_info = f" | CSV: {rows_saved}" if save_file else ""
        status_text.set_text(
            f"Klatka #{frame_count[0]}{saved_info}   {stats}")

    ani = animation.FuncAnimation(fig, update_frame,
                                  interval=50, cache_frame_data=False)

    plt.tight_layout(rect=[0, 0.05, 1, 1])
    plt.show()

    ser.close()
    if csv_file and not csv_file.closed:
        csv_file.close()
    print(f"\nZapisano {rows_saved} wierszy.")


def main():
    parser = argparse.ArgumentParser(
        description='VL53L8CX 8x8 live visualizer + CSV logger')
    parser.add_argument('--port', default=None)
    parser.add_argument('--baud', type=int, default=DEFAULT_BAUD)
    parser.add_argument('--save', default=None,
                        help='Plik CSV do zapisu (opcjonalny)')
    parser.add_argument('--rows', type=int, default=0,
                        help='Ile wierszy zapisać (0 = bez limitu)')
    args = parser.parse_args()

    port = args.port or find_port()
    if not port:
        sys.exit('[BŁĄD] Nie znaleziono portu. Podaj --port /dev/ttyACM1')

    run(port, args.baud, args.save, args.rows)


if __name__ == '__main__':
    main()