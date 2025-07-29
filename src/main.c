#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <llvm-c/Core.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/Types.h>
#include <llvm-c/BitWriter.h>

static int CurrentToken = 0; // Current token type
static char TokenVal[1024]; // Buffer the text value of the token
static FILE* SourceFile = NULL;

enum Token {
    TOK_EOF = -1,
    TOK_IDENTIFIER = -2,
    TOK_STRING = -3,
    TOK_NUMBER = -4
};

typedef struct AST AST;
struct AST {
    enum { AST_NUMBER, AST_STRING, AST_VARIABLE, AST_CALL } type;
    union {
        struct AST_NUMBER { int value; } AST_NUMBER;
        struct AST_STRING { char* value; } AST_STRING;
        struct AST_VARIABLE { char* name; } AST_VARIABLE;
        struct AST_CALL { char* callee; AST** args; int arg_count; } AST_CALL;
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

static struct AST* ParsePrimary() {
    if (CurrentToken == TOK_NUMBER) {
        return ParseNumber();
    } else if (CurrentToken == TOK_STRING) {
        return ParseString();
    } else if (CurrentToken == TOK_IDENTIFIER) {
        return ParseIdentifier();
    } else if (CurrentToken == '(') {
        return ParseParenExpr();
    } else {
        return logError("Unexpected token in expression");
    }
}

static LLVMValueRef GenerateIR(LLVMModuleRef module, LLVMBuilderRef builder, struct AST* node);


LLVMTypeRef GetFunctionType(LLVMModuleRef module, LLVMBuilderRef builder, struct AST_CALL* call) {
    // TODO: map the function name + arity to cached map
    LLVMTypeRef* arg_types = malloc(sizeof(LLVMTypeRef) * call->arg_count);
    for (int i = 0; i < call->arg_count; ++i) {
        arg_types[i] = LLVMPointerType(LLVMInt8Type(), 0);
    }
    LLVMTypeRef func_type = LLVMFunctionType(LLVMInt32Type(), arg_types, call->arg_count, 1);
    LLVMValueRef func = LLVMAddFunction(module, call->callee, func_type);
    LLVMSetLinkage(func, LLVMExternalLinkage);
    return func_type;
}

LLVMValueRef* GetFunctionArgs(LLVMModuleRef module, LLVMBuilderRef builder, struct AST_CALL* call) {
    LLVMValueRef* args = malloc(sizeof(LLVMValueRef) * call->arg_count);
    for (int i = 0; i < call->arg_count; i++) {
        args[i] = GenerateIR(module, builder, call->args[i]);
    }
    return args;
}


static LLVMValueRef GenerateIR(LLVMModuleRef module, LLVMBuilderRef builder, struct AST* node) {
    switch (node->type) {
        case AST_NUMBER: {
            return LLVMConstInt(LLVMInt32Type(), node->data.AST_NUMBER.value, 0);
        }
        case AST_STRING: {
            LLVMValueRef global_string = LLVMBuildGlobalString(builder, node->data.AST_STRING.value, "str_const");
            return global_string;
        }
        case AST_VARIABLE: {
            return LLVMGetNamedGlobal(module, node->data.AST_VARIABLE.name);
        }
        case AST_CALL: {
            LLVMTypeRef func_type = GetFunctionType(module, builder, &node->data.AST_CALL);
            LLVMValueRef callee = LLVMGetNamedFunction(module, node->data.AST_CALL.callee);
            LLVMValueRef* args = GetFunctionArgs(module, builder, &node->data.AST_CALL);
            return LLVMBuildCall2(builder, func_type, callee, args, node->data.AST_CALL.arg_count, "calltmp");
        }
        default:
            logError("Unknown AST node type");
            return NULL;
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

    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("conlang_module", context);
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);

    // Create main function for execution
    LLVMTypeRef main_func_type = LLVMFunctionType(LLVMInt32Type(), NULL, 0, 0);
    LLVMValueRef main_func = LLVMAddFunction(module, "main", main_func_type);
    LLVMBasicBlockRef entry_block = LLVMAppendBasicBlockInContext(context, main_func, "entry");
    LLVMPositionBuilderAtEnd(builder, entry_block);

    getNextToken(); // Initialize the first token
    while (CurrentToken != TOK_EOF) {
        struct AST* node = ParsePrimary();
        if (node) {
            GenerateIR(module, builder, node);
        } else {
            fprintf(stderr, "Error parsing expression\n");
            getNextToken(); // Advance token to avoid infinite loop
        }
    }

    LLVMBuildRet(builder, LLVMConstInt(LLVMInt32Type(), 0, 0));

    fclose(SourceFile);

    if (LLVMWriteBitcodeToFile(module, "output.ll") != 0) {
        fprintf(stderr, "Error writing bitcode to file\n");
    }

    LLVMDisposeBuilder(builder);
    LLVMDisposeModule(module);
    LLVMContextDispose(context);
    
    return 0;
}

