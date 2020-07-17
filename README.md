# alan
An interpreted language for defining and running turing machines. Inspired by the book _Annotated Turing_.

## What is this?
Alan is a language for defining _m-configurations_ - instructions that can be executed by a turing machine - and an interpreter for running these configurations.

## What does this language look like?
While m-configurations are mathematically useful, they are not very terse and easy to write in. This language adds some niceties on top of the simple instructions presented by Turing in his paper. Here is an example that prints the good old 'hello world':
```
h: none | P01101000 | e

e: none | P01100101, L, PE | l

l: E    | R,  P01101100, L, PL | l
   L    | R,  P01101100 | o
   R    | R,  P01101100 | d


o: none | P01101111 | space
   W    | R, P01101111 | r

space: none  | P00100000 | w
       D     | R, P00100000 | h

w: none | P01110111, L, PW | o
   else | PX | h

r: none | P01110010, L, PR | l

d: none | P01100100, L, PD | space
```
And running this program will look like this:
```
alan.exe examples\helloworld.aln 11

 Binary:        0110100001100101011011000110110001101111001000000111011101101111011100100110110001100100
 String:        hello world
 Float:         0.4077976
```
Yikes! So like I said, not very terse, but it gets the job done. We will be going throught the syntax after presenting how to set up the interpreter and start running some example programs.


## Setting up and running programs
The default Makefile uses Clang to compile. GCC and CL should probably also work fine. On linux and mac os, you have to pass -lm to link with the math library.

If you have Clang, simply run `make` to compile. 
```
make
```
Now you can pass the example .aln files to the interpreter, as well as how many passes you want the machine to make, like this:
```
On Windows:
alan.exe example/helloworld.aln 11

On Linux/Mac OS
./alan example/helloworld.aln 11

 Binary:        0110100001100101011011000110110001101111001000000111011101101111011100100110110001100100
 String:        hello world
 Float:         0.4077976
```

### Great! How do I write these _m-configurations_ though?
The following is a very simple example from _Annotated Turing_, which produces the decimals in binary for the fraction 1/4.
```
b: none | P0, R | c
c: none | R     | d
d: none | P1, R | e
e: none | R     | f
f: none | P0, R | e

```
At the beginning of each line, we have the **name** of the configuration. This is used to identify the configuration, such that other configurations can get to it.
After the name, following the `:`, we have the details of the configuration. This consists of three different fields, separated by `|`.

The first field is the **match symbol**, which in all the configurations above is `none`. When the turing machine is running, it reads a value off its memory. If the value it reads matches this symbol, the configuration will be executed. `none` is a special keyword that refers to a blank piece of memory. The other valid values for this position are `any`, `else` and [printable ascii symbols](https://theasciicode.com.ar/) (except `space` and `~`).

Here is a small overview of the keywords:
| Keyword | Description |
|-|-|
| `none`  | A blank piece of memory |
| `any`   | Matches to either `0` or `1` |
| `else`  | Matches any character |

Note here that a single configuration can have several **branches** with different match symbols, like you saw in the "hello world" example above:.
```
l: E    | R,  P01101100, L, PL | l
   L    | R,  P01101100        | o
   R    | R,  P01101100        | d
```
Here we see that you can use branches to perform different operations in reaction to different symbols. Note that even though we have not explained what the next fields are, we already understand that what will be done depends on the match symbol of the branch!

The next field is the **operations** to be executed. There is a small set of instructions built in:
| Operation | Description |
|-|-|
| N | Do nothing, or _halt_ as it is called in some literature |
| Pc[cs] | Print the following ascii symbol(s) (like P1 or P10001) |
| E | Erase the symbol at the current pointer position |
| R[n] | Move pointer one or several steps to the right (like R or R2)|
| L[n] | Move pointer one or several steps to the left (just like R) |

There can practically be as many operations as you want.
Also note that for when passing a series of symbols to `P`, like `P101`, 
there will be inserted two right steps between each symbols. We will come back to this.

The final field is the name of the **next** configuration to execute.
