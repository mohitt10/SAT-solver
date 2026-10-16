all:
	g++ -std=c++17 main.cpp nnf_converter.cpp dpll_solver.cpp -o sat_solver

clean:
	rm -f sat_solver