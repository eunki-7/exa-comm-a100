#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt
from argparse import ArgumentParser
ap = ArgumentParser()
ap.add_argument('csv')
ap.add_argument('-o','--out', default='plot.png')
ap.add_argument('--title', default='Performance')
args = ap.parse_args()
df = pd.read_csv(args.csv)
plt.figure()
plt.plot(df['msg_bytes'], df['bus_bw_GBs'], marker='o')
plt.xscale('log')
plt.xlabel('Message size (bytes)')
plt.ylabel('Bus BW (GB/s)')
plt.title(args.title)
plt.grid(True, which='both', linestyle='--', alpha=0.5)
plt.tight_layout()
plt.savefig(args.out, dpi=150)
print(f"Saved plot -> {args.out}")
