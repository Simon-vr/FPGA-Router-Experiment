#!/usr/bin/env python3
"""Parse test_logs/*.log into experiment_results.json (authoritative, this run)."""
import json
import os
import re
import glob

LOG_DIR = "test_logs"

def parse_log(path):
    text = open(path, encoding="utf-8", errors="replace").read()
    r = {}
    m = re.search(r"Grid size:\s*(\d+)x(\d+)", text)
    r["grid"] = f"{m.group(1)}x{m.group(2)}" if m else None
    m = re.search(r"Successful routes:\s*(\d+)/(\d+)", text)
    if m:
        r["successRoutes"] = int(m.group(1))
        r["totalRoutes"] = int(m.group(2))
    else:
        r["successRoutes"] = None
        r["totalRoutes"] = None
    r["verifyPassed"] = bool(re.search(r"Routing check passed", text))
    m = re.search(r"Segments used:\s*(\d+)", text)
    if m:
        r["segments"] = int(m.group(1))
    else:
        segs = re.findall(r"Segments:\s*(\d+)\s*\|", text)
        r["segments"] = int(segs[-1]) if segs else None
    m = re.search(r"\[SUCCESS\] Routing converged at iteration (\d+)", text)
    if m:
        r["converged"] = True
        r["iterations"] = int(m.group(1))
    elif re.search(r"Failed to converge after (\d+) iterations", text):
        m = re.search(r"Failed to converge after (\d+) iterations", text)
        r["converged"] = False
        r["iterations"] = int(m.group(1))
    else:
        r["converged"] = None
        r["iterations"] = None
    m = re.search(r"ELAPSED=(\d+)", text)
    r["elapsedSec"] = int(m.group(1)) if m else None
    # final congestion (last iteration line)
    cong = re.findall(r"Congestion:\s*(\d+)\s*nodes,\s*(\d+)\s*overflow", text)
    if cong:
        r["finalCongestedNodes"] = int(cong[-1][0])
        r["finalOverflow"] = int(cong[-1][1])
    else:
        r["finalCongestedNodes"] = None
        r["finalOverflow"] = None
    return r

# name -> (benchmark, W, router, threads, maxiter)
manifest = {}
for b in ("lg_sparse", "large_dense", "huge"):
    for m in ("bfs", "astar", "mikami"):
        manifest[f"detail_{b}_W30_{m}"] = (b, 30, m, 16, 30)
for m in ("bfs", "astar", "mikami"):
    manifest[f"detail_large_dense_W25_{m}"] = ("large_dense", 25, m, 16, 30)
for W in (30, 25, 20):
    manifest[f"neg_med_dense_W{W}_t16"] = ("med_dense", W, "negotiated", 16, 50)
for t in (4, 8, 16):
    manifest[f"neg_med_dense_W25_t{t}"] = ("med_dense", 25, "negotiated", t, 50)
for W in (30, 25):
    manifest[f"neg_lg_sparse_W{W}_t18"] = ("lg_sparse", W, "negotiated", 18, 50)
for W in (30, 25):
    manifest[f"neg_large_dense_W{W}_t18"] = ("large_dense", W, "negotiated", 18, 50)
for W in (30, 25):
    manifest[f"neg_huge_W{W}_t18"] = ("huge", W, "negotiated", 18, 50)

results = []
for name, meta in manifest.items():
    path = os.path.join(LOG_DIR, name + ".log")
    if not os.path.exists(path):
        print("MISSING", path)
        continue
    parsed = parse_log(path)
    bench, W, router, threads, maxiter = meta
    rec = {
        "name": name,
        "benchmark": bench,
        "W": W,
        "router": router,
        "threads": threads,
        "maxiter": maxiter,
        "successRoutes": parsed["successRoutes"],
        "verifyPassed": parsed["verifyPassed"],
        "segments": parsed["segments"],
        "converged": parsed["converged"],
        "iterations": parsed["iterations"],
        "elapsedSec": parsed["elapsedSec"],
        "finalCongestedNodes": parsed["finalCongestedNodes"],
    }
    results.append(rec)

with open("experiment_results.json", "w", encoding="utf-8") as f:
    json.dump(results, f, indent=2, ensure_ascii=False)
print(f"Wrote {len(results)} records to experiment_results.json")
for rec in results:
    print(rec["name"], rec["verifyPassed"], rec["converged"], rec["elapsedSec"])
