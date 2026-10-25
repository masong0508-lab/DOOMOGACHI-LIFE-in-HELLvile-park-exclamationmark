"""scheduler.py - split dirty shots between emulator instances (owner: ENGINE
lane 1; spec 11.2).

Each job gets a contiguous run of shots (in episode order) of about equal
frame totals, so one boot serves many shots. Inside a job, consecutive
shots are one capture.Run (the player moves on to the next shot by itself);
a gap starts a new Run with a seek.
"""
from __future__ import annotations

from .capture import Run


def split(shots: list[int], lens: list[int], jobs: int) -> list[list[int]]:
    """shots: sorted indices; lens: frames per episode shot -> up to `jobs` groups."""
    shots = sorted(shots)
    if not shots:
        return []
    jobs = max(1, min(jobs, len(shots)))
    total = sum(lens[s] for s in shots)
    groups, cur, acc = [], [], 0
    for s in shots:
        cur.append(s)
        acc += lens[s]
        left_groups = jobs - len(groups) - 1
        if left_groups > 0 and acc >= total * (len(groups) + 1) / jobs:
            groups.append(cur)
            cur = []
    if cur:
        groups.append(cur)
    return groups


def runs_for(group: list[int], lens: list[int]) -> list[Run]:
    out = []
    for s in group:
        if out and out[-1].last == s - 1:
            out[-1].count += lens[s]
            out[-1].last = s
        else:
            out.append(Run(shot=s, frame=0, count=lens[s], last=s))
    return out
