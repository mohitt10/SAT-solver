"""Small subprocess wrapper for the C++ GUI backend."""

import json
import os
import subprocess
import time
from pathlib import Path


# Change this path if the backend executable is stored elsewhere.
BACKEND_EXECUTABLE = Path(__file__).resolve().parents[1] / (
    "sat_solver_backend.exe" if os.name == "nt" else "sat_solver_backend"
)


class BackendError(RuntimeError):
    """Raised when the backend process cannot complete a request."""


class BackendCancelled(BackendError):
    """Raised when a caller cancels an in-flight backend request."""


class Backend:
    def __init__(self, executable=BACKEND_EXECUTABLE, timeout=30):
        self.executable = str(executable)
        self.timeout = timeout

    def request(
        self, operation, formula="", options=None, puzzle_input="", cancel_event=None
    ):
        command = [self.executable, "--gui", operation]
        for name, value in (options or {}).items():
            command.extend(["--" + name.replace("_", "-"), str(value)])

        try:
            if cancel_event is None:
                completed = subprocess.run(
                    command,
                    input=puzzle_input if operation == "puzzle" else formula,
                    capture_output=True,
                    text=True,
                    encoding="utf-8",
                    timeout=self.timeout,
                    check=False,
                )
            else:
                completed = self._run_cancellable(
                    command,
                    puzzle_input if operation == "puzzle" else formula,
                    cancel_event,
                )
        except BackendCancelled:
            raise
        except subprocess.TimeoutExpired as error:
            raise BackendError(
                "The SAT backend took too long and was stopped."
            ) from error
        except OSError as error:
            raise BackendError(
                "Could not start the SAT backend at "
                + self.executable
                + ": "
                + str(error)
            ) from error

        if completed.returncode != 0:
            detail = completed.stderr.strip() or "No diagnostic was provided."
            raise BackendError(
                "The SAT backend exited with code "
                + str(completed.returncode)
                + ": "
                + detail
            )
        try:
            response = json.loads(completed.stdout)
        except json.JSONDecodeError as error:
            raise BackendError(
                "The SAT backend returned invalid JSON: " + str(error)
            ) from error
        if not isinstance(response, dict):
            raise BackendError("The SAT backend response was not a JSON object.")
        return response

    def _run_cancellable(self, command, request_text, cancel_event):
        process = subprocess.Popen(
            command,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
        )
        try:
            first_call = True
            deadline = time.monotonic() + self.timeout
            while True:
                if cancel_event.is_set():
                    process.terminate()
                    try:
                        process.communicate(timeout=2)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.communicate()
                    raise BackendCancelled("Truth-table generation cancelled.")
                if time.monotonic() >= deadline:
                    process.kill()
                    process.communicate()
                    raise BackendError("The SAT backend took too long and was stopped.")
                try:
                    stdout, stderr = process.communicate(
                        input=request_text if first_call else None,
                        timeout=min(0.1, max(0.01, deadline - time.monotonic())),
                    )
                    return subprocess.CompletedProcess(
                        command, process.returncode, stdout, stderr
                    )
                except subprocess.TimeoutExpired:
                    first_call = False
        finally:
            if process.poll() is None:
                process.kill()
                process.communicate()

    def analyze(self, formula):
        return self.request("analyze", formula)

    def cnf(self, formula):
        return self.request("cnf", formula)

    def solve(self, formula, unit_prop=True, pure_literal=True, trace=True):
        options = {
            "unit_prop": "on" if unit_prop else "off",
            "pure_literal": "on" if pure_literal else "off",
        }
        if trace:
            options["trace_limit"] = 10000
            return self.request("trace", formula, options)
        return self.request("solve", formula, options)

    def truth_table(self, formula, start=0, count=100, cancel_event=None):
        return self.request(
            "truth-table",
            formula,
            {"start": start, "count": count},
            cancel_event=cancel_event,
        )

    def puzzle(self, nodes, colours, edges, trace_unit=True, trace_pure=True):
        edge_text = "".join("{} {}\n".format(first, second) for first, second in edges)
        options = {
            "nodes": nodes,
            "colours": colours,
            "trace_unit": "on" if trace_unit else "off",
            "trace_pure": "on" if trace_pure else "off",
            "trace_limit": 10000,
        }
        return self.request("puzzle", options=options, puzzle_input=edge_text)
