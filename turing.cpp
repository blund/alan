
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <cassert>
#include <iostream>

#define TAPE_LENGTH 128
#define MAX_BRANCH_COUNT 8
#define MAX_OPERATION_COUNT 8
#define MAX_CONF 8
#define NONE ' '
#define ELSE 0xff
#define COMMENT_CHAR '!'
#define CONFIGURATION_COUNT 32
#define CONFIGURATION_LENGTH 32

enum Op { N = 0, P, E, R, L };

struct Operation {
  Op op;
  uint8_t parameter;
};

struct Branch {
  int matchSymbol;
  int nextConfiguration;
  Operation operations[MAX_OPERATION_COUNT];
};

struct Configuration {
  Branch branch[MAX_BRANCH_COUNT];
};

struct Machine {
  int pointer;
  char tape[TAPE_LENGTH];

  int configuration;
  Configuration configurations[MAX_CONF];
};

char *trim(char *str) {
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

// uint8_t findOrInsert(char *(*strArray), char *str) {
int findOrInsert(char strArray[][CONFIGURATION_COUNT], char *str) {
  // TODO Hardkodet størrelse på array
  for (int index = 0; index < CONFIGURATION_COUNT; index++) {
    if (strArray[index][0] == 0) {
      // We have found an empty spot,
      // meaning we have not found our identifier
      // and can safely put it here
      assert(strcpy_s(strArray[index], sizeof(strArray[index]), str) == 0);
      return index;
    } else if (strcmp(strArray[index], str) == 0) {
      // identifier already exists, return index
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
  char *unit = strtok_s(text, delimiter, &context);
  while (unit != NULL) {
    slots[index++] = unit;
    unit = strtok_s(NULL, delimiter, &context);
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

bool StringIsWhitespace(char *string) {
  char *c;
  for (c = string; *c != '\0'; c++) {
    if (*c != ' ' || *c != '\t' || *c != '\t' || *c != '\n') {
      return false;
    }
  }
  return true;
}

Machine Parse(char *code) {
  // TODO Hardkodet størrelse

  char codeToParse[265];
  assert(strcpy_s(codeToParse, sizeof(codeToParse), code) == 0);

  Machine m = {};
  for (int i = 0; i < TAPE_LENGTH; i++) {
    m.tape[i] = ' ';
  }

  char configNames[CONFIGURATION_COUNT][CONFIGURATION_LENGTH] = {};

  // Definer delimitere
  char newline[] = "\n";
  char commentDelim[] = "!";
  char configNameDelim[] = ":";
  char branchDelim[] = ";";
  char inBranchDelim[] = "|";
  char operationDelim[] = ",";

  char *lines[CONFIGURATION_LENGTH] = {};
  int lineCount = splitOn(lines, codeToParse, newline);

  Configuration *c;
  int branchIndex = 0;
  for (int i = 0; i < lineCount; ++i) {
    char line[128];
    assert(strcpy_s(line, sizeof(line), lines[i]) == 0);

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
      char *name = strtok_s(NULL, configNameDelim, &lineContext);
      name = trim(name);

      int actualIndex = findOrInsert(configNames, name);
      c = &m.configurations[actualIndex];  // TODO shitty navn

      branchIndex = 0; // Reset branch index since we are in a new configuration
    }

    Branch *b = &c->branch[branchIndex++]; // Increment branch index for next pass

    char toParse[265];
    assert(strcpy_s(toParse, sizeof(toParse), line) == 0);

    char *symbol = strtok_s(NULL, inBranchDelim, &lineContext);
    symbol = trim(symbol);
    char *opsString = strtok_s(NULL, inBranchDelim, &lineContext);
    opsString = trim(opsString);
    char *next = strtok_s(NULL, inBranchDelim, &lineContext);
    next = trim(next);

    // Avgjør hva match-symbol skal vare, ta hensyn til definerte variabler
    if (strcmp(symbol, "none") == 0) {
      b->matchSymbol = NONE;
    } else if (strcmp(symbol, "else") == 0) {
      b->matchSymbol = ELSE;
    } else {
      b->matchSymbol = (uint8_t)*symbol;
    }

    char *ops[MAX_OPERATION_COUNT] = {};
    int opCount = splitOn(ops, opsString, operationDelim);

    for (int i = 0; i < opCount; ++i) {
      char *op = trim(ops[i]);
      if (*op == 'N') {
        b->operations[i].op = N;
      } else if (*op == 'P') {
        b->operations[i].op = P;
        b->operations[i].parameter = *(op + 1);
      } else if (*op == 'E') {
        b->operations[i].op = E;
      } else if (*op == 'R') {
        b->operations[i].op = R;
      } else if (*op == 'L') {
        b->operations[i].op = L;
      }
    }

    // Sett inn index til neste konfigurasjon
    int index = findOrInsert(configNames, next);
    assert(index < 0xff);
    b->nextConfiguration = (uint8_t)index;
  }
  return m;
}

// The operations the machine can perform on the tape.
// TODO Assert m->tape[m->pointer] != (0 || 1)
inline void Print(Machine *m, char sym) { m->tape[m->pointer] = sym; }

inline void Erase(Machine *m) { m->tape[m->pointer] = 0; }

inline char Read(Machine *m) { return m->tape[m->pointer]; }

// TODO Assert m->pointer != TAPE_LENGTH;
inline void Right(Machine *m) { ++m->pointer; }

// TODO Assert m->pointer != 0;
inline void Left(Machine *m) { --m->pointer; }

void RunMachine(Machine *m, int iterations) {
  char outputBuffer[2 * TAPE_LENGTH];  // Buffer used for printing
  char pointerBuffer[TAPE_LENGTH +
                     1];  // Buffer used for displaying where the pointer is
  int topPointerAccessed = 0;  // Value used for determining how much to print

  // Print initial condition before beginning the iteration
  for (int i = 0; i < TAPE_LENGTH; i++) {
    pointerBuffer[i] = ' ';
  }
  pointerBuffer[m->pointer + 1] = 'v';
  pointerBuffer[m->pointer + 2] = '\0';
  assert(strcpy_s(outputBuffer, sizeof(outputBuffer), m->tape) == 0);
  outputBuffer[topPointerAccessed] = '\0';
  std::cout << pointerBuffer << "\n[" << outputBuffer << " ]\n" << std::endl;

  while (iterations-- > 0) {
    // TODO Assert at pointer er innenfor TAPE_LENGTH
    Configuration c = m->configurations[m->configuration];
    for (int branchIndex = 0; branchIndex < MAX_BRANCH_COUNT; ++branchIndex) {
      char symbol = m->tape[m->pointer];
      Branch branch = c.branch[branchIndex];

      if (branch.matchSymbol != ELSE && branch.matchSymbol != symbol) {
        continue;
      }

      // Set configuration for next iteration
      m->configuration = branch.nextConfiguration;

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
            Print(m, operation.parameter);
          } break;
          case E: {
            Erase(m);
          } break;
          case R: {
            Right(m);

            // Change topPointerAccessed if we have
            // touched a higher pointer.
            // This is for printing purposes.
            if (m->pointer > topPointerAccessed) {
              topPointerAccessed = m->pointer;
            }
          } break;
          case L: {
            Left(m);
          } break;
        }

        if (nop) {
          break;
        }
      }

      //  Since we have found the correct branch, computaion
      // for this m-configuration is completed.
      //  This means we can print the tape state
      // and move on to the next m-configuration

      // TODO change this to only copy touched parts of memory
      // Copy memory to a temporary buffer,
      // then add a string terminator at the index
      // after the highest pointer accessed, to only
      // print that.
      for (int i = 0; i < TAPE_LENGTH; i++) {
        pointerBuffer[i] = ' ';
      }
      pointerBuffer[m->pointer + 1] = 'v';
      pointerBuffer[m->pointer + 2] = '\0';
      assert(strcpy_s(outputBuffer, sizeof(outputBuffer), m->tape) == 0);
      outputBuffer[topPointerAccessed] = '\0';
      std::cout << pointerBuffer << "\n[" << outputBuffer << " ]\n"
                << std::endl;

      break;
    }
  }
}

char *ReadSource(char *filename) {
  FILE *file;
  fopen_s(&file, filename,
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

int IsNumber(char *string) {
  char *c;
  for (c = string; *c != '\0'; c++) {
    if (!isdigit((int)*c)) {
      return false;
    }
  }
  return true;
}

int main(int argc, char *argv[]) {
#if 0
    int timesToRun = -1;
    char *filename = 0;
    char tmp[] = "test";

    for (int i = 1; i < argc; ++i) {
        if (IsNumber(argv[i])) {
            char *endPtr;
            timesToRun = strtol(argv[i], &endPtr, 10);
        } else if (!filename) {
            filename = argv[i];
        }
    }

    if (filename == 0) {
        std::cout << "Error: no filename specified" << std::endl;
        return 1;
    }
    if (timesToRun == -1) {
        std::cout << "Error: please specify number of passes to make" << std::endl;
        return 1;
    }
#else
  char filename[] = "test.tur";
  int timesToRun = 8;
#endif

  char *bytecode = ReadSource(filename);
  Machine m = Parse(bytecode);
  RunMachine(&m, timesToRun);

  return 0;
}
