CXX ?= g++
AR ?= ar
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic
CPPFLAGS ?= -I. -Icode
QT_PACKAGE ?= Qt6Widgets
BUILD_DIR := build

ifeq ($(OS),Windows_NT)
EXE_EXT := .exe
MKDIR_BUILD := cmd /c "if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)"
DELETE_FILE = cmd /c "if exist $(1) del /q $(1)"
else
EXE_EXT :=
MKDIR_BUILD := mkdir -p $(BUILD_DIR)
DELETE_FILE = rm -f $(1)
endif

WORKLET1_SOURCES := lexer.cpp parser.cpp truth_table.cpp nnf.cpp
WORKLET1_HEADERS := ast.h lexer.h parser.h truth_table.h nnf.h parse_error.h
SAT_SOLVER_SOURCES := code/nnf_converter.cpp code/dpll_solver.cpp
SAT_SOLVER_HEADERS := code/cnf_structures.hpp
SAT_SOLVER_OBJECTS := $(BUILD_DIR)/nnf_converter.o $(BUILD_DIR)/dpll_solver.o

CNF_TEST := $(BUILD_DIR)/nnf_to_cnf_test$(EXE_EXT)
END_TO_END_TEST := $(BUILD_DIR)/end_to_end_test$(EXE_EXT)
WORKLET1_TEST := $(BUILD_DIR)/worklet1_tests$(EXE_EXT)
SAT_SOLVER_LIBRARY := $(BUILD_DIR)/libsat_solver.a

.PHONY: all gui test test-worklet1 test-cnf test-e2e clean

all: sat_solver$(EXE_EXT) $(SAT_SOLVER_LIBRARY)

gui: sat_solver_gui$(EXE_EXT)

sat_solver$(EXE_EXT): $(WORKLET1_SOURCES) main.cpp $(WORKLET1_HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WORKLET1_SOURCES) main.cpp -o $@

sat_solver_gui$(EXE_EXT): $(WORKLET1_SOURCES) gui_main.cpp $(WORKLET1_HEADERS)
	@pkg-config --exists $(QT_PACKAGE) || { \
		echo "Qt Widgets package '$(QT_PACKAGE)' not found; install Qt Widgets or set QT_PACKAGE."; \
		exit 1; \
	}
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $$(pkg-config --cflags $(QT_PACKAGE)) \
		$(WORKLET1_SOURCES) gui_main.cpp $$(pkg-config --libs $(QT_PACKAGE)) -o $@

$(SAT_SOLVER_LIBRARY): $(SAT_SOLVER_OBJECTS)
	$(AR) rcs $@ $^

$(BUILD_DIR)/nnf_converter.o: code/nnf_converter.cpp $(SAT_SOLVER_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/dpll_solver.o: code/dpll_solver.cpp $(SAT_SOLVER_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(CNF_TEST): code/nnf_converter.cpp tests/nnf_to_cnf_test.cpp $(SAT_SOLVER_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) code/nnf_converter.cpp tests/nnf_to_cnf_test.cpp -o $@

$(END_TO_END_TEST): tests/end_to_end_test.cpp $(SAT_SOLVER_LIBRARY) $(SAT_SOLVER_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/end_to_end_test.cpp $(SAT_SOLVER_LIBRARY) -o $@

$(WORKLET1_TEST): $(WORKLET1_SOURCES) tests/worklet1_tests.cpp $(WORKLET1_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WORKLET1_SOURCES) tests/worklet1_tests.cpp -o $@

test: test-worklet1 test-cnf test-e2e

test-worklet1: $(WORKLET1_TEST)
	./$(WORKLET1_TEST)

test-cnf: $(CNF_TEST)
	./$(CNF_TEST)

test-e2e: $(END_TO_END_TEST)
	./$(END_TO_END_TEST)

$(BUILD_DIR):
	$(MKDIR_BUILD)

clean:
	$(call DELETE_FILE,sat_solver$(EXE_EXT))
	$(call DELETE_FILE,sat_solver_gui$(EXE_EXT))
	$(call DELETE_FILE,$(SAT_SOLVER_LIBRARY))
	$(call DELETE_FILE,$(BUILD_DIR)/nnf_converter.o)
	$(call DELETE_FILE,$(BUILD_DIR)/dpll_solver.o)
	$(call DELETE_FILE,$(CNF_TEST))
	$(call DELETE_FILE,$(END_TO_END_TEST))
	$(call DELETE_FILE,$(WORKLET1_TEST))
