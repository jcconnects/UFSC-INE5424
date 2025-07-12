#!/usr/bin/env python3
"""run_many.py – Repeat a shell command multiple times, log each run, and report statistics.

Usage:
    python tools/run_many.py -n 20 [-p 4] [-t 20] "make run_system_demo"

Creates a timestamped directory under tests/logs/ containing one log file per run.
Shows a live progress bar (tqdm) with success/failure counts and average run time.
At completion prints a summary with timing statistics and, for failed runs, the
trailing lines of each log file to aid debugging.

Requires: Python 3.8+, rich
"""

import argparse
import datetime as _dt
import os
import signal
import subprocess
import sys
import threading
import time
from collections import deque
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from statistics import mean
from typing import Deque, List, Tuple

# Rich
from rich.console import Console
from rich.progress import (
    BarColumn,
    MofNCompleteColumn,
    Progress,
    TextColumn,
    TimeElapsedColumn,
    TimeRemainingColumn,
)
from rich.text import Text

# ---------------------------------------------------------------------------
# Utility functions
# ---------------------------------------------------------------------------


def _make_log_dir() -> Path:
    """Return a freshly created log directory path under tests/logs/.

    The directory name is based on the current timestamp: YYYYmmdd_HHMMSS.
    """
    project_root = Path(__file__).resolve().parent.parent  # one level above tools/
    logs_base = project_root / "tests" / "logs"
    timestamp = _dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    log_dir = logs_base / timestamp
    log_dir.mkdir(parents=True, exist_ok=False)
    return log_dir


def _tail(path: Path, lines: int) -> List[str]:
    """Return the *lines* last lines of the file *path*.

    Reads the file in binary mode to efficiently grab the tail without loading
    the entire file into memory.
    """
    if lines <= 0:
        return []

    # Read in blocks from the end until we have enough newline characters.
    block_size = 1024
    data: Deque[bytes] = deque()
    with path.open("rb") as f:
        f.seek(0, os.SEEK_END)
        file_size = f.tell()
        remaining = file_size
        newline_count = 0
        while remaining > 0 and newline_count <= lines:
            read_size = min(block_size, remaining)
            remaining -= read_size
            f.seek(remaining)
            chunk = f.read(read_size)
            data.appendleft(chunk)
            newline_count += chunk.count(b"\n")
            if remaining == 0:
                break
    # Combine, split lines, take tail.
    text = b"".join(data).decode(errors="replace")
    return text.splitlines()[-lines:]


# ---------------------------------------------------------------------------
# Core worker
# ---------------------------------------------------------------------------


def _run_once(run_id: int, command: str, log_dir: Path) -> Tuple[int, float, Path]:
    """Execute *command* once, writing its combined output to a log file.

    Parameters
    ----------
    run_id : int
        1-based index of this run (used for log file naming and stats).
    command : str
        The exact shell command to execute.
    log_dir : Path
        Directory where log files should be written.

    Returns
    -------
    exit_code : int
        The process return code (0 => success).
    elapsed : float
        Wall-clock time in seconds spent executing the command.
    log_path : Path
        Path to the log file generated for this run.
    """
    log_path = log_dir / f"run_{run_id:03d}.log"
    start_ts = time.perf_counter()
    with log_path.open("w", buffering=1, encoding="utf-8", errors="replace") as log_file:
        # Run the command, redirecting both stdout and stderr to the log file.
        proc = subprocess.run(
            command,
            shell=True,
            stdout=log_file,
            stderr=subprocess.STDOUT,
            text=True,
        )
    elapsed = time.perf_counter() - start_ts
    return proc.returncode, elapsed, log_path


# ---------------------------------------------------------------------------
# Main routine
# ---------------------------------------------------------------------------


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Repeat a shell command multiple times, log each run, and report statistics.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("command", help="Full shell command to execute (quote if it contains spaces)")
    parser.add_argument("-n", "--runs", type=int, required=True, help="Number of times to run the command")
    parser.add_argument(
        "-p", "--parallel", type=int, default=1, help="Number of runs to execute in parallel"
    )
    parser.add_argument(
        "-t",
        "--tail",
        type=int,
        default=20,
        help="Number of lines from the end of failed logs to show in summary",
    )
    return parser.parse_args()


def main() -> None:
    args = _parse_args()

    if args.runs <= 0:
        print("--runs must be positive", file=sys.stderr)
        sys.exit(2)
    if args.parallel <= 0:
        print("--parallel must be positive", file=sys.stderr)
        sys.exit(2)

    log_dir = _make_log_dir()
    console = Console()

    # ------------------------------------------------------------------
    # Pretty header
    # ------------------------------------------------------------------
    console.print("Cleaning build artifacts …", style="italic")  # Placeholder line
    console.rule()

    console.print(Text("Multi-Test Runner", style="bold cyan"), justify="center")
    console.print()
    console.print(f"[bold]Command:[/bold] [orange1]{args.command}[/orange1]")
    console.print(f"[bold]Iterations:[/bold] {args.runs}")
    console.print(f"[bold]Logs directory:[/bold] [green]{log_dir.relative_to(Path.cwd())}[/green]")
    console.rule()

    completed_lock = threading.Lock()
    results: List[Tuple[int, float, Path, List[str]]] = []  # (exit_code, elapsed, log_path, failed_tests)
    all_failed_tests: List[str] = []

    start_overall = time.perf_counter()

    # Graceful Ctrl-C handling
    stop_event = threading.Event()

    def _handle_sigint(signum, frame):  # pylint: disable=unused-argument
        stop_event.set()
        console.print("\n[red]Interrupted by user; waiting for running jobs to finish…[/red]")

    original_handler = signal.signal(signal.SIGINT, _handle_sigint)

    progress = Progress(
        TextColumn("Progress:"),
        BarColumn(),
        MofNCompleteColumn(),
        TextColumn("({task.percentage:>3.0f}%)"),
        TextColumn("• OK={task.fields[ok]}"),
        TextColumn("EXIT={task.fields[exit]}"),
        TextColumn("FAILED={task.fields[failed_t]}"),
        TimeElapsedColumn(),
        TextColumn("avg={task.fields[avg]:.1f}s"),
        TimeRemainingColumn(),
        console=console,
    )

    task_id = progress.add_task(
        "overall", total=args.runs, ok=0, exit=0, failed_t=0, avg=0.0
    )

    try:
        with progress, ThreadPoolExecutor(max_workers=args.parallel) as pool:
            futures = {
                pool.submit(_run_once, run_id, args.command, log_dir): run_id
                for run_id in range(1, args.runs + 1)
            }

            for future in as_completed(futures):
                run_id = futures[future]
                if stop_event.is_set():
                    break

                try:
                    exit_code, elapsed, log_path = future.result()
                except Exception as exc:  # pylint: disable=broad-except
                    exit_code, elapsed, log_path = 1, 0.0, Path("<internal>")
                    console.print(f"[red]Run {run_id} raised exception: {exc}[/red]", file=sys.stderr)

                # ---- Scan log for "FAILED" unit-test lines ----
                failed_tests: List[str] = []
                try:
                    with log_path.open("r", encoding="utf-8", errors="replace") as lf:
                        for line in lf:
                            if "[  FAILED  ]" in line:
                                # Extract test name before ':' or EOL
                                parts = line.split("[  FAILED  ]", 1)[1].strip()
                                test_name = parts.split(":", 1)[0].strip()
                                failed_tests.append(test_name)
                except OSError as err:
                    console.print(f"[red]Could not scan log {log_path}: {err}[/red]")

                with completed_lock:
                    results.append((exit_code, elapsed, log_path, failed_tests))
                    all_failed_tests.extend(failed_tests)

                    successes = sum(1 for r, _, _, _ in results if r == 0)
                    exit_failures = len(results) - successes
                    failed_tests_total = len(all_failed_tests)
                    avg_time = mean(d for _, d, _, _ in results)

                    progress.update(
                        task_id,
                        advance=1,
                        ok=successes,
                        exit=exit_failures,
                        failed_t=failed_tests_total,
                        avg=avg_time,
                    )
    finally:
        signal.signal(signal.SIGINT, original_handler)

    total_elapsed = time.perf_counter() - start_overall
    successes = sum(1 for r, _, _, _ in results if r == 0)
    exit_failures = len(results) - successes
    durations = [d for _, d, _, _ in results]

    console.rule("Summary")
    console.print(f"Total runs   : {len(results)}")
    console.print(f"Succeeded    : {successes}")
    console.print(f"EXIT (non-0) : {exit_failures}")
    console.print(f"FAILED tests : {len(all_failed_tests)}")
    console.print(f"Success rate : {successes / len(results) * 100:.1f} %")

    console.print(f"Total elapsed (all) : {total_elapsed:.2f} s")
    console.print(f"Per-run time (avg)  : {mean(durations):.2f} s")
    console.print(f"Per-run time (min)  : {min(durations):.2f} s")
    console.print(f"Per-run time (max)  : {max(durations):.2f} s")

    if exit_failures:
        console.rule("Exited runs detail (non-zero exit)")
        for idx, (exit_code, _, log_path, _) in enumerate(results, 1):
            if exit_code == 0:
                continue
            console.print(f"[red]Run #{idx:03d} | exit code {exit_code} | log: {log_path.relative_to(Path.cwd())}[/red]")
            tail_lines = _tail(log_path, args.tail)
            if tail_lines:
                console.print("Last lines:")
                for line in tail_lines:
                    console.print("  " + line)
            else:
                console.print("<log file empty>")

    if all_failed_tests:
        console.rule("FAILED unit-tests (names only)")
        for name in all_failed_tests:
            console.print(f"[yellow]- {name}")

    # Overall exit status: 0 if all succeeded, 1 otherwise.
    sys.exit(0 if exit_failures == 0 and not all_failed_tests else 1)


if __name__ == "__main__":
    main() 