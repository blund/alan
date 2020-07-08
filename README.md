# turing
A turing machine implementation written to run programs from the book _Annotated Turing_.

## How do i write programs?
To ease the process of writing one of these programs, I wrote an interpreter that churns out bytecode(?) that the virtual machine can run. 
It takes in code formated like this:
```
print0: none, P0 R, print1
print1: none, P1 R, print0
```
This particular program will print alternating 1s and 0s. 
### Formatting

Turing machines work on a series of *m-configurations*. Each of these configurations contain information about which operations to perform for different symbols, and which configuration to move like next.
Here is an *m-configuration* from the snippet above, called `print0`, which fittingly will print the value `0` into the current position in memory.

```
print0: none, P0 R, print1
```

After name comes the instructions for the given *m-configuration*. These can branch, with branches seperated by semicolons:

```
0or1: none, P0 R, 0or1; else, P1 R, 0r1
```
Also note that it is ok for names to have spaces in them, for readability:

```
0 or 1: none, P0 R, 0 or 1
```
Each branch has three fields, separated by commas.

#### Symbols
The first field contains a _symbol_. When an _m-configuration_ is executed, a _symbol_ is read from memory, and a branch is selected if its symbol corresponds to the one that is read.

In the snippet above, the symbols used were `none` and `else`. These are two special keywords, corresponding to a blank piece of memory and "any symbol" respectively. Other than these, all ascii characters are valid symbols, except for `0` and `ÿ` (which are used internally to represend `none` and `else`).

#### Operations
The next fields contain the operations to perform for the current branch. This can be a series of several operations, separated by spaces. The defined operations are `N`, `P`, `E`, `R` and `L`. `N` corresponds to doing nothing, or a _halt_ in some litterature. `P` is print, and must be followed by a ascii value to be printed in memory. Finally there are `R` and `L`, which correspond to moving right and left in memory.

#### Next
The final value is the name of the next *m-configuration* to move to.

### Example
The following is a simple example from _Annotated Turing_, which produces the decimals in binary for the fraction 1/4.
```
b: none, P0 R, c
c: none, R, d
d: none, P1 R, e
e: none, R, f
f: none, P0 R, e
```
## Other info / setting up
This is currently set up for development in Windows, using MSVC, in a partition `W:` with a folder `build` in the root. Running `shell.bat` will add `cl` to your path. Running `build.bat` will then compile the program. You can just compile it in place, but this is useful for debugging.. I promise.
## TODO

- Printing is currently.. broken. The "blank" memory is stored as 0. This will probably be fixed by changing it to the ascii value for a space.
- Interactive running would be fun. Like manually moving forward in the exeuction, seeing the branch being executed, the tape moving.. 
- Uh oh! The tape is a piece of memory. This means that it is very much _not_ infinite in either direction. I will have to see if I can find a reasonable way to move backwards in memory, perhaps allocating more memory dynamically.. We will see.
- Add more examples! I will type out the sqrt(2)/2 example from the book when printing works, so I can verify that it works..
- Write a Makefile.. I am currently using an archaic and mystical `build.bat` file that i run in Windows cmd, but I can write this. If not just use `cl` or any compiler for c++ you have lying around..
