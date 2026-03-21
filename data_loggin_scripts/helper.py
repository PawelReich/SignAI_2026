#!/usr/bin/env python3
"""
visualize_classes.py
Wizualizacja zebranych danych CSV dla każdej klasy ToF.
Użycie: python visualize_classes.py
"""

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import os
import glob

# ── Pliki klas — dopasuj nazwy do swoich plików ──────────────
CLASS_FILES = {
    "free":    "data_tests/1/class_free.csv",
    "center":  "data_tests/1/class_center.csv",
    "left":    "data_tests/1/class_left.csv",
    "right":   "data_tests/1/class_right.csv",
    "top":     "data_tests/1/class_top.csv",
    "bottom":  "data_tests/1/class_bottom.csv",
    "corner":  "data_tests/1/class_corner.csv",
}

DIST_MAX = 4000


def load_csv(filepath):
    rows = []
    with open(filepath) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            try:
                vals = [int(x) for x in line.split(',') if x]
                if len(vals) == 64:
                    rows.append(vals)
            except ValueError:
                continue
    return np.array(rows, dtype=float) if rows else None


def plot_class_overview():
    available = {name: path for name, path in CLASS_FILES.items()
                 if os.path.exists(path)}

    if not available:
        found = glob.glob("*.csv")
        print(f"Nie znaleziono plikow klas. Dostepne CSV: {found}")
        print("Zmien CLASS_FILES w skrypcie na swoje nazwy plikow.")
        return

    n = len(available)
    fig = plt.figure(figsize=(5 * n, 10))
    fig.patch.set_facecolor('#1a1a2e')
    fig.suptitle('Porownanie klas - srednia heatmapa 8x8',
                 color='white', fontsize=14, y=1.01)

    gs = gridspec.GridSpec(2, n, hspace=0.4, wspace=0.3)

    for col, (name, path) in enumerate(available.items()):
        data = load_csv(path)
        if data is None:
            continue

        print(f"  {name:12s}: {len(data):4d} wierszy  "
              f"min={int(data.min()):5d}  max={int(data.max()):5d}  "
              f"sr={int(data.mean()):5d} mm")

        mean_grid = data.mean(axis=0).reshape(8, 8)

        ax_h = fig.add_subplot(gs[0, col])
        ax_h.set_facecolor('#0d0d1a')
        im = ax_h.imshow(mean_grid, cmap='plasma',
                         vmin=0, vmax=DIST_MAX,
                         interpolation='nearest', aspect='equal')
        ax_h.set_title(f'{name}\n({len(data)} sampli)',
                       color='white', fontsize=10)
        ax_h.set_xticks([])
        ax_h.set_yticks([])

        for r in range(8):
            for c in range(8):
                v = int(mean_grid[r, c])
                ax_h.text(c, r, str(v) if v < DIST_MAX else '-',
                          ha='center', va='center', fontsize=5,
                          color='black' if mean_grid[r, c] > DIST_MAX * 0.5
                          else 'white', fontweight='bold')

        fig.colorbar(im, ax=ax_h, fraction=0.046, pad=0.04).ax.tick_params(
            colors='white', labelsize=6)

        ax_hist = fig.add_subplot(gs[1, col])
        ax_hist.set_facecolor('#16213e')
        ax_hist.tick_params(colors='white', labelsize=7)
        ax_hist.spines['top'].set_visible(False)
        ax_hist.spines['right'].set_visible(False)
        ax_hist.spines['bottom'].set_color('#555')
        ax_hist.spines['left'].set_color('#555')
        ax_hist.set_xlabel('mm', color='white', fontsize=8)
        ax_hist.set_ylabel('count', color='white', fontsize=8)

        valid = data[data < DIST_MAX - 100].flatten()
        if len(valid):
            ax_hist.hist(valid, bins=30, color='#e94560',
                         edgecolor='none', alpha=0.85)
        ax_hist.set_xlim(0, DIST_MAX)

    plt.tight_layout()
    plt.savefig('classes_overview.png', dpi=150, bbox_inches='tight',
                facecolor='#1a1a2e')
    print("\nZapisano: classes_overview.png")
    plt.show()


def plot_sample_frames():
    available = {name: path for name, path in CLASS_FILES.items()
                 if os.path.exists(path)}
    if not available:
        return

    N_SAMPLES = 5
    n_classes = len(available)

    fig, axes = plt.subplots(n_classes, N_SAMPLES,
                             figsize=(N_SAMPLES * 2.5, n_classes * 2.5))
    fig.patch.set_facecolor('#1a1a2e')
    fig.suptitle('Przykladowe klatki per klasa',
                 color='white', fontsize=13, y=1.01)

    if n_classes == 1:
        axes = [axes]

    for row, (name, path) in enumerate(available.items()):
        data = load_csv(path)
        if data is None or len(data) < N_SAMPLES:
            continue

        indices = np.linspace(0, len(data) - 1, N_SAMPLES, dtype=int)

        for col, idx in enumerate(indices):
            ax = axes[row][col]
            ax.set_facecolor('#0d0d1a')
            ax.imshow(data[idx].reshape(8, 8), cmap='plasma',
                      vmin=0, vmax=DIST_MAX,
                      interpolation='nearest', aspect='equal')
            ax.set_xticks([])
            ax.set_yticks([])
            if col == 0:
                ax.set_ylabel(name, color='white', fontsize=9,
                              rotation=0, labelpad=40, va='center')
            if row == 0:
                ax.set_title(f'#{idx}', color='#aaa', fontsize=8)

    plt.tight_layout()
    plt.savefig('classes_samples.png', dpi=150, bbox_inches='tight',
                facecolor='#1a1a2e')
    print("Zapisano: classes_samples.png")
    plt.show()


def plot_distance_over_time():
    available = {name: path for name, path in CLASS_FILES.items()
                 if os.path.exists(path)}
    if not available:
        return

    fig, ax = plt.subplots(figsize=(14, 5))
    fig.patch.set_facecolor('#1a1a2e')
    ax.set_facecolor('#16213e')
    ax.tick_params(colors='white')
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.spines['bottom'].set_color('#555')
    ax.spines['left'].set_color('#555')
    ax.set_xlabel('Klatka', color='white', fontsize=10)
    ax.set_ylabel('Sredni dystans [mm]', color='white', fontsize=10)
    ax.set_title('Sredni dystans w czasie - wszystkie klasy',
                 color='white', fontsize=12)

    colors = ['#e94560', '#0f9b8e', '#f5a623', '#7b68ee',
              '#50fa7b', '#ff79c6', '#8be9fd']

    for (name, path), color in zip(available.items(), colors):
        data = load_csv(path)
        if data is None:
            continue
        means = []
        for row in data:
            valid = row[row < DIST_MAX - 100]
            means.append(float(valid.mean()) if len(valid) else float(DIST_MAX))
        ax.plot(means, label=name, color=color, linewidth=1.5, alpha=0.85)

    ax.legend(facecolor='#1a1a2e', edgecolor='#444',
              labelcolor='white', fontsize=9)
    plt.tight_layout()
    plt.savefig('classes_timeline.png', dpi=150, bbox_inches='tight',
                facecolor='#1a1a2e')
    print("Zapisano: classes_timeline.png")
    plt.show()


if __name__ == '__main__':
    print("=== Wizualizacja danych klas ToF ===\n")

    csvs = glob.glob("*.csv")
    print(f"Znalezione pliki CSV: {csvs}\n")

    print("1. Srednie heatmapy...")
    plot_class_overview()

    print("2. Przykladowe klatki...")
    plot_sample_frames()

    print("3. Dystans w czasie...")
    plot_distance_over_time()

    print("\nGotowe!")