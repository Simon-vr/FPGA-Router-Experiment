#!/usr/bin/env python3
"""Build compact visualization data for docs from old vistual/results_* dumps.

NOTE: this only reshapes previously generated visualization JSON so the docs
page can display heatmaps / curves interactively. It does NOT represent the
results of the current experiment run and it does not regenerate any step JSON.

For each scenario it uniformly samples at most MAX_STEPS representative steps,
always including the first (index 0) and the last step.
"""
import json
import os

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(BASE, "docs", "data")
MAX_STEPS = 10

# scenario dir name -> (title, source directory under vistual/)
SCENARIOS = {
    "huge": ("huge — 41×41 网格，W=25，协商布线", "results_huge"),
    "large_dense": ("large_dense — 21×21 网格，W=25，协商布线", "results_large_dense"),
    "large_sparse": ("large_sparse — 21×21 网格，W=25，协商布线", "results_large_sparse"),
}


def sample_indices(n, k):
    """Uniformly pick k indices from range(n), always incl. 0 and n-1."""
    if n <= 0:
        return []
    if n <= k:
        return list(range(n))
    indices = sorted({round(i * (n - 1) / (k - 1)) for i in range(k)})
    return indices


def build_scenario(scenario, title, src_dir):
    manifest_path = os.path.join(BASE, "vistual", src_dir, "manifest.json")
    with open(manifest_path, encoding="utf-8") as f:
        manifest = json.load(f)

    step_entries = manifest["steps"]
    step_labels = [s["label"] for s in step_entries]
    indices = sample_indices(len(step_entries), MAX_STEPS)

    steps = []
    for idx in indices:
        entry = step_entries[idx]
        step_path = os.path.join(BASE, "vistual", src_dir, entry["file"])
        with open(step_path, encoding="utf-8") as f:
            step = json.load(f)
        tiles = [
            [t["x"], t["y"], t["occupancy"], t["capacity"], round(t["congestion_ratio"], 3)]
            for t in step["tiles"]
        ]
        steps.append({
            "index": idx,
            "label": entry["label"],
            "tiles": tiles,
        })

    return {
        "scenario": scenario,
        "title": title,
        "grid_width": manifest["grid_width"],
        "grid_height": manifest["grid_height"],
        "tracks": manifest["tracks"],
        "step_labels": step_labels,
        "congestion_history": manifest["congestion_history"],
        "resource_usage_history": manifest["resource_usage_history"],
        "steps": steps,
    }


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    total = 0
    summary = []
    for scenario, (title, src_dir) in SCENARIOS.items():
        data = build_scenario(scenario, title, src_dir)
        out_path = os.path.join(OUT_DIR, scenario + ".json")
        with open(out_path, "w", encoding="utf-8") as f:
            json.dump(data, f, ensure_ascii=False, separators=(",", ":"))
        size = os.path.getsize(out_path)
        total += size
        summary.append((scenario, data, len(data["steps"]), size))
    for scenario, data, nsteps, size in summary:
        print(f"{scenario:14s} {data['grid_width']}x{data['grid_height']} "
              f"steps_sampled={nsteps}/{len(data['step_labels'])}  {size/1024:.1f} KB")
    print(f"TOTAL {total/1024:.1f} KB")


if __name__ == "__main__":
    main()
