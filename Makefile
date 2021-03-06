ifeq ($(OS),Windows_NT)     # is Windows_NT on XP, 2000, 7, Vista, 10...
 	target = alan.exe
	link =
else
 	target = alan
	link = -lm
endif

$(target): alan.c
	clang alan.c $(link) -std=c99 -o $(target)

debug: alan.c
	clang alan.c $(link) -std=c99 -g -gcodeview -o w:/build/$(target)

clean:
	rm -f turing.o
