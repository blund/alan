# turing
A turing machine implementation written to run programs from the book _Annotated Turing_.

## How do i write programs?
To ease the process of writing one of these programs, I wrote an interpreter that churns out bytecode(?) that the virtual machine can run. 
It takes in code formated like this:
```
print 0: none, P0 R, print 1
print 1: none, P1 R, print 0
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
