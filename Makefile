CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic
CPPFLAGS ?= -I.

CORE_SOURCES = lexer.cpp parser.cpp truth_table.cpp nnf.cpp

.PHONY: all test clean

all: sat_solver

sat_solver: $(CORE_SOURCES) main.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

worklet1_tests: $(CORE_SOURCES) tests/worklet1_tests.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

test: worklet1_tests
	./worklet1_tests

clean:
	rm -f sat_solver worklet1_tests
