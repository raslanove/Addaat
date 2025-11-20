
//
// Translates from Addaat (Bites) to C.
//
// By Omar El Sayyed.
// The 8th of August, 2022.
//

#include <LanguageDefinition.h>
#include <CodeGeneration.h>

#include <NCC.h>
#include <NSystemUtils.h>
#include <NCString.h>
#include <NError.h>

#define PRINT_TREES 1
#define PRINT_COLORED_TREES 1

#define PERFORM_ERROR_CHECKING_TESTS 0
#define PERFORM_REGULAR_TESTS 0

static boolean generate(struct NCC* ncc, const char* code, struct NString* outCode);

static void test(struct NCC* ncc, const char* code) {
    NLOGI("", "%sTesting: %s%s", NTCOLOR(GREEN_BRIGHT), NTCOLOR(BLUE_BRIGHT), code);

    struct NString generatedCode;
    NString.initialize(&generatedCode, "");
    generate(ncc, code, &generatedCode);
    NLOGI(0, "%s", NString.get(&generatedCode));
    NString.destroy(&generatedCode);
}

static boolean generate(struct NCC* ncc, const char* code, struct NString* outCode) {

    boolean success = False;
    struct NCC_MatchingResult matchingResult;
    struct NCC_ASTNode_Data tree;
    boolean matched = NCC_match(ncc, code, &matchingResult, &tree);
    if (matched && tree.node) {

        // Print tree,
        #if PRINT_TREES
        NString.set(outCode, "");
        NCC_ASTTreeToString(tree.node, 0, outCode, PRINT_COLORED_TREES /* should check isatty() */);
        NLOGI(0, "%s", NString.get(outCode));
        #endif

        // Generate code,
        NString.set(outCode, "");
        success = generateCode(tree.node, outCode);

        // Cleanup,
        NCC_deleteASTNode(&tree, 0);
    }
    int32_t codeLength = NCString.length(code);
    if (matched && matchingResult.matchLength == codeLength) {
        NLOGI(0, "Success!");
    } else {
        success = False;
        struct NString errorMessage;
        NString.initialize(&errorMessage, "Failed! Match: %s, length: %d\n", matched ? "True" : "False", matchingResult.matchLength);

        // Find the line and column numbers,
        int32_t line=1, column=1;
        for (int32_t i=0; i<ncc->maxMatchLength; i++) {
            if (code[i] == '\n') {
                line++;
                column = 1;
            } else {
                column++;
            }
        }
        NString.append(&errorMessage, "          Max match length: %d, line: %d, column: %d\n", ncc->maxMatchLength, line, column);

        // Print parent rules,
        const char* ruleName;
        while (NVector.popBack(&ncc->maxMatchRuleStack, &ruleName)) NString.append(&errorMessage, "            %s\n", ruleName);

        // Print the error message,
        NERROR(0, "%s", NString.get(&errorMessage));
        NString.destroy(&errorMessage);
    }
    NLOGI("", "");

    return success;
}

static boolean translateSingleFile(struct NCC* ncc, char* filePath) {

    if (!NCString.endsWith(filePath, ".addaat")) {
        NERROR("Addaat.translateSingleFile()", "Expecting a %s.addaat%s file, found: %s%s%s", NTCOLOR(HIGHLIGHT), NTCOLOR(STREAM_DEFAULT), NTCOLOR(HIGHLIGHT), filePath, NTCOLOR(STREAM_DEFAULT));
        return False;
    }

    // Read input file,
    uint32_t fileSize = NSystemUtils.getFileSize(filePath, False);
    char* code = NMALLOC(fileSize+1, "Addaat.translateSingleFile() code");
    NSystemUtils.readFromFile(filePath, False, 0, 0, code);
    code[fileSize] = 0;

    // Generate code,
    struct NString generatedCode;
    NString.initialize(&generatedCode, "");
    boolean success = generate(ncc, code, &generatedCode);
    NFREE(code, "Addaat.translateSingleFile() code");
    NLOGI(0, "%s", NString.get(&generatedCode));

    // Generate output file name (.addaat to .c),
    struct NString* outputFilePath = NString.create("%s", filePath);
    struct NString* tempString = NString.subString(outputFilePath, 0, NCString.length(filePath)-6);
    NString.set(outputFilePath, "%sc", NString.get(tempString));
    NString.destroyAndFree(tempString);

    // Write to output file,
    NSystemUtils.writeToFile(NString.get(outputFilePath), NString.get(&generatedCode), NString.length(&generatedCode), False);
    NString.destroyAndFree(outputFilePath);
    NString.destroy(&generatedCode);

    return success;
}

void NMain() {

    NSystemUtils.logI("", "besm Allah :)\n\n");

    // Language definition,
    struct NCC ncc;
    NCC_initializeNCC(&ncc);
    defineLanguage(&ncc);

    // Test,
    #if PERFORM_ERROR_CHECKING_TESTS
    test(&ncc, "class MyFirstClass;\n"
               "class MyFirstClass;\n"
               "class MyFirstClass {\n"
               "    int a, b;\n"
               "    float c;\n"
               "}\n"
               "class MyFirstClass {\n"
               "    int a, b;\n"
               "    float c;\n"
               "}\n");

    test(&ncc, "class MyFirstClass;\n"
               "class MyFirstClass;\n"
               "class MyFirstClass {\n"
               "    int a, b;\n"
               "    float c;\n"
               "}\n"
               "class MyFirstClass {\n"
               "    int a;\n"
               "}\n");

    test(&ncc, "class MyFirstClass {\n"
               "    static int[] a, b;\n"
               "    static int[][] c, d;\n"
               "    float d;\n"
               "}");
    #endif

    #if PERFORM_REGULAR_TESTS
    test(&ncc, "class MyFirstClass;");
    test(&ncc, "class MyFirstClass {}");
    test(&ncc, "class MyFirstClass {\n"
               "    static int[] a, b;\n"
               "    static double[][] c, d;\n"
               "    float e, f;\n"
               "}");

    test(&ncc, "void main();");
    test(&ncc, "void main() {\n"
               "    printf(\"besm Allah\\n\");\n"
               "}");

    test(&ncc, "int a;\n"
               "int a;\n"
               "void main() {\n"
               "    int a, b, d;\n"
               "    static int c;\n"
               "    {\n"
               "        int a;\n"
               "        static int c;\n"
               "        insideScope:;\n"
               "    }\n"
               "    goto insideScope;\n"
               "    if (1) {\n"
               "        printf(\"True\");\n"
               "    } else {\n"
               "        printf(\"False\");\n"
               "    }\n"
               "    while(1);\n"
               "    do;while(1);\n"
               "    for (int i; i<100; i++);\n"
               "    int i;\n"
               "    for (i=12+15; i<100; i++);\n"
               "    finish: return;\n"
               "}");
    #endif

    // Read test file,
    translateSingleFile(&ncc, "testCode.addaat");

    // Clean up,
    NCC_destroyNCC(&ncc);
    NError.logAndTerminate();
}
