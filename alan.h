#ifndef ALAN_H
#define ALAN_H

typedef struct Machine Machine;
typedef struct Configuration Configuration;
typedef struct Branch Branch;
typedef struct Operation Operation;

typedef struct IOperation IOperation;
typedef struct IBranch IBranch;
typedef struct IConfig IConfig;
typedef struct IR IR;

typedef struct Error Error;
typedef struct Context Context;

IR *parse(Context *context, IR *ir, char *bytecode);
Machine translate(IR *ir);
void run_machine(Context *context, Machine *m, int iterations, char *result, bool verbose);

#endif
