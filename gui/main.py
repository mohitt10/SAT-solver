"""Plain Tkinter interface for the propositional SAT solver."""

import queue
import threading
import tkinter as tk
from tkinter import ttk

from backend import Backend, BackendCancelled, BackendError


EXAMPLES = [
    "A | ~A",
    "A & ~A",
    "A -> B",
    "(A & ~B) -> C",
]


class SatGui:
    def __init__(self, root):
        self.root = root
        self.backend = Backend()
        self.results_queue = queue.Queue()
        self.busy = False
        self.analysis = None
        self.truth_page_size = 100
        self.truth_start = 0
        self.truth_cancel_event = None
        self.active_job_buttons = ()
        self.active_job_status = None
        self.puzzle_edges = []

        root.title("Propositional Logic & SAT Solver")
        root.geometry("980x720")
        root.configure(background="white")

        self.tabs = ttk.Notebook(root)
        self.tabs.pack(fill="both", expand=True, padx=8, pady=8)
        self.formula_tab = tk.Frame(self.tabs, bg="white")
        self.puzzle_tab = tk.Frame(self.tabs, bg="white")
        self.tabs.add(self.formula_tab, text="Formula")
        self.tabs.add(self.puzzle_tab, text="Puzzle: Graph Colouring")

        self._build_formula_tab()
        self._build_puzzle_tab()
        self.root.after(50, self._check_results)

    def _build_formula_tab(self):
        page = self.formula_tab
        tk.Label(
            page, text="Propositional formula", bg="white", fg="black"
        ).pack(anchor="w", padx=8, pady=(8, 2))

        self.formula = tk.Text(page, height=4, wrap="word", bg="white", fg="black")
        self.formula.pack(fill="x", padx=8)
        self.formula.tag_configure("error", background="#ffdede")
        self.error_position = tk.Label(page, text="", bg="white", fg="#b00020", anchor="w")
        self.error_position.pack(fill="x", padx=8)

        tk.Label(
            page,
            text="Symbols: AND  &     OR  |     NOT  ~     IMPLIES  ->     IFF  <->",
            bg="white",
            fg="black",
        ).pack(anchor="w", padx=8, pady=4)

        row = tk.Frame(page, bg="white")
        row.pack(fill="x", padx=8, pady=4)
        self.example = tk.StringVar(value="Choose an example")
        examples = ttk.Combobox(
            row, textvariable=self.example, values=EXAMPLES, state="readonly", width=30
        )
        examples.pack(side="left")
        examples.bind("<<ComboboxSelected>>", self._load_example)

        self.unit_prop = tk.BooleanVar(value=True)
        self.pure_literal = tk.BooleanVar(value=True)
        tk.Checkbutton(
            row, text="Unit propagation", variable=self.unit_prop, bg="white"
        ).pack(side="left", padx=(12, 0))
        tk.Checkbutton(
            row, text="Pure literal elimination", variable=self.pure_literal, bg="white"
        ).pack(side="left")

        self.analyze_button = tk.Button(row, text="Analyse", command=self.analyze)
        self.analyze_button.pack(side="right", padx=3)
        self.solve_button = tk.Button(row, text="Solve", command=self.solve)
        self.solve_button.pack(side="right", padx=3)
        self.clear_button = tk.Button(row, text="Clear", command=self.clear)
        self.clear_button.pack(side="right", padx=3)

        self.status = tk.Label(
            page, text="Enter a formula, then choose Analyse or Solve.",
            bg="white", fg="black", anchor="w", justify="left", wraplength=900
        )
        self.status.pack(fill="x", padx=8, pady=4)
        self.classification = tk.Label(
            page, text="Classification: not analysed", bg="white", fg="black", anchor="w"
        )
        self.classification.pack(fill="x", padx=8)

        self.result_tabs = ttk.Notebook(page)
        self.result_tabs.pack(fill="both", expand=True, padx=8, pady=8)
        self.ast_text = self._add_text_tab("AST")
        self._build_truth_table_tab()
        self.cnf_text = self._add_text_tab("CNF")
        self.solve_text = self._add_text_tab("Solve / DPLL trace")
        self._set_text(self.ast_text, "Analyse a formula to view its AST.")
        self._set_text(self.cnf_text, "Analyse a formula to view its CNF.")
        self._set_text(self.solve_text, "Solve a formula to view its result and metrics.")

    def _build_truth_table_tab(self):
        self.truth_frame = tk.Frame(self.result_tabs, bg="white")
        self.result_tabs.add(self.truth_frame, text="Truth table")

        controls = tk.Frame(self.truth_frame, bg="white")
        controls.pack(fill="x", padx=8, pady=8)
        self.truth_generate_button = tk.Button(
            controls, text="Generate", command=self.generate_truth_table
        )
        self.truth_generate_button.pack(side="left")
        self.truth_cancel_button = tk.Button(
            controls, text="Cancel", command=self.cancel_truth_table, state="disabled"
        )
        self.truth_cancel_button.pack(side="left", padx=4)
        self.truth_previous_button = tk.Button(
            controls, text="Previous", command=self.previous_truth_page, state="disabled"
        )
        self.truth_previous_button.pack(side="right", padx=3)
        self.truth_next_button = tk.Button(
            controls, text="Next", command=self.next_truth_page, state="disabled"
        )
        self.truth_next_button.pack(side="right", padx=3)

        self.truth_summary = tk.Label(
            self.truth_frame,
            text="Analyse a formula to see the number of variables and rows.",
            bg="white",
            fg="black",
            anchor="w",
        )
        self.truth_summary.pack(fill="x", padx=8)
        self.truth_status = tk.Label(
            self.truth_frame,
            text="Truth-table generation is explicit; pages contain at most 100 rows.",
            bg="white",
            fg="black",
            anchor="w",
        )
        self.truth_status.pack(fill="x", padx=8, pady=(2, 6))

        table_frame = tk.Frame(self.truth_frame, bg="white")
        table_frame.pack(fill="both", expand=True, padx=8, pady=(0, 8))
        self.truth_table = ttk.Treeview(table_frame, show="headings")
        vertical = ttk.Scrollbar(
            table_frame, orient="vertical", command=self.truth_table.yview
        )
        horizontal = ttk.Scrollbar(
            table_frame, orient="horizontal", command=self.truth_table.xview
        )
        self.truth_table.configure(
            yscrollcommand=vertical.set, xscrollcommand=horizontal.set
        )
        self.truth_table.grid(row=0, column=0, sticky="nsew")
        vertical.grid(row=0, column=1, sticky="ns")
        horizontal.grid(row=1, column=0, sticky="ew")
        table_frame.rowconfigure(0, weight=1)
        table_frame.columnconfigure(0, weight=1)

    def _add_text_tab(self, title):
        frame = tk.Frame(self.result_tabs, bg="white")
        self.result_tabs.add(frame, text=title)
        text = tk.Text(frame, wrap="none", bg="white", fg="black", state="disabled")
        vertical = ttk.Scrollbar(frame, orient="vertical", command=text.yview)
        horizontal = ttk.Scrollbar(frame, orient="horizontal", command=text.xview)
        text.configure(yscrollcommand=vertical.set, xscrollcommand=horizontal.set)
        text.grid(row=0, column=0, sticky="nsew")
        vertical.grid(row=0, column=1, sticky="ns")
        horizontal.grid(row=1, column=0, sticky="ew")
        frame.rowconfigure(0, weight=1)
        frame.columnconfigure(0, weight=1)
        return text

    @staticmethod
    def _set_text(widget, value):
        widget.configure(state="normal")
        widget.delete("1.0", "end")
        widget.insert("1.0", value)
        widget.configure(state="disabled")

    def _build_puzzle_tab(self):
        page = self.puzzle_tab
        tk.Label(
            page, text="Graph-colouring puzzle", bg="white", fg="black"
        ).pack(anchor="w", padx=8, pady=(8, 2))

        setup = tk.Frame(page, bg="white")
        setup.pack(fill="x", padx=8, pady=4)
        self.puzzle_example = tk.StringVar(value="Choose a built-in example")
        examples = ttk.Combobox(
            setup,
            textvariable=self.puzzle_example,
            values=("Triangle with 3 colours (SAT)", "Triangle with 2 colours (UNSAT)"),
            state="readonly",
            width=34,
        )
        examples.pack(side="left")
        examples.bind("<<ComboboxSelected>>", self._load_puzzle_example)

        tk.Label(setup, text="Nodes:", bg="white", fg="black").pack(
            side="left", padx=(12, 2)
        )
        self.node_count = tk.Spinbox(
            setup, from_=1, to=12, width=5, command=self._puzzle_dimensions_changed
        )
        self.node_count.delete(0, "end")
        self.node_count.insert(0, "3")
        self.node_count.pack(side="left")
        self.node_count.bind("<FocusOut>", self._puzzle_dimensions_changed)

        tk.Label(setup, text="Colours:", bg="white", fg="black").pack(
            side="left", padx=(8, 2)
        )
        self.colour_count = tk.Spinbox(setup, from_=1, to=12, width=5)
        self.colour_count.delete(0, "end")
        self.colour_count.insert(0, "3")
        self.colour_count.pack(side="left")
        self.colour_count.configure(command=self._puzzle_dimensions_changed)
        self.colour_count.bind("<FocusOut>", self._puzzle_dimensions_changed)

        edge_row = tk.Frame(page, bg="white")
        edge_row.pack(fill="x", padx=8, pady=4)
        tk.Label(edge_row, text="Add edge:", bg="white", fg="black").pack(
            side="left"
        )
        self.edge_first = tk.Spinbox(edge_row, from_=1, to=3, width=5)
        self.edge_first.pack(side="left", padx=(6, 2))
        tk.Label(edge_row, text="to", bg="white", fg="black").pack(side="left")
        self.edge_second = tk.Spinbox(edge_row, from_=1, to=3, width=5)
        self.edge_second.pack(side="left", padx=2)
        self.add_edge_button = tk.Button(
            edge_row, text="Add edge", command=self.add_edge
        )
        self.add_edge_button.pack(side="left", padx=4)
        self.remove_edge_button = tk.Button(
            edge_row, text="Remove selected", command=self.remove_selected_edge
        )
        self.remove_edge_button.pack(side="left", padx=4)
        self.solve_puzzle_button = tk.Button(
            edge_row, text="Solve puzzle", command=self.solve_puzzle
        )
        self.solve_puzzle_button.pack(side="right")

        edge_frame = tk.Frame(page, bg="white")
        edge_frame.pack(fill="x", padx=8, pady=4)
        tk.Label(edge_frame, text="Edges:", bg="white", fg="black").pack(
            side="left", anchor="n"
        )
        self.edge_list = tk.Listbox(
            edge_frame, height=4, bg="white", fg="black", exportselection=False
        )
        self.edge_list.pack(side="left", fill="x", expand=True, padx=8)
        edge_scroll = ttk.Scrollbar(
            edge_frame, orient="vertical", command=self.edge_list.yview
        )
        edge_scroll.pack(side="left", fill="y")
        self.edge_list.configure(yscrollcommand=edge_scroll.set)

        self.puzzle_status = tk.Label(
            page, text="Add graph edges or choose an example.",
            bg="white", fg="black", anchor="w"
        )
        self.puzzle_status.pack(fill="x", padx=8, pady=4)

        self.puzzle_results = ttk.Notebook(page)
        self.puzzle_results.pack(fill="both", expand=True, padx=8, pady=4)
        self.puzzle_solution = self._add_puzzle_text_tab("Solution")
        self.puzzle_encoding = self._add_puzzle_text_tab("Encoding")
        self.puzzle_trace = self._add_puzzle_text_tab("DPLL trace")

        metrics = tk.LabelFrame(
            page, text="Metrics and DPLL comparison", bg="white", fg="black"
        )
        metrics.pack(fill="x", padx=8, pady=(2, 8))
        self.puzzle_metrics = ttk.Treeview(
            metrics, columns=("metric", "value"), show="headings", height=6
        )
        self.puzzle_metrics.heading("metric", text="Metric")
        self.puzzle_metrics.heading("value", text="Value")
        self.puzzle_metrics.column("metric", width=250, stretch=True)
        self.puzzle_metrics.column("value", width=220, stretch=True)
        self.puzzle_metrics.pack(fill="x", padx=4, pady=4)
        self.puzzle_comparison = ttk.Treeview(
            metrics,
            columns=("unit", "pure", "result", "time", "decisions", "backtracks"),
            show="headings",
            height=4,
        )
        for column, title in (
            ("unit", "Unit prop"),
            ("pure", "Pure literal"),
            ("result", "Result"),
            ("time", "Time (ms)"),
            ("decisions", "Decisions"),
            ("backtracks", "Backtracks"),
        ):
            self.puzzle_comparison.heading(column, text=title)
            self.puzzle_comparison.column(column, width=100, stretch=True)
        self.puzzle_comparison.pack(fill="x", padx=4, pady=(0, 4))

    def _add_puzzle_text_tab(self, title):
        frame = tk.Frame(self.puzzle_results, bg="white")
        self.puzzle_results.add(frame, text=title)
        text = tk.Text(frame, wrap="none", bg="white", fg="black", state="disabled")
        vertical = ttk.Scrollbar(frame, orient="vertical", command=text.yview)
        horizontal = ttk.Scrollbar(frame, orient="horizontal", command=text.xview)
        text.configure(yscrollcommand=vertical.set, xscrollcommand=horizontal.set)
        text.grid(row=0, column=0, sticky="nsew")
        vertical.grid(row=0, column=1, sticky="ns")
        horizontal.grid(row=1, column=0, sticky="ew")
        frame.rowconfigure(0, weight=1)
        frame.columnconfigure(0, weight=1)
        return text

    def _update_edge_ranges(self, _event=None):
        try:
            nodes = int(self.node_count.get())
        except ValueError:
            return
        if nodes < 1 or nodes > 12:
            return
        for selector in (self.edge_first, self.edge_second):
            selector.configure(to=nodes)
            if int(selector.get()) > nodes:
                selector.delete(0, "end")
                selector.insert(0, str(nodes))

    def _puzzle_dimensions_changed(self, _event=None):
        self._update_edge_ranges()
        self._clear_puzzle_results()
        self.puzzle_status.configure(
            text="Graph dimensions changed. Solve again to refresh results.",
            fg="black",
        )

    def _load_puzzle_example(self, _event=None):
        self.node_count.delete(0, "end")
        self.node_count.insert(0, "3")
        self._update_edge_ranges()
        if self.puzzle_example.get() == "Triangle with 3 colours (SAT)":
            self.colour_count.delete(0, "end")
            self.colour_count.insert(0, "3")
        else:
            self.colour_count.delete(0, "end")
            self.colour_count.insert(0, "2")
        self.puzzle_edges = [(1, 2), (2, 3), (3, 1)]
        self._refresh_edge_list()
        self._clear_puzzle_results()
        self.puzzle_status.configure(
            text="Example loaded. Select Solve puzzle to run the solver.", fg="black"
        )

    def _refresh_edge_list(self):
        self.edge_list.delete(0, "end")
        for first, second in self.puzzle_edges:
            self.edge_list.insert("end", "{} - {}".format(first, second))

    def _clear_puzzle_results(self):
        self._set_text(self.puzzle_solution, "Solve a graph to see its colouring.")
        self._set_text(self.puzzle_encoding, "Solve a graph to see its SAT encoding.")
        self._set_text(self.puzzle_trace, "Solve a graph to see its DPLL trace.")
        self.puzzle_metrics.delete(*self.puzzle_metrics.get_children())
        self.puzzle_comparison.delete(*self.puzzle_comparison.get_children())

    def add_edge(self):
        try:
            nodes = int(self.node_count.get())
            first = int(self.edge_first.get())
            second = int(self.edge_second.get())
        except ValueError:
            self.puzzle_status.configure(
                text="Error: enter whole numbers for nodes and edge endpoints.",
                fg="#b00020",
            )
            return
        if nodes < 1 or nodes > 12:
            self.puzzle_status.configure(
                text="Error: node count must be between 1 and 12.", fg="#b00020"
            )
            return
        if not (1 <= first <= nodes and 1 <= second <= nodes):
            self.puzzle_status.configure(
                text="Error: edge endpoints must be between 1 and {}.".format(nodes),
                fg="#b00020",
            )
            return
        if first == second:
            self.puzzle_status.configure(
                text="Error: self-loops are not allowed.", fg="#b00020"
            )
            return
        edge = (min(first, second), max(first, second))
        if edge in self.puzzle_edges:
            self.puzzle_status.configure(
                text="Error: that edge already exists.", fg="#b00020"
            )
            return
        self.puzzle_edges.append(edge)
        self._refresh_edge_list()
        self._clear_puzzle_results()
        self.puzzle_status.configure(
            text="Edge {} - {} added.".format(*edge), fg="black"
        )

    def remove_selected_edge(self):
        selected = self.edge_list.curselection()
        if not selected:
            self.puzzle_status.configure(
                text="Error: select an edge to remove.", fg="#b00020"
            )
            return
        removed = self.puzzle_edges.pop(selected[0])
        self._refresh_edge_list()
        self._clear_puzzle_results()
        self.puzzle_status.configure(
            text="Edge {} - {} removed.".format(*removed), fg="black"
        )

    def solve_puzzle(self):
        try:
            nodes = int(self.node_count.get())
            colours = int(self.colour_count.get())
        except ValueError:
            self.puzzle_status.configure(
                text="Error: node and colour counts must be whole numbers.",
                fg="#b00020",
            )
            return
        if not (1 <= nodes <= 12):
            self.puzzle_status.configure(
                text="Error: node count must be between 1 and 12.", fg="#b00020"
            )
            return
        if not (1 <= colours <= 12):
            self.puzzle_status.configure(
                text="Error: colour count must be between 1 and 12.", fg="#b00020"
            )
            return
        for first, second in self.puzzle_edges:
            if not (1 <= first <= nodes and 1 <= second <= nodes):
                self.puzzle_status.configure(
                    text="Error: remove edges outside the current node range.",
                    fg="#b00020",
                )
                return

        edges = list(self.puzzle_edges)
        self._start_job(
            "Solving graph-colouring puzzle...",
            lambda: self.backend.puzzle(nodes, colours, edges),
            self._show_puzzle_result,
            buttons=(
                self.solve_puzzle_button,
                self.add_edge_button,
                self.remove_edge_button,
            ),
            status_widget=self.puzzle_status,
        )

    def _show_puzzle_result(self, response):
        if not response.get("ok"):
            self.puzzle_status.configure(
                text="Error: " + response.get("error", "puzzle solve failed."),
                fg="#b00020",
            )
            return
        satisfiable = response["satisfiable"]
        if satisfiable:
            self.puzzle_status.configure(
                text="SAT — Valid colouring: {}".format(
                    "YES" if response["valid_coloring"] else "NO"
                ),
                fg="#176b2c",
            )
            solution = [
                "SAT",
                "",
                "Valid colouring: {}".format(
                    "YES" if response["valid_coloring"] else "NO"
                ),
            ]
            solution.extend(
                "Node {} -> Colour {}".format(index, colour)
                for index, colour in enumerate(response["colouring"], 1)
            )
        else:
            self.puzzle_status.configure(
                text="UNSAT — No valid colouring with {} colours.".format(
                    response["colours"]
                ),
                fg="#b00020",
            )
            solution = [
                "UNSAT",
                "No valid colouring with {} colours.".format(response["colours"]),
            ]
        self._set_text(self.puzzle_solution, "\n".join(solution))

        encoding = [
            "Graph-colouring constraints:",
            "1. Each node has at least one colour.",
            "2. Each node has at most one colour.",
            "3. Adjacent nodes have different colours.",
            "",
            "Encoding uses the project's propositional parser and NNF-to-CNF "
            "distribution.",
            (
                "Truth table: too many variables for a truth table (N > 24)."
                if response["variable_count"] > 24
                else "No truth table is provided on the puzzle tab."
            ),
            "",
            "Nodes: {}".format(response["nodes"]),
            "Colours: {}".format(response["colours"]),
            "Edges: {}".format(response["edge_count"]),
            "Variables: {}".format(response["variable_count"]),
            "Clauses: {}".format(response["clause_count"]),
            "Literals: {}".format(response["literal_count"]),
            "Formula length: {}".format(response["formula_length"]),
        ]
        self._set_text(self.puzzle_encoding, "\n".join(encoding))

        metrics = [
            ("Nodes", response["nodes"]),
            ("Colours", response["colours"]),
            ("Variables", response["variable_count"]),
            ("Edges", response["edge_count"]),
            ("Clauses", response["clause_count"]),
            ("Literals", response["literal_count"]),
            ("Formula length", response["formula_length"]),
            ("Encoding time (ms)", "{:.3f}".format(response["encoding_time_ms"])),
            ("Parsing time (ms)", "{:.3f}".format(response["parsing_time_ms"])),
            ("CNF conversion time (ms)", "{:.3f}".format(response["cnf_time_ms"])),
        ]
        self.puzzle_metrics.delete(*self.puzzle_metrics.get_children())
        for name, value in metrics:
            self.puzzle_metrics.insert("", "end", values=(name, value))

        self.puzzle_comparison.delete(*self.puzzle_comparison.get_children())
        for result in response["comparisons"]:
            self.puzzle_comparison.insert(
                "",
                "end",
                values=(
                    "ON" if result["unit_propagation"] else "OFF",
                    "ON" if result["pure_literal"] else "OFF",
                    "SAT" if result["satisfiable"] else "UNSAT",
                    "{:.3f}".format(result["solver_time_ms"]),
                    result["decisions"],
                    result["backtracks"],
                ),
            )

        trace_lines = []
        for event in response["trace"]["events"]:
            trace_lines.append(self._event_text(event))
        if response["trace"]["trace_truncated"]:
            trace_lines.append("Trace truncated at the configured event limit.")
        self._set_text(
            self.puzzle_trace,
            "\n".join(trace_lines) if trace_lines else "No trace events were recorded.",
        )

    def _load_example(self, _event=None):
        self.formula.delete("1.0", "end")
        self.formula.insert("1.0", self.example.get())
        self._clear_error_highlight()

    def _formula_value(self):
        return self.formula.get("1.0", "end-1c")

    def _clear_error_highlight(self):
        self.formula.tag_remove("error", "1.0", "end")
        self.error_position.configure(text="")

    def _mark_parse_error(self, error):
        self._clear_error_highlight()
        source = self._formula_value()
        offset = max(0, min(int(error.get("offset", 0)), len(source.encode("utf-8"))))
        char_offset = len(source.encode("utf-8")[:offset].decode("utf-8", "ignore"))
        start = "1.0 + {} chars".format(char_offset)
        found = error.get("found", "")
        if found:
            end = "1.0 + {} chars".format(
                char_offset + len(found.encode("utf-8").decode("utf-8", "ignore"))
            )
            self.formula.tag_add("error", start, end)
        else:
            self.formula.mark_set("insert", start)
            self.formula.see(start)
            self.error_position.configure(
                text="Caret: ^ at line {}, column {} (end of input)".format(
                    error.get("line"), error.get("column")
                )
            )

    def _start_job(
        self,
        label,
        action,
        on_success,
        cancel_event=None,
        buttons=None,
        status_widget=None,
    ):
        if self.busy:
            return
        self.busy = True
        self.active_cancel_event = cancel_event
        self.active_job_buttons = (
            self.analyze_button,
            self.solve_button,
            self.clear_button,
        )
        if buttons:
            self.active_job_buttons += tuple(buttons)
        self.active_job_status = (
            status_widget if status_widget is not None else self.status
        )
        self.active_job_status.configure(text=label, fg="black")
        for button in self.active_job_buttons:
            button.configure(state="disabled")

        def work():
            try:
                self.results_queue_put((on_success, action(), None))
            except (BackendError, KeyError, TypeError, ValueError) as error:
                self.results_queue_put((on_success, None, error))

        threading.Thread(target=work, daemon=True).start()

    def results_queue_put(self, result):
        self.results_queue.put(result)

    def _check_results(self):
        try:
            while True:
                callback, response, error = self.results_queue.get_nowait()
                self.busy = False
                self.active_cancel_event = None
                for button in self.active_job_buttons:
                    button.configure(state="normal")
                self.active_job_buttons = ()
                status_widget = (
                    self.active_job_status
                    if self.active_job_status is not None
                    else self.status
                )
                self.active_job_status = None
                if error:
                    if isinstance(error, BackendCancelled):
                        self.truth_status.configure(text="Cancelled.")
                        status_widget.configure(
                            text="Truth-table generation cancelled.", fg="black"
                        )
                    else:
                        status_widget.configure(
                            text="Error: " + str(error), fg="#b00020"
                        )
                else:
                    callback(response)
        except queue.Empty:
            pass
        self.root.after(50, self._check_results)

    def analyze(self):
        formula = self._formula_value()
        if not formula.strip():
            self.status.configure(text="Error: enter a formula first.", fg="#b00020")
            return

        def action():
            analysis = self.backend.analyze(formula)
            if not analysis.get("ok"):
                return analysis, None
            cnf = self.backend.cnf(formula)
            if not cnf.get("ok"):
                raise BackendError(cnf.get("error", "CNF conversion failed."))
            return analysis, cnf

        self._start_job("Analysing formula...", action, self._show_analysis)

    def _show_analysis(self, value):
        analysis, cnf = value
        if not analysis.get("ok"):
            self.analysis = None
            error = analysis["parse_error"]
            self._mark_parse_error(error)
            found = error.get("found") or "end of input"
            expected = ", ".join(error.get("expected", [])) or "not specified"
            self.status.configure(
                text="Error: line {}, column {}: found {}; expected {}".format(
                    error.get("line"), error.get("column"), found, expected
                ),
                fg="#b00020",
            )
            self.classification.configure(text="Classification: unavailable", fg="black")
            self._set_text(self.ast_text, "Formula could not be parsed.")
            self._set_text(self.cnf_text, "Formula could not be parsed.")
            self.truth_summary.configure(text="A valid formula is required.")
            self.truth_generate_button.configure(state="disabled")
            self._clear_truth_table()
            return

        self.analysis = analysis
        self._clear_error_highlight()
        self.status.configure(text="Parse OK", fg="#176b2c")
        classification = analysis.get("classification")
        if classification:
            summary = "Classification: " + classification
        else:
            summary = "Classification: unavailable — " + analysis.get(
                "classification_message", "truth-table limit exceeded"
            )
        self.classification.configure(text=summary, fg="black")
        self._set_text(self.ast_text, analysis.get("ast", ""))

        variables = analysis.get("variable_count", 0)
        rows = analysis.get("truth_table_rows")
        if rows is None:
            self.truth_summary.configure(
                text="{} variables = 2^n rows. Too many variables for a truth table "
                "(N > 24).".format(variables)
            )
            self.truth_generate_button.configure(state="disabled")
            self._clear_truth_table()
            self.truth_status.configure(
                text="The backend supports full truth tables for at most 24 variables."
            )
        else:
            self.truth_summary.configure(
                text="{} variables = 2^n = {} rows.".format(variables, rows)
            )
            self.truth_generate_button.configure(state="normal")
            self._clear_truth_table()
            self.truth_status.configure(
                text="Select Generate to load up to {} rows at a time.".format(
                    self.truth_page_size
                )
            )

        lines = ["CNF by NNF distribution", ""]
        for clause in cnf.get("clauses", []):
            if not clause:
                lines.append("(empty clause)")
                continue
            lines.append(
                " OR ".join(
                    ("NOT " if item["negated"] else "") + item["variable"]
                    for item in clause
                )
            )
        lines.extend(
            [
                "",
                "Clauses: {}".format(cnf.get("clause_count", 0)),
                "Literals: {}".format(cnf.get("literal_count", 0)),
                "Note: distribution can grow exponentially.",
            ]
        )
        self._set_text(self.cnf_text, "\n".join(lines))

    def _clear_truth_table(self):
        self.truth_table.configure(columns=())
        self.truth_table.delete(*self.truth_table.get_children())
        self.truth_previous_button.configure(state="disabled")
        self.truth_next_button.configure(state="disabled")
        self.truth_start = 0

    def generate_truth_table(self):
        if not self.analysis or self.analysis.get("truth_table_rows") is None:
            return
        self._load_truth_page(0)

    def previous_truth_page(self):
        self._load_truth_page(max(0, self.truth_start - self.truth_page_size))

    def next_truth_page(self):
        if self.analysis:
            self._load_truth_page(self.truth_start + self.truth_page_size)

    def cancel_truth_table(self):
        if self.truth_cancel_event is not None:
            self.truth_cancel_event.set()
            self.truth_status.configure(text="Cancelling truth-table request...")
            self.truth_cancel_button.configure(state="disabled")

    def _load_truth_page(self, start):
        if self.busy or not self.analysis:
            return
        if start >= self.analysis["truth_table_rows"]:
            return
        cancel_event = threading.Event()
        self.truth_cancel_event = cancel_event
        formula = self._formula_value()
        self.truth_cancel_button.configure(state="normal")
        self.truth_previous_button.configure(state="disabled")
        self.truth_next_button.configure(state="disabled")
        self._start_job(
            "Generating truth-table page...",
            lambda: self.backend.truth_table(
                formula,
                start=start,
                count=self.truth_page_size,
                cancel_event=cancel_event,
            ),
            self._show_truth_page,
            cancel_event=cancel_event,
        )

    def _show_truth_page(self, response):
        self.truth_cancel_button.configure(state="disabled")
        if not response.get("ok"):
            self.truth_status.configure(
                text="Error: " + response.get("error", "truth-table request failed.")
            )
            return

        variables = response["variables"]
        columns = ["row"] + variables + ["formula"]
        self.truth_table.configure(columns=columns)
        for column in columns:
            if column == "row":
                heading = "Row"
            elif column == "formula":
                heading = "Formula"
            else:
                heading = column
            self.truth_table.heading(column, text=heading)
            self.truth_table.column(column, width=100, minwidth=60, stretch=True)
        self.truth_table.delete(*self.truth_table.get_children())

        start = response["start"]
        for index, row in enumerate(response["rows"]):
            values = [start + index + 1]
            values.extend("T" if value else "F" for value in row["values"])
            values.append("T" if row["result"] else "F")
            self.truth_table.insert("", "end", values=values)

        self.truth_start = start
        end = start + len(response["rows"])
        self.truth_status.configure(
            text="Rows {}-{} of {}.".format(
                start + 1, end, response["total_rows"]
            )
        )
        self.truth_previous_button.configure(
            state="normal" if start > 0 else "disabled"
        )
        self.truth_next_button.configure(
            state="normal" if end < response["total_rows"] else "disabled"
        )

    def solve(self):
        formula = self._formula_value()
        if not formula.strip():
            self.status.configure(text="Error: enter a formula first.", fg="#b00020")
            return
        unit = self.unit_prop.get()
        pure = self.pure_literal.get()
        self._start_job(
            "Solving formula...",
            lambda: self.backend.solve(formula, unit, pure),
            self._show_solve,
        )

    def _show_solve(self, response):
        if not response.get("ok"):
            error = response.get("parse_error")
            if error:
                self._mark_parse_error(error)
                self.status.configure(
                    text="Error: line {}, column {}: {}".format(
                        error.get("line"), error.get("column"), error.get("message")
                    ),
                    fg="#b00020",
                )
            else:
                self.status.configure(
                    text="Error: " + response.get("error", "solve failed"),
                    fg="#b00020",
                )
            return
        self._clear_error_highlight()
        sat = response["satisfiable"]
        status = "SAT" if sat else "UNSAT"
        self.status.configure(text="Parse OK — " + status, fg="#176b2c" if sat else "#b00020")
        lines = [status]
        if sat:
            lines.append("Satisfying assignment:")
            if response["assignment"]:
                lines.extend(
                    "{} = {}".format(name, str(value).upper())
                    for name, value in response["assignment"].items()
                )
            else:
                lines.append("(No variables in the formula.)")
        else:
            lines.append("No satisfying assignment exists.")
        lines.extend(
            [
                "",
                "Variables: {}".format(response["variable_count"]),
                "Clauses: {}".format(response["clause_count"]),
                "Literals: {}".format(response["literal_count"]),
                "Solver time: {:.3f} ms".format(response["solver_time_ms"]),
                "Decisions: {}".format(response["decisions"]),
                "Backtracks: {}".format(response["backtracks"]),
                "",
                "DPLL events:",
            ]
        )
        trace = response.get("trace", {})
        for event in trace.get("events", []):
            lines.append(self._event_text(event))
        if trace.get("trace_truncated"):
            lines.append("Trace truncated at the configured event limit.")
        self._set_text(self.solve_text, "\n".join(lines))
        self.result_tabs.select(self.solve_text.master)

    @staticmethod
    def _event_text(event):
        kind = event["type"]
        level = event["level"]
        indent = "  " * max(level - 1, 0)
        value = str(event["value"]).upper()
        if kind == "decision":
            message = "Decide {} = {}".format(event["variable"], value)
        elif kind == "unit_propagation":
            message = "Unit propagation: {} = {}".format(event["variable"], value)
        elif kind == "pure_literal":
            message = "Pure literal: {} = {}".format(event["variable"], value)
        elif kind == "conflict":
            message = "CONFLICT"
        elif kind == "backtrack":
            message = "BACKTRACK to level {}".format(level)
        else:
            message = "Result: {}".format("SAT" if event["value"] else "UNSAT")
        return indent + message

    def clear(self):
        self.formula.delete("1.0", "end")
        self._clear_error_highlight()
        self.status.configure(
            text="Enter a formula, then choose Analyse or Solve.", fg="black"
        )
        self.classification.configure(text="Classification: not analysed", fg="black")
        self._set_text(self.ast_text, "Analyse a formula to view its AST.")
        self.analysis = None
        self.truth_summary.configure(
            text="Analyse a formula to see the number of variables and rows."
        )
        self.truth_status.configure(
            text="Truth-table generation is explicit; pages contain at most 100 rows."
        )
        self.truth_generate_button.configure(state="normal")
        self.truth_cancel_button.configure(state="disabled")
        self._clear_truth_table()
        self._set_text(self.cnf_text, "Analyse a formula to view its CNF.")
        self._set_text(self.solve_text, "Solve a formula to view its result and metrics.")


def main():
    root = tk.Tk()
    SatGui(root)
    root.mainloop()


if __name__ == "__main__":
    main()
