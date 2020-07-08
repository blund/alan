ifeq ($(OS),Windows_NT)     # is Windows_NT on XP, 2000, 7, Vista, 10...
 	target = turing.exe
else
 	target = turing
endif

$(target): turing.cpp
	clang++ -o $(target) turing.cpp

clean:
	rm -f turing.o
