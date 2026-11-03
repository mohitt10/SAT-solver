CXX ?= g++
AR ?= ar
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic
CPPFLAGS ?= -Iinclude -Iinclude/sat_solver
BUILD_DIR := build

ifeq ($(OS),Windows_NT)
EXE_EXT := .exe
MKDIR_BUILD := cmd /c "if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)"
DELETE_FILE = cmd /c "for %F in ($(1)) do if exist %F del /q %F"
else
EXE_EXT :=
MKDIR_BUILD := mkdir -p $(BUILD_DIR)
DELETE_FILE = rm -f $(1)
endif

WORKLET1_SOURCES := src/frontend/lexer.cpp src/frontend/parser.cpp src/frontend/truth_table.cpp src/frontend/nnf.cpp
WORKLET1_HEADERS := include/sat_solver/ast.h include/sat_solver/lexer.h include/sat_solver/parser.h include/sat_solver/truth_table.h include/sat_solver/nnf.h include/sat_solver/parse_error.h
SAT_SOLVER_SOURCES := src/sat/nnf_converter.cpp src/sat/tseitin_converter.cpp src/sat/dpll_solver.cpp
SAT_SOLVER_HEADERS := include/sat_solver/ast.h include/sat_solver/sat_types.hpp
SAT_SOLVER_OBJECTS := $(BUILD_DIR)/nnf_converter.o $(BUILD_DIR)/tseitin_converter.o $(BUILD_DIR)/dpll_solver.o
GRAPH_COLORING_SOURCES := src/graph/graph_coloring.cpp $(SAT_SOLVER_SOURCES)

WORKLET1_TEST := $(BUILD_DIR)/worklet1_tests$(EXE_EXT)
WORKLET1_INTEGRATION_TEST := $(BUILD_DIR)/worklet_1_integration_testing$(EXE_EXT)
WORKLET1_VALIDATION_TEST := $(BUILD_DIR)/worklet1_validation_tests$(EXE_EXT)
CNF_TEST := $(BUILD_DIR)/worklet_2_nnf_to_cnf_unit_test$(EXE_EXT)
DPLL_TEST := $(BUILD_DIR)/worklet_2_dpll_unit_test$(EXE_EXT)
INTEGRATION_TEST := $(BUILD_DIR)/worklet_2_integration_testing$(EXE_EXT)
WORKLET1_UNIT_TEST := $(BUILD_DIR)/worklet1_unit_tests$(EXE_EXT)
GRAPH_COLORING_TEST := $(BUILD_DIR)/graph_coloring_test$(EXE_EXT)
TSEITIN_TEST := $(BUILD_DIR)/tseitin_test$(EXE_EXT)
END_TO_END_TEST := $(BUILD_DIR)/end_to_end_test$(EXE_EXT)
PIGEON_TEST := $(BUILD_DIR)/pigeon$(EXE_EXT)
SUDOKU_SOURCE := src/sudoku/sudoku.cpp
SUDOKU_TARGET := sudoku$(EXE_EXT)

SAT_SOLVER_LIBRARY := $(BUILD_DIR)/libsat_solver.a
TEST_BINARIES := $(WORKLET1_TEST) $(WORKLET1_UNIT_TEST) $(WORKLET1_INTEGRATION_TEST) \
	$(WORKLET1_VALIDATION_TEST) $(CNF_TEST) $(DPLL_TEST) $(INTEGRATION_TEST) \
	$(GRAPH_COLORING_TEST) $(TSEITIN_TEST) $(END_TO_END_TEST)
SAT_SOLVER_OUTPUTS := sat_solver$(EXE_EXT) sat_solver_backend$(EXE_EXT) \
	performance$(EXE_EXT) $(SUDOKU_TARGET)

.PHONY: all benchmark gui-backend e2e-test test clean

all: sat_solver$(EXE_EXT) $(SAT_SOLVER_LIBRARY) $(SUDOKU_TARGET)

gui-backend: sat_solver_backend$(EXE_EXT)

benchmark: performance$(EXE_EXT)

e2e-test: $(END_TO_END_TEST)
	./$(END_TO_END_TEST)

sat_solver_backend$(EXE_EXT): $(WORKLET1_SOURCES) $(SAT_SOLVER_SOURCES) src/graph/graph_coloring.cpp apps/gui_backend.cpp include/sat_solver/graph_coloring.h $(WORKLET1_HEADERS) $(SAT_SOLVER_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WORKLET1_SOURCES) $(SAT_SOLVER_SOURCES) src/graph/graph_coloring.cpp apps/gui_backend.cpp -o $@

sat_solver$(EXE_EXT): $(WORKLET1_SOURCES) apps/main.cpp $(WORKLET1_HEADERS) $(SAT_SOLVER_LIBRARY)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WORKLET1_SOURCES) apps/main.cpp $(SAT_SOLVER_LIBRARY) -o $@

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


$(WORKLET1_TEST): $(WORKLET1_SOURCES) tests/worklet1_tests.cpp $(WORKLET1_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WORKLET1_SOURCES) tests/worklet1_tests.cpp -o $@

$(WORKLET1_INTEGRATION_TEST): $(WORKLET1_SOURCES) tests/worklet_1_integration_testing.cpp $(WORKLET1_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WORKLET1_SOURCES) tests/worklet_1_integration_testing.cpp -o $@

$(CNF_TEST): src/sat/nnf_converter.cpp tests/worklet_2_nnf_to_cnf_unit_test.cpp $(SAT_SOLVER_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) src/sat/nnf_converter.cpp tests/worklet_2_nnf_to_cnf_unit_test.cpp -o $@

$(DPLL_TEST): src/sat/dpll_solver.cpp tests/worklet_2_dpll_unit_test.cpp $(SAT_SOLVER_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) src/sat/dpll_solver.cpp tests/worklet_2_dpll_unit_test.cpp -o $@

$(INTEGRATION_TEST): tests/worklet_2_integration_testing.cpp $(SAT_SOLVER_LIBRARY) $(SAT_SOLVER_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/worklet_2_integration_testing.cpp $(SAT_SOLVER_LIBRARY) -o $@

$(WORKLET1_UNIT_TEST): $(WORKLET1_SOURCES) tests/worklet1_unit_tests.cpp $(WORKLET1_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WORKLET1_SOURCES) tests/worklet1_unit_tests.cpp -o $@

$(WORKLET1_VALIDATION_TEST): $(WORKLET1_SOURCES) tests/worklet1_validation_tests.cpp $(WORKLET1_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WORKLET1_SOURCES) tests/worklet1_validation_tests.cpp -o $@

$(GRAPH_COLORING_TEST): $(WORKLET1_SOURCES) $(GRAPH_COLORING_SOURCES) tests/graph_coloring_test.cpp include/sat_solver/graph_coloring.h $(WORKLET1_HEADERS) $(SAT_SOLVER_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WORKLET1_SOURCES) $(GRAPH_COLORING_SOURCES) tests/graph_coloring_test.cpp -o $@

$(TSEITIN_TEST): tests/tseitin_test.cpp $(WORKLET1_SOURCES) $(SAT_SOLVER_SOURCES) $(WORKLET1_HEADERS) $(SAT_SOLVER_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WORKLET1_SOURCES) $(SAT_SOLVER_SOURCES) tests/tseitin_test.cpp -o $@

$(END_TO_END_TEST): tests/end_to_end_test.cpp $(WORKLET1_SOURCES) $(SAT_SOLVER_SOURCES) $(WORKLET1_HEADERS) $(SAT_SOLVER_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WORKLET1_SOURCES) $(SAT_SOLVER_SOURCES) tests/end_to_end_test.cpp -o $@

$(PIGEON_TEST): tests/pigeon.cpp $(SAT_SOLVER_LIBRARY) $(SAT_SOLVER_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/pigeon.cpp $(SAT_SOLVER_LIBRARY) -o $@

$(SUDOKU_TARGET): $(SUDOKU_SOURCE) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@

test: $(TEST_BINARIES)
	./$(WORKLET1_UNIT_TEST)
	./$(WORKLET1_TEST)
	./$(WORKLET1_INTEGRATION_TEST)
	./$(WORKLET1_VALIDATION_TEST) --quiet
	./$(CNF_TEST)
	./$(DPLL_TEST)
	./$(INTEGRATION_TEST)
	./$(GRAPH_COLORING_TEST)
	./$(TSEITIN_TEST)
	./$(END_TO_END_TEST)

$(BUILD_DIR):
	$(MKDIR_BUILD)

clean:
	$(call DELETE_FILE,$(SAT_SOLVER_OUTPUTS))
	$(call DELETE_FILE,$(SAT_SOLVER_LIBRARY))
	$(call DELETE_FILE,$(SAT_SOLVER_OBJECTS))
	$(call DELETE_FILE,$(TEST_BINARIES) $(PIGEON_TEST))