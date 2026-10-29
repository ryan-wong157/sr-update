"""TEMP thesis benchmark helper: summarises ota-bench CSVs, one block per image.

Timing stats only use confirmed runs, so a failed or reverted run doesn't skew them.
"""
import argparse
import csv
import statistics
from collections import Counter, defaultdict
from pathlib import Path

COLUMNS = ["total_us", "transport_us", "erase_us", "erase_avg_ns", "write_us", "write_avg_ns", "hash_us", "sig_us", "swap_us",
           "stack_tx_bytes", "stack_boot_bytes"]


def summarise(image: str, rows: list[dict]) -> None:
    confirmed = [row for row in rows if row["result"] == "confirmed"]
    sizes = sorted({row["bytes"] for row in rows if row["bytes"]})
    print(f"\n=== {image} ({', '.join(sizes) or '?'} bytes) ===")
    print(f"runs: {len(rows)}, confirmed: {len(confirmed)} ({100 * len(confirmed) / len(rows):.1f}%)")

    # failures grouped by result, error rows include the stage they died in
    others = Counter(row["result"] for row in rows if row["result"] != "confirmed")
    for result, count in others.most_common():
        print(f"  {count:>4} x {result}")

    if not confirmed:
        return
    print(f"  {'column':<18}{'mean':>12}{'stdev':>12}{'min':>12}{'max':>12}{'n':>6}")
    for column in COLUMNS:
        values = [float(row[column]) for row in confirmed if row.get(column)]
        if not values:
            print(f"  {column:<18}{'-':>12}")
            continue
        stdev = statistics.stdev(values) if len(values) > 1 else 0.0
        print(f"  {column:<18}{statistics.mean(values):>12.1f}{stdev:>12.1f}{min(values):>12.0f}{max(values):>12.0f}{len(values):>6}")


def main():
    parser = argparse.ArgumentParser(description="Average the ota-bench CSV columns per image")
    parser.add_argument("csv", type=Path, nargs="+", help="One or more ota-bench CSV files")
    args = parser.parse_args()

    by_image = defaultdict(list)
    for path in args.csv:
        with path.open(newline="") as f:
            for row in csv.DictReader(f):
                by_image[row["image"]].append(row)

    for image, rows in by_image.items():
        summarise(image, rows)


if __name__ == "__main__":
    main()
