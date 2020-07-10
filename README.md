# alan
A language and interpreter for running turing machines. Inspired by the book _Annotated Turing_.

## What does it do?
This project defines a language for defining _m-configurations_ to be executed by a turing machine. It also contains an interpreter for said language, that shows the progress and result of the execution.

## Setting up and running programs
Simply run `make` to compile the interpreter. You can then run it with the file to interpret and the number of passes to make, like this:
```
make
// turing.exe examplel\example.tur 3 (on windows)
turing example/example.tur 3     // on linux/macos

 v
[ ]
 v
[0]
  v
[0 ]
   v
[0 1]
```
Just change the makefile to fit your needs, for instance if you want to use a different compiler.

### Example
The following is a simple example from _Annotated Turing_, which produces the decimals in binary for the fraction 1/4.
```
b: none | P0, R | c
c: none | R     | d
d: none | P1, R | e
e: none | R     | f
f: none | P0, R | e

```



## How do I write programs?
To ease the process of writing one of these programs, I wrote an interpreter that churns out bytecode(?) that the virtual machine can run.
It takes in code formated like this:
```
print0: none | P0, R | print1
print1: none | P1, R | print0
```
This particular program will print alternating 1s and 0s.
### Formatting

Turing machines work on a series of *m-configurations*. Each of these configurations contain information about which operations to perform for different symbols, and which configuration to move like next.
Here is an *m-configuration* from the snippet above, called `print0`, which fittingly will print the value `0` into the current position in memory.

```
print0: none | P0, R | print1
```

Each configuration consists of one or more branches, separated by lines. Each of these branches consists of three fields, sperated by vertical bars `|`. These contain the _symbol_ the branch reacts to, the _operations_ to perform for the branch and the name of the _next configuration_.

A configuration with branches will look like this: 

```
print 0 or 1: none | P0, R | print 0 or 1
              else | P1, R | print 1
              
print 1: ...
```
Also note that it is ok for names to have spaces in them for readability.

#### Symbols
The first field contains a _symbol_. When an _m-configuration_ is executed, a _symbol_ is read from memory, and a branch is selected if its symbol corresponds to the one that is read.

_Symbols_ in this case denotes ascii characters and the keywords `none` and `else`. `none` represents a blank piece of memory, and `else` means the branch matches any symbol read.
It is worth noting here that `ÿ` (`0xFF`) is not a valid symbol, as it is used to represent `else` internally. Neither can you actually refer to the _space_ symbol in a configuration, but the `none` keyword translates literally to a space (`0x20`), so that is no issue. Other than that, all sensible ascii letters should be usable.

#### Operations
The next fields contain the operations to perform for the current branch. This can be a series of several operations, separated by commas. The defined operations are:
| Operation | Description |
|-|-|
| N | Do nothing, or _halt_ as it is called in some litterature |
| P_ | Print the following ascii symbol (like P1) |
| E | Erase the symbol of the current pointer position |
| R | Move pointer location to the right |
| L | Move pointer location to the left |

#### Next
The final value is the name of the next *m-configuration* to move to.


