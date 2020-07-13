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

#define TAPE_LENGTH 256
#define MAX_BRANCH_COUNT 32
#define MAX_OPERATION_COUNT 16
#define MAX_CONF_COUNT 32
#define MAX_ERROR 8

#define NONE ' '
#define COMMENT_CHAR '!'
#define ELSE 0xff

#define WINDOWSIZE 32

#define ARGUMENT_ERROR -1
#define FILE_ERROR -2

#define NOT_DEFINED -1

/*
 * Here we define the structures that encompass the
 * bytecode that the source will be translated to,
 * and which will be executed on the virtual
 * machine.
 */

typedef enum Op { N = 0, P, E, R, L } Op;

// TODO
// https://en.wikipedia.org/wiki/Flexible_array_member

typedef struct Operation {
  Op op;
  union {
    char *string;
    int number;
  } parameter;
} Operation;

typedef struct Branch {
  char matchSymbol;
  int nextConfiguration;
  Operation operations[MAX_OPERATION_COUNT];
} Branch;

typedef struct Configuration {
  Branch branch[MAX_BRANCH_COUNT];
} Configuration;

typedef struct Machine {
  int pointer;
  char tape[TAPE_LENGTH];

  int configuration;
  int branch;
  Configuration configurations[MAX_CONF_COUNT];

} Machine;

/*
 * Here we define structures used for the intermediary representation
 * of the turing program, which helps us detect errors at parse-time,
 * and allows us to print detailed information at runtime.
 */
typedef struct IOperation {
  char op;
  bool isnum;  // True if num, false if string
  union {
    char *string;
    int number;
  } parameter;
} IOperation;

typedef struct IBranch {
  int definedOn;
  char *matchSymbol;
  IConfig *next;
  IOperation ops[MAX_OPERATION_COUNT];
} IBranch;

typedef struct IConfig {
  int definedOn;
  bool defined;
  char *name;
  IBranch branches[MAX_BRANCH_COUNT];
} IConfig;

typedef struct IR {
  IConfig configs[MAX_CONF_COUNT];
} IR;

typedef struct Error {
  char *message;
  int line;
} Error;

typedef struct Context {
  IR parseInfo;

  bool error;
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

void error(Context *c, char *msg, int line) {
  c->error = true;
  if (c->nextError < MAX_ERROR) {
    // TODO bound check and assert
    c->errors[c->nextError].message = msg;
    c->errors[c->nextError].line = line;
    c->nextError++;
  } else {
    c->errorOverflow = true;
  }
}

void handle_errors(Context *c) {
  for (int i = 0; i < c->nextError; i++) {
    int line = c->errors[i].line;
    char *msg = c->errors[i].message;
    if (line == FILE_ERROR) {
      fprintf(stderr, "\n\tFile error:\n\t  %s\n", msg);
    } else if (line == ARGUMENT_ERROR) {
      fprintf(stderr, "\n\tArgument error:\n\t  %s\n", msg);
    } else {
      fprintf(stderr, "\n\tError in line %i:\n\t  %s\n", ++line, msg);
    }
  }
  if (c->error) {
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
  float n = 1;

  while (*c != '\0') {
    float i = (float)(*c++ - 48);
    sum += i * 1.0f / powf(2.0f, n);
    n *= 2;
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
      // TODO handle bas size
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
    if(array[i].name == NULL) {
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
    if (islower((int)*c) || *c == ' ') {
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
  char inBranchDelim[] = "|";
  char operationDelim[] = ",";

  char *lines[MAX_CONF_COUNT] = {0};
  int lineCount = split_on(lines, code, newline);
  assert(lineCount < MAX_CONF_COUNT);

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
      char *name = strtok_r(NULL, configNameDelim, &lineContext);
      name = trim(name);
      if (is_empty(lineContext) && !name) {
        error(c, "missing configuration name", currentLine);
        lineContext = name;
      };

      if (!legal_config_name(name)) {
        error(c, "configuration name must be all lower case", currentLine);
      }

      int prevIndex = find_config(ir->configs, name);
      if (ir->configs[prevIndex].defined) {
        error(c, "redefinition of configuration", currentLine);
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
      error(c, "missing configuration name (declare like 'begin: ...'",
            currentLine);
      continue;
    }
    if (is_empty(lineContext)) {
      error(c, "configuration is missing parameters", currentLine);
      continue;
    }

    // Increment branch index for next pass
    IBranch *branch = &conf->branches[branchIndex];
    branch->definedOn = currentLine;

    char *symbol = strtok_r(NULL, inBranchDelim, &lineContext);
    symbol = trim(symbol);
    branch->matchSymbol = symbol;

    for (int i = 0; i < branchIndex; i++) {
      if (strcmp(conf->branches[i].matchSymbol, symbol) == 0) {
        error(c, "branch for given symbol is already defined", currentLine);
      }
    }
    branchIndex++;

    if (is_empty(symbol)) {
      error(c, "branch is missing match symbol (first parameter)", currentLine);
    };

    char *opsString = strtok_r(NULL, inBranchDelim, &lineContext);
    opsString = trim(opsString);
    // TODO set ops for a branch..
    // strcpy(branch->ops, opsString);

    char *nextName = strtok_r(NULL, inBranchDelim, &lineContext);
    nextName = trim(nextName);

    bool hasNext = true;
    if (is_empty(nextName)) {
      error(c, "branch is missing next configuration (last parameter)",
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

    // TODO TODO TODO this is the translation to bytecode, must be done later
    /*
    if (strcmp(symbol, "none") == 0) {
      branch->matchSymbol = NONE;
    } else if (strcmp(symbol, "else") == 0) {
      branch->matchSymbol = ELSE;
    } else {
      branch->matchSymbol = *symbol;
    }

    // TODO Error handling during this parsing...
    // TODO Parameter is no longer varParameter, but
    // parameter.num or parameter.string
    // TODO Also handle bool op.isnum
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
      } else {
        // TODO Error: invalid function name.
      }
    }
    */
  }
  // Iterate over branches of configs and gooo
  /* TODO TODO TODO
  for (int ci = 0; ci < MAX_CONF_COUNT; ci++) {
    char name = ir.configs[ci].name[0];
    bool defined = ir.configs[ci].defined;
    if (name && !defined) {
      error(c, "a configuration is not properly defined", 0);
    }
  }
  */

  /*
  // TODO actually make the bytecode.
  Machine m = {0};
  for (int i = 0; i < TAPE_LENGTH; i++) {
    m.tape[i] = ' ';
  }
  return m;
  */
  // TODO handle_errors(c);
  return ir;
}

void right(Machine *m, int count) {
  assert(m->pointer != TAPE_LENGTH);
  assert(count > 0 && count < (TAPE_LENGTH - m->pointer));
  m->pointer += count;
}

void left(Machine *m, int count) {
  assert(m->pointer != 0);
  // TODO fix den under
  // assert(count > 0 && (TAPE_LENGTH - m->pointer) > count);
  m->pointer -= count;
}
// The operations the machine can perform on the tape.
void print(Machine *m, char *sym) {
  assert(strlen(sym) > 0);
  if (strlen(sym) > 1) {
    while (*sym != 0) {
      m->tape[m->pointer] = *sym++;
      right(m, 2);
    }
  } else {
    m->tape[m->pointer] = *sym;
  }
}

void erase(Machine *m) { m->tape[m->pointer] = 0; }

char read(Machine *m) { return m->tape[m->pointer]; }

void copy_n(size_t SourceACount, char *SourceA, size_t DestCount, char *Dest) {
  for (int Index = 0; Index < SourceACount; ++Index) {
    *Dest++ = *SourceA++;
  }
  *Dest++ = 0;
}

/*
void PrintMachine(Machine *m, int topPointerAccessed, int lowerBound,
                  int upperBound, bool verbose) {
  char outputBuffer[TAPE_LENGTH];  // Buffer used for printing
  char pointerBuffer[TAPE_LENGTH];

  int pointer = (m->pointer == 0) ? 1 : m->pointer;

  for (int i = 0; i <= pointer; i++) {
    pointerBuffer[i] = ' ';
  }

  // On the first pass the pointer will be set to 0, but
  // we want it to point on a blank space in the tape
  strcpy(outputBuffer, m->tape);
  outputBuffer[topPointerAccessed + 1] = '\0';

  pointerBuffer[pointer - lowerBound + 1] = 'v';
  pointerBuffer[pointer - lowerBound + 2] = '\0';

  // TODO test for bad size
  if (lowerBound != -1 || upperBound != -1) {
    char boundBuffer[WINDOWSIZE + 1];
    outputBuffer[upperBound] = 0;
    strcpy(boundBuffer, outputBuffer + lowerBound);
    strcpy(outputBuffer, boundBuffer);

    outputBuffer[lowerBound + upperBound] = '\0';
  }

  if (verbose) {
    Configuration *c = &m->configurations[m->configuration];
    Branch *b = &c->branch[m->branch];

    char leftLimit = '[';
    char rightLimit = ']';
    if (upperBound < topPointerAccessed) {
      rightLimit = '>';
    }
    if (lowerBound > 0) {
      leftLimit = '<';
    }

    printf("> %s:%s | %s | %s\n%s\n%c%s%c\n\n\n", c->name, b->symbol, b->ops,
           b->next, pointerBuffer, leftLimit, outputBuffer, rightLimit);
  } else {
    printf("%s \n[%s]\n\n", pointerBuffer, outputBuffer);
  }
}
*/

void run_machine(Context *context, Machine *m, int iterations, char *result, bool verbose) {
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
            print(m, operation.parameter.string);
          } break;
          case E: {
            erase(m);
          } break;
          case R: {
            right(m, operation.parameter.number);
          } break;
          case L: {
            left(m, operation.parameter.number);
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
      if (m->pointer >= highBound || m->pointer <= lowBound) {
        if (topPointerAccessed - m->pointer >= window / 2) {
          printf("more");
          highBound = m->pointer + window / 2;
          lowBound = m->pointer - window / 2;
        } else {
          highBound = topPointerAccessed + 1;
          lowBound = highBound - window;
        }
      }

      if (verbose) {
        // PrintMachine(m, topPointerAccessed, lowBound, highBound, true);
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

  int maxIndex = 2 * (int)floor(topPointerAccessed / 2) + 2;
  int resultIndex = 0;
  for (int tapeIndex = 0; tapeIndex < maxIndex; tapeIndex += 2) {
    result[resultIndex++] = m->tape[tapeIndex];
  }
}

Machine translate(IR ir) {
  /*
   * Iterer over konfigurasjoner i ir.configs
   * For hver, legg til alle branches i ir.configs[i].branches
   * For hver branch, legg til alle operasjoner.
   * Legg til 'next' sin indeks ved find(ir.configs[i].next.name);
   *
   */
  Machine m;
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

//  char result[256] = {0};

  char *bytecode = read_source(&c, filename);

  IR ir = {0}; //(IR *)malloc(sizeof(IR));

  parse(&c, &ir, bytecode);
  // Machine m = translate(ir);
/*  run_machine(&c, &m, timesToRun, result, verbose);

   char stringResult[256];
  parse_string(stringResult, result);

  float floatResult = parse_binary_point_value(result);

  // Print interpretations of the result
  printf("\nBinary:\t%s\nString:\t%s\nFloat:\t%f\n", result, stringResult,
         floatResult);
         */

  return 0;
}
