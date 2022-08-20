
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Code constructs
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct ClassInfo {
    struct NString name;
    struct NVector members; // struct VariableInfo*.
    boolean defined;
};

struct VariableInfo {
    struct NString name;
    int32_t type;
    int32_t classIndex;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Forward declarations
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct CodeGenerationData;

static void generateCodeImplementation(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData);
static boolean handleIgnorables(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData);

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

static void destroyAndDeleteVariableInfo(struct VariableInfo* variableInfo) {
    NString.destroy(&variableInfo->name);
    NFREE(variableInfo, "CodeGeneration.destroyAndDeleteVariableInfo() variableInfo");
}

static void destroyAndDeleteClassInfo(struct ClassInfo* classInfo) {
    NString.destroy(&classInfo->name);
    for (int32_t i=NVector.size(&classInfo->members)-1; i>=0; i--) {
        destroyAndDeleteVariableInfo(*(struct VariableInfo **) NVector.getLast(&classInfo->members));
    }
    NVector.destroy(&classInfo->members);
    NFREE(classInfo, "CodeGeneration.destroyAndDeleteClassInfo() classInfo");
}

static void destroyCodeGenerationData(struct CodeGenerationData* codeGenerationData) {
    NString.destroy(&codeGenerationData->outString);
    NVector.destroy(&codeGenerationData->colorStack);

    // Classes,
    for (int32_t i=NVector.size(&codeGenerationData->classes)-1; i>=0; i--) {
        destroyAndDeleteClassInfo(*(struct ClassInfo**) NVector.getLast(&codeGenerationData->classes));
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
            NString.append(&codeGenerationData->outString, "   ");
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
    while (handleIgnorables(*((struct NCC_ASTNode**) NVector.get(&tree->childNodes, currentChildIndex)), codeGenerationData)) currentChildIndex++;

#define Begin \
    int32_t currentChildIndex = 0; \
    SkipIngorables \
    struct NCC_ASTNode* currentChild = *((struct NCC_ASTNode**) NVector.get(&tree->childNodes, currentChildIndex));

#define NextChild \
    currentChildIndex++; \
    SkipIngorables \
    currentChild = *((struct NCC_ASTNode**) NVector.get(&tree->childNodes, currentChildIndex));

#define Equals(text) \
    NCString.equals(NString.get(&currentChild->name), text)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Variable
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO: pass vector...
static void parseVariableDeclaration(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

    // declaration: static int[][] c = {{12}, {24}};
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
    // ├─init-declarator-list: c = {{12}, {24}}
    // │ └─init-declarator: c = {{12}, {24}}
    // │   ├─identifier: c
    // │   ├─insert space:
    // │   ├─=: =
    // │   ├─insert space:
    // │   ├─OB: {
    // │   ├─OB: {
    // │   ├─assignment-expression: 12
    // │   │ └─conditional-expression: 12
    // │   │   └─logical-or-expression: 12
    // │   │     └─logical-and-expression: 12
    // │   │       └─or-expression: 12
    // │   │         └─xor-expression: 12
    // │   │           └─and-expression: 12
    // │   │             └─equality-expression: 12
    // │   │               └─relational-expression: 12
    // │   │                 └─shift-expression: 12
    // │   │                   └─additive-expression: 12
    // │   │                     └─multiplicative-expression: 12
    // │   │                       └─cast-expression: 12
    // │   │                         └─unary-expression: 12
    // │   │                           └─postfix-expression: 12
    // │   │                             └─primary-expression: 12
    // │   │                               └─constant: 12
    // │   │                                 └─integer-constant: 12
    // │   │
    // │   ├─CB: }
    // │   ├─,: ,
    // │   ├─OB: {
    // │   ├─assignment-expression: 24
    // │   │ └─conditional-expression: 24
    // │   │   └─logical-or-expression: 24
    // │   │     └─logical-and-expression: 24
    // │   │       └─or-expression: 24
    // │   │         └─xor-expression: 24
    // │   │           └─and-expression: 24
    // │   │             └─equality-expression: 24
    // │   │               └─relational-expression: 24
    // │   │                 └─shift-expression: 24
    // │   │                   └─additive-expression: 24
    // │   │                     └─multiplicative-expression: 24
    // │   │                       └─cast-expression: 24
    // │   │                         └─unary-expression: 24
    // │   │                           └─postfix-expression: 24
    // │   │                             └─primary-expression: 24
    // │   │                               └─constant: 24
    // │   │                                 └─integer-constant: 24
    // │   │
    // │   ├─CB: }
    // │   └─CB: }
    // │
    // └─;: ;

    // ${declaration-specifiers} {${+ } ${init-declarator-list}}|${ε} ${} ${;}

    // TODO: create variable...

    Begin

    if (Equals("static")) {
        // ...xxx
    }


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

static struct ClassInfo* getClass(struct CodeGenerationData* codeGenerationData, const char* className) {
    for (int32_t i=NVector.size(&codeGenerationData->classes)-1; i>=0; i--) {
        struct ClassInfo* classInfo = *(struct ClassInfo**) NVector.getLast(&codeGenerationData->classes);
        if (NCString.equals(className, NString.get(&classInfo->name))) return classInfo;
    }
    return 0;
}

static void parseClassDeclaration(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

    // ${class} ${+ } ${identifier}
    //   {${} ${;} ${+\n}} |
    //   {${+ } ${OB} ${+\n} ${declaration-list} ${} ${CB} ${+\n}}

    Begin

    // Skip the "class" keyword,
    codeAppend(codeGenerationData, "struct");
    NextChild

    // Parse class name, if not and existing one, create new,
    const char* className = NString.get(&currentChild->value);
    struct ClassInfo* class = getClass(codeGenerationData, className);
    if (!class) class = createClass(codeGenerationData, className);
    NextChild

    // Return if semi-colon found (forward-declaration),
    if (Equals(";")) return;

    // Skip open bracket,
    if (class->defined) {
        NERROR("parseClassSpecifier()", "Class redefinition.");
        return ;
    }
    class->defined = True;

    // Parse declarations,
    /*while (True)*/ {
        // Check if closing bracket reached,
        NextChild
        if (Equals("CB")) return;

        // Parse variable declaration,
        parseVariableDeclaration(currentChild, codeGenerationData);
        //...xxx
    }

    //"${declaration} ${+\n} ${declaration-list}|${ε}"));

    // ${declaration-list} ${} ${CB} ${+\n}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Generation loop
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static boolean handleIgnorables(struct NCC_ASTNode* tree, struct CodeGenerationData* codeGenerationData) {

    const char* ruleNameCString = NString.get(&tree->name);
    if (NCString.equals(ruleNameCString, "insert space")) {
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

    struct CodeGenerationData codeGenerationData;
    initializeCodeGenerationData(&codeGenerationData);
    generateCodeImplementation(tree, &codeGenerationData);
    NString.set(outString, "%s", NString.get(&codeGenerationData.outString));
    destroyCodeGenerationData(&codeGenerationData);
}
