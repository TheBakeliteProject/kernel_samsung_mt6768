#!/usr/bin/env python3
"""Summarize CPUFreq residency snapshots from measure_governor.sh."""

import argparse
import csv
import sys
from pathlib import Path


def read(path):
    try:
        value = path.read_text().strip()
    except FileNotFoundError:
        return None
    return None if value == "NA" else value


def number(path):
    value = read(path)
    return int(value) if value is not None else None


def residency(path):
    result = {}
    for line in path.read_text().splitlines():
        frequency, ticks = line.split()
        result[int(frequency)] = int(ticks)
    return result


def decimal(value, digits=1):
    return "NA" if value is None else f"{value:.{digits}f}"


def summarize(run):
    meta = dict(
        line.split("\t", 1) for line in (run / "meta.tsv").read_text().splitlines()
    )
    before = run / "before"
    after = run / "after"
    elapsed = number(after / "epoch_seconds") - number(before / "epoch_seconds")
    charge_before = number(before / "battery.charge_counter")
    charge_after = number(after / "battery.charge_counter")
    charge_delta = None
    if charge_before is not None and charge_after is not None:
        charge_delta = (charge_before - charge_after) / 1000
    temperature_before = number(before / "battery.temp")
    temperature_after = number(after / "battery.temp")

    old_files = sorted(before.glob("policy*.time_in_state"))
    if not old_files:
        raise ValueError(f"No CPUFreq residency data found in {run}")
    for old_file in old_files:
        policy = old_file.name.removesuffix(".time_in_state")
        if (read(before / f"{policy}.governor") != meta["governor"] or
                read(after / f"{policy}.governor") != meta["governor"]):
            raise ValueError(f"Governor changed during {run}: {policy}")
        old = residency(old_file)
        new = residency(after / old_file.name)
        delta = {freq: new.get(freq, 0) - old.get(freq, 0) for freq in old.keys() | new.keys()}
        if any(ticks < 0 for ticks in delta.values()):
            raise ValueError(f"CPUFreq counters reset during {run}: {policy}")
        total = sum(delta.values())
        mean_mhz = (sum(freq * ticks for freq, ticks in delta.items()) / total / 1000
                    if total else None)
        top_pct = 100 * delta[max(delta)] / total if total else None
        old_trans = number(before / f"{policy}.total_trans")
        new_trans = number(after / f"{policy}.total_trans")
        transitions = (new_trans - old_trans
                       if old_trans is not None and new_trans is not None else None)
        if transitions is not None and transitions < 0:
            raise ValueError(f"CPUFreq transition counter reset during {run}: {policy}")
        yield [
            run.name, meta["governor"], policy, elapsed,
            decimal(mean_mhz), decimal(top_pct),
            transitions if transitions is not None else "NA",
            decimal(charge_delta, 2),
            decimal(temperature_before / 10 if temperature_before is not None else None),
            decimal(temperature_after / 10 if temperature_after is not None else None),
            read(before / "battery.status") or "NA",
            read(after / "battery.status") or "NA",
        ]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("runs", type=Path, nargs="+", help="measurement directories pulled from the phone")
    args = parser.parse_args()
    writer = csv.writer(sys.stdout, delimiter="\t")
    writer.writerow((
        "run", "governor", "policy", "elapsed_s", "mean_MHz", "at_max_percent",
        "transitions", "discharged_mAh", "battery_start_C", "battery_end_C",
        "status_start", "status_end",
    ))
    for run in args.runs:
        writer.writerows(summarize(run))


if __name__ == "__main__":
    main()
