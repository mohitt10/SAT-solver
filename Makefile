CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic

TARGETS = puzzle sudoku

all: $(TARGETS)

puzzle: puzzle.cpp
	$(CXX) $(CXXFLAGS) puzzle.cpp -o puzzle

sudoku: sudoku.cpp
	$(CXX) $(CXXFLAGS) sudoku.cpp -o sudoku

clean:
	rm -f $(TARGETS)

.PHONY: all clean