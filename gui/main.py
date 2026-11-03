"""Plain Tkinter interface for the propositional SAT solver."""

import queue
import math
import random
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

PALETTE = ("#2563eb", "#e11d48", "#059669", "#d97706", "#7c3aed", "#0891b2")
def generate_sudoku(clue_count=40):
    """Create a valid Sudoku puzzle by shuffling a solved grid and hiding cells."""
    if not 17 <= clue_count <= 81:
        raise ValueError("Clue count must be between 17 and 81.")

    rng = random.Random()

    def pattern(row, column):
        return (row * 3 + row // 3 + column) % 9

    row_order = [
        band * 3 + row
        for band in rng.sample(range(3), 3)
        for row in rng.sample(range(3), 3)
    ]
    column_order = [
        stack * 3 + column
        for stack in rng.sample(range(3), 3)
        for column in rng.sample(range(3), 3)
    ]
    digit_order = rng.sample(range(1, 10), 9)
    grid = [
        [digit_order[pattern(row, column)] for column in column_order]
        for row in row_order
    ]

    for cell in rng.sample(range(81), 81 - clue_count):
        grid[cell // 9][cell % 9] = 0
    return grid


class GraphCanvas(tk.Canvas):
    def __init__(self, master, on_node_click):
        super().__init__(master, background="#f8fafc", highlightthickness=0)
        self.on_node_click = on_node_click
        self.nodes = 6
        self.edges = []
        self.colouring = []
        self.selected_node = None
        self.bind("<Configure>", self._draw)
        self.bind("<Button-1>", self._click)

    def update_graph(self, nodes, edges, colouring, selected_node):
        self.nodes = nodes
        self.edges = list(edges)
        self.colouring = list(colouring)
        self.selected_node = selected_node
        self._draw()

    def _positions(self):
        width = max(self.winfo_width(), 180)
        height = max(self.winfo_height(), 180)
        center_x, center_y = width / 2, height / 2
        if self.nodes <= 12:
            self.node_radius = 20
            radius = max(self.node_radius + 12, min(width, height) * 0.37)
            return [
                (
                    center_x + radius * math.cos(
                        2 * math.pi * index / self.nodes - math.pi / 2
                    ),
                    center_y + radius * math.sin(
                        2 * math.pi * index / self.nodes - math.pi / 2
                    ),
                )
                for index in range(self.nodes)
            ]

        self.node_radius = 7 if self.nodes > 40 else 12
        outer_radius = max(
            self.node_radius + 4,
            min(width, height) * 0.43 - self.node_radius,
        )
        ring_spacing = self.node_radius * 2 + (2 if self.nodes > 40 else 6)
        ring_radii = []
        radius = outer_radius
        while radius >= self.node_radius:
            ring_radii.append(radius)
            radius -= ring_spacing
        if not ring_radii:
            ring_radii.append(0)

        positions = []
        remaining = self.nodes
        for ring_index, radius in enumerate(ring_radii):
            capacity = (
                1
                if radius < self.node_radius * 1.5
                else max(
                    3,
                    int(
                        2 * math.pi * radius
                        / (self.node_radius * 2 + (2 if self.nodes > 40 else 5))
                    ),
                )
            )
            count = min(remaining, capacity)
            for index in range(count):
                angle = (
                    2 * math.pi * index / count
                    - math.pi / 2
                    + (ring_index % 2) * math.pi / max(count, 1)
                )
                positions.append(
                    (
                        center_x + radius * math.cos(angle),
                        center_y + radius * math.sin(angle),
                    )
                )
            remaining -= count
            if remaining == 0:
                break

        return positions

    def _draw(self, _event=None):
        self.delete("all")
        positions = self._positions()
        for first, second in self.edges:
            x1, y1 = positions[first - 1]
            x2, y2 = positions[second - 1]
            self.create_line(x1, y1, x2, y2, fill="#94a3b8", width=2)

        for index, (x, y) in enumerate(positions, 1):
            assigned_colour = (
                self.colouring[index - 1]
                if index <= len(self.colouring)
                else 0
            )
            fill = (
                PALETTE[(assigned_colour - 1) % len(PALETTE)]
                if assigned_colour > 0
                else "#ffffff"
            )
            outline = "#f59e0b" if index == self.selected_node else "#334155"
            self.create_oval(
                x - self.node_radius,
                y - self.node_radius,
                x + self.node_radius,
                y + self.node_radius,
                fill=fill,
                outline=outline,
                width=4 if index == self.selected_node else 2,
                tags=("node", str(index)),
            )
            self.create_text(
                x,
                y,
                text=str(index),
                fill="#ffffff" if assigned_colour else "#0f172a",
                font=(
                    "TkDefaultFont",
                    10 if self.nodes <= 12 else 7 if self.nodes > 40 else 8,
                    "bold",
                ),
            )

    def _click(self, event):
        for index, (x, y) in enumerate(self._positions(), 1):
            if (event.x - x) ** 2 + (event.y - y) ** 2 <= (self.node_radius + 4) ** 2:
                self.on_node_click(index)
                return


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
        self.active_job_inputs = ()
        self.active_job_status = None
        self.puzzle_edges = []
        self.selected_node = None
        self.puzzle_colouring = []

        root.title("Propositional Logic & SAT Solver")
        root.geometry("1180x820")
        root.minsize(1000, 620)
        root.configure(background="#f1f5f9")
        self._configure_theme()

        header = ttk.Frame(root, style="Header.TFrame", padding=(24, 18))
        header.pack(fill="x")
        ttk.Label(header, text="SAT Studio", style="Title.TLabel").pack(anchor="w")
        ttk.Label(
            header,
            text="Explore formulas, graph colouring, and Sudoku with SAT",
            style="Subtitle.TLabel",
        ).pack(anchor="w", pady=(3, 0))

        self.tabs = ttk.Notebook(root, style="Studio.TNotebook")
        self.tabs.pack(fill="both", expand=True, padx=20, pady=(0, 20))
        self.formula_tab = tk.Frame(self.tabs, bg="white")
        self.puzzle_tab = tk.Frame(self.tabs, bg="white")
        self.sudoku_tab = tk.Frame(self.tabs, bg="white")
        self.solver_results_tab = tk.Frame(self.tabs, bg="white")
        self.tabs.add(self.formula_tab, text="Formula")
        self.tabs.add(self.puzzle_tab, text="Graph colouring")
        self.tabs.add(self.sudoku_tab, text="Sudoku")
        self.tabs.add(self.solver_results_tab, text="Solver results")

        self._build_formula_tab()
        self._build_puzzle_tab()
        self._build_sudoku_tab()
        self._build_solver_results_tab()
        self.root.after(50, self._check_results)

    @staticmethod
    def _configure_theme():
        style = ttk.Style()
        if "clam" in style.theme_names():
            style.theme_use("clam")
        style.configure(".", font=("TkDefaultFont", 10))
        style.configure("TFrame", background="#f1f5f9")
        style.configure("Card.TFrame", background="#ffffff")
        style.configure("Header.TFrame", background="#0f172a")
        style.configure("Title.TLabel", background="#0f172a", foreground="#f8fafc",
                        font=("TkDefaultFont", 20, "bold"))
        style.configure("Subtitle.TLabel", background="#0f172a", foreground="#cbd5e1")
        style.configure("Studio.TNotebook", background="#f1f5f9", borderwidth=0)
        style.configure(
            "Studio.TNotebook.Tab",
            padding=(18, 11),
            background="#e2e8f0",
            foreground="#334155",
            font=("TkDefaultFont", 10, "bold"),
        )
        style.map(
            "Studio.TNotebook.Tab",
            background=[("selected", "#ffffff")],
            foreground=[("selected", "#1d4ed8")],
        )
        style.configure("TButton", padding=(12, 7), background="#e2e8f0",
                        foreground="#0f172a")
        style.map("TButton", background=[("active", "#cbd5e1")])
        style.configure("Primary.TButton", padding=(14, 8), background="#2563eb",
                        foreground="#ffffff", font=("TkDefaultFont", 10, "bold"))
        style.map("Primary.TButton", background=[("active", "#1d4ed8")])
        style.configure("TLabel", background="#ffffff", foreground="#0f172a")
        style.configure("Section.TLabel", background="#ffffff", foreground="#0f172a",
                        font=("TkDefaultFont", 13, "bold"))
        style.configure("Muted.TLabel", background="#ffffff", foreground="#64748b")

    def _build_formula_tab(self):
        page = self.formula_tab
        page.configure(bg="#f1f5f9")
        editor = ttk.Frame(page, padding=16, style="Card.TFrame")
        editor.pack(fill="x", padx=12, pady=12)
        ttk.Label(editor, text="Formula workbench", style="Section.TLabel").pack(
            anchor="w"
        )
        ttk.Label(
            editor,
            text="Analyse its structure or solve it with the SAT backend.",
            style="Muted.TLabel",
        ).pack(anchor="w", pady=(3, 10))

        self.formula = tk.Text(
            editor,
            height=4,
            wrap="word",
            bg="#f8fafc",
            fg="#0f172a",
            insertbackground="#2563eb",
            font=("TkFixedFont", 11),
            relief="flat",
            highlightthickness=1,
            highlightbackground="#cbd5e1",
            highlightcolor="#2563eb",
            padx=10,
            pady=8,
        )
        self.formula.pack(fill="x")
        self.formula.tag_configure("error", background="#ffdede")
        self.error_position = tk.Label(
            editor, text="", bg="white", fg="#b91c1c", anchor="w"
        )
        self.error_position.pack(fill="x", pady=(2, 0))

        ttk.Label(
            editor,
            text="Operators:  NOT ~    AND &    OR |    IMPLIES ->    IFF <->",
            style="Muted.TLabel",
        ).pack(anchor="w", pady=(6, 8))

        row = ttk.Frame(editor, style="Card.TFrame")
        row.pack(fill="x")
        self.example = tk.StringVar(value="Choose an example")
        examples = ttk.Combobox(
            row, textvariable=self.example, values=EXAMPLES, state="readonly", width=30
        )
        examples.pack(side="left")
        examples.bind("<<ComboboxSelected>>", self._load_example)

        self.unit_prop = tk.BooleanVar(value=True)
        self.pure_literal = tk.BooleanVar(value=True)
        ttk.Checkbutton(row, text="Unit propagation", variable=self.unit_prop).pack(
            side="left", padx=(12, 0)
        )
        ttk.Checkbutton(
            row, text="Pure literal elimination", variable=self.pure_literal
        ).pack(side="left")

        self.analyze_button = ttk.Button(row, text="Analyse", command=self.analyze)
        self.analyze_button.pack(side="right", padx=3)
        self.solve_button = ttk.Button(
            row, text="Solve", style="Primary.TButton", command=self.solve
        )
        self.solve_button.pack(side="right", padx=3)
        self.clear_button = ttk.Button(row, text="Clear", command=self.clear)
        self.clear_button.pack(side="right", padx=3)

        self.status = tk.Label(
            page, text="Enter a formula, then choose Analyse or Solve.",
            bg="#f1f5f9", fg="#334155", anchor="w", justify="left", wraplength=1100
        )
        self.status.pack(fill="x", padx=16, pady=(0, 5))
        self.classification = tk.Label(
            page,
            text="Classification: not analysed",
            bg="#f1f5f9",
            fg="#334155",
            anchor="w",
        )
        self.classification.pack(fill="x", padx=16)

        self.result_tabs = ttk.Notebook(page, style="Studio.TNotebook")
        self.result_tabs.pack(fill="both", expand=True, padx=12, pady=12)
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
        self.truth_generate_button = ttk.Button(
            controls, text="Generate", command=self.generate_truth_table
        )
        self.truth_generate_button.pack(side="left")
        self.truth_cancel_button = ttk.Button(
            controls, text="Cancel", command=self.cancel_truth_table, state="disabled"
        )
        self.truth_cancel_button.pack(side="left", padx=4)
        self.truth_previous_button = ttk.Button(
            controls, text="Previous", command=self.previous_truth_page, state="disabled"
        )
        self.truth_previous_button.pack(side="right", padx=3)
        self.truth_next_button = ttk.Button(
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
        page.configure(bg="#f1f5f9")
        heading = ttk.Frame(page, padding=(18, 14), style="Card.TFrame")
        heading.pack(fill="x", padx=12, pady=(12, 8))
        ttk.Label(heading, text="Graph colouring", style="Section.TLabel").pack(anchor="w")
        ttk.Label(
            heading,
            text=(
                "Click two nodes to connect them. The solver colours the graph "
                "so adjacent nodes differ. Graph and Sudoku share Solver results."
            ),
            style="Muted.TLabel",
        ).pack(anchor="w", pady=(3, 0))

        setup = ttk.Frame(page, padding=(14, 10), style="Card.TFrame")
        setup.pack(fill="x", padx=12, pady=(0, 8))
        ttk.Label(setup, text="Nodes").pack(side="left", padx=(0, 5))
        self.node_count = tk.Spinbox(
            setup, from_=3, to=100, width=4, command=self._puzzle_dimensions_changed
        )
        self.node_count.delete(0, "end")
        self.node_count.insert(0, "8")
        self.node_count.pack(side="left", padx=(0, 14))
        self.node_count.bind("<FocusOut>", self._puzzle_dimensions_changed)

        ttk.Label(setup, text="Colours").pack(side="left", padx=(0, 5))
        self.colour_count = tk.Spinbox(setup, from_=1, to=12, width=5)
        self.colour_count.delete(0, "end")
        self.colour_count.insert(0, "3")
        self.colour_count.pack(side="left", padx=(0, 14))
        self.colour_count.configure(command=self._puzzle_dimensions_changed)
        self.colour_count.bind("<FocusOut>", self._puzzle_dimensions_changed)

        self.trace_unit = tk.BooleanVar(value=True)
        self.trace_pure = tk.BooleanVar(value=True)
        self.trace_unit_button = ttk.Checkbutton(
            setup,
            text="Trace: unit propagation",
            variable=self.trace_unit,
        )
        self.trace_unit_button.pack(side="left", padx=(0, 8))
        self.trace_pure_button = ttk.Checkbutton(
            setup,
            text="Trace: pure literal",
            variable=self.trace_pure,
        )
        self.trace_pure_button.pack(side="left", padx=(0, 8))

        self.solve_puzzle_button = ttk.Button(
            setup, text="Solve graph", style="Primary.TButton", command=self.solve_puzzle
        )
        self.solve_puzzle_button.pack(side="right")
        self.clear_graph_button = ttk.Button(
            setup, text="Clear graph", command=self.clear_graph
        )
        self.clear_graph_button.pack(side="right", padx=8)

        graph_row = ttk.Frame(page)
        graph_card = ttk.Frame(graph_row, padding=8, style="Card.TFrame")
        graph_card.pack(side="left", fill="both", expand=True)
        self.graph_canvas = GraphCanvas(graph_card, self._graph_node_clicked)
        self.graph_canvas.pack(fill="both", expand=True)

        edge_card = ttk.Frame(graph_row, padding=14, style="Card.TFrame")
        edge_card.pack(side="left", fill="y", padx=(8, 0))
        ttk.Label(edge_card, text="Connections", style="Section.TLabel").pack(anchor="w")
        ttk.Label(
            edge_card,
            text="Select an edge to remove it.",
            style="Muted.TLabel",
        ).pack(anchor="w", pady=(3, 8))
        self.edge_list = tk.Listbox(
            edge_card,
            height=12,
            width=24,
            bg="#f8fafc",
            fg="#0f172a",
            selectbackground="#dbeafe",
            selectforeground="#1d4ed8",
            relief="flat",
            highlightthickness=1,
            highlightbackground="#e2e8f0",
            exportselection=False,
        )
        self.edge_list.pack(fill="both", expand=True)
        edge_scroll = ttk.Scrollbar(
            edge_card, orient="vertical", command=self.edge_list.yview
        )
        edge_scroll.place(relx=1.0, rely=0, relheight=1.0, anchor="ne")
        self.edge_list.configure(yscrollcommand=edge_scroll.set)
        self.remove_edge_button = ttk.Button(
            edge_card, text="Remove selected", command=self.remove_selected_edge
        )
        self.remove_edge_button.pack(fill="x", pady=(10, 0))

        self.puzzle_status = tk.Label(
            page,
            text=(
                "Click a node, then another node to add an edge. "
                "Graph and Sudoku share Solver results."
            ),
            bg="#f1f5f9",
            fg="#334155",
            anchor="w",
        )
        self.puzzle_status.pack(fill="x", padx=16, pady=(0, 8))
        graph_row.pack(fill="both", expand=True, padx=12, pady=(0, 12))

    def _build_solver_results_tab(self):
        page = self.solver_results_tab
        page.configure(bg="#f1f5f9")
        self.puzzle_results = ttk.Notebook(page, style="Studio.TNotebook")
        self.puzzle_results.pack(fill="both", expand=True, padx=12, pady=12)
        self.puzzle_solution = self._add_puzzle_text_tab("Solution")
        self.puzzle_encoding = self._add_puzzle_text_tab("Encoding")
        self.puzzle_trace = self._add_puzzle_text_tab("DPLL trace")
        metrics_page = tk.Frame(self.puzzle_results, bg="white")
        self.puzzle_results.add(metrics_page, text="Performance & metrics")
        metrics_panel = ttk.Frame(metrics_page, padding=(8, 6))
        metrics_panel.pack(side="left", fill="both", expand=True)
        ttk.Label(metrics_panel, text="Encoding metrics", style="Section.TLabel").pack(
            anchor="w", pady=(0, 4)
        )
        self.puzzle_metrics = ttk.Treeview(
            metrics_panel, columns=("metric", "value"), show="headings", height=5
        )
        self.puzzle_metrics.heading("metric", text="Metric")
        self.puzzle_metrics.heading("value", text="Value")
        self.puzzle_metrics.column("metric", width=210, stretch=True)
        self.puzzle_metrics.column("value", width=180, stretch=True)
        self.puzzle_metrics.pack(fill="both", expand=True)
        comparison_panel = ttk.Frame(metrics_page, padding=(8, 6))
        comparison_panel.pack(side="left", fill="both", expand=True)
        ttk.Label(
            comparison_panel, text="Solver performance",
            style="Section.TLabel",
        ).pack(anchor="w", pady=(0, 4))
        self.puzzle_comparison = ttk.Treeview(
            comparison_panel,
            columns=("unit", "pure", "result", "time", "decisions", "backtracks"),
            show="headings",
            height=5,
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
            self.puzzle_comparison.column(column, width=82, stretch=True)
        self.puzzle_comparison.pack(fill="both", expand=True)
        self.puzzle_results.select(metrics_page)
        self._clear_puzzle_results()

    def _build_sudoku_tab(self):
        page = self.sudoku_tab
        page.configure(bg="#f1f5f9")
        heading = ttk.Frame(page, padding=(18, 14), style="Card.TFrame")
        heading.pack(fill="x", padx=12, pady=(12, 8))
        ttk.Label(heading, text="Sudoku solver", style="Section.TLabel").pack(anchor="w")
        ttk.Label(
            heading,
            text=(
                "Enter clues or generate a puzzle. Unit propagation and pure "
                "literal are on by default; disabling them may slow solving."
            ),
            style="Muted.TLabel",
        ).pack(anchor="w", pady=(3, 0))

        actions = ttk.Frame(page, padding=(14, 10), style="Card.TFrame")
        actions.pack(fill="x", padx=12, pady=(0, 8))
        ttk.Label(actions, text="Generated puzzle clues").pack(
            side="left", padx=(0, 6)
        )
        self.sudoku_clue_count = tk.Spinbox(actions, from_=17, to=81, width=4)
        self.sudoku_clue_count.delete(0, "end")
        self.sudoku_clue_count.insert(0, "40")
        self.sudoku_clue_count.pack(side="left", padx=(0, 6))
        self.sudoku_generate_button = ttk.Button(
            actions, text="Generate puzzle", command=self.generate_sudoku_puzzle
        )
        self.sudoku_generate_button.pack(side="left")
        self.sudoku_trace_unit_button = ttk.Checkbutton(
            actions,
            text="Unit propagation",
            variable=self.trace_unit,
        )
        self.sudoku_trace_unit_button.pack(side="left", padx=(14, 8))
        self.sudoku_trace_pure_button = ttk.Checkbutton(
            actions,
            text="Pure literal",
            variable=self.trace_pure,
        )
        self.sudoku_trace_pure_button.pack(side="left", padx=(0, 8))
        self.sudoku_solve_button = ttk.Button(
            actions,
            text="Solve puzzle",
            style="Primary.TButton",
            command=self.solve_sudoku,
        )
        self.sudoku_solve_button.pack(side="right")
        self.sudoku_clear_button = ttk.Button(
            actions, text="Clear grid", command=self.clear_sudoku
        )
        self.sudoku_clear_button.pack(side="right", padx=8)

        content = ttk.Frame(page)
        content.pack(fill="both", expand=True, padx=12, pady=(0, 12))
        self.sudoku_board_card = ttk.Frame(content, padding=14, style="Card.TFrame")
        self.sudoku_board_card.pack(side="left", fill="both", expand=True)
        self.sudoku_board = tk.Frame(self.sudoku_board_card, bg="#94a3b8")
        self.sudoku_board_card.bind("<Configure>", self._resize_sudoku_board)
        self.sudoku_cells = [[None for _ in range(9)] for _ in range(9)]
        validate_digit = (self.root.register(self._valid_sudoku_entry), "%P")
        for grid_index in range(11):
            if grid_index in (3, 7):
                self.sudoku_board.rowconfigure(grid_index, minsize=5)
                self.sudoku_board.columnconfigure(grid_index, minsize=5)
            else:
                self.sudoku_board.rowconfigure(grid_index, weight=1, uniform="sudoku")
                self.sudoku_board.columnconfigure(
                    grid_index, weight=1, uniform="sudoku"
                )
        for row in range(9):
            for column in range(9):
                entry = tk.Entry(
                    self.sudoku_board,
                    width=2,
                    justify="center",
                    font=("TkDefaultFont", 17, "bold"),
                    relief="flat",
                    bd=0,
                    bg="#f8fafc" if (row // 3 + column // 3) % 2 == 0 else "#ffffff",
                    fg="#0f172a",
                    insertbackground="#2563eb",
                    validate="key",
                    validatecommand=validate_digit,
                )
                entry.grid(
                    row=row + row // 3,
                    column=column + column // 3,
                    sticky="nsew",
                    padx=1,
                    pady=1,
                )
                entry.bind(
                    "<KeyRelease>",
                    lambda event, r=row, c=column: self._advance_sudoku_cell(
                        event, r, c
                    ),
                )
                self.sudoku_cells[row][column] = entry

        info_card = ttk.Frame(content, padding=18, style="Card.TFrame")
        info_card.pack(side="left", fill="y", padx=(8, 0))
        ttk.Label(info_card, text="Puzzle status", style="Section.TLabel").pack(anchor="w")
        self.sudoku_status = tk.Label(
            info_card,
            text="Ready for a 9×9 puzzle.",
            bg="#ffffff",
            fg="#334155",
            anchor="w",
            justify="left",
            wraplength=250,
        )
        self.sudoku_status.pack(fill="x", pady=(8, 14))
        ttk.Separator(info_card).pack(fill="x", pady=(0, 12))
        ttk.Label(
            info_card,
            text=(
                "Digits 1–9 are clues. Leave a cell empty for the solver to fill it. "
                "Encoding, DPLL trace, and performance are in Solver results."
            ),
            style="Muted.TLabel",
            wraplength=250,
            justify="left",
        ).pack(anchor="w")
        self.sudoku_metrics = tk.Label(
            info_card,
            text="Encoding, DPLL trace, and performance are in Solver results.",
            bg="#ffffff",
            fg="#64748b",
            anchor="w",
            justify="left",
        )
        self.sudoku_metrics.pack(fill="x", pady=(18, 0))

    def _resize_sudoku_board(self, event):
        size = max(1, min(event.width - 32, event.height - 32))
        self.sudoku_board.place(
            relx=0.5,
            rely=0.5,
            anchor="center",
            width=size,
            height=size,
        )

    def _valid_sudoku_entry(self, value):
        return not self.busy and (
            value == "" or (len(value) == 1 and value in "0123456789")
        )

    def _advance_sudoku_cell(self, event, row, column):
        if event.keysym in ("BackSpace", "Delete", "Left", "Right", "Up", "Down"):
            return
        value = self.sudoku_cells[row][column].get()
        if value and value != "0":
            next_index = row * 9 + column + 1
            if next_index < 81:
                next_row, next_column = divmod(next_index, 9)
                self.sudoku_cells[next_row][next_column].focus_set()
                self.sudoku_cells[next_row][next_column].icursor("end")

    def _sudoku_grid(self):
        return [
            [
                int(self.sudoku_cells[row][column].get() or 0)
                for column in range(9)
            ]
            for row in range(9)
        ]

    def clear_sudoku(self):
        for row in self.sudoku_cells:
            for cell in row:
                cell.delete(0, "end")
                cell.configure(fg="#0f172a")
        self.sudoku_metrics.configure(text="Solver metrics will appear here.")
        self.sudoku_status.configure(text="Grid cleared.", fg="#334155")
        self._clear_solver_results()

    def _load_sudoku_grid(self, grid):
        self.clear_sudoku()
        for row, values in enumerate(grid):
            for column, value in enumerate(values):
                if value:
                    cell = self.sudoku_cells[row][column]
                    cell.insert(0, str(value))
                    cell.configure(fg="#0f172a")

    def generate_sudoku_puzzle(self):
        try:
            clue_count = int(self.sudoku_clue_count.get())
            grid = generate_sudoku(clue_count)
        except ValueError as error:
            self.sudoku_status.configure(text="Error: " + str(error), fg="#b91c1c")
            return
        self._load_sudoku_grid(grid)
        self.sudoku_status.configure(
            text=(
                "Random puzzle generated with {} clues; it has at least one solution."
            ).format(clue_count),
            fg="#334155",
        )
        self.sudoku_metrics.configure(
            text="Generated puzzles are not checked for a unique solution."
        )

    def solve_sudoku(self):
        grid = self._sudoku_grid()
        self._start_job(
            "Solving Sudoku...",
            lambda: self.backend.sudoku(
                grid,
                trace_unit=self.trace_unit.get(),
                trace_pure=self.trace_pure.get(),
            ),
            self._show_sudoku_result,
            buttons=(
                self.sudoku_solve_button,
                self.sudoku_generate_button,
                self.sudoku_clear_button,
                self.sudoku_trace_unit_button,
                self.sudoku_trace_pure_button,
            ),
            inputs=(
                *[cell for row in self.sudoku_cells for cell in row],
                self.sudoku_clue_count,
                self.sudoku_trace_unit_button,
                self.sudoku_trace_pure_button,
            ),
            status_widget=self.sudoku_status,
        )

    def _show_sudoku_result(self, response):
        if not response.get("ok"):
            self.sudoku_status.configure(
                text="Error: " + response.get("error", "Sudoku solve failed."),
                fg="#b91c1c",
            )
            return
        satisfiable = response["satisfiable"]
        if satisfiable:
            for row, values in enumerate(response["grid"]):
                for column, value in enumerate(values):
                    cell = self.sudoku_cells[row][column]
                    was_clue = cell.get() not in ("", "0")
                    cell.delete(0, "end")
                    cell.insert(0, str(value))
                    cell.configure(fg="#0f172a" if was_clue else "#2563eb")
            self.sudoku_status.configure(
                text="Solved — a valid completion is shown in blue.",
                fg="#047857",
            )
            solution = ["SAT — Sudoku solution"]
            solution.extend(" ".join(map(str, row)) for row in response["grid"])
        else:
            self.sudoku_status.configure(
                text="No solution exists for these clues.",
                fg="#b91c1c",
            )
            self.sudoku_metrics.configure(text="Try correcting one or more clues.")
            solution = ["UNSAT — no completion satisfies these clues."]

        encoding = [
            "Sudoku encoding: direct CNF",
            "Exactly one digit is selected for each cell.",
            "At most one of each digit appears in each row, column, and 3×3 box.",
            "",
            "Variables: {}".format(response["variable_count"]),
            "Clues: {}".format(response["clue_count"]),
            "Clauses: {}".format(response["clause_count"]),
            "Literals: {}".format(response["literal_count"]),
        ]
        metrics = [
            ("Problem", "Sudoku"),
            ("Variables", response["variable_count"]),
            ("Clues", response["clue_count"]),
            ("Clauses", response["clause_count"]),
            ("Literals", response["literal_count"]),
            ("Encoding time (ms)", "{:.3f}".format(response["encoding_time_ms"])),
            ("Selected solver time (ms)", "{:.3f}".format(response["solver_time_ms"])),
            ("Selected decisions", response["decisions"]),
            ("Selected backtracks", response["backtracks"]),
        ]
        self._show_solver_results(
            "Sudoku",
            "\n".join(solution),
            "\n".join(encoding),
            metrics,
            response["comparisons"],
            response["trace"],
        )

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

    def _puzzle_dimensions_changed(self, _event=None):
        try:
            nodes = int(self.node_count.get())
            colours = int(self.colour_count.get())
        except ValueError:
            return
        if not (3 <= nodes <= 100 and 1 <= colours <= 12):
            return
        self.puzzle_edges = [
            (first, second)
            for first, second in self.puzzle_edges
            if first <= nodes and second <= nodes
        ]
        self.selected_node = None
        self._clear_puzzle_results()
        self._refresh_edge_list()
        self.puzzle_status.configure(
            text="Graph dimensions changed. Solve again to refresh results.",
            fg="black",
        )

    def _refresh_edge_list(self):
        self.edge_list.delete(0, "end")
        for first, second in self.puzzle_edges:
            self.edge_list.insert("end", "{} - {}".format(first, second))
        self._draw_graph()

    def _draw_graph(self):
        try:
            nodes = int(self.node_count.get())
        except ValueError:
            return
        if 3 <= nodes <= 100:
            self.graph_canvas.update_graph(
                nodes,
                self.puzzle_edges,
                self.puzzle_colouring,
                self.selected_node,
            )

    def _clear_puzzle_results(self):
        self.puzzle_colouring = []
        self._clear_solver_results()
        self._draw_graph()

    def _clear_solver_results(self):
        self._set_text(
            self.puzzle_solution,
            "Solve a graph or Sudoku puzzle to see its solution.",
        )
        self._set_text(
            self.puzzle_encoding,
            "Solve a graph or Sudoku puzzle to see its encoding.",
        )
        self._set_text(
            self.puzzle_trace,
            "Solve a graph or Sudoku puzzle to see its DPLL trace.",
        )
        self.puzzle_metrics.delete(*self.puzzle_metrics.get_children())
        self.puzzle_metrics.insert(
            "", "end", values=("Solve a puzzle to see metrics.", "")
        )
        self.puzzle_comparison.delete(*self.puzzle_comparison.get_children())
        self.puzzle_comparison.insert(
            "",
            "end",
            values=("Solve a puzzle to compare heuristics.", "", "", "", "", ""),
        )

    def _graph_node_clicked(self, node):
        if self.busy:
            return
        if self.selected_node is None:
            self.selected_node = node
            self._draw_graph()
            self.puzzle_status.configure(
                text="Node {} selected. Choose another node to connect it.".format(node),
                fg="#334155",
            )
            return
        first, second = self.selected_node, node
        self.selected_node = None
        if first == second:
            self._draw_graph()
            self.puzzle_status.configure(text="A node cannot connect to itself.", fg="#b91c1c")
            return
        edge = (min(first, second), max(first, second))
        if edge in self.puzzle_edges:
            self._draw_graph()
            self.puzzle_status.configure(text="That connection already exists.", fg="#b91c1c")
            return
        self.puzzle_edges.append(edge)
        self._refresh_edge_list()
        self._clear_puzzle_results()
        self.puzzle_status.configure(
            text="Connected nodes {} and {}.".format(*edge), fg="#334155"
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

    def clear_graph(self):
        self.puzzle_edges = []
        self.selected_node = None
        self._refresh_edge_list()
        self._clear_puzzle_results()
        self.puzzle_status.configure(text="Graph cleared.", fg="#334155")

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
        if not (3 <= nodes <= 100):
            self.puzzle_status.configure(
                text="Use between 3 and 100 nodes.", fg="#b00020"
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
            lambda: self.backend.puzzle(
                nodes,
                colours,
                edges,
                trace_unit=self.trace_unit.get(),
                trace_pure=self.trace_pure.get(),
            ),
            self._show_puzzle_result,
            buttons=(
                self.solve_puzzle_button,
                self.remove_edge_button,
                self.clear_graph_button,
            ),
            inputs=(
                self.node_count,
                self.colour_count,
                self.trace_unit_button,
                self.trace_pure_button,
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
            self.puzzle_colouring = response["colouring"]
            self.puzzle_status.configure(
                text=(
                    "SAT — Valid colouring: {}. See Solver results for "
                    "encoding, trace, and performance."
                ).format("YES" if response["valid_coloring"] else "NO"),
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
            self.puzzle_colouring = []
            self.puzzle_status.configure(
                text=(
                    "UNSAT — No valid colouring with {} colours. "
                    "See Solver results for encoding, trace, and performance."
                ).format(response["colours"]),
                fg="#b00020",
            )
            solution = [
                "UNSAT",
                "No valid colouring with {} colours.".format(response["colours"]),
            ]
        self._draw_graph()
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
        self._show_solver_results(
            "Graph colouring",
            "\n".join(solution),
            "\n".join(encoding),
            metrics,
            response["comparisons"],
            response["trace"],
        )

    def _show_solver_results(
        self, problem, solution, encoding, metrics, comparisons, trace
    ):
        self._set_text(self.puzzle_solution, solution)
        self._set_text(self.puzzle_encoding, encoding)
        self.puzzle_metrics.delete(*self.puzzle_metrics.get_children())
        for name, value in metrics:
            self.puzzle_metrics.insert("", "end", values=(name, value))

        self.puzzle_comparison.delete(*self.puzzle_comparison.get_children())
        for result in comparisons:
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

        trace_lines = [
            "Problem: {}".format(problem),
            "Selected trace heuristics: unit propagation {}, pure literal {}".format(
                "ON" if self.trace_unit.get() else "OFF",
                "ON" if self.trace_pure.get() else "OFF",
            ),
            "",
        ]
        trace_lines.extend(self._event_text(event) for event in trace["events"])
        if trace["trace_truncated"]:
            trace_lines.append("Trace truncated at the configured event limit.")
        if not trace["events"]:
            trace_lines.append("No trace events were recorded.")
        self._set_text(self.puzzle_trace, "\n".join(trace_lines))
        self.puzzle_results.select(3)
        self.tabs.select(self.solver_results_tab)

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
        inputs=None,
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
        self.active_job_inputs = tuple(inputs or ())
        for widget in self.active_job_inputs:
            widget.configure(state="disabled")
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
                for widget in self.active_job_inputs:
                    widget.configure(state="normal")
                self.active_job_inputs = ()
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
