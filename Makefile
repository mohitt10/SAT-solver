CXX ?= g++
AR ?= ar
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic
CPPFLAGS ?= -Iinclude -Iinclude/sat_solver
QT_PACKAGE ?= Qt6Widgets
BUILD_DIR := build

ifeq ($(OS),Windows_NT)
EXE_EXT := .exe
MKDIR_BUILD := cmd /c "if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)"
DELETE_FILE = cmd /c "if exist $(subst /,\,$(1)) del /q $(subst /,\,$(1))"
QT_CHECK = @pkg-config --exists $(QT_PACKAGE) || (echo Qt Widgets package '$(QT_PACKAGE)' not found. Install Qt Widgets or set QT_PACKAGE. & exit /b 1)
else
EXE_EXT :=
MKDIR_BUILD := mkdir -p $(BUILD_DIR)
DELETE_FILE = rm -f $(1)
QT_CHECK = @pkg-config --exists $(QT_PACKAGE) || { echo "Qt Widgets package '$(QT_PACKAGE)' not found; install Qt Widgets or set QT_PACKAGE."; exit 1; }
endif

QT_CFLAGS = $(shell pkg-config --cflags $(QT_PACKAGE))
QT_LIBS = $(shell pkg-config --libs $(QT_PACKAGE))

WORKLET1_SOURCES := src/frontend/lexer.cpp src/frontend/parser.cpp src/frontend/truth_table.cpp src/frontend/nnf.cpp
WORKLET1_HEADERS := include/sat_solver/ast.h include/sat_solver/lexer.h include/sat_solver/parser.h include/sat_solver/truth_table.h include/sat_solver/nnf.h include/sat_solver/parse_error.h
SAT_SOLVER_SOURCES := src/sat/nnf_converter.cpp src/sat/tseitin_converter.cpp src/sat/dpll_solver.cpp
SAT_SOLVER_HEADERS := include/sat_solver/ast.h include/sat_solver/sat_types.hpp
SAT_SOLVER_OBJECTS := $(BUILD_DIR)/nnf_converter.o $(BUILD_DIR)/tseitin_converter.o $(BUILD_DIR)/dpll_solver.o
GRAPH_COLORING_SOURCES := src/graph/graph_coloring.cpp $(SAT_SOLVER_SOURCES)

CNF_TEST := $(BUILD_DIR)/nnf_to_cnf_test$(EXE_EXT)
END_TO_END_TEST := $(BUILD_DIR)/end_to_end_test$(EXE_EXT)
WORKLET1_TEST := $(BUILD_DIR)/worklet1_tests$(EXE_EXT)
GRAPH_COLORING_TEST := $(BUILD_DIR)/graph_coloring_test$(EXE_EXT)
TSEITIN_TEST := $(BUILD_DIR)/tseitin_test$(EXE_EXT)
SAT_SOLVER_LIBRARY := $(BUILD_DIR)/libsat_solver.a

.PHONY: all benchmark gui gui-backend test test-worklet1 test-cnf test-e2e test-coloring test-tseitin clean

all: sat_solver$(EXE_EXT) $(SAT_SOLVER_LIBRARY)

gui: sat_solver_gui$(EXE_EXT)

gui-backend: sat_solver_backend$(EXE_EXT)

benchmark: performance$(EXE_EXT)

sat_solver_backend$(EXE_EXT): $(WORKLET1_SOURCES) $(SAT_SOLVER_SOURCES) src/graph/graph_coloring.cpp apps/gui_backend.cpp include/sat_solver/graph_coloring.h $(WORKLET1_HEADERS) $(SAT_SOLVER_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WORKLET1_SOURCES) $(SAT_SOLVER_SOURCES) src/graph/graph_coloring.cpp apps/gui_backend.cpp -o $@

sat_solver$(EXE_EXT): $(WORKLET1_SOURCES) apps/main.cpp $(WORKLET1_HEADERS) $(SAT_SOLVER_LIBRARY)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WORKLET1_SOURCES) apps/main.cpp $(SAT_SOLVER_LIBRARY) -o $@

sat_solver_gui$(EXE_EXT): $(WORKLET1_SOURCES) $(GRAPH_COLORING_SOURCES) apps/gui_main.cpp include/sat_solver/graph_coloring.h $(WORKLET1_HEADERS) $(SAT_SOLVER_HEADERS)
	$(QT_CHECK)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(QT_CFLAGS) \
		$(WORKLET1_SOURCES) $(GRAPH_COLORING_SOURCES) apps/gui_main.cpp \
		$(QT_LIBS) -o $@

performance$(EXE_EXT): $(WORKLET1_SOURCES) apps/encoder_performance_simple.cpp $(GRAPH_COLORING_SOURCES) include/sat_solver/graph_coloring.h $(WORKLET1_HEADERS) $(SAT_SOLVER_HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WORKLET1_SOURCES) $(GRAPH_COLORING_SOURCES) apps/encoder_performance_simple.cpp -o $@

$(SAT_SOLVER_LIBRARY): $(SAT_SOLVER_OBJECTS)
	$(AR) rcs $@ $^

$(BUILD_DIR)/nnf_converter.o: src/sat/nnf_converter.cpp $(SAT_SOLVER_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/dpll_solver.o: src/sat/dpll_solver.cpp $(SAT_SOLVER_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/tseitin_converter.o: src/sat/tseitin_converter.cpp src/sat/nnf_converter.cpp $(SAT_SOLVER_HEADERS) $(WORKLET1_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(CNF_TEST): src/sat/nnf_converter.cpp tests/nnf_to_cnf_test.cpp $(SAT_SOLVER_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) src/sat/nnf_converter.cpp tests/nnf_to_cnf_test.cpp -o $@

$(END_TO_END_TEST): tests/end_to_end_test.cpp $(SAT_SOLVER_LIBRARY) $(SAT_SOLVER_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/end_to_end_test.cpp $(SAT_SOLVER_LIBRARY) -o $@

$(TSEITIN_TEST): tests/tseitin_test.cpp $(WORKLET1_SOURCES) $(SAT_SOLVER_SOURCES) $(WORKLET1_HEADERS) $(SAT_SOLVER_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WORKLET1_SOURCES) $(SAT_SOLVER_SOURCES) tests/tseitin_test.cpp -o $@

$(WORKLET1_TEST): $(WORKLET1_SOURCES) tests/worklet1_tests.cpp $(WORKLET1_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WORKLET1_SOURCES) tests/worklet1_tests.cpp -o $@

$(GRAPH_COLORING_TEST): $(WORKLET1_SOURCES) $(GRAPH_COLORING_SOURCES) tests/graph_coloring_test.cpp include/sat_solver/graph_coloring.h $(WORKLET1_HEADERS) $(SAT_SOLVER_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WORKLET1_SOURCES) $(GRAPH_COLORING_SOURCES) tests/graph_coloring_test.cpp -o $@

test: test-worklet1 test-cnf test-e2e test-coloring test-tseitin

test-worklet1: $(WORKLET1_TEST)
	./$(WORKLET1_TEST)

test-cnf: $(CNF_TEST)
	./$(CNF_TEST)

test-e2e: $(END_TO_END_TEST)
	./$(END_TO_END_TEST)

test-coloring: $(GRAPH_COLORING_TEST)
	./$(GRAPH_COLORING_TEST)

test-tseitin: $(TSEITIN_TEST)
	./$(TSEITIN_TEST)

$(BUILD_DIR):
	$(MKDIR_BUILD)

clean:
	$(call DELETE_FILE,sat_solver$(EXE_EXT))
	$(call DELETE_FILE,sat_solver_gui$(EXE_EXT))
	$(call DELETE_FILE,sat_solver_backend$(EXE_EXT))
	$(call DELETE_FILE,performance$(EXE_EXT))
	$(call DELETE_FILE,$(SAT_SOLVER_LIBRARY))
	$(call DELETE_FILE,$(BUILD_DIR)/nnf_converter.o)
	$(call DELETE_FILE,$(BUILD_DIR)/tseitin_converter.o)
	$(call DELETE_FILE,$(BUILD_DIR)/dpll_solver.o)
	$(call DELETE_FILE,$(CNF_TEST))
	$(call DELETE_FILE,$(END_TO_END_TEST))
	$(call DELETE_FILE,$(WORKLET1_TEST))
	$(call DELETE_FILE,$(GRAPH_COLORING_TEST))
	$(call DELETE_FILE,$(TSEITIN_TEST))
