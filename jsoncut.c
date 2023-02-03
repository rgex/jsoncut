#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define IN_JSON_START 0
#define IN_JSON_KEY 1
#define IN_JSON_ARRAY_VAL 2
#define IN_JSON_OBJECT_VAL 3
#define IN_JSON_OBJECT 4
#define IN_JSON_ARRAY 5

#define MAXIMUM_DEPTH 96
#define MAXIMUM_KEY_LENGTH 512
#define BUFSIZE 1048576

#define CHECK_MAXIMUM_DEPTH_MACRO \
        if(stackSize >= MAXIMUM_DEPTH) { \
            printf("MAXIMUM DEPTH reached, aborting!\n"); \
            return 0; \
        }

void stackToPointerString(char* ret, char ** keyStack, int keyStackSize) {
    int cursor = 0;
    for(int i = 0; i < keyStackSize; i++) {
        strcpy(ret + cursor, keyStack[i]);
        cursor += strlen(keyStack[i]);
        memcpy(ret + cursor, "/", 1);
        cursor++;
    }
    strcpy(ret + cursor, "\0");
}

void pointerToStack(char* pointerString, char** pointerStack, int* pointerStackSize) {
    int j = 0;
    int cursor = 0;
    for(int i=0; pointerString[i] != '\0'; i++){
        if(pointerString[i] == '/' && pointerString[i + 1] == '\0') {
            break;
        } else if(pointerString[i] == '/') {
            pointerStack[j][cursor] = '\0';
            cursor = 0;
            j++;
        } else {
            pointerStack[j][cursor] = pointerString[i];
            cursor++;
        }
    }

    *pointerStackSize = j + 1;
}

int stackCompare(char** stack1, int stack1Size, char** stack2, int stack2Size) {
    if(stack1Size != stack2Size) {
        return -1;
    }
    for(int i = 0; i < stack1Size; i++) {
        if(strcmp(stack1[i], stack2[i]) != 0) {
            return -1;
        }
    }
    return 0;
}

u_int8_t getCurrentState(u_int8_t *stateStack, int stateStackSize) {
    if(stateStackSize == 0) {
		return IN_JSON_START;
	}
    return stateStack[stateStackSize - 1];
}

int main(int argc, char *argv[]) {
    char buf[BUFSIZE];

    if(argc < 3) {
        printf("Too few arguments, format is ./jsoncut file.json key1/key2");
        return 0;
    } else if(argc > 3) {
        printf("Too many arguments, format is ./jsoncut file.json key1/key2");
        return 0;
    }

    FILE *f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* pointer = argv[2];

    char **pointerStack = (char**) calloc((MAXIMUM_DEPTH + 4), sizeof(char*));
    for (int i = 0; i < (MAXIMUM_DEPTH + 4); i++ )
    {
        pointerStack[i] = (char*) calloc(MAXIMUM_KEY_LENGTH, sizeof(char));
    }
    int pointerStackSize;
    pointerToStack(argv[2], pointerStack, &pointerStackSize);

/*
    char* ret = malloc(MAXIMUM_KEY_LENGTH * 16);
    stackToPointerString(ret, pointerStack, pointerStackSize);
    printf("RET: %s \n", ret);
    return 0;
*/

    char* pointer2 = malloc(MAXIMUM_KEY_LENGTH*16);

    int currentState = IN_JSON_START;
	int startCut = -1;
	int endCut = -1;

    u_int8_t *stateStack = malloc((MAXIMUM_DEPTH + 4)*sizeof(u_int8_t));
    int stackSize = 0;

    char **keyStack = (char**) calloc((MAXIMUM_DEPTH + 4), sizeof(char*));
    for (int i = 0; i < (MAXIMUM_DEPTH + 4); i++ )
    {
        keyStack[i] = (char*) calloc(MAXIMUM_KEY_LENGTH, sizeof(char));
    }

    char *currentRead = malloc(MAXIMUM_KEY_LENGTH + 2);
    int currentReadLength = 0;

    char currentChar = 0;
    char prevChar = 0;

    int readSize = BUFSIZE;

    for(int x = 0; x < fsize; x += BUFSIZE) {

        if(startCut != -1) {
            printf("%.*s", readSize - startCut , buf + startCut);
            startCut = 0;
        }

        if(fsize - x < readSize) {
            readSize = fsize - x;
        }

        fread(buf, readSize, 1, f);

        for(int i = 0; i < readSize; i++) {
            switch(currentState) {
                case IN_JSON_START: 
                    if(buf[i] == '}') {
                        stackSize--;
                        currentState = getCurrentState(stateStack, stackSize);
                    } else if(buf[i] == ',' && getCurrentState(stateStack, stackSize) == IN_JSON_OBJECT) {
                        stackSize--;
                        currentState = getCurrentState(stateStack, stackSize);
                    } else if(buf[i] == '{') {
                        currentState = IN_JSON_OBJECT;
                    } else if(buf[i] == '[') {
                        currentState = IN_JSON_ARRAY;
                        stateStack[stackSize] = IN_JSON_ARRAY;
                        memcpy(keyStack[stackSize], "(array)\0", 8);
                        stackSize++;
                        CHECK_MAXIMUM_DEPTH_MACRO
                    }
                break;
                case IN_JSON_OBJECT:
                    if(buf[i] == ':') {
                        //stackToPointerString(pointer2, keyStack, stackSize);
                        if(stackCompare(pointerStack, pointerStackSize, keyStack, stackSize) == 0) {
                            startCut = i + 1;
                        }
                    } else if(buf[i] == '"' && (prevChar == '{' || prevChar == ',')) {
                        currentState = IN_JSON_KEY;
                    } else if(buf[i] == '"' && prevChar == ':') {
                        currentState = IN_JSON_OBJECT_VAL;
                    } else if(buf[i] == '[' && prevChar == ':') {
                        currentState = IN_JSON_ARRAY;
                        stateStack[stackSize] = IN_JSON_ARRAY;
                        memcpy(keyStack[stackSize], "(array)\0", 8);
                        stackSize++;
                        CHECK_MAXIMUM_DEPTH_MACRO
                    } else if(buf[i] == '}') {
                        stackSize--;
                        currentState = getCurrentState(stateStack, stackSize);

                        //stackToPointerString(pointer2, keyStack, stackSize);
                        if(stackCompare(pointerStack, pointerStackSize, keyStack, stackSize) == 0) {
                            endCut = i + 1;
                            printf("%.*s", endCut - startCut , buf + startCut);
                            free(keyStack);
                            return 0;
                            // to do exit;
                        }
                    } else if(buf[i] == ',') {
                        stackSize--;
                    }
                break;
                case IN_JSON_KEY:
                    if(buf[i] == '"' && prevChar != '\\') {
                        currentState = IN_JSON_OBJECT;
                        memcpy(keyStack[stackSize], currentRead, currentReadLength);
                        strcpy(keyStack[stackSize] + currentReadLength, "\0");
                        stateStack[stackSize] = IN_JSON_OBJECT;
                        stackSize++;
                        CHECK_MAXIMUM_DEPTH_MACRO
                        currentReadLength = 0;
                    } else {
                        currentRead[currentReadLength] = buf[i];
                        currentReadLength++;
                    }
                break;
                case IN_JSON_OBJECT_VAL:
                    if(buf[i] == '"' && prevChar != '\\') {
                        currentState = IN_JSON_OBJECT;
                    }
                break;
                case IN_JSON_ARRAY:
                    if(buf[i] == '"' && (prevChar == '[' || prevChar == ',')) {
                        currentState = IN_JSON_ARRAY_VAL;
                    } else if(buf[i] == '{' && (prevChar == '[' || prevChar == ',')) {
                        currentState = IN_JSON_OBJECT;
                    } else if(buf[i] == ']') {
                        stackSize--;
                        currentState = getCurrentState(stateStack, stackSize);
                    } else if(buf[i] == '[' && (prevChar == '[' || prevChar == ',')) {
                        stateStack[stackSize] = IN_JSON_ARRAY;
                        memcpy(keyStack[stackSize], "(array)\0", 8);
                        stackSize++;
                        CHECK_MAXIMUM_DEPTH_MACRO
                    }
                break;
                case IN_JSON_ARRAY_VAL:
                        if(buf[i] == '"' && prevChar != '\\') {
                                currentState = IN_JSON_ARRAY;
                        }
                break;
            }

            prevChar = buf[i];
        }
    }
}
