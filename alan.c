#define _CRT_SECURE_NO_WARNINGS 1
#if defined(_MSC_VER)  // Check if we are using a windows compiler
#define strtok_r strtok_s
#endif

#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alan.h"

#define TAPE_LENGTH 4096
#define MAX_BRANCH_COUNT 32
#define MAX_OPERATION_COUNT 16
#define MAX_CONF_COUNT 32
#define MAX_LINE_COUNT 256
#define MAX_ERROR 8

#define NONE ' '
#define COMMENT_CHAR '!'
#define ANY '\x7e'
#define ELSE '\x7f'

#define WINDOWSIZE 10

#define ARGUMENT_ERROR -1
#define FILE_ERROR -2
#define NOLINE -1

#define NOT_DEFINED -1

/*
 * Here we define the structures that encompass the
 * bytecode that the source will be translated to,
 * and which will be executed on the virtual
 * machine.
 */

typedef enum Op { N = 0, P, E, R, L } Op;

typedef struct Operation {
    Op name;
    union {
        char *string;
        int number;
    };
} Operation;

typedef struct Branch {
    char matchSymbol;
    int nextConfiguration;
    IBranch *info;
    Operation ops[MAX_OPERATION_COUNT];
} Branch;

typedef struct Configuration {
    IConfig *info;
    Branch branches[MAX_BRANCH_COUNT];
} Configuration;

typedef struct Machine {
    int pointer;
    char tape[TAPE_LENGTH];

    Configuration configurations[MAX_CONF_COUNT];
} Machine;

/*
 * Here we define structures used for the intermediary representation
 * of the turing program, which helps us detect errors at parse-time,
 * and allows us to print detailed information at runtime.
 */
typedef struct IOperation {
    char name;
    bool isNum;
    union {
        char *string;
        int number;
    };
} IOperation;

typedef struct IBranch {
    int definedOn;
    char *matchSymbol;
    IConfig *next;
    int opCount;
    char *opsString;
    IOperation ops[MAX_OPERATION_COUNT];
} IBranch;

typedef struct IConfig {
    int definedOn;
    bool defined;
    char *name;
    int branchCount;
    IBranch branches[MAX_BRANCH_COUNT];
} IConfig;

typedef struct IR {
    int configCount;
    IConfig configs[MAX_CONF_COUNT];
} IR;

typedef enum ErrorType { Err, Warn } ErrorType;

typedef struct Error {
    ErrorType type;
    char *message;
    int line;
} Error;

typedef struct Context {
    IR parseInfo;

    bool errorOverflow;
    int nextError;
    Error errors[MAX_ERROR];
} Context;

char *trim(char *str) {
    // TODO Write a simpler version of this
    // https://stackoverflow.com/questions/122616/how-do-i-trim-leading-trailing-whitespace-in-a-standard-way
    size_t len = 0;
    char *frontp = str;
    char *endp = NULL;

    if (str == NULL) {
        return NULL;
    }
    if (str[0] == '\0') {
        return str;
    }

    len = strlen(str);
    endp = str + len;

    /* Move the front and back pointers to address the first non-whitespace
     * characters from each end.
     */
    while (isspace((unsigned char)*frontp)) {
        ++frontp;
    }
    if (endp != frontp) {
        while (isspace((unsigned char)*(--endp)) && endp != frontp) {
        }
    }

    if (frontp != str && endp == frontp)
        *str = '\0';
    else if (str + len - 1 != endp)
        *(endp + 1) = '\0';

    /* Shift the string so that it starts at str so that if it's dynamically
     * allocated, we can still free it on the returned pointer.  Note the reuse
     * of endp to mean the front of the string buffer now.
     */
    endp = str;
    if (frontp != str) {
        while (*frontp) {
            *endp++ = *frontp++;
        }
        *endp = '\0';
    }

    return str;
}

void parse_error(Context *c, char *msg, int line) {
    if (c->nextError < MAX_ERROR) {
        // TODO bound check and assert
        c->errors[c->nextError].message = msg;
        c->errors[c->nextError].line = line;
        c->errors[c->nextError].type = Err;
        c->nextError++;
    } else {
        c->errorOverflow = true;
    }
}

void error(Context *c, char *msg, int line) {
    if (c->nextError < MAX_ERROR) {
        // TODO bound check and assert
        c->errors[c->nextError].message = msg;
        c->errors[c->nextError].line = line;
        c->errors[c->nextError].type = Err;
        c->nextError++;
    } else {
        c->errorOverflow = true;
    }
}

void warning(Context *c, char *msg, int line) {
    if (c->nextError < MAX_ERROR) {
        // TODO bound check and assert
        c->errors[c->nextError].message = msg;
        c->errors[c->nextError].line = line;
        c->errors[c->nextError].type = Warn;
        c->nextError++;
    } else {
        c->errorOverflow = true;
    }
}

void handle_errors(Context *c) {
    bool fatal;
    for (int i = 0; i < c->nextError; i++) {
        int line = c->errors[i].line;
        char *msg = c->errors[i].message;
        if (c->errors[i].type == Warn) {
            fprintf(stderr, "\n\tWarning in line %i:\n\t  %s\n", ++line, msg);
        } else if (line == FILE_ERROR) {
            fprintf(stderr, "\n\tFile error:\n\t  %s\n", msg);
        } else if (line == ARGUMENT_ERROR) {
            fprintf(stderr, "\n\tArgument error:\n\t  %s\n", msg);
        } else {
            fprintf(stderr, "\n\tError in line %i:\n\t  %s\n", ++line, msg);
        }
        if (c->errors[i].type == Err) {
            fatal = true;
        }
    }
    if (fatal) {
        exit(EXIT_FAILURE);
    }
}

char *read_source(Context *c, char *filename) {
    // https://www.tutorialspoint.com/cprogramming/c_file_io.htm
    FILE *file =
        fopen(filename, "rb");  // TODO Might be problematic outside of windows
    if (!file) {
        error(c, "File could not be loaded. Does it exist?", FILE_ERROR);
        return 0;
    };
    fseek(file, 0, SEEK_END);
    long fsize = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *bytecode = (char *)malloc(fsize + 1);
    fread(bytecode, fsize, 1, file);
    fclose(file);
    bytecode[fsize] = 0;  // Add line-terminator

    return bytecode;
}

float parse_binary_point_value(char *numstring) {
    float sum = 0;
    char *c = numstring;
    float n = -1.0f;

    while (*c != '\0') {
        float i = (float)(*c++ - 48);
        sum += i * powf(2.0f, n);
        n -= 1.0f;
    }

    return sum;
}

char *parse_string(char *result, char *binary) {
    char *c = binary;
    int resultIndex = 0;
    while (*c != 0) {
        int n = 128;
        char sum = 0;
        for (int i = 0; i < 8; i++) {
            if (*c == 0) {
                break;
            }
            if (*c == '1') {
                assert(n < 0xff);
                sum += (char)n * 1;
            }
            n /= 2;
            c++;
        }
        result[resultIndex++] = sum;
    }
    return result;
}

int insert_config(IConfig array[MAX_CONF_COUNT], char *str) {
    for (int i = 0; i < MAX_CONF_COUNT; i++) {
        if (array[i].name == NULL) {
            // We have found an empty spot,
            // which means the identifier is not in the list,
            // so we put it here
            // TODO handle bad size
            array[i].name = str;
            return i;

        } else if (strcmp(array[i].name, str) == 0) {
            // The identifier already exists, so we return its return index
            return i;
        }
        continue;
    }
    assert(false);
    return -1;
}

int find_config(IConfig array[MAX_CONF_COUNT], char *str) {
    for (int i = 0; i < MAX_CONF_COUNT; i++) {
        if (array[i].name == NULL) {
            continue;
        }
        if (strcmp(array[i].name, str) == 0) {
            // The identifier already exists, so we return its return index
            return i;
        }
        continue;
    }
    return -1;
}

int split_on(char *slots[], char *text, char *delimiter) {
    char *context;

    int index = 0;
    char *unit = strtok_r(text, delimiter, &context);
    while (unit != NULL) {
        slots[index++] = unit;
        unit = strtok_r(NULL, delimiter, &context);
    }
    return index;
}

bool find_in_string(char *string, char symbol) {
    char *c;
    for (c = string; *c != '\0'; c++) {
        if (*c == symbol) {
            return true;
        }
    }
    return false;
}

int is_number(char *string) {
    char *c;
    for (c = string; *c != '\0'; c++) {
        if (!isdigit((int)*c)) {
            return false;
        }
    }
    return true;
}

bool is_empty(char *string) { return string == NULL || string[0] == '\0'; }

bool legal_config_name(char *string) {
    char *c;
    for (c = string; *c != '\0'; c++) {
        if (islower((int)*c) || isdigit((int)*c)|| *c == ' ') {
            continue;
        }
        return false;
    }
    return true;
}

IR *parse(Context *c, IR *ir, char *code) {
    // Definer delimitere
    char newline[] = "\n";
    char commentDelim[] = "!";
    char configNameDelim[] = ":";
    char branchDelim[] = ";";
    char branchParamDelim[] = "|";
    char operationDelim[] = ",";

    char *lines[MAX_LINE_COUNT] = {0};
    int lineCount = split_on(lines, code, newline);
    IConfig *conf = NULL;

    int branchIndex = 0;
    for (int currentLine = 0; currentLine < lineCount; ++currentLine) {
        char *lineContext = lines[currentLine];

        // Skip the line if it is a comment
        if (*lineContext == COMMENT_CHAR) {
            continue;
        }
        if (*trim(lineContext) == '\0') {
            continue;
        }

        // Determine whether or not this line is the declaration of a new
        // configuration. If it is, create the new configuration in the
        // machine struct and change 'c' to refer to it.
        if (find_in_string(lineContext, ':')) {
            ir->configCount += 1;
            char *name = strtok_r(NULL, configNameDelim, &lineContext);
            name = trim(name);
            if (is_empty(lineContext) && !name) {
                parse_error(c, "missing configuration name", currentLine);
                lineContext = name;
            };

            if (!legal_config_name(name)) {
                parse_error(c, "configuration name must be all lower case",
                        currentLine);
            }

            int prevIndex = find_config(ir->configs, name);
            if (ir->configs[prevIndex].defined) {
                parse_error(c, "redefinition of configuration", currentLine);
            }

            int configIndex = insert_config(ir->configs, name);

            // We have found a n actual declaration of the
            // configuration, so we set it to defined.
            // This does not mean that the definition
            // is valid, only that it is not referenced
            // by a different configuration without
            // being defined.
            ir->configs[configIndex].defined = true;
            conf = &ir->configs[configIndex];

            conf->name = name;
            conf->definedOn = currentLine;

            // Reset branch index since we are in a new configuration
            branchIndex = 0;
        }
        if (conf == NULL) {
            parse_error(c, "missing configuration name (declare like 'begin: ...'",
                    currentLine);
            continue;
        }
        if (is_empty(lineContext)) {
            parse_error(c, "configuration is missing parameters", currentLine);
            continue;
        }

        // Increment branch index for next pass
        IBranch *branch = &conf->branches[branchIndex];
        branch->definedOn = currentLine;
        conf->branchCount += 1;

        char *symbol = strtok_r(NULL, branchParamDelim, &lineContext);
        symbol = trim(symbol);
        branch->matchSymbol = symbol;

        for (int i = 0; i < branchIndex; i++) {
            if (strcmp(conf->branches[i].matchSymbol, symbol) == 0) {
                parse_error(c, "branch for given symbol is already defined",
                        currentLine);
            }
        }
        branchIndex++;

        if (is_empty(symbol)) {
            parse_error(c, "branch is missing match symbol (first parameter)",
                    currentLine);
        };

        char *opsString = strtok_r(NULL, branchParamDelim, &lineContext);
        opsString = trim(opsString);

        // Make copy to use for printing machine state
        char *tmp = (char *)malloc(strlen(opsString) + 1);
        strcpy(tmp, opsString);
        branch->opsString = tmp;

        char *ops[MAX_OPERATION_COUNT] = {0};
        int opCount = split_on(ops, opsString, operationDelim);

        for (int i = 0; i < opCount; ++i) {
            branch->opCount += 1;
            char *op = trim(ops[i]);
            char *param = op + 1;

            switch (*op) {
                case 'N': {
                              if (*param != '\0') {
                                  error(c, "Function N had a parameter, but takes none", currentLine);
                              }
                          } break;
                case 'E': {
                              if (param == NULL) {
                                  break;
                              }
                              if (*param != '\0') {
                                  error(c, "Function E had a parameter, but takes none", currentLine);
                              }
                          } break;
                case 'P': {
                              if (param == NULL || *param == '\0') {
                                  error(c, "Function P need an argument", currentLine);
                              }
                              branch->ops[i].string = param;
                          } break;
                case 'R': {
                              if (param == NULL) {
                                  break;
                              }
                              if (isdigit(*param)) {
                                  branch->ops[i].number = *param - 48;
                              } else if (*param != 0) {
                                  error(c, "R had a parameter that was not a number", currentLine);
                              }
                          } break;
                case 'L': {
                              if (param == NULL) {
                                  break;
                              }
                              if (isdigit(*param)) {
                                  branch->ops[i].number = *param - 48;
                              } else if (*param != 0) {
                                  error(c, "L had a parameter that was not a number", currentLine);
                              }
                          } break;
                default: {
                             char buffer[128];
                             sprintf(buffer, "Undefined function %c name was referenced", *op);
                             error(c, buffer, currentLine);
                         }
            }
            branch->ops[i].name = *op;
        }

        char *nextName = strtok_r(NULL, branchParamDelim, &lineContext);
        nextName = trim(nextName);

        bool hasNext = true;
        if (is_empty(nextName)) {
            parse_error(c, "branch is missing next configuration (last parameter)",
                    currentLine);
            hasNext = false;
        };

        if (hasNext) {
            int nextConfigIndex = find_config(ir->configs, nextName);
            if (nextConfigIndex == NOT_DEFINED) {
                nextConfigIndex = insert_config(ir->configs, nextName);
                ir->configs[nextConfigIndex].definedOn = currentLine;
            }

            branch->next = &ir->configs[nextConfigIndex];
        }
    }
    // Iterate over branches of configs and check that their
    // 'next' function is defined.
    /* TODO TODO TODO
       for (int ci = 0; ci < MAX_CONF_COUNT; ci++) {
       char name = ir.configs[ci].name[0];
       bool defined = ir.configs[ci].defined;
       if (name && !defined) {
       error(c, "a configuration is not properly defined", 0);
       }
       }
       */

    for (int ci = 0; ci < ir->configCount; ci++) {
        IConfig *config = &ir->configs[ci];
        for (int bi = 0; bi < config->branchCount; bi++) {
            IBranch *branch = &ir->configs[ci].branches[bi];
            if ((strcmp(branch->matchSymbol, "else") == 0) &&
                    bi != config->branchCount - 1) {
                warning(c,
                        "Keyword 'else' was used before final branch of configuration",
                        branch->definedOn);
            }
            if (!config->branches[bi].next->defined) {
                char buffer[128];
                sprintf(buffer, "Configuration '%s' was referenced but not defined",
                        branch->next->name);
                parse_error(c, buffer, config->definedOn);
            }
        }
    }

    handle_errors(c);
    c->parseInfo = *ir;
    return ir;
}

void right(Machine *m, int count) {
    // TODO Assert that we don't go outside tape bounds
    m->pointer += count;
}

void left(Machine *m, int count) {
    // TODO Assert that we don't go outside tape bounds
    m->pointer -= count;
}
void print(Machine *m, char *sym) {
    assert(sym != NULL);
    if (strlen(sym) > 1) {
        while (*sym != '\0') {
            m->tape[m->pointer] = *sym++;
            right(m, 2);
        }
    } else {
        m->tape[m->pointer] = *sym;
    }
}

void erase(Machine *m) { m->tape[m->pointer] = ' '; }

char read(Machine *m) { return m->tape[m->pointer]; }

void copy_n(size_t SourceACount, char *SourceA, size_t DestCount, char *Dest) {
    for (int Index = 0; Index < SourceACount; ++Index) {
        *Dest++ = *SourceA++;
    }
    *Dest++ = 0;
}

void print_machine(int passCount, IConfig *configInfo, IBranch *branchInfo,
        Machine *m, int topPointerAccessed, int lowerBound,
        int upperBound, bool verbose) {
    char outputBuffer[TAPE_LENGTH];  // Buffer used for printing
    char pointerBuffer[TAPE_LENGTH];

    int pointer = (m->pointer == 0) ? 1 : m->pointer;

    for (int i = 0; i <= pointer; i++) {
        pointerBuffer[i] = ' ';
    }
    char *name = configInfo->name;
    char *match = branchInfo->matchSymbol;
    char *ops = branchInfo->opsString;
    char *next = branchInfo->next->name;
    printf("\n Pass %i:\n  %s:\t%s | %s | %s\n", passCount, name, match, ops,
            next);

    // On the first pass the pointer will be set to 0, but
    // we want it to point on a blank space in the tape
    strcpy(outputBuffer, m->tape);
    outputBuffer[topPointerAccessed + 1] = '\0';

    // +1 since we need to make up for the '[' character
    pointerBuffer[pointer - lowerBound + 1] = 'v';
    pointerBuffer[pointer - lowerBound + 2] = '\0';

    if (lowerBound != -1 || upperBound != -1) {
        char boundBuffer[TAPE_LENGTH * 2];
        outputBuffer[upperBound] = 0;
        strcpy(boundBuffer, outputBuffer + lowerBound);
        strcpy(outputBuffer, boundBuffer);
        outputBuffer[lowerBound + upperBound] = '\0';
    }

    char leftLimit = '[';
    char rightLimit = ']';
    if (upperBound < topPointerAccessed) {
        rightLimit = '>';
    }
    if (lowerBound > 0) {
        leftLimit = '<';
    }

    printf("  %s\n  %c%s%c\n\n", pointerBuffer, leftLimit, outputBuffer,
            rightLimit);
}

// void run_machine(Context *context, Machine *m, int iterations, char *result,
void run_machine(Context *context, Machine *m, int iterations, char *result,
        bool verbose) {
    int topPointerAccessed = 1;  // Value used for determining how much to print
    int window = 48;
    int highBound = window;
    int lowBound = 0;

    int configuration;

    int passCount = 0;

    while (iterations-- > 0) {
        ++passCount;
        assert(m->pointer <= TAPE_LENGTH);

        Configuration config = m->configurations[configuration];
        for (int branchIndex = 0; branchIndex < MAX_BRANCH_COUNT;
                branchIndex++) {
            char symbol = m->tape[m->pointer];

            if (branchIndex >= config.info->branchCount) {
                char buffer[256];
                sprintf(buffer, "No branch matching the symbol '%c' was found for configuration '%s'", symbol, config.info->name);

                error(context, buffer, config.info->definedOn);
                return;
            }

            Branch branch = config.branches[branchIndex];

            // Here we are checking whether or not we are in the correct branch
            // if we are not, we will go on to the next iteration.
            // If we are, we will execute the branch and move on to the next
            // configuration
            if (branch.matchSymbol == symbol ||
                    (branch.matchSymbol == ANY && symbol == '0') ||
                    (branch.matchSymbol == ANY && symbol == '1') ||
                    (branch.matchSymbol == ELSE)) {
                // Execute all operations in branch until a N
                bool nop = false;
                for (int operationIndex = 0; operationIndex < MAX_OPERATION_COUNT;
                        ++operationIndex) {
                    Operation operation = branch.ops[operationIndex];
                    switch (operation.name) {
                        case N: {
                                    nop = true;
                                } break;
                        case P: {
                                    print(m, operation.string);
                                } break;
                        case E: {
                                    erase(m);
                                } break;
                        case R: {
                                    right(m, operation.number);
                                } break;
                        case L: {
                                    left(m, operation.number);
                                } break;
                    }
                    if (nop) {
                        break;
                    }
                }
            } else {
                continue;
            }
            // Change topPointerAccessed if we have
            // touched a higher pointer.
            // This is for printing purposes.
            if (m->pointer > topPointerAccessed) {
                topPointerAccessed = m->pointer;
            }
            // Adjust the window of the tape to print
            // if we move out of the defined boundaries
            if (m->pointer >= highBound || m->pointer <= lowBound) {
                if (topPointerAccessed - m->pointer >= window / 2) {
                    highBound = m->pointer + window / 2;
                    lowBound = m->pointer - window / 2;
                } else {
                    highBound = topPointerAccessed + 1;
                    lowBound = highBound - window;
                }
            }
            if (verbose) {
                print_machine(passCount, config.info, branch.info, m, topPointerAccessed,
                        lowBound, highBound, true);
            }
            configuration = branch.nextConfiguration;
            break;
        }
    }

    // Here we want to print the result of the computation
    // into a buffer for printing.
    // We do this by writing every second value from the turing
    // machine's tape into the result buffer (following turing's
    // conventions).

    int maxIndex = 2 * (int)floor(topPointerAccessed / 2) + 2;
    int resultIndex = 0;
    for (int tapeIndex = 0; tapeIndex < maxIndex; tapeIndex += 2) {
        result[resultIndex++] = m->tape[tapeIndex];
    }
}

Machine translate(IR *ir) {
    Machine m;

    for (int i = 0; i < TAPE_LENGTH; i++) {
        m.tape[i] = NONE;
    }

    for (int ci = 0; ci < ir->configCount; ci++) {
        Configuration *conf = &m.configurations[ci];
        IConfig iconf = ir->configs[ci];
        conf->info = &ir->configs[ci];

        // Iterate over each branch in each configuration
        // and fill in its information from the ir
        for (int bi = 0; bi < iconf.branchCount; bi++) {
            Branch *branch = &conf->branches[bi];
            IBranch ibranch = iconf.branches[bi];
            branch->info = &ir->configs[ci].branches[bi];

            // Set the value for the match symbol,
            // translating keywords into their correlated value.
            if (strcmp(ibranch.matchSymbol, "none") == 0) {
                branch->matchSymbol = NONE;
            } else if (strcmp(ibranch.matchSymbol, "any") == 0) {
                branch->matchSymbol = ANY;
            } else if (strcmp(ibranch.matchSymbol, "else") == 0) {
                branch->matchSymbol = ELSE;
            } else {
                branch->matchSymbol = ibranch.matchSymbol[0];
            }

            // Determine the index of the next configuration
            // by iterating over the configurations and finding
            // the one with the correct name.
            for (int ci = 0; ci < ir->configCount; ci++) {
                if (ibranch.next->name == ir->configs[ci].name) {
                    branch->nextConfiguration = ci;
                }
            }

            // Fill in the operations for the current branch
            for (int oi = 0; oi < ibranch.opCount; oi++) {
                Operation *op = &branch->ops[oi];
                IOperation iop = ibranch.ops[oi];
                switch (iop.name) {
                    case 'N': {
                                  op->name = N;
                              } break;
                    case 'E': {
                                  op->name = E;
                              } break;
                    case 'P': {
                                  op->name = P;
                                  op->string = iop.string;
                              } break;
                    case 'R': {
                                  op->name = R;
                                  op->number = iop.number;
                                  if (iop.number == 0) {
                                      op->number = 1;
                                  }
                              } break;
                    case 'L': {
                                  op->name = L;
                                  if (iop.number == 0) {
                                      op->number = 1;
                                  }
                              } break;
                }
            }
        }
    }
    return m;
}

int main(int argc, char *argv[]) {
    Context c = {0};

    int timesToRun = -1;
    char *filename = 0;
    bool verbose = false;

    for (int i = 1; i < argc; ++i) {
        if (is_number(argv[i])) {
            char *endPtr;
            timesToRun = strtol(argv[i], &endPtr, 10);
        } else if (*argv[i] == '-') {
            if (*(argv[i] + 1) == 'v') {
                verbose = true;
            }

        } else if (!filename) {
            filename = argv[i];
        }
    }

    if (filename == 0) {
        error(&c, "no filename specified", FILE_ERROR);
    }

    if (timesToRun == -1) {
        error(&c, "please specify number of passes to make", FILE_ERROR);
    }

    char *bytecode = read_source(&c, filename);

    IR ir = {0};

    parse(&c, &ir, bytecode);
    Machine m = translate(&ir);

    char result[TAPE_LENGTH / 2];
    run_machine(&c, &m, timesToRun, result, verbose);
    handle_errors(&c);

    // Skip the '@'s in the tape during parsing of values
    int resultBegin = 0;
    for (int i; i < TAPE_LENGTH; i++) {
        if (result[i] == '@') {
            resultBegin++;
        } else {
            break;
        }
    }

    char *normalizedResult = &result[resultBegin];

    char stringResult[256];
    parse_string(stringResult, normalizedResult);

    float floatResult = parse_binary_point_value(normalizedResult);

    // Prints interpretations of the result
    printf("\n Binary:\t%s\n String:\t%s\n Float: \t%0.7f\n", result, stringResult,
            floatResult);

    return 0;
}
