
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
#define PERFORM_REGULAR_TESTS 1

static void test(struct NCC* ncc, const char* code) {

    NLOGI("", "%sTesting: %s%s", NTCOLOR(GREEN_BRIGHT), NTCOLOR(BLUE_BRIGHT), code);
    struct NCC_MatchingResult matchingResult;
    struct NCC_ASTNode_Data tree;
    boolean matched = NCC_match(ncc, code, &matchingResult, &tree);
    if (matched && tree.node) {
        struct NString treeString;

        // Print tree,
        NString.initialize(&treeString, "");
        #if PRINT_TREES
        NCC_ASTTreeToString(tree.node, 0, &treeString, PRINT_COLORED_TREES /* should check isatty() */);
        NLOGI(0, "%s", NString.get(&treeString));
        #endif

        // Generate code,
        NString.set(&treeString, "");
        generateCode(tree.node, &treeString);
        NLOGI(0, "%s", NString.get(&treeString));

        // Cleanup,
        NString.destroy(&treeString);
        NCC_deleteASTNode(&tree, 0);
    }
    int32_t codeLength = NCString.length(code);
    if (matched && matchingResult.matchLength == codeLength) {
        NLOGI("test()", "Success!");
    } else {
        NERROR("test()", "Failed! Match: %s, length: %d", matched ? "True" : "False", matchingResult.matchLength);
    }
    NLOGI("", "");
}

void NMain() {

    NSystemUtils.logI("", "besm Allah :)\n\n");

    // Language definition,
    struct NCC ncc;
    NCC_initializeNCC(&ncc);
    defineLanguage(&ncc);

    // Test,
    //test(&ncc, "\"besm Allah\" //asdasdasdas\n  \"AlRa7maan AlRa7eem\"");

    #if PERFORM_ERROR_CHECKING_TESTS
    test(&ncc, "class MyFirstClass;\n"   \
               "class MyFirstClass;\n"   \
               "class MyFirstClass {\n"  \
               "    int a, b;\n"         \
               "    float c;\n"          \
               "}\n"                    \
               "class MyFirstClass {\n"  \
               "    int a;\n"            \
               "}"
    );
    test(&ncc, "class MyFirstClass {\n"   \
               "    static int[] a, b;\n" \
               "    static int[][] c, d;\n" \
               "    float d;\n"           \
               "}");
    #endif

    #if PERFORM_REGULAR_TESTS

    test(&ncc, "class MyFirstClass;");
//    test(&ncc, "class MyFirstClass {}");
//    test(&ncc, "class MyFirstClass {\n"   \
//               "    static int[] a, b;\n" \
//               "    static int[][] c, d;\n" \
//               "    float e;\n"           \
//               "}");
    #endif



    // Clean up,
    NCC_destroyNCC(&ncc);
    NError.logAndTerminate();
}
