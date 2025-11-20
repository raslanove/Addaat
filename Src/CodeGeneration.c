
//
// Converting Addaat code to C code, enforcing language semantics in the process.
//
// By Omar El Sayyed.
// The 8th of August, 2022.
//

// TODO: add addToken() to NCC. It should be the same as add rule, but adds a rule for a token.
//       When tokens are present, the input text is tokenized before the rules are applied. If
//       a substitute node refers to a token, the token name (not value) is matched. When tokenizing,
//       reuse tokens with the same name a value.
// TODO: rename token node into selection node...

// TODO: add failed rule to matching result...
// TODO: perform address translation based on a flag...
// TODO: fix arrays are not primitive types...
// TODO: remove the need to write "class" before declaring a class variable...
// TODO: Make functions a first class citizen...
// TODO: Add managed/new/delete/weak keywords to be used for allocations, where:
//          => new and delete allocate normally, no automatic collection.
//          => managed is garbage collected. It's a replacement for new/delete.
//          => weak is a qualifier (like const). A weak reference doesn't influence collection, it's used to test if an object is still alive.

#include <CodeGeneration.h>

#include <NCC.h>
#include <NSystemUtils.h>
#include <NCString.h>
#include <NError.h>

#define COLORIZE_CODE 0

#define TAB "    "

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Code constructs
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define TYPE_VOID    0
#define TYPE_CLASS   1
#define TYPE_ENUM    2
#define TYPE_CHAR    3
#define TYPE_SHORT   4
#define TYPE_INT     5
#define TYPE_LONG    6
#define TYPE_FLOAT   7
#define TYPE_DOUBLE  8

struct VariableType {
    int32_t type;
    int32_t classIndex;
    int32_t arrayDepth;
};

struct VariableInfo {
    struct NString name;
    struct VariableType type;
    boolean isStatic;
};

struct Scope {
    int32_t id;
    struct NVector localVariables; // struct VariableInfo*.
};

struct FunctionInfo {
    struct NString name;
    struct NVector parameters; // struct VariableInfo*.
    struct VariableType returnType;
    struct NCC_ASTNode* body;
    boolean isStatic;
};

struct ClassInfo {
    struct NString name;
    struct NVector members; // struct VariableInfo*.
    boolean defined;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forward declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct CodeGenerationData;

static boolean parseStatement(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData);
static boolean parseCompoundStatement(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData, struct NVector* predefinedLocalVariables);
static boolean parseExpression(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData);
static boolean parseAssignmentExpression(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData);
static boolean parseCastExpression(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData);

static void destroyAndDeleteVariableInfo(struct VariableInfo* variableInfo);
static void destroyAndDeleteVariableInfos(struct NVector* variableInfosVector);
static void destroyAndDeleteFunctionInfo(struct FunctionInfo* functionInfo);
static void destroyAndDeleteClassInfo(struct ClassInfo* classInfo);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Code generation data
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct CodeGenerationData {

    // Generated code,
    struct NString outString;

    // Code coloring,
    struct NVector colorStack; // const char*
    const char* lastUsedColor;

    // Indentation,
    int32_t indentationCount;

    // Symbols,
    struct NVector globalVariables; // struct VariableInfo*
    struct NVector functions;       // struct FunctionInfo*
    struct NVector classes;         // struct ClassInfo*

    // Context,
    struct ClassInfo* currentClass;
    struct FunctionInfo* currentFunction;
    struct NVector scopesStack; // struct Scope*
    int32_t scopesCount;
};

static void initializeCodeGenerationData(struct CodeGenerationData* codeGenerationData) {

    // Generated code,
    NString.initialize(&codeGenerationData->outString, "");

    // Code coloring,
    NVector.initialize(&codeGenerationData->colorStack, 0, sizeof(const char*));
    codeGenerationData->lastUsedColor = 0;

    // Indentation,
    codeGenerationData->indentationCount = 0;

    // Symbols,
    NVector.initialize(&codeGenerationData->globalVariables, 0, sizeof(struct VariableInfo*));
    NVector.initialize(&codeGenerationData->functions      , 0, sizeof(struct FunctionInfo*));
    NVector.initialize(&codeGenerationData->classes        , 0, sizeof(struct ClassInfo   *));

    // Context,
    codeGenerationData->currentClass = 0;
    codeGenerationData->currentFunction = 0;
    NVector.initialize(&codeGenerationData->scopesStack, 0, sizeof(struct Scope*));
    codeGenerationData->scopesCount = 0;
}

static void destroyCodeGenerationData(struct CodeGenerationData* codeGenerationData) {
    NString.destroy(&codeGenerationData->outString);
    NVector.destroy(&codeGenerationData->colorStack);

    // Global variables,
    destroyAndDeleteVariableInfos(&codeGenerationData->globalVariables);
    NVector.destroy(&codeGenerationData->globalVariables);

    // Functions,
    for (int32_t i=NVector.size(&codeGenerationData->functions)-1; i>=0; i--) {
        destroyAndDeleteFunctionInfo(*(struct FunctionInfo**) NVector.get(&codeGenerationData->functions, i));
    }
    NVector.destroy(&codeGenerationData->functions);

    // Classes,
    for (int32_t i=NVector.size(&codeGenerationData->classes)-1; i>=0; i--) {
        destroyAndDeleteClassInfo(*(struct ClassInfo**) NVector.get(&codeGenerationData->classes, i));
    }
    NVector.destroy(&codeGenerationData->classes);

    // Context,
    NVector.destroy(&codeGenerationData->scopesStack);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helper functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void codeAppend(struct CodeGenerationData* codeGenerationData, const char* text) {

    // Append indentation,
    if (NCString.endsWith(NString.get(&codeGenerationData->outString), "\n")) {
        for (int32_t i=0; i<codeGenerationData->indentationCount; i++) {
            NString.append(&codeGenerationData->outString, TAB);
        }
    }

    // Add color,
    if (COLORIZE_CODE) {
        if (!(NCString.equals(text, " ") || NCString.equals(text, "\n"))) {
            const char* color;
            if (NVector.size(&codeGenerationData->colorStack)) {
                color = *(const char**) NVector.getLast(&codeGenerationData->colorStack);
            } else {
                color = NTCOLOR(STREAM_DEFAULT);
            }
            // Print color only if it's different from last color used,
            if (color != codeGenerationData->lastUsedColor) {
                NString.append(&codeGenerationData->outString, "%s", color);
                codeGenerationData->lastUsedColor = color;
            }
        }
    }

    // Append text,
    NString.append(&codeGenerationData->outString, "%s", text);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Parsing
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define Begin \
    int32_t currentChildIndex = 0; \
    struct NCC_ASTNode* currentChild; \
    { \
        struct NCC_ASTNode** node = NVector.get(&tree->childNodes, 0); \
        currentChild = node ? *node : 0; \
    }

#define NextChild \
    { \
        currentChildIndex++; \
        struct NCC_ASTNode** node = NVector.get(&tree->childNodes, currentChildIndex); \
        currentChild = node ? *node : 0; /* This assumes that NVECTOR_BOUNDARY_CHECK is set */ \
    }

#define NAME  NString.get(&currentChild->name )
#define VALUE NString.get(&currentChild->value)

#define Equals(text) \
    NCString.equals(NString.get(&currentChild->name), text)

#define Append(text) \
    codeAppend(codeGenerationData, text);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Scopes
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static struct Scope* pushNewScope(struct CodeGenerationData* codeGenerationData) {

    // Create a new scope,
    struct Scope* newScope = NMALLOC(sizeof(struct Scope), "CodeGeneration.pushNewScope() newScope");
    newScope->id = ++(codeGenerationData->scopesCount);
    NVector.initialize(&newScope->localVariables, 0, sizeof(struct VariableInfo*));

    // Push to the stack,
    NVector.pushBack(&codeGenerationData->scopesStack, &newScope);

    return newScope;
}

static void popScope(struct CodeGenerationData* codeGenerationData) {

    // Pop scope,
    struct Scope* scope;
    NVector.popBack(&codeGenerationData->scopesStack, &scope);

    // Delete scope variables and scope,
    destroyAndDeleteVariableInfos(&scope->localVariables);
    NVector.destroy(&scope->localVariables);
    NFREE(scope, "CodeGeneration.popScope() scope");
}

static struct Scope* getCurrentScope(struct CodeGenerationData* codeGenerationData) {
    struct Scope** currentScope = NVector.getLast(&codeGenerationData->scopesStack);
    if (currentScope) return *currentScope;
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void destroyAndDeleteVariableInfo(struct VariableInfo* variableInfo) {
    NString.destroy(&variableInfo->name);
    NFREE(variableInfo, "CodeGeneration.destroyAndDeleteVariableInfo() variableInfo");
}

static void destroyAndDeleteVariableInfos(struct NVector* variableInfosVector) {
    for (int32_t i=NVector.size(variableInfosVector)-1; i>=0; i--) {
        struct VariableInfo** variableInfo = NVector.get(variableInfosVector, i);
        if (*variableInfo) destroyAndDeleteVariableInfo(*variableInfo);
    }
    NVector.clear(variableInfosVector);
}

static struct VariableInfo* getVariable(struct NVector* variables, const char* variableName) {
    for (int32_t i=NVector.size(variables)-1; i>=0; i--) {
        struct VariableInfo* variableInfo = *(struct VariableInfo**) NVector.get(variables, i);
        if (NCString.equals(variableName, NString.get(&variableInfo->name))) return variableInfo;
    }
    return 0;
}

static boolean typesEqual(struct VariableType* type1, struct VariableType* type2) {
    return
        (type1->type       == type2->type      ) &&
        (type1->classIndex == type2->classIndex) &&
        (type1->arrayDepth == type2->arrayDepth);
}

static struct VariableType* parseTypeSpecifier(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

    // type-specifier: int[][]
    // ├─int: int
    // ├─array-specifier: []
    // │ ├─[: [
    // │ └─]: ]
    // │
    // └─array-specifier: []
    //   ├─[: [
    //   └─]: ]

    // #{{void}     {char}
    //   {short}    {int}      {long}
    //   {float}    {double}
    //   {class-specifier}
    //   {enum-specifier}}
    // {${} ${array-specifier}}^*

    struct VariableType* variableType = NMALLOC(sizeof(struct VariableType), "CodeGeneration.parseTypeSpecifier() variableType");
    NSystemUtils.memset(variableType, 0, sizeof(struct VariableType));

    Begin
    if (Equals("void")) {
        variableType->type = TYPE_VOID;
        NextChild
        if (currentChild) {
            NERROR("parseTypeSpecifier()", "Can't make arrays of void type.");
            return 0;
        }
    }
    else if (Equals("char"  )) { variableType->type = TYPE_CHAR  ; }
    else if (Equals("short" )) { variableType->type = TYPE_SHORT ; }
    else if (Equals("int"   )) { variableType->type = TYPE_INT   ; }
    else if (Equals("long"  )) { variableType->type = TYPE_LONG  ; }
    else if (Equals("float" )) { variableType->type = TYPE_FLOAT ; }
    else if (Equals("double")) { variableType->type = TYPE_DOUBLE; }
    else {
        // TODO: enum and class...
    }

    NextChild
    while (currentChild) {
        // Parse array specifier(s),
        variableType->arrayDepth++;
        NextChild
    }

    return variableType;
}

static void appendVariableTypeCode(struct VariableType* type, struct CodeGenerationData* codeGenerationData) {
    switch (type->type) {
        case TYPE_VOID  : Append("void"   ) break;
        case TYPE_CHAR  : Append("char"   ) break;
        case TYPE_SHORT : Append("short"  ) break;
        case TYPE_INT   : Append("int32_t") break;
        case TYPE_LONG  : Append("int64_t") break;
        case TYPE_FLOAT : Append("float"  ) break;
        case TYPE_DOUBLE: Append("double" ) break;
        default:
            // TODO: enum and class...
            break;
    }

    for (int32_t i=0; i<type->arrayDepth; i++) Append("*")
}

static struct VariableInfo* cloneVariable(struct VariableInfo* variableToClone, const char* newName) {
    struct VariableInfo* newVariable = NMALLOC(sizeof(struct VariableInfo), "CodeGeneration.cloneVariable() newVariable");
    *newVariable = *variableToClone;
    NString.initialize(&newVariable->name, "%s", newName);
    return newVariable;
}

static boolean parseVariableDeclaration(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData, struct NVector* outputVector, boolean allowDuplicates) {

    // declaration: static int[][] c, d;
    // ├─static: static
    // ├─insert space:
    // ├─type-specifier: int[][]
    // │ ├─int: int
    // │ ├─array-specifier: []
    // │ │ ├─[: [
    // │ │ └─]: ]
    // │ │
    // │ └─array-specifier: []
    // │   ├─[: [
    // │   └─]: ]
    // │
    // ├─insert space:
    // ├─identifier: c
    // ├─,: ,
    // ├─insert space:
    // ├─identifier: d
    // └─;: ;

    // ${declaration-specifiers} ${+ } ${identifier-list} ${} ${;}

    // Create variable,
    struct VariableInfo* newVariable = NMALLOC(sizeof(struct VariableInfo), "CodeGeneration.parseVariableDeclaration() newVariable");

    Begin

    // Parse storage class specifier (we only have static),
    if (Equals("static")) {
        newVariable->isStatic = True;
        NextChild
    } else {
        newVariable->isStatic = False;
    }

    // Parse type specifier,
    struct VariableType* variableType = parseTypeSpecifier(currentChild, codeGenerationData);
    if (!variableType) {
        NFREE(newVariable, "CodeGeneration.parseVariableDeclaration() newVariable 1");
        return False;
    }

    // Check for voids,
    if (variableType->type == TYPE_VOID) {
        NERROR("parseVariableDeclaration()", "Void is not a valid variable type.");
        NFREE( newVariable, "CodeGeneration.parseVariableDeclaration() newVariable 2");
        NFREE(variableType, "CodeGeneration.parseVariableDeclaration() variableType 1");
        return False;
    }

    // Assign type,
    newVariable->type = *variableType;
    NFREE(variableType, "CodeGeneration.parseVariableDeclaration() variableType 2");

    // Parse name and make sure it's not a redefinition,
    NextChild
    struct VariableInfo* existingVariable = getVariable(outputVector, VALUE);
    if (existingVariable &&
        (!allowDuplicates || !typesEqual(&newVariable->type, &existingVariable->type))) {
        NERROR("parseVariableDeclaration()", "Variable redefinition: %s%s%s.", NTCOLOR(HIGHLIGHT), VALUE, NTCOLOR(STREAM_DEFAULT));
        NFREE( newVariable, "CodeGeneration.parseVariableDeclaration() newVariable 3");
        return False;
    }
    NString.initialize(&newVariable->name, "%s", VALUE);
    NVector.pushBack(outputVector, &newVariable);

    // Look for additional variables,
    NextChild
    while (Equals(",")) {

        // Parse name and make sure it's not a redefinition,
        NextChild
        existingVariable = getVariable(outputVector, VALUE);
        if (existingVariable &&
            (!allowDuplicates || !typesEqual(&newVariable->type, &existingVariable->type))) {
            NERROR("parseVariableDeclaration()", "Variable redefinition: %s%s%s.", NTCOLOR(HIGHLIGHT), VALUE, NTCOLOR(STREAM_DEFAULT));
            return False;
        }

        // Create a new variable that's a copy of the first one but with a different name,
        struct VariableInfo* anotherNewVariable = cloneVariable(newVariable, VALUE);
        NVector.pushBack(outputVector, &anotherNewVariable);
        NextChild
    }

    return True;
}

static void appendVariableDeclarationCode(struct VariableInfo* variable, struct CodeGenerationData* codeGenerationData, const char* prefix, const char* postfix) {
    appendVariableTypeCode(&variable->type, codeGenerationData);
    Append(" ")
    Append(prefix)
    Append(NString.get(&variable->name))
    Append(postfix)
    Append(";")
}

static boolean addLocalVariable(struct VariableInfo* newLocalVariable, struct CodeGenerationData* codeGenerationData) {

    // Get the current scope,
    struct Scope* scope = getCurrentScope(codeGenerationData);

    // Check duplicates within this scope,
    if (getVariable(&scope->localVariables, NString.get(&newLocalVariable->name))) {
        NERROR("CodeGeneration.addLocalVariable()", "Variable redefinition: %s%s%s.", NTCOLOR(HIGHLIGHT), NString.get(&newLocalVariable->name), NTCOLOR(STREAM_DEFAULT));
        return False;
    }

    // Add to local variables,
    NVector.pushBack(&scope->localVariables, &newLocalVariable);

    // Statics are also declared globally,
    if (newLocalVariable->isStatic) {
        // TODO: check for duplicates?
        struct NString* globalVersionName = NString.create("_scope%d_%s_", scope->id, NString.get(&newLocalVariable->name));
        struct VariableInfo* globalVersion = cloneVariable(newLocalVariable, NString.get(globalVersionName));
        NString.destroyAndFree(globalVersionName);
        NVector.pushBack(&codeGenerationData->globalVariables, &globalVersion);
    } else {
        appendVariableDeclarationCode(newLocalVariable, codeGenerationData, "", "");
        Append("\n")
    }

    return True;
}

static boolean parseLocalVariableDeclaration(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

    boolean success=False;

    // Parse the variable(s) into a temporary vector,
    struct NVector* newVariables = NVector.create(0, sizeof(struct VariableInfo*));
    if (!parseVariableDeclaration(tree, codeGenerationData, newVariables, False)) goto finish;

    // Process the newly declared variables and house them into the proper scopes,
    int32_t variablesCount = NVector.size(newVariables);
    for (int32_t i=0; i<variablesCount; i++) {
        struct VariableInfo** newLocalVariablePtr = NVector.get(newVariables, i);
        struct VariableInfo* newLocalVariable = *newLocalVariablePtr;

        if (!addLocalVariable(newLocalVariable, codeGenerationData)) goto finish;
        *newLocalVariablePtr = 0;
    }
    NVector.clear(newVariables);
    success = True;

    finish:

    // Clean up,
    destroyAndDeleteVariableInfos(newVariables);
    NVector.destroyAndFree(newVariables);
    return success;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void destroyAndDeleteFunctionInfo(struct FunctionInfo* functionInfo) {
    NString.destroy(&functionInfo->name);
    destroyAndDeleteVariableInfos(&functionInfo->parameters);
    NVector.destroy(&functionInfo->parameters);
    NFREE(functionInfo, "CodeGeneration.destroyAndDeleteFunctionInfo() functionInfo");
}

static struct FunctionInfo* getFunction(struct NVector* functions, const char* functionName) {
    for (int32_t i=NVector.size(functions)-1; i>=0; i--) {
        struct FunctionInfo* functionInfo = *(struct FunctionInfo**) NVector.get(functions, i);
        if (NCString.equals(functionName, NString.get(&functionInfo->name))) return functionInfo;
    }
    return 0;
}

static boolean sameParameters(struct FunctionInfo* function1, struct FunctionInfo* function2) {

    // Check parameters count,
    int32_t function1ParametersCount = NVector.size(&function1->parameters);
    int32_t function2ParametersCount = NVector.size(&function2->parameters);
    if (function1ParametersCount != function2ParametersCount) return False;

    // Check parameters' types,
    for (int32_t i=function1ParametersCount-1; i>=0; i--) {
        struct VariableInfo* function1Parameter = *(struct VariableInfo**) NVector.get(&function1->parameters, i);
        struct VariableInfo* function2Parameter = *(struct VariableInfo**) NVector.get(&function2->parameters, i);
        if (!typesEqual(&function1Parameter->type, &function2Parameter->type)) return False;
    }

    return True;
}

static boolean sameSignature(struct FunctionInfo* function1, struct FunctionInfo* function2) {

    // Check return values,
    if (!typesEqual(&function1->returnType, &function2->returnType)) return False;

    // Check parameters,
    return sameParameters(function1, function2);
}

static struct FunctionInfo* parseFunctionHead(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

    // function-head =
    //             ${declaration-specifiers} ${}
    //             ${identifier} ${}
    //             ${(} ${} ${parameter-list}|${ε} ${} ${)}

    Begin

    // Create function,
    struct FunctionInfo* newFunction = NMALLOC(sizeof(struct FunctionInfo), "CodeGeneration.parseFunctionHead() newFunction");
    newFunction->body = 0;

    // Parse storage class specifier (we only have static),
    if (Equals("static")) {
        newFunction->isStatic = True;
        NextChild
    } else {
        newFunction->isStatic = False;
    }

    // Parse type specifier,
    struct VariableType* returnType = parseTypeSpecifier(currentChild, codeGenerationData);
    if (!returnType) {
        NFREE(newFunction, "CodeGeneration.parseFunctionHead() newFunction");
        return 0;
    }

    // Assign type,
    newFunction->returnType = *returnType;
    NFREE(returnType, "CodeGeneration.parseFunctionHead() returnType");

    // Parse name,
    NextChild
    NString.initialize(&newFunction->name, "%s", VALUE);

    // Parse parameter list,
    NVector.initialize(&newFunction->parameters, 0, sizeof(struct VariableInfo*));
    NextChild
    while (currentChild) {

        // Parse type specifier,
        struct VariableType* parameterType = parseTypeSpecifier(currentChild, codeGenerationData);
        if (!parameterType) {
            destroyAndDeleteFunctionInfo(newFunction);
            return 0;
        }

        // Check for voids,
        if (parameterType->type == TYPE_VOID) {
            NERROR("parseFunctionHead()", "Void is not a valid parameter type.");
            NFREE(parameterType, "CodeGeneration.parseFunctionHead() parameterType 1");
            destroyAndDeleteFunctionInfo(newFunction);
            return 0;
        }

        // Check for duplicates,
        NextChild
        if (getVariable(&newFunction->parameters, VALUE)) {
            NERROR("parseFunctionHead()", "Parameter redefinition: %s%s%s.", NTCOLOR(HIGHLIGHT), VALUE, NTCOLOR(STREAM_DEFAULT));
            NFREE(parameterType, "CodeGeneration.parseFunctionHead() parameterType 2");
            destroyAndDeleteFunctionInfo(newFunction);
            return 0;
        }

        // Create a new parameter,
        struct VariableInfo* newParameter = NMALLOC(sizeof(struct VariableInfo), "CodeGeneration.parseFunctionHead() newParameter");
        NString.initialize(&newParameter->name, "%s", VALUE);
        newParameter->isStatic = False;
        newParameter->type = *parameterType;
        NFREE(parameterType, "CodeGeneration.parseFunctionHead() parameterType 3");
        NVector.pushBack(&newFunction->parameters, &newParameter);

        // Skip comma,
        NextChild
        if (Equals(",")) NextChild
    }

    return newFunction;
}

static void appendFunctionHeadCode(struct FunctionInfo* function, struct CodeGenerationData* codeGenerationData, const char* prefix, const char* postfix) {
    if (function->isStatic) Append("static ")
    appendVariableTypeCode(&function->returnType, codeGenerationData);
    Append(" ")
    Append(prefix)
    Append(NString.get(&function->name))
    Append(postfix)
    Append("(")

    int32_t parametersCount = NVector.size(&function->parameters);
    for (int32_t i=0; i<parametersCount; i++) {
        if (i) Append(", ")
        struct VariableInfo* parameter = *(struct VariableInfo**) NVector.get(&function->parameters, i);
        appendVariableTypeCode(&parameter->type, codeGenerationData);
        Append(" ")
        Append(NString.get(&parameter->name))
    }

    Append(")")
}

static void appendFunctionDeclarationCode(struct FunctionInfo* function, struct CodeGenerationData* codeGenerationData, const char* prefix, const char* postfix) {
    appendFunctionHeadCode(function, codeGenerationData, prefix, postfix);
    Append(";")
}

static boolean parseGlobalFunctionDeclaration(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

    Begin
    struct FunctionInfo* newFunction = parseFunctionHead(currentChild, codeGenerationData);
    if (!newFunction) return False;

    // If it's new, add it and return,
    struct FunctionInfo* existingFunction = getFunction(&codeGenerationData->functions, NString.get(&newFunction->name));
    if (!existingFunction) {
        NVector.pushBack(&codeGenerationData->functions, &newFunction);
        appendFunctionDeclarationCode(newFunction, codeGenerationData, "", "");
        return True;
    }

    // If it's a duplicate, check if the signature changed,
    // TODO: allow polymorphism...
    boolean duplicate = sameSignature(newFunction, existingFunction);
    if (duplicate) {
        appendFunctionDeclarationCode(newFunction, codeGenerationData, "", "");
    } else {
        NERROR("CodeGeneration.parseGlobalFunctionDeclaration()", "Function %s%s%s redeclared with a different signature.", NTCOLOR(HIGHLIGHT), NString.get(&existingFunction->name), NTCOLOR(STREAM_DEFAULT));
    }
    destroyAndDeleteFunctionInfo(newFunction);

    return duplicate;
}

static boolean parseGlobalFunctionDefinition(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

    Begin
    struct FunctionInfo* newFunction = parseFunctionHead(currentChild, codeGenerationData);
    if (!newFunction) return False;

    // Look for an existing declaration,
    struct FunctionInfo* existingFunction = getFunction(&codeGenerationData->functions, NString.get(&newFunction->name));
    if (existingFunction) {

        // If it's redefinition, throw,
        if (existingFunction->body) {
            NERROR("CodeGeneration.parseGlobalFunctionDefinition()", "Function %s%s%s redefinition.", NTCOLOR(HIGHLIGHT), NString.get(&existingFunction->name), NTCOLOR(STREAM_DEFAULT));
            destroyAndDeleteFunctionInfo(newFunction);
            return False;
        }

        // Check if the signature changed,
        // TODO: allow polymorphism...
        if (!sameSignature(newFunction, existingFunction)) {
            NERROR("CodeGeneration.parseGlobalFunctionDefinition()", "Function %s%s%s defined with a different signature.", NTCOLOR(HIGHLIGHT), NString.get(&existingFunction->name), NTCOLOR(STREAM_DEFAULT));
            destroyAndDeleteFunctionInfo(newFunction);
            return False;
        }
    } else {
        NVector.pushBack(&codeGenerationData->functions, &newFunction);
    }

    appendFunctionHeadCode(newFunction, codeGenerationData, "", "");
    Append(" ")

    // Parse function body,
    NextChild
    newFunction->body = currentChild;
    codeGenerationData->currentFunction = newFunction;
    boolean success = parseCompoundStatement(currentChild, codeGenerationData, &newFunction->parameters);
    codeGenerationData->currentFunction = 0;

    // Clean up,
    if (existingFunction) destroyAndDeleteFunctionInfo(newFunction);

    return success;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Class
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static struct ClassInfo* createClass(struct CodeGenerationData* codeGenerationData, const char* className) {
    struct ClassInfo* newClass = NMALLOC(sizeof(struct ClassInfo), "CodeGeneration.createClass() newClass");
    NString.initialize(&newClass->name, "%s", className);
    NVector.initialize(&newClass->members, 0, sizeof(struct VariableInfo*));
    newClass->defined = False;

    NVector.pushBack(&codeGenerationData->classes, &newClass);
    return newClass;
}

static void destroyAndDeleteClassInfo(struct ClassInfo* classInfo) {
    NString.destroy(&classInfo->name);
    destroyAndDeleteVariableInfos(&classInfo->members);
    NVector.destroy(&classInfo->members);
    NFREE(classInfo, "CodeGeneration.destroyAndDeleteClassInfo() classInfo");
}

static struct ClassInfo* getClass(struct CodeGenerationData* codeGenerationData, const char* className) {
    for (int32_t i=NVector.size(&codeGenerationData->classes)-1; i>=0; i--) {
        struct ClassInfo* classInfo = *(struct ClassInfo**) NVector.get(&codeGenerationData->classes, i);
        if (NCString.equals(className, NString.get(&classInfo->name))) return classInfo;
    }
    return 0;
}

static boolean parseClassDeclaration(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

    // ${class} ${+ } ${identifier}
    //   {${} ${;} ${+\n}} |
    //   {${+ } ${OB} {${+\n} ${declaration-list}}|${ε} ${} ${CB} ${+\n}}
    Begin

    // Skip the "class" keyword,
    Append("struct ");
    NextChild

    // Parse class name, if not and existing one, create new,
    const char* className = VALUE;
    struct ClassInfo* class = getClass(codeGenerationData, className);
    if (!class) class = createClass(codeGenerationData, className);
    Append(className);
    NextChild

    // Return if semi-colon found (forward-declaration),
    if (Equals(";")) {
        Append(";")
        return True;
    }

    // Skip open bracket,
    if (class->defined) {
        NERROR("parseClassSpecifier()", "Class redefinition.");
        return False;
    }
    class->defined = True;
    Append(" {")
    NextChild
    if (!Equals("CB")) Append("\n")

    // Parse declarations,
    while (True) {

        // Check if closing bracket reached,
        if (Equals("CB")) {

            // Append non-static variables code,
            int32_t membersCount = NVector.size(&class->members);
            for (int32_t i=0; i<membersCount; i++) {
                struct VariableInfo* currentVariable = *(struct VariableInfo**) NVector.get(&class->members, i);
                if (currentVariable->isStatic) continue;
                Append(TAB)
                appendVariableDeclarationCode(currentVariable, codeGenerationData, "", "");
                Append("\n")
            }
            Append("};\n")

            // Append static variables code,
            struct NString prefix;
            NString.initialize(&prefix, "_%s_", className);
            const char* prefixCString = NString.get(&prefix);
            for (int32_t i=0; i<membersCount; i++) {
                struct VariableInfo* currentVariable = *(struct VariableInfo**) NVector.get(&class->members, i);
                if (!currentVariable->isStatic) continue;
                appendVariableDeclarationCode(currentVariable, codeGenerationData, prefixCString, "_");
                Append("\n")
            }
            NString.destroy(&prefix);

            return True;
        }

        // Parse variable declaration,
        if (!parseVariableDeclaration(currentChild, codeGenerationData, &class->members, False)) return False;

        NextChild
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Expression
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static boolean parseIdentifier(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {
    // TODO: Substitute the correct identifier (account for "this" and for statics)...
    Begin
    Append(VALUE)
    return True;
}

static boolean parsePrimaryExpression(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

    // primary-expression = ${identifier}     |
    //                      ${constant}       |
    //                      ${string-literal} |
    //                      { ${(} ${} ${expression} ${} ${)} }

    Begin

    if (Equals("identifier")) return parseIdentifier(currentChild, codeGenerationData);

    if (Equals("expression")) {
        Append("(")
        if (!parseExpression(currentChild, codeGenerationData)) return False;
        Append(")")
        return True;
    }

    // This is either a string-literal or a constant,
    Append(VALUE)
    return True;
}

static boolean parseArgumentExpressionList(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {
    // argument-expression-list = ${assignment-expression} {
    //                               ${} ${,} ${} ${assignment-expression}
    //                            }^*

    Begin

    if (!parseAssignmentExpression(currentChild, codeGenerationData)) return False;
    NextChild

    while (currentChild) {
        Append(", ")
        NextChild

        if (!parseAssignmentExpression(currentChild, codeGenerationData)) return False;
        NextChild
    }
    return True;
}

static boolean parsePostFixExpression(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

    // postfix-expression = ${primary-expression} {
    //                         {${} ${[}  ${} ${expression} ${} ${]} } |
    //                         {${} ${(}  ${} ${argument-expression-list}|${ε} ${} ${)} } |
    //                         {${} ${.}  ${} ${identifier}} |
    //                         {${} ${++} } |
    //                         {${} ${--} }
    //                      }^*

    Begin

    if (!parsePrimaryExpression(currentChild, codeGenerationData)) return False;
    NextChild

    while (currentChild) {

        if (Equals("[")) {
            Append("[")
            NextChild

            if (!parseExpression(currentChild, codeGenerationData)) return False;
            NextChild

            Append("]")
        } else if (Equals("argument-expression-list")) {
            Append("(")
            if (!parseArgumentExpressionList(currentChild, codeGenerationData)) return False;
            Append(")")
        } else if (Equals(".")) {
            Append(".")
            NextChild

            if (!parseIdentifier(currentChild, codeGenerationData)) return False;
        } else {
            Append(VALUE)
        }

        NextChild
    }

    return True;
}

static boolean parseUnaryExpression(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

    // unary-expression  = ${postfix-expression} |
    //                     { ${++}             ${} ${unary-expression} } |
    //                     { ${--}             ${} ${unary-expression} } |
    //                     { ${unary-operator} ${}  ${cast-expression} }

    Begin
    if (Equals("postfix-expression")) return parsePostFixExpression(currentChild, codeGenerationData);

    // Parse operator,
    Append(VALUE)
    NextChild

    boolean success = False;
    if (Equals("unary-expression")) {
        success = parseUnaryExpression(currentChild, codeGenerationData);
    } else if (Equals("cast-expression")) {
        success = parseCastExpression(currentChild, codeGenerationData);
    }

    return success;
}

static boolean parseCastExpression(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {
    // cast-expression = ${unary-expression} |
    //                   { ${(} ${} ${identifier} ${} ${)} ${} ${cast-expression} }

    Begin

    if (Equals("unary-expression")) return parseUnaryExpression(currentChild, codeGenerationData);

    // TODO: make sure the identifier is a valid type name (take care of classes)...
    Append("(")
    Append(VALUE)
    NextChild
    Append(")")

    return parseCastExpression(currentChild, codeGenerationData);
}

static boolean parseMultiplicativeExpression(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {
    // multiplicative-expression = ${cast-expression} {
    //                                ${} ${*}|${/}|${%} ${} ${cast-expression}
    //                             }^*

    Begin

    if (!parseCastExpression(currentChild, codeGenerationData)) return False;
    NextChild

    while (currentChild) {
        Append(" ")
        Append(VALUE)
        Append(" ")
        NextChild

        if (!parseCastExpression(currentChild, codeGenerationData)) return False;
        NextChild
    }
    return True;
}

static boolean parseAdditiveExpression(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {
    // additive-expression = ${multiplicative-expression} {
    //                          ${} ${+}|${-} ${} ${multiplicative-expression}
    //                       }^*

    Begin

    if (!parseMultiplicativeExpression(currentChild, codeGenerationData)) return False;
    NextChild

    while (currentChild) {
        Append(" ")
        Append(VALUE)
        Append(" ")
        NextChild

        if (!parseMultiplicativeExpression(currentChild, codeGenerationData)) return False;
        NextChild
    }
    return True;
}

static boolean parseShiftExpression(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {
    // shift-expression = ${additive-expression} {
    //                       ${} ${<<}|${>>} ${} ${additive-expression}
    //                    }^*

    Begin

    if (!parseAdditiveExpression(currentChild, codeGenerationData)) return False;
    NextChild

    while (currentChild) {
        Append(" ")
        Append(VALUE)
        Append(" ")
        NextChild

        if (!parseAdditiveExpression(currentChild, codeGenerationData)) return False;
        NextChild
    }
    return True;
}

static boolean parseRelationalExpression(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {
    // relational-expression = ${shift-expression} {
    //                            ${} #{{<} {>} {<=} {>=}} ${} ${shift-expression}
    //                         }^*

    Begin

    if (!parseShiftExpression(currentChild, codeGenerationData)) return False;
    NextChild

    while (currentChild) {
        Append(" ")
        Append(VALUE)
        Append(" ")
        NextChild

        if (!parseShiftExpression(currentChild, codeGenerationData)) return False;
        NextChild
    }
    return True;
}

static boolean parseEqualityExpression(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {
    // equality-expression = ${relational-expression} {
    //                          ${} ${==}|${!=} ${} ${relational-expression}
    //                       }^*

    Begin

    if (!parseRelationalExpression(currentChild, codeGenerationData)) return False;
    NextChild

    while (currentChild) {
        Append(" ")
        Append(VALUE)
        Append(" ")
        NextChild

        if (!parseRelationalExpression(currentChild, codeGenerationData)) return False;
        NextChild
    }
    return True;
}

static boolean parseAndExpression(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {
    // and-expression = ${equality-expression} {
    //                     ${} #{{&} {&&} != {&&}} ${} ${equality-expression}
    //                  }^*

    Begin

    if (!parseEqualityExpression(currentChild, codeGenerationData)) return False;
    NextChild

    while (currentChild) {
        Append(" & ")
        NextChild

        if (!parseEqualityExpression(currentChild, codeGenerationData)) return False;
        NextChild
    }
    return True;
}

static boolean parseXorExpression(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {
    // xor-expression = ${and-expression} {
    //                     ${} ${^} ${} ${and-expression}
    //                  }^*

    Begin

    if (!parseAndExpression(currentChild, codeGenerationData)) return False;
    NextChild

    while (currentChild) {
        Append(" ^ ")
        NextChild

        if (!parseAndExpression(currentChild, codeGenerationData)) return False;
        NextChild
    }
    return True;
}

static boolean parseOrExpression(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {
    // or-expression = ${xor-expression} {
    //                    ${} #{{|} {||} != {||}} ${} ${xor-expression}
    //                 }^*

    Begin

    if (!parseXorExpression(currentChild, codeGenerationData)) return False;
    NextChild

    while (currentChild) {
        Append(" | ")
        NextChild

        if (!parseXorExpression(currentChild, codeGenerationData)) return False;
        NextChild
    }
    return True;
}

static boolean parseLogicalAndExpression(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {
    // logical-and-expression = ${or-expression} {
    //                             ${} ${&&} ${} ${or-expression}
    //                          }^*

    Begin

    if (!parseOrExpression(currentChild, codeGenerationData)) return False;
    NextChild

    while (currentChild) {
        Append(" && ")
        NextChild

        if (!parseOrExpression(currentChild, codeGenerationData)) return False;
        NextChild
    }
    return True;
}

static boolean parseLogicalOrExpression(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {
    // logical-or-expression = ${logical-and-expression} {
    //                            ${} ${||} ${} ${logical-and-expression}
    //                         }^*

    Begin

    if (!parseLogicalAndExpression(currentChild, codeGenerationData)) return False;
    NextChild

    while (currentChild) {
        Append(" || ")
        NextChild

        if (!parseLogicalAndExpression(currentChild, codeGenerationData)) return False;
        NextChild
    }
    return True;
}

static boolean parseConditionalExpression(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {
    // conditional-expression = ${logical-or-expression} |
    //                          {${logical-or-expression} ${} ${?} ${} ${expression} ${} ${:} ${} ${conditional-expression}}

    Begin

    if (!parseLogicalOrExpression(currentChild, codeGenerationData)) return False;
    NextChild
    if (!currentChild) return True;

    Append(" ? ")
    if (!parseExpression(currentChild, codeGenerationData)) return False;
    NextChild

    Append(" : ")
    return parseConditionalExpression(currentChild, codeGenerationData);
}

static boolean parseAssignmentExpression(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {
    // assignment-expression = ${conditional-expression} |
    //                         {${unary-expression} ${} ${assignment-operator} ${} ${assignment-expression}}

    Begin

    // Parse conditional expression,
    if (Equals("conditional-expression")) return parseConditionalExpression(currentChild, codeGenerationData);

    // Parse assignee
    if (!parseUnaryExpression(currentChild, codeGenerationData)) return False;
    NextChild

    // Operator,
    Append(" ")
    Append(VALUE)
    Append(" ")
    NextChild

    // Parse assignment expression,
    return parseAssignmentExpression(currentChild, codeGenerationData);
}

static boolean parseExpression(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {
    // expression = ${assignment-expression} {
    //                 ${} ${,} ${} ${assignment-expression}
    //              }^*

    Begin

    if (!parseAssignmentExpression(currentChild, codeGenerationData)) return False;
    NextChild

    while (currentChild) {
        NextChild // Skip the comma.
        if (!parseAssignmentExpression(currentChild, codeGenerationData)) return False;
        NextChild
    }
    return True;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Statements
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static boolean isStatementEmpty(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

    struct NCC_ASTNode* statementNode = *(struct NCC_ASTNode**) NVector.get(&tree->childNodes, 0);
    if (NCString.equals(NString.get(&statementNode->name), "expression-statement")) {
        struct NCC_ASTNode* firstChildNode = *(struct NCC_ASTNode**) NVector.get(&statementNode->childNodes, 0);
        return NCString.equals(NString.get(&firstChildNode->name), ";");
    }
    return False;
}

static boolean parseLabeledStatement(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

    // labeled-statement =
    //                 {${identifier}                      ${} ${:} ${} ${statement}} |
    //                 {${case} ${} ${constant-expression} ${} ${:} ${} ${statement}} |
    //                 {${default}                         ${} ${:} ${} ${statement}}

    Begin

    if (Equals("case")) {
        Append("case ")
        NextChild
    }

    Append(VALUE)
    Append(": ")
    NextChild

    return parseStatement(currentChild, codeGenerationData);
}

static boolean parseCompoundStatement(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData, struct NVector* predefinedLocalVariables) {

    // compound-statement = ${OB} ${} ${block-item-list}|${ε} ${} ${CB}
    // block-item = #{{declaration} {statement}}

    Begin

    boolean parsedSuccessfully = False;

    // Create a new scope and load it with existing local variables (if any),
    pushNewScope(codeGenerationData);
    if (predefinedLocalVariables) {
        int32_t predefinedLocalVariablesCount = NVector.size(predefinedLocalVariables);
        for (int32_t i=0; i<predefinedLocalVariablesCount; i++) {
            addLocalVariable(*(struct VariableInfo**) NVector.get(predefinedLocalVariables, i), codeGenerationData);
        }
    }

    // Skip {,
    Append("{")
    NextChild
    if (!Equals("CB")) Append("\n")
    codeGenerationData->indentationCount++;

    // Parse block items,
    while (!Equals("CB")) {

        if (Equals("declaration")) {
            if (!parseLocalVariableDeclaration(currentChild, codeGenerationData)) goto finish;
        } else if (Equals("statement")) {
            if (!parseStatement(currentChild, codeGenerationData)) goto finish;
        } else {
            NERROR("CodeGeneration.parseCompoundStatement()", "Unreachable code. Found a %s%s%s.", NTCOLOR(HIGHLIGHT), VALUE, NTCOLOR(STREAM_DEFAULT));
            goto finish;
        }

        NextChild
    }

    codeGenerationData->indentationCount--;
    Append("}\n")
    parsedSuccessfully = True;

    finish:

    // Delete scope,
    popScope(codeGenerationData);
    return parsedSuccessfully;
}

static boolean parseExpressionStatement(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

    // expression-statement = ${expression}|${ε} ${} ${;}

    Begin
    boolean success = True;
    if (!Equals(";")) success = parseExpression(currentChild, codeGenerationData);
    Append(";\n")
    return success;
}

static boolean parseSelectionStatement(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

    // selection-statement =
    //                   { ${if}     ${} ${(} ${} ${expression} ${} ${)} ${} ${statement} {${} ${else} ${} ${statement}}|${ε} }
    //                   { ${switch} ${} ${(} ${} ${expression} ${} ${)} ${} ${statement}                                     }

    Begin

    Append(VALUE)
    Append(" (")
    NextChild

    if (!parseExpression(currentChild, codeGenerationData)) return False;
    NextChild

    Append(") ")
    if (!parseStatement(currentChild, codeGenerationData)) return False;
    NextChild

    // If no else,
    if (!currentChild) return True;

    // Remove the newline if a compound statement came before the else,
    if (NCString.endsWith(NString.get(&codeGenerationData->outString), "}\n")) {
        NString.trimEnd(&codeGenerationData->outString, "\n");
        Append(" else ")
    } else {
        Append("else ")
    }
    NextChild

    // Parse the else statement,
    return parseStatement(currentChild, codeGenerationData);
}

static boolean parseIterationStatement(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

    // iteration-statement =
    //                   { ${while} ${}                           ${(} ${} ${expression} ${} ${)} ${} ${statement} } |
    //                   { ${do}    ${} ${statement} ${} ${while} ${(} ${} ${expression} ${} ${)} ${} ${;}         } |
    //                   { ${for}   ${} ${(} ${} ${expression}|${ε} ${} ${;} ${} ${expression}|${ε} ${} ${;} ${} ${expression}|${ε} ${} ${)} ${} ${statement} } |
    //                   { ${for}   ${} ${(} ${} ${declaration}              ${} ${expression}|${ε} ${} ${;} ${} ${expression}|${ε} ${} ${)} ${} ${statement} }

    Begin

    if (Equals("while")) {

        // ${while} ${} ${(} ${} ${expression} ${} ${)} ${} ${statement}

        Append("while (")
        NextChild
        if (!parseExpression(currentChild, codeGenerationData)) return False;

        NextChild

        if (isStatementEmpty(currentChild, codeGenerationData)) {
            Append(");\n")
        } else {
            Append(") ");
            return parseStatement(currentChild, codeGenerationData);
        }
        return True;

    } else if (Equals("do")) {

        // ${do} ${} ${statement} ${} ${while} ${(} ${} ${expression} ${} ${)} ${} ${;}

        Append("do ")
        NextChild

        if (!parseStatement(currentChild, codeGenerationData)) return False;
        NextChild

        Append("while (")
        NextChild

        if (!parseExpression(currentChild, codeGenerationData)) return False;

        Append(");\n")
        return True;

    } else if (Equals("for")) {

        // ${for} ${} ${(} ${} ${expression}|${ε} ${} ${;} ${} ${expression}|${ε} ${} ${;} ${} ${expression}|${ε} ${} ${)} ${} ${statement}
        // ${for} ${} ${(} ${} ${declaration}              ${} ${expression}|${ε} ${} ${;} ${} ${expression}|${ε} ${} ${)} ${} ${statement}

        boolean success=False;
        pushNewScope(codeGenerationData);

        Append("for (")
        NextChild

        if (Equals(";")) {
            // Just skip,
            Append(";")
            NextChild
        } else if (Equals("expression")) {
            if (!parseExpression(currentChild, codeGenerationData)) goto forFinish;
            NextChild

            // Skip the ;
            Append(";")
            NextChild
        } else if (Equals("declaration")) {
            if (!parseLocalVariableDeclaration(currentChild, codeGenerationData)) goto forFinish;
            NextChild
            NString.trimEnd(&codeGenerationData->outString, "\n");
        }

        // Now parse the condition expression (if any),
        if (Equals("expression")) {
            Append(" ")
            if (!parseExpression(currentChild, codeGenerationData)) goto forFinish;
            NextChild
        }

        // Skip the ;
        Append(";")
        NextChild

        // Then parse the increment expression (if any),
        if (Equals("expression")) {
            Append(" ")
            if (!parseExpression(currentChild, codeGenerationData)) goto forFinish;
            NextChild
        }

        if (isStatementEmpty(currentChild, codeGenerationData)) {
            Append(");\n")
        } else {
            Append(") ")
            // Parse the statement,
            if (!parseStatement(currentChild, codeGenerationData)) goto forFinish;
        }

        success = True;
        forFinish:
        popScope(codeGenerationData);
        return success;
    }

    return False; // Unreachable.
}

static boolean parseJumpStatement(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

    // jump-statement =
    //              { ${goto}     ${} ${identifier}      ${} ${;} } |
    //              { ${continue} ${}                        ${;} } |
    //              { ${break}    ${}                        ${;} } |
    //              { ${return}   ${} ${expression}|${ε} ${} ${;} }

    Begin

    Append(VALUE)
    NextChild

    if (Equals("expression")) {
        Append(" ")
        if (!parseExpression(currentChild, codeGenerationData)) return False;
    } else if (Equals("identifier")) {
        Append(" ")
        Append(VALUE)
    }

    Append(";\n")

    return True;
}

static boolean parseStatement(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

    // statement = #{   {labeled-statement}
    //                 {compound-statement}
    //               {expression-statement}
    //                {selection-statement}
    //                {iteration-statement}
    //                     {jump-statement}}

    Begin

    boolean success=False;
    if (Equals("labeled-statement")) {
        success = parseLabeledStatement(currentChild, codeGenerationData);
    } else if (Equals("compound-statement")) {
        success = parseCompoundStatement(currentChild, codeGenerationData, 0);
    } else if (Equals("expression-statement")) {
        success = parseExpressionStatement(currentChild, codeGenerationData);
    } else if (Equals("selection-statement")) {
        success = parseSelectionStatement(currentChild, codeGenerationData);
    } else if (Equals("iteration-statement")) {
        success = parseIterationStatement(currentChild, codeGenerationData);
    } else if (Equals("jump-statement")) {
        success = parseJumpStatement(currentChild, codeGenerationData);
    }

    return success;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Translation unit
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// K&R function definition style. See: https://stackoverflow.com/a/18820829/1942069
//int foo(a,b) int a, b; {
//}

static boolean parseExternalDeclaration(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

    // external-declaration = #{{function-declaration}
    //                          {function-definition}
    //                          {declaration}
    //                          {class-declaration}}

    Begin
    boolean parsedSuccessfully = False;
    if (Equals("function-declaration")) {
        parsedSuccessfully = parseGlobalFunctionDeclaration(currentChild, codeGenerationData);
    } else if (Equals("function-definition")) {
        parsedSuccessfully = parseGlobalFunctionDefinition(currentChild, codeGenerationData);
    } else if (Equals("declaration")) {
        parsedSuccessfully = parseVariableDeclaration(currentChild, codeGenerationData, &codeGenerationData->globalVariables, True);
    } else if (Equals("class-declaration")) {
        parsedSuccessfully = parseClassDeclaration(currentChild, codeGenerationData);
    }

    return parsedSuccessfully;
}

static boolean parseTranslationUnit(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

    // translation-unit =
    //                 ${} ${external-declaration} {{
    //                     ${} ${external-declaration}
    //                 }^*} ${}

    // We have to check because this gets called from outside,
    if (!NCString.equals(NString.get(&tree->name), "translation-unit")) {
        NERROR("CodeGeneration.parseTranslationUnit()", "Expecting translation unit, found: %s%s%s.", NTCOLOR(HIGHLIGHT), NString.get(&tree->name), NTCOLOR(STREAM_DEFAULT));
        return False;
    }

    Begin

    while (currentChild) {
        if (!parseExternalDeclaration(currentChild, codeGenerationData)) return False;
        NextChild
    }

    return True;
}

boolean generateCode(struct NCC_ASTNode* tree, struct NString* outString) {

    // TODO: Update class-specifier rule to set a new listener ?

    // Generate code,
    boolean codeGeneratedSuccessfully = False;
    struct CodeGenerationData codeGenerationData;
    initializeCodeGenerationData(&codeGenerationData);
    if (!parseTranslationUnit(tree, &codeGenerationData)) goto finish;
    codeGeneratedSuccessfully = True;

    // Copy generated code onto output,
    NString.set(outString, "%s", NString.get(&codeGenerationData.outString));

    // Generate global variables code,
    NString.set(&codeGenerationData.outString, "");
    int32_t globalVariablesCount = NVector.size(&codeGenerationData.globalVariables);
    for (int32_t i=0; i<globalVariablesCount; i++) {
        struct VariableInfo* variable = *(struct VariableInfo**) NVector.get(&codeGenerationData.globalVariables, i);
        appendVariableDeclarationCode(variable, &codeGenerationData, "", "");
        codeAppend(&codeGenerationData, "\n");
    }
    if (globalVariablesCount) codeAppend(&codeGenerationData, "\n");

    // Prepend to the rest of the code,
    NString.append(&codeGenerationData.outString, "%s", NString.get(outString));
    NString.set(outString, "%s", NString.get(&codeGenerationData.outString));

    finish:
    destroyCodeGenerationData(&codeGenerationData);
    return codeGeneratedSuccessfully;
}
