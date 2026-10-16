CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic
CPPFLAGS ?= -I.
QT_PACKAGE ?= Qt6Widgets

CORE_SOURCES = lexer.cpp parser.cpp truth_table.cpp nnf.cpp

.PHONY: all gui test clean

all: sat_solver

gui: sat_solver_gui

sat_solver: $(CORE_SOURCES) main.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

sat_solver_gui: $(CORE_SOURCES) gui_main.cpp
	@pkg-config --exists $(QT_PACKAGE) || { \
		echo "Qt Widgets package '$(QT_PACKAGE)' not found; install Qt 6 Widgets or set QT_PACKAGE."; \
		exit 1; \
	}
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $$(pkg-config --cflags $(QT_PACKAGE)) $^ \
		$$(pkg-config --libs $(QT_PACKAGE)) -o $@

worklet1_tests: $(CORE_SOURCES) tests/worklet1_tests.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

test: worklet1_tests
	./worklet1_tests

clean:
	rm -f sat_solver sat_solver_gui worklet1_tests
