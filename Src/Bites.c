#include <LanguageDefinition.h>

#include <NCC.h>
#include <NSystemUtils.h>
#include <NCString.h>
#include <NError.h>

#define PRINT_TREES 1
#define PRINT_COLORED_TREES 1

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
    test(&ncc, "\"besm Allah\" //asdasdasdas\n  \"AlRa7maan AlRa7eem\"");

    // Clean up,
    NCC_destroyNCC(&ncc);
    NError.logAndTerminate();
}