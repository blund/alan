ifeq ($(OS),Windows_NT)     # is Windows_NT on XP, 2000, 7, Vista, 10...
 	target = alan.exe
	link =
else
 	target = alan
	link = -lm
endif

$(target): alan.c
	clang -o $(target) alan.c $(link)

clean:
	rm -f turing.o
