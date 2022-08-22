
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

struct ClassInfo {
    struct NString name;
    struct NVector members; // struct VariableInfo*.
    boolean defined;
};

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forward declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct CodeGenerationData;

static void generateCodeImplementation(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData);
static boolean handleIgnorables(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData);
static boolean handleIgnorablesSilently(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData);

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
    struct NVector classes; // struct ClassInfo*
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
    NVector.initialize(&codeGenerationData->classes, 0, sizeof(struct ClassInfo*));
}

static void destroyCodeGenerationData(struct CodeGenerationData* codeGenerationData) {
    NString.destroy(&codeGenerationData->outString);
    NVector.destroy(&codeGenerationData->colorStack);

    // Classes,
    for (int32_t i=NVector.size(&codeGenerationData->classes)-1; i>=0; i--) {
        destroyAndDeleteClassInfo(*(struct ClassInfo**) NVector.get(&codeGenerationData->classes, i));
    }
    NVector.destroy(&codeGenerationData->classes);
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

#define SkipIngorables \
    while (True) { \
        struct NCC_ASTNode** node = NVector.get(&tree->childNodes, currentChildIndex); \
        if (!node) break; \
        if (!handleIgnorables(*node, codeGenerationData)) break; \
        currentChildIndex++; \
    }

#define SkipIngorablesSilently \
    while (True) { \
        struct NCC_ASTNode** node = NVector.get(&tree->childNodes, currentChildIndex); \
        if (!node) break; \
        if (!handleIgnorablesSilently(*node, codeGenerationData)) break; \
        currentChildIndex++; \
    }

#define Begin \
    int32_t currentChildIndex = 0; \
    SkipIngorablesSilently \
    struct NCC_ASTNode* currentChild = *((struct NCC_ASTNode**) NVector.get(&tree->childNodes, currentChildIndex));

#define NextChild \
    { \
        currentChildIndex++; \
        SkipIngorables \
        struct NCC_ASTNode** node = NVector.get(&tree->childNodes, currentChildIndex); \
        currentChild = node ? *node : 0; \
    }

#define NextChildSilently \
    { \
        currentChildIndex++; \
        SkipIngorablesSilently \
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
        NextChildSilently
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

    NextChildSilently
    while(currentChild) {
        // Parse array specifier(s),
        variableType->arrayDepth++;
        NextChildSilently
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
        NextChildSilently
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
    NextChildSilently
    if (getVariable(outputVector, VALUE)) {
        NERROR("parseVariableDeclaration()", "Variable redefinition: %s%s%s.", NTCOLOR(HIGHLIGHT), VALUE, NTCOLOR(STREAM_DEFAULT));
        NFREE( newVariable, "CodeGeneration.parseVariableDeclaration() newVariable 3");
        return False;
    }
    NString.initialize(&newVariable->name, "%s", VALUE);
    NVector.pushBack(outputVector, &newVariable);

    // Look for additional variables,
    NextChildSilently
    while (Equals(",")) {

        // Parse name and make sure it's not a redefinition,
        NextChildSilently
        if (getVariable(outputVector, VALUE)) {
            NERROR("parseVariableDeclaration()", "Variable redefinition: %s%s%s.", NTCOLOR(HIGHLIGHT), VALUE, NTCOLOR(STREAM_DEFAULT));
            return False;
        }

        // Create a new variable that's a copy of the first one but with a different name,
        struct VariableInfo* anotherNewVariable = NMALLOC(sizeof(struct VariableInfo), "CodeGeneration.parseVariableDeclaration() anotherNewVariable");
        *anotherNewVariable = *newVariable;
        NString.initialize(&anotherNewVariable->name, "%s", VALUE);
        NVector.pushBack(outputVector, &anotherNewVariable);
        NextChildSilently
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
    Append("struct");
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
    Append("{")
    NextChildSilently
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

        NextChildSilently
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Generation loop
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static boolean handleIgnorables(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

    if (!tree) return False;

    const char* ruleNameCString = NString.get(&tree->name);
    if        (NCString.equals(ruleNameCString, "insert space")) {
        codeAppend(codeGenerationData, " ");
    } else if (NCString.equals(ruleNameCString, "insert \n")) {
        if (!NCString.endsWith(NString.get(&codeGenerationData->outString), "\n")) codeAppend(codeGenerationData, "\n");
    } else if (NCString.equals(ruleNameCString, "insert \ns")) {
        codeAppend(codeGenerationData, "\n");
    } else if (NCString.equals(ruleNameCString, "line-cont")) {
        codeAppend(codeGenerationData, " \\\n");
    } else if (NCString.equals(ruleNameCString, "line-comment") || NCString.equals(ruleNameCString, "block-comment")) {
        NVector.pushBack(&codeGenerationData->colorStack, &NTCOLOR(BLACK_BRIGHT));
        codeAppend(codeGenerationData, NString.get(&tree->value));
        const char *color; NVector.popBack(&codeGenerationData->colorStack, &color);
    } else {
        return False;
    }
    return True;
}

static boolean handleIgnorablesSilently(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

    if (!tree) return False;

    const char* ruleNameCString = NString.get(&tree->name);
    return
        NCString.equals(ruleNameCString, "insert space" ) ||
        NCString.equals(ruleNameCString, "insert \n"    ) ||
        NCString.equals(ruleNameCString, "insert \ns"   ) ||
        NCString.equals(ruleNameCString, "line-cont"    ) ||
        NCString.equals(ruleNameCString, "line-comment" ) ||
        NCString.equals(ruleNameCString, "block-comment");
}

// K&R function definition style. See: https://stackoverflow.com/a/18820829/1942069
//int foo(a,b) int a, b; {
//}

static void generateCodeImplementation(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {
    // Moved the implementation to a separate function to remove the codeGenerationData from the interface.

    const char* ruleNameCString = NString.get(&tree->name);

    boolean proceedToChildren = False;
    if (handleIgnorables(tree, codeGenerationData)) {
    } else if (NCString.equals(ruleNameCString, "OB")) {
        codeAppend(codeGenerationData, "{");
        codeGenerationData->indentationCount++;
    } else if (NCString.equals(ruleNameCString, "CB")) {
        codeGenerationData->indentationCount--;
        codeAppend(codeGenerationData, "}");
    } else if (NCString.equals(ruleNameCString, "class-declaration")) {
        parseClassDeclaration(tree, codeGenerationData);
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
