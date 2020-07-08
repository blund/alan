turing.exe: turing.cpp
	clang++ -o turing.exe turing.cpp

clean:
	rm -f turing.o
