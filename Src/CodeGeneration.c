
//
// Converting Addaat code to C code, enforcing language symantics in the process.
//
// By Omar El Sayyed.
// The 8th of August, 2022.
//

// TODO: add failed rule to matching result...
// TODO: perform address translation based on a flag...
// TODO: fix arrays are not primitive types...
// TODO: remove the need to write "class" before declaring a class...
// TODO: replace function pointers with interfaces...

#include <CodeGeneration.h>

#include <NCC.h>
#include <NSystemUtils.h>
#include <NCString.h>
#include <NError.h>

#define COLORIZE_CODE 1

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

static void generateCodeImplementation(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData);
static boolean parseCompoundStatement(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData);

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
    struct NVector classes;   // struct ClassInfo*
    struct NVector functions; // struct FunctionInfo*

    // Context,
    struct ClassInfo* currentClass;
    struct FunctionInfo* currentFunction;
    struct NVector scopeVariables; // struct NVector* (struct VariableInfo*)
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
    NVector.initialize(&codeGenerationData->classes  , 0, sizeof(struct ClassInfo*));
    NVector.initialize(&codeGenerationData->functions, 0, sizeof(struct FunctionInfo*));

    // Context,
    codeGenerationData->currentClass = 0;
    codeGenerationData->currentFunction = 0;
    NVector.initialize(&codeGenerationData->scopeVariables, 0, sizeof(struct NVector*));
}

static void destroyCodeGenerationData(struct CodeGenerationData* codeGenerationData) {
    NString.destroy(&codeGenerationData->outString);
    NVector.destroy(&codeGenerationData->colorStack);

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
    NVector.destroy(&codeGenerationData->scopeVariables);
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
        currentChild = node ? *node : 0; \
    }

#define VALUE NString.get(&currentChild->value)

#define Equals(text) \
    NCString.equals(NString.get(&currentChild->name), text)

#define Append(text) \
    codeAppend(codeGenerationData, text);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Variable
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void destroyAndDeleteVariableInfo(struct VariableInfo* variableInfo) {
    NString.destroy(&variableInfo->name);
    NFREE(variableInfo, "CodeGeneration.destroyAndDeleteVariableInfo() variableInfo");
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
    while(currentChild) {
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

static boolean parseVariableDeclaration(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData, struct NVector* outputVector) {

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
    if (getVariable(outputVector, VALUE)) {
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
        if (getVariable(outputVector, VALUE)) {
            NERROR("parseVariableDeclaration()", "Variable redefinition: %s%s%s.", NTCOLOR(HIGHLIGHT), VALUE, NTCOLOR(STREAM_DEFAULT));
            return False;
        }

        // Create a new variable that's a copy of the first one but with a different name,
        struct VariableInfo* anotherNewVariable = NMALLOC(sizeof(struct VariableInfo), "CodeGeneration.parseVariableDeclaration() anotherNewVariable");
        *anotherNewVariable = *newVariable;
        NString.initialize(&anotherNewVariable->name, "%s", VALUE);
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void destroyAndDeleteFunctionInfo(struct FunctionInfo* functionInfo) {
    NString.destroy(&functionInfo->name);
    for (int32_t i=NVector.size(&functionInfo->parameters)-1; i>=0; i--) {
        destroyAndDeleteVariableInfo(*(struct VariableInfo**) NVector.get(&functionInfo->parameters, i));
    }
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

static struct FunctionInfo* parseFunctionDeclaration(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

    // function-declaration: void main();
    // ├─type-specifier: void
    // │ └─void: void
    // │
    // ├─insert space:
    // ├─identifier: main
    // │ └─identifier-content: main
    // │
    // ├─(: (
    // ├─): )
    // ├─;: ;
    // └─insert \n:

    // ${declaration-specifiers} ${+ }
    // ${identifier} ${}
    // ${(} ${} ${parameter-list}|${ε} ${} ${)} ${} ${;} ${+\n}

    Begin

    // Create function,
    struct FunctionInfo* newFunction = NMALLOC(sizeof(struct FunctionInfo), "CodeGeneration.parseFunctionDeclaration() newFunction");
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
        NFREE(newFunction, "CodeGeneration.parseFunctionDeclaration() newFunction");
        return 0;
    }

    // Assign type,
    newFunction->returnType = *returnType;
    NFREE(returnType, "CodeGeneration.parseFunctionDeclaration() returnType");

    // Parse name,
    NextChild
    NString.initialize(&newFunction->name, "%s", VALUE);

    // Skip open parenthesis,
    NextChild

    // Parse parameter list,
    NVector.initialize(&newFunction->parameters, 0, sizeof(struct VariableInfo*));
    NextChild
    while (!Equals(")")) {

        // Parse type specifier,
        struct VariableType* parameterType = parseTypeSpecifier(currentChild, codeGenerationData);
        if (!parameterType) {
            destroyAndDeleteFunctionInfo(newFunction);
            return 0;
        }

        // Check for voids,
        if (parameterType->type == TYPE_VOID) {
            NERROR("parseFunctionDeclaration()", "Void is not a valid parameter type.");
            NFREE(parameterType, "CodeGeneration.parseFunctionDeclaration() parameterType 1");
            destroyAndDeleteFunctionInfo(newFunction);
            return 0;
        }

        // Check for duplicates,
        NextChild
        if (getVariable(&newFunction->parameters, VALUE)) {
            NERROR("parseFunctionDeclaration()", "Parameter redefinition: %s%s%s.", NTCOLOR(HIGHLIGHT), VALUE, NTCOLOR(STREAM_DEFAULT));
            NFREE(parameterType, "CodeGeneration.parseFunctionDeclaration() parameterType 2");
            destroyAndDeleteFunctionInfo(newFunction);
            return 0;
        }

        // Create a new parameter,
        struct VariableInfo* newParameter = NMALLOC(sizeof(struct VariableInfo), "CodeGeneration.parseFunctionDeclaration() newParameter");
        NString.initialize(&newParameter->name, "%s", VALUE);
        newParameter->isStatic = False;
        newParameter->type = *parameterType;
        NFREE(parameterType, "CodeGeneration.parseFunctionDeclaration() parameterType 3");
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

static void parseGlobalFunctionDeclaration(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

    Begin
    struct FunctionInfo* newFunction = parseFunctionDeclaration(currentChild, codeGenerationData);
    if (!newFunction) return;

    // If it's new, add it and return,
    struct FunctionInfo* existingFunction = getFunction(&codeGenerationData->functions, NString.get(&newFunction->name));
    if (!existingFunction) {
        NVector.pushBack(&codeGenerationData->functions, &newFunction);
        appendFunctionDeclarationCode(newFunction, codeGenerationData, "", "");
        return ;
    }

    // If it's a duplicate, check if the signature changed,
    // TODO: allow polymorphism...
    if (sameSignature(newFunction, existingFunction)) {
        appendFunctionDeclarationCode(newFunction, codeGenerationData, "", "");
    } else {
        NERROR("CodeGeneration.parseGlobalFunctionDeclaration()", "Function %s%s%s redeclared with a different signature.", NTCOLOR(HIGHLIGHT), NString.get(&existingFunction->name), NTCOLOR(STREAM_DEFAULT));
    }
    destroyAndDeleteFunctionInfo(newFunction);
}

static void parseGlobalFunctionDefinition(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

    Begin
    struct FunctionInfo* newFunction = parseFunctionDeclaration(currentChild, codeGenerationData);
    if (!newFunction) return;

    // Look for an existing declaration,
    struct FunctionInfo* existingFunction = getFunction(&codeGenerationData->functions, NString.get(&newFunction->name));
    if (existingFunction) {

        // If it's redefinition, throw,
        if (existingFunction->body) {
            NERROR("CodeGeneration.parseGlobalFunctionDefinition()", "Function %s%s%s redefinition.", NTCOLOR(HIGHLIGHT), NString.get(&existingFunction->name), NTCOLOR(STREAM_DEFAULT));
            destroyAndDeleteFunctionInfo(newFunction);
            return;
        }

        // Check if the signature changed,
        // TODO: allow polymorphism...
        if (!sameSignature(newFunction, existingFunction)) {
            NERROR("CodeGeneration.parseGlobalFunctionDefinition()", "Function %s%s%s defined with a different signature.", NTCOLOR(HIGHLIGHT), NString.get(&existingFunction->name), NTCOLOR(STREAM_DEFAULT));
            destroyAndDeleteFunctionInfo(newFunction);
            return;
        }
    } else {
        NVector.pushBack(&codeGenerationData->functions, &newFunction);
    }

    appendFunctionHeadCode(newFunction, codeGenerationData, "", "");
    Append(" ")
    codeGenerationData->indentationCount++;

    // Parse function body,
    NextChild
    newFunction->body = currentChild;
    codeGenerationData->currentFunction = newFunction;
    parseCompoundStatement(currentChild, codeGenerationData);
    codeGenerationData->currentFunction = 0;

    // Clean up,
    if (existingFunction) destroyAndDeleteFunctionInfo(newFunction);
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
    for (int32_t i=NVector.size(&classInfo->members)-1; i>=0; i--) {
        destroyAndDeleteVariableInfo(*(struct VariableInfo **) NVector.get(&classInfo->members, i));
    }
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

static void parseClassDeclaration(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

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
        return;
    }

    // Skip open bracket,
    if (class->defined) {
        NERROR("parseClassSpecifier()", "Class redefinition.");
        return ;
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

            return;
        }

        // Parse variable declaration,
        if (!parseVariableDeclaration(currentChild, codeGenerationData, &class->members)) return;

        NextChild
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Statements
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static boolean parseCompoundStatement(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

    // compound-statement = ${OB} ${} ${block-item-list}|${ε} ${} ${CB}
    // block-item = #{{declaration} {statement}}

    boolean parsedSuccessfully = False;

    // Create scope variables vector,
    struct NVector* scopeVariables = NVector.create(0, sizeof(struct VariableInfo*));
    NVector.pushBack(&codeGenerationData->scopeVariables, &scopeVariables);

    Begin

    // Skip {,
    NextChild

    // Parse block items,
    while (!Equals("CB")) {

        if (Equals("declaration")) {
            if (!parseVariableDeclaration(currentChild, codeGenerationData, scopeVariables)) goto finish;

            // TODO: pass static variables handler to parseVariableDeclaration()...
            // TODO: static variables should be declared globally with a prefix...
            // TODO: each scope should have a monotonically increasing number...

        }
    }

    // ...xxx

    finish:
    // Clean up,
    NVector.popBack(&codeGenerationData->scopeVariables, &scopeVariables);
    for (int32_t i=NVector.size(scopeVariables)-1; i>=0; i--) {
        destroyAndDeleteVariableInfo(*(struct VariableInfo**) NVector.get(scopeVariables, i));
    }
    NVector.destroyAndFree(scopeVariables);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Generation loop
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// K&R function definition style. See: https://stackoverflow.com/a/18820829/1942069
//int foo(a,b) int a, b; {
//}

static void generateCodeImplementation(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {
    // Moved the implementation to a separate function to remove the codeGenerationData from the interface.

    // TODO: get rid of this function eventually...

    const char* ruleNameCString = NString.get(&tree->name);

    boolean proceedToChildren = False;
    if (NCString.equals(ruleNameCString, "OB")) {
        codeAppend(codeGenerationData, "{");
        codeGenerationData->indentationCount++;
    } else if (NCString.equals(ruleNameCString, "CB")) {
        codeGenerationData->indentationCount--;
        codeAppend(codeGenerationData, "}");
    } else if (NCString.equals(ruleNameCString, "class-declaration")) {
        parseClassDeclaration(tree, codeGenerationData);
    } else if (NCString.equals(ruleNameCString, "function-declaration")) {
        parseGlobalFunctionDeclaration(tree, codeGenerationData);
    } else if (NCString.equals(ruleNameCString, "function-definition")) {
        parseGlobalFunctionDefinition(tree, codeGenerationData);
    } else {
        if (NVector.size(&tree->childNodes)) {
            proceedToChildren = True;
        } else {
            // Leaf node,
            codeAppend(codeGenerationData, NString.get(&tree->value));
        }
    }

    if (!proceedToChildren) return;
    // Not a leaf, print children,
    int32_t childrenCount = NVector.size(&tree->childNodes);
    for (int32_t i=0; i<childrenCount; i++) generateCodeImplementation(*((struct NCC_ASTNode**) NVector.get(&tree->childNodes, i)), codeGenerationData);
}

void generateCode(struct NCC_ASTNode* tree, struct NString* outString) {

    // TODO: Update class-specifier rule to set a new listener ?

    struct CodeGenerationData codeGenerationData;
    initializeCodeGenerationData(&codeGenerationData);
    generateCodeImplementation(tree, &codeGenerationData);
    NString.set(outString, "%s", NString.get(&codeGenerationData.outString));
    destroyCodeGenerationData(&codeGenerationData);
}
