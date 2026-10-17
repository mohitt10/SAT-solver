CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic
TARGET = puzzle
SOURCE = puzzle.cpp
all: $(TARGET)
$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) $(SOURCE) -o $(TARGET)
clean:
	rm -f $(TARGET)
.PHONY: all clean
