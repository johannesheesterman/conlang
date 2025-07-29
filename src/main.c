#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int CurrentToken = 0; // Current token type
static char TokenVal[1024]; // Buffer the text value of the token
static FILE* SourceFile = NULL;

enum Token {
    TOK_EOF = -1,
    TOK_EXTERN = -2,
    TOK_IDENTIFIER = -3,
    TOK_STRING = -4,
    TOK_NUMBER = -5
};

typedef struct AST AST;
struct AST {
    enum { AST_NUMBER, AST_STRING, AST_VARIABLE, AST_BINARY, AST_CALL, AST_EXTERN } type;
    union {
        struct AST_NUMBER { int value; } AST_NUMBER;
        struct AST_STRING { char* value; } AST_STRING;
        struct AST_VARIABLE { char* name; } AST_VARIABLE;
        struct AST_BINARY { AST* left;  AST* right; char op; } AST_BINARY;
        struct AST_CALL { char* callee; AST** args; int arg_count; } AST_CALL;
        struct AST_EXTERN { char* name; } AST_EXTERN;
    } data;
};

// Forward declarations for parser functions
static struct AST* ParseNumber();
static struct AST* ParseString();
static struct AST* ParseParenExpr();
static struct AST* ParseIdentifier();
static struct AST* ParsePrimary();

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
        while ((lastChar = fgetc(SourceFile)) != EOF && lastChar != '\n');
        return getToken();
    }

    if (lastChar == EOF) return TOK_EOF;
        
    int thisChar = lastChar;
    lastChar = fgetc(SourceFile);
    return thisChar; 
}

static int getNextToken() {
    return CurrentToken = getToken();
}

static AST* logError(const char* msg) {
    fprintf(stderr, "Error: %s\n", msg);
    return NULL;
}

AST *ast_new(AST ast) {
  AST *ptr = malloc(sizeof(AST));
  if (ptr) *ptr = ast;
  return ptr;
}   

#define AST_NEW(tag, ...) \
  ast_new((AST){tag, {.tag=(struct tag){__VA_ARGS__}}})

static struct AST* ParseNumber() {
    int value = strtol(TokenVal, NULL, 10);
    struct AST* node = AST_NEW(AST_NUMBER, value);
    getNextToken();
    return node;
}

static struct AST* ParseString() {
    char* value = strdup(TokenVal); // Memory leak
    struct AST* node = AST_NEW(AST_STRING, value);
    getNextToken();
    return node;
}

static struct AST* ParseParenExpr() {
    getNextToken(); // Consume '('
    struct AST* node = ParsePrimary();
    if (!node) return NULL;
    if (CurrentToken != ')') {
        return logError("Expected ')'");
    }
    getNextToken(); // Consume ')'
    return node;
}

static struct AST* ParseIdentifier() {
    char* name = strdup(TokenVal); // Memory leak
     
    getNextToken();
    if (CurrentToken != '(') {
        return AST_NEW(AST_VARIABLE, name);
    }

    // Function call
    getNextToken(); // Consume '('
    // Support up to 64 arguments for now
    struct AST* args[64];
    int arg_count = 0;
    if (CurrentToken != ')') {
        for(;;) {
            struct AST* arg = ParsePrimary();
            if (arg) args[arg_count++] = arg;
            else return NULL; // Error in argument parsing
            if (CurrentToken == ')') break;
            if (CurrentToken != ',') return logError("Expected ',' or ')'");
            getNextToken(); // Consume ','
        }
    }

    getNextToken(); // Consume ')'
    // Allocate array for arguments
    AST** args_copy = malloc(sizeof(AST*) * arg_count);
    for (int i = 0; i < arg_count; i++) args_copy[i] = args[i];
    return AST_NEW(AST_CALL, name, args_copy, arg_count);
}

static struct AST* ParseExtern() {
    char* name = strdup(TokenVal); // Memory leak
    getNextToken(); // Consume 'extern'
    if (CurrentToken != TOK_IDENTIFIER) {
        return logError("Expected identifier after 'extern'");
    }
    name = strdup(TokenVal); // Memory leak
    getNextToken(); // Consume identifier
    return AST_NEW(AST_EXTERN, name);
}

static struct AST* ParsePrimary() {
    if (CurrentToken == TOK_NUMBER) {
        return ParseNumber();
    } else if (CurrentToken == TOK_STRING) {
        return ParseString();
    } else if (CurrentToken == TOK_IDENTIFIER) {
        return ParseIdentifier();
    } else if (CurrentToken == TOK_EXTERN) {
        return ParseExtern();
    } else if (CurrentToken == '(') {
        return ParseParenExpr();
    } else {
        return logError("Unexpected token in expression");
    }
}

static void PrintAst(struct AST* node, int indent) {
    if (!node) return;
    for (int i = 0; i < indent; i++) printf("  ");
    switch (node->type) {
        case AST_NUMBER:
            printf("AST Number: %d\n", node->data.AST_NUMBER.value);
            break;
        case AST_STRING:
            printf("AST String: %s\n", node->data.AST_STRING.value);
            break;
        case AST_VARIABLE:
            printf("AST Variable: %s\n", node->data.AST_VARIABLE.name);
            break;
        case AST_BINARY:
            printf("AST Binary Operation: %c\n", node->data.AST_BINARY.op);
            PrintAst(node->data.AST_BINARY.left, indent + 1);
            PrintAst(node->data.AST_BINARY.right, indent + 1);
            break;
        case AST_CALL:
            printf("AST Function Call: %s with %d args\n", node->data.AST_CALL.callee, node->data.AST_CALL.arg_count);
            for (int i = 0; i < node->data.AST_CALL.arg_count; i++) {
                PrintAst(node->data.AST_CALL.args[i], indent + 1);
            }
            break;
        case AST_EXTERN:
            printf("AST Extern function: %s\n", node->data.AST_EXTERN.name);
            break;
        default:
            fprintf(stderr, "Unknown AST node type: %d\n", node->type);
            break;
    }
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

    struct AST* ast = NULL;
    getNextToken(); // Initialize the first token
    while (CurrentToken != TOK_EOF) {
        struct AST* node = ParsePrimary();
        if (node) {
            PrintAst(node, 0);
        } else {
            fprintf(stderr, "Error parsing expression\n");
            getNextToken(); // Advance token to avoid infinite loop
        }
    }

    fclose(SourceFile);
    
    return 0;
}