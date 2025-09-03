#!/usr/bin/env python3
"""Parse nccl-tests output (all_reduce_perf/all_gather_perf) to CSV.
Columns: msg_bytes,bus_bw_GBs,avg_lat_us
Usage:
  python3 parse_nccl.py logs/allreduce.log -o allreduce.csv
"""
import re, csv, sys
from argparse import ArgumentParser
ap = ArgumentParser()
ap.add_argument('log')
ap.add_argument('-o', '--out', default='out.csv')
args = ap.parse_args()
pat = re.compile(r"\s*(\d+(?:\.\d+)?)([kKmMgG]?)\s+\S+\s+\S+\s+(\d+(?:\.\d+)?)\s+(\d+(?:\.\d+)?)")
unit = {'k':1024, 'K':1024, 'm':1024**2, 'M':1024**2, 'g':1024**3, 'G':1024**3}
rows = []
with open(args.log, 'r', encoding='utf-8', errors='ignore') as f:
    for line in f:
        m = pat.match(line)
        if not m: 
            continue
        size_val, size_unit, busbw, avglat = m.groups()
        size_b = float(size_val) * (unit.get(size_unit, 1))
        rows.append([int(size_b), float(busbw), float(avglat)])
if not rows:
    print("No data parsed. Check your log format.")
    sys.exit(1)
with open(args.out, 'w', newline='') as wf:
    w = csv.writer(wf)
    w.writerow(['msg_bytes','bus_bw_GBs','avg_lat_us'])
    w.writerows(rows)
print(f"Wrote {len(rows)} rows -> {args.out}")
