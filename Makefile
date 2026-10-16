CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -Icode
BUILD_DIR := build

CORE_SOURCES := code\nnf_converter.cpp code\dpll_solver.cpp
CORE_HEADERS := code\sat_types.hpp

CNF_TEST := $(BUILD_DIR)\nnf_to_cnf_test.exe
END_TO_END_TEST := $(BUILD_DIR)\end_to_end_test.exe

.PHONY: all test test-cnf test-e2e clean

all: $(CNF_TEST) $(END_TO_END_TEST)

test: test-cnf test-e2e

test-cnf: $(CNF_TEST)
	.\$(CNF_TEST)

test-e2e: $(END_TO_END_TEST)
	.\$(END_TO_END_TEST)

$(CNF_TEST): code\nnf_converter.cpp tests\nnf_to_cnf_test.cpp $(CORE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) code\nnf_converter.cpp tests\nnf_to_cnf_test.cpp -o $@

$(END_TO_END_TEST): $(CORE_SOURCES) tests\end_to_end_test.cpp $(CORE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CORE_SOURCES) tests\end_to_end_test.cpp -o $@

$(BUILD_DIR):
	cmd /c "if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)"

clean:
	cmd /c "if exist $(CNF_TEST) del /q $(CNF_TEST)"
	cmd /c "if exist $(END_TO_END_TEST) del /q $(END_TO_END_TEST)"
