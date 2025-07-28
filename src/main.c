#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

enum Token {
    TOK_EOF = -1,
    TOK_EXTERN = -2,
    TOK_IDENTIFIER = -3,
    TOK_STRING = -4,
    TOK_NUMBER = -5
};

static char TokenVal[1024]; // Buffer the text value of the token
static FILE* SourceFile = NULL;

static int getToken() {
    static int lastChar = ' ';
    while(isspace(lastChar)) lastChar = fgetc(SourceFile);
    
    if (isalpha(lastChar)) {
        TokenVal[0] = lastChar;
        int ix = 1;
        while(isalnum((lastChar = fgetc(SourceFile)))) TokenVal[ix++] = lastChar;
        TokenVal[ix] = '\0';
        if (strcmp(TokenVal, "extern") == 0) return TOK_EXTERN;
        return TOK_IDENTIFIER;
    }

    if (isdigit(lastChar) || lastChar == '.') {
        int ix = 0;
        if (lastChar == '-') {
            TokenVal[ix++] = lastChar;
            lastChar = fgetc(SourceFile);
        }
        while(isdigit(lastChar) || lastChar == '.') {
            TokenVal[ix++] = lastChar;
            lastChar = fgetc(SourceFile);
        }
        TokenVal[ix] = '\0';
        return TOK_NUMBER;
    }
    
    if (lastChar == '"') {
        int ix = 0;
        while ((lastChar = fgetc(SourceFile)) != '"' && lastChar != EOF) {
            if (ix < sizeof(TokenVal) - 1) {
                TokenVal[ix++] = lastChar;
            }
        }
        TokenVal[ix] = '\0';
        lastChar = fgetc(SourceFile); // Consume the closing quote
        return TOK_STRING;
    }

    if (lastChar == '#') {
        // Skip comments
        while ((lastChar = fgetc(SourceFile)) != EOF && lastChar != '\n');
        return getToken(); // Recurse to get the next token
    }

    if (lastChar == EOF) return TOK_EOF;
        
    int thisChar = lastChar;
    lastChar = fgetc(SourceFile);
    return thisChar; 
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    const char* filename = argv[1];
    SourceFile = fopen(filename, "r");
    if (!SourceFile) {
        perror("Error opening file");
        return 1;
    }

    int token;
    while ((token = getToken()) != TOK_EOF) {
        switch (token) {
            case TOK_EXTERN:
                printf("Found extern\n");
                break;
            case TOK_IDENTIFIER:
                printf("Found identifier: %s\n", TokenVal);
                break;
            case TOK_STRING:
                printf("Found string: %s\n", TokenVal);
                break;
            case TOK_NUMBER:
                printf("Found number: %f\n", strtod(TokenVal, NULL));
                break;
            default:
                fprintf(stderr, "Unknown token: %d %c\n", token, token);
                break;
        }
    }   

    

    fclose(SourceFile);

    return 0;
}