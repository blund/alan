#define _CRT_SECURE_NO_WARNINGS 1
#if defined(_MSC_VER) // Check if we are using a windows compiler
#define strtok_r strtok_s
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <math.h>

#define TAPE_LENGTH 512
#define MAX_BRANCH_COUNT 8
#define MAX_OPERATION_COUNT 32
#define MAX_CONF 32
#define NONE ' '
#define ELSE 0xff
#define COMMENT_CHAR '!'
#define CONFIGURATION_COUNT MAX_CONF
#define CONFIGURATION_LENGTH 64

typedef enum Op { N = 0, P, E, R, L } Op;

typedef struct Operation {
    Op op;
    char parameter;
    char varParameter[64];

} Operation;

typedef struct Branch {
    char info[256];
    int matchSymbol;
    int nextConfiguration;
    Operation operations[MAX_OPERATION_COUNT];

} Branch;

typedef struct Configuration {
    char name[256];
    Branch branch[MAX_BRANCH_COUNT];

} Configuration;

typedef struct Machine {
    int pointer;
    char tape[TAPE_LENGTH];

    int configuration;
    int branch;
    Configuration configurations[MAX_CONF];

} Machine;

char *ReadSource(char *filename) {

    // https://www.tutorialspoint.com/cprogramming/c_file_io.htm
    FILE *file = fopen(filename,
            "rb");  // TODO Might be problematic outside of windows
    fseek(file, 0, SEEK_END);
    long fsize = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *bytecode = (char *)malloc(fsize + 1);
    fread(bytecode, fsize, 1, file);
    fclose(file);
    bytecode[fsize] = 0;  // Add line-terminator

    return bytecode;
}

float ParseBinaryPointValue(char *numstring) {
    float sum;
    char *c = numstring;
    float n = 1;

    while(*c != '\0') {
        float i = (float)(*c++-48);
        sum += i * 1.0f/powf(2.0f, n);
        n *= 2;
    }

    return sum;
}

char *ParseBinaryString(char *result, char *binary) {
    char *c = binary;
    int resultIndex = 0;
    while(*c != 0) {
        int n = 128;
        char sum = 0;
        for(int i = 0; i < 8; i++) {
            if(*c == 0) {
                break;
            }
            if(*c == '1') {
                sum += n * 1;
            }
            n /= 2;
            c++;
        }
        result[resultIndex++] = sum;
    }
    return result;
}
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

int findOrInsert(char strArray[CONFIGURATION_COUNT][CONFIGURATION_LENGTH], char *str) {
    for (int index = 0; index < CONFIGURATION_COUNT; index++) {
        if (strArray[index][0] == 0) {

            // We have found an empty spot,
            // which means the identifier is not in the list,
            // so we put it here
            // TODO handle bas size
            strcpy(strArray[index], str);
            return index;

        } else if (strcmp(strArray[index], str) == 0) {

            // The identifier already exists, so we return its return index
            return index;
        }
        continue;
    }
    assert(false);
    return -1;
}

int splitOn(char *slots[], char *text, char *delimiter) {
    char *context;

    int index = 0;
    char *unit = strtok_r(text, delimiter, &context);
    while (unit != NULL) {
        slots[index++] = unit;
        unit = strtok_r(NULL, delimiter, &context);
    }
    return index;
}

bool FindInString(char *string, char symbol) {
    char *c;
    for (c = string; *c != '\0'; c++) {
        if (*c == symbol) {
            return true;
        }
    }
    return false;
}



int IsNumber(char *string) {
    char *c;
    for (c = string; *c != '\0'; c++) {
        if (!isdigit((int)*c)) {
            return false;
        }
    }
    return true;
}

Machine Parse(char *code) {
    Machine m = {0};
    for (int i = 0; i < TAPE_LENGTH; i++) {
        m.tape[i] = ' ';
    }

    char configNames[CONFIGURATION_COUNT][CONFIGURATION_LENGTH] = {0};

    // Definer delimitere
    char newline[] = "\n";
    char commentDelim[] = "!";
    char configNameDelim[] = ":";
    char branchDelim[] = ";";
    char inBranchDelim[] = "|";
    char operationDelim[] = ",";

    char codeToParse[2048];
    // TODO test for bad size
    strcpy(codeToParse, code);

    char *lines[CONFIGURATION_LENGTH] = {0};
    int lineCount = splitOn(lines, codeToParse, newline);

    Configuration *c;
    int branchIndex = 0;
    for (int i = 0; i < lineCount; ++i) {
        char line[256];
        // TODO test for bad size
        strcpy(line, lines[i]);

        // Skip the line if it is a comment
        if (*line == COMMENT_CHAR) {
            continue;
        }
        if (*trim(line) == 0) {
            continue;
        }

        // Determine whether or not this line is the declaration of a new
        // configuration. If it is, create the new configuration in the
        // machine struct and change 'c' to refer to it.
        char *lineContext = line;
        if (FindInString(lines[i], ':')) {
            // Tokeniser de ulike dele av branchen
            // TODO test for bad size
            char *name = strtok_r(NULL, configNameDelim, &lineContext);
            name = trim(name);

            int configurationIndex = findOrInsert(configNames, name);
            c = &m.configurations[configurationIndex];

            // TODO test for bad size
            strcpy(c->name, name);


            // Reset branch index since we are in a new configuration
            branchIndex = 0;
        }
        // Increment branch index for next pass
        Branch *b = &c->branch[branchIndex++];

        // At this point, what is left in lineContext is
        // the information of the branch on the line
        //char *branchInfo = lineContext;

        // Copy the branch information into the branch, for
        // printing purposes..
        // TODO test for bad size
        strcpy(b->info, lineContext);

        char *symbol = strtok_r(NULL, inBranchDelim, &lineContext);
        symbol = trim(symbol);
        char *opsString = strtok_r(NULL, inBranchDelim, &lineContext);
        opsString = trim(opsString);
        char *next = strtok_r(NULL, inBranchDelim, &lineContext);
        next = trim(next);


        // Avgjør hva match-symbol skal vare, ta hensyn til definerte variabler
        if (strcmp(symbol, "none") == 0) {
            b->matchSymbol = NONE;
        } else if (strcmp(symbol, "else") == 0) {
            b->matchSymbol = ELSE;
        } else {
            b->matchSymbol = *symbol;
        }

        char *ops[MAX_OPERATION_COUNT] = {0};
        int opCount = splitOn(ops, opsString, operationDelim);

        for (int i = 0; i < opCount; ++i) {
            char *op = trim(ops[i]);
            if (*op == 'N') {
                b->operations[i].op = N;
            } else if (*op == 'P') {
                b->operations[i].op = P;
                strcpy(b->operations[i].varParameter, (op + 1));
            } else if (*op == 'E') {
                b->operations[i].op = E;
            } else if (*op == 'R') {
                b->operations[i].op = R;
                // TODO assert that param is a number
                char param = *(op + 1);
                if (param == ',' || param == 0) {
                    b->operations[i].parameter = 1;
                } else {
                    b->operations[i].parameter = param - 48;
                }
            } else if (*op == 'L') {
                b->operations[i].op = L;
                // TODO assert that param is a number
                char param = *(op + 1);
                if (param == ',' || param == 0) {
                    b->operations[i].parameter = 1;
                } else {
                    b->operations[i].parameter = param - 48;
                }
            }
        }

        // Sett inn index til neste konfigurasjon
        int index = findOrInsert(configNames, next);
        assert(index < 0xff);
        b->nextConfiguration = (uint8_t)index;
    }
    return m;
}
void Right(Machine *m, int count) {
    assert(m->pointer != TAPE_LENGTH);
    assert(count > 0 && count < (TAPE_LENGTH - m->pointer));
    m->pointer += count;
}

void Left(Machine *m, int count) {
    assert(m->pointer != 0);
    // TODO fix den under
    // assert(count > 0 && (TAPE_LENGTH - m->pointer) > count);
    m->pointer -= count;
}
// The operations the machine can perform on the tape.
void Print(Machine *m, char *sym) {
    assert(strlen(sym) > 0);
    if(strlen(sym) > 1) {
        while(*sym != 0) {
            m->tape[m->pointer] = *sym++;
            Right(m, 2);
        }
    } else {
        m->tape[m->pointer] = *sym;
    }
}

void Erase(Machine *m) { m->tape[m->pointer] = 0; }

char Read(Machine *m) { return m->tape[m->pointer]; }

void CopyN(size_t SourceACount, char *SourceA, size_t DestCount, char *Dest) {
    for (int Index = 0; Index < SourceACount; ++Index) {
        *Dest++ = *SourceA++;
    }
    *Dest++ = 0;
}

void PrintMachine(Machine *m, int topPointerAccessed, int lowerBound, int upperBound, bool verbose) {

    char outputBuffer[TAPE_LENGTH];  // Buffer used for printing
    char pointerBuffer[TAPE_LENGTH];

    int pointer = (m->pointer == 0) ? 1 : m->pointer;

    for (int i = 0; i <= pointer ; i++) {
        pointerBuffer[i] = ' ';
    }

    // On the first pass the pointer will be set to 0, but
    // we want it to point on a blank space in the tape
    strcpy(outputBuffer,  m->tape);
    outputBuffer[topPointerAccessed + 1] = '\0';

    pointerBuffer[pointer - lowerBound + 1] = 'v';
    pointerBuffer[pointer - lowerBound + 2] = '\0';

    // TODO test for bad size
    if ( lowerBound != -1 || upperBound != -1 ) {
        char boundBuffer[lowerBound + upperBound + 1];
        outputBuffer[upperBound] = 0;
        strcpy(boundBuffer,  outputBuffer+lowerBound);
        strcpy(outputBuffer, boundBuffer);

        outputBuffer[lowerBound+upperBound] = '\0';
    }

    if(verbose) {
        Configuration *c = &m->configurations[m->configuration];
        Branch *b = &c->branch[m->branch];

        char leftLimit = '[';
        char rightLimit = ']';
        if(upperBound < topPointerAccessed) {
            rightLimit = '>';
        }
        if(lowerBound > 0) {
            leftLimit = '<';
        }

        printf("> %s:%s\n%s\n%c%s%c\n\n\n", c->name, b->info, pointerBuffer, leftLimit, outputBuffer, rightLimit);
    } else {
        printf("%s \n[%s]\n\n", pointerBuffer, outputBuffer);
    }
}



void RunMachine(Machine *m, int iterations, char *result, bool verbose) {
    int topPointerAccessed = 1;  // Value used for determining how much to print

    int window = 48;
    int highBound = window;
    int lowBound = 0;

    while (iterations-- > 0) {
        assert(m->pointer <= TAPE_LENGTH);

        Configuration c = m->configurations[m->configuration];
        for (int branchIndex = 0; branchIndex < MAX_BRANCH_COUNT; branchIndex++) {
            char symbol = m->tape[m->pointer];
            Branch branch = c.branch[branchIndex];

            // Here we are checking whether or not we are in the correct branch
            // if we are not, we will go on to the next iteration.
            // If we are, we will execute the branch and move on to the next
            // configuration
            if (branch.matchSymbol != ELSE && branch.matchSymbol != symbol) {
                continue;
            }
            // For printing purposes
            m->branch = branchIndex;

            // Execute all operations in branch until a N
            bool nop = false;
            for (int operationIndex = 0; operationIndex < MAX_OPERATION_COUNT;
                    ++operationIndex) {
                Operation operation = branch.operations[operationIndex];
                switch (operation.op) {
                    case N: {
                                nop = true;
                            } break;
                    case P: {
                                Print(m, operation.varParameter);
                            } break;
                    case E: {
                                Erase(m);
                            } break;
                    case R: {
                                Right(m, operation.parameter);
                            } break;
                    case L: {
                                Left(m, operation.parameter);
                            } break;
                }
                if (nop) {
                    break;
                }
            }

            // Change topPointerAccessed if we have
            // touched a higher pointer.
            // This is for printing purposes.
            if (m->pointer > topPointerAccessed) {
                topPointerAccessed = m->pointer;
            }
            // Adjust the window of the tape to print
            // if we move out of the defined boundaries
            if ( m->pointer >= highBound || m->pointer <= lowBound) {
                if (topPointerAccessed - m->pointer >= window/2) {
                    printf("more");
                    highBound = m->pointer + window/2 ;
                    lowBound  = m->pointer - window/2 ;
                } else {
                    highBound = topPointerAccessed + 1;
                    lowBound = highBound - window ;
                }
            }

            if(verbose) {
                PrintMachine(m, topPointerAccessed, lowBound, highBound, true);
            }
            m->configuration = branch.nextConfiguration;
            break;
        }
    }

    // Here we want to print the result of the computation
    // into a buffer for printing.
    // We do this by writing every second value from the turing
    // machine's tape into the result buffer (following turing's
    // conventions).

    int maxIndex = 2*floor(topPointerAccessed/2) + 2;
    int resultIndex;
    for(int tapeIndex = 0; tapeIndex < maxIndex; tapeIndex += 2) {
        result[resultIndex++] = m->tape[tapeIndex];
    }

}

int main(int argc, char *argv[]) {
    int timesToRun = -1;
    char *filename = 0;
    bool verbose = false;

    for (int i = 1; i < argc; ++i) {
        if (IsNumber(argv[i])) {
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
        printf("%s\n", "Error: no filename specified");
        return 1;
    }

    if (timesToRun == -1) {
        printf("%s\n", "Error: please specify number of passes to make");
        return 1;
    }

    char result[256] = {0};

    char *bytecode = ReadSource(filename);
    Machine m = Parse(bytecode);

    RunMachine(&m, timesToRun, result, verbose);

    char stringResult[256];
    ParseBinaryString(stringResult, result);

    float floatResult = ParseBinaryPointValue(result);

    // Print interpretations of the result
    printf("\nBinary:\t%s\nString:\t%s\nFloat:\t%f\n", result, stringResult, floatResult);


    return 0;
}
