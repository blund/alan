ifeq ($(OS),Windows_NT)     # is Windows_NT on XP, 2000, 7, Vista, 10...
 	target = alan.exe
else
 	target = alan
endif

$(target): alan.c
	clang -o $(target) alan.c -lm

clean:
	rm -f turing.o
