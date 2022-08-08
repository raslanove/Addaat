
#include <LanguageDefinition.h>

#include <NCC.h>
#include <NSystemUtils.h>

static boolean printListener(struct NCC_MatchingData* matchingData) {
    NLOGI("HelloCC", "ruleName: %s", NString.get(&matchingData->node.rule->ruleName));
    NLOGI("HelloCC", "        Match length: %s%d%s", NTCOLOR(HIGHLIGHT), matchingData->matchLength, NTCOLOR(STREAM_DEFAULT));
    NLOGI("HelloCC", "        Matched text: %s%s%s", NTCOLOR(HIGHLIGHT), matchingData->matchedText, NTCOLOR(STREAM_DEFAULT));
    return True;
}

void defineLanguage(struct NCC* ncc) {

    struct NCC_RuleData plainRuleData, pushingRuleData;
    NCC_initializeRuleData(&  plainRuleData, ncc, "", "",                 0,                 0,                0);
    NCC_initializeRuleData(&pushingRuleData, ncc, "", "", NCC_createASTNode, NCC_deleteASTNode, NCC_matchASTNode);

    // Spaces and comments,
    NCC_addRule(  plainRuleData.set(&  plainRuleData, "ε", ""));
    NCC_addRule(pushingRuleData.set(&pushingRuleData, "line-cont", "\\\\\n"));
    NCC_addRule(  plainRuleData.set(&  plainRuleData, "white-space", "{\\ |\t|\r|\n|${line-cont}} {\\ |\t|\r|\n|${line-cont}}^*"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData, "line-comment", "${white-space} // {{* \\\\\n}^*} * \n|${ε}"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData, "block-comment", "${white-space} /\\* * \\*/"));
    NCC_addRule(  plainRuleData.set(&  plainRuleData, "ignorable", "#{{white-space} {line-comment} {block-comment}}"));
    NCC_addRule(  plainRuleData.set(&  plainRuleData,  "",              "${ignorable}^*"));
    NCC_addRule(  plainRuleData.set(&  plainRuleData, " ", "${ignorable} ${ignorable}^*"));

    // Literals,
    NCC_addRule(  plainRuleData.set(&  plainRuleData, "digit", "0-9"));
    NCC_addRule(  plainRuleData.set(&  plainRuleData, "non-zero-digit", "1-9"));
    NCC_addRule(  plainRuleData.set(&  plainRuleData, "non-digit", "_|a-z|A-Z"));
    NCC_addRule(  plainRuleData.set(&  plainRuleData, "hexadecimal-prefix", "0x|X"));
    NCC_addRule(  plainRuleData.set(&  plainRuleData, "hexadecimal-digit", "0-9|a-f|A-F"));
    NCC_addRule(  plainRuleData.set(&  plainRuleData, "hex-quad", "${hexadecimal-digit}${hexadecimal-digit}${hexadecimal-digit}${hexadecimal-digit}"));
    NCC_addRule(  plainRuleData.set(&  plainRuleData, "universal-character-name", "{\\\\u ${hex-quad}} | {\\\\U ${hex-quad} ${hex-quad}}"));

    // Identifier,
    NCC_addRule(  plainRuleData.set(&  plainRuleData, "identifier-non-digit", "${non-digit} | ${universal-character-name}"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData, "identifier", "${identifier-non-digit} {${digit} | ${identifier-non-digit}}^*"));

    // Constants,
    // Integer constant,
    NCC_addRule(plainRuleData.set(&plainRuleData, "decimal-constant", "${non-zero-digit} ${digit}^*"));
    NCC_addRule(plainRuleData.set(&plainRuleData, "octal-constant", "0 0-7^*"));
    NCC_addRule(plainRuleData.set(&plainRuleData, "hexadecimal-constant", "${hexadecimal-prefix} ${hexadecimal-digit} ${hexadecimal-digit}^*"));
    NCC_addRule(plainRuleData.set(&plainRuleData, "integer-suffix", "{ u|U l|L|{ll}|{LL}|${ε} } | { l|L|{ll}|{LL} u|U|${ε} }"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData, "integer-constant", "${decimal-constant}|${octal-constant}|${hexadecimal-constant} ${integer-suffix}|${ε}"));

    // Decimal floating point,
    NCC_addRule(plainRuleData.set(&plainRuleData, "fractional-constant", "{${digit}^* . ${digit} ${digit}^*} | {${digit} ${digit}^* . }"));
    NCC_addRule(plainRuleData.set(&plainRuleData, "exponent-part", "e|E +|\\-|${ε} ${digit} ${digit}^*"));
    NCC_addRule(plainRuleData.set(&plainRuleData, "floating-suffix", "f|l|F|L"));
    NCC_addRule(plainRuleData.set(&plainRuleData, "decimal-floating-constant",
                                  "{${fractional-constant} ${exponent-part}|${ε} ${floating-suffix}|${ε}} | "
                                  "{${digit} ${digit}^* ${exponent-part} ${floating-suffix}|${ε}}"));

    // Hexadecimal floating point,
    NCC_addRule(plainRuleData.set(&plainRuleData, "hexadecimal-fractional-constant",
                                  "{${hexadecimal-digit}^* . ${hexadecimal-digit} ${hexadecimal-digit}^*} | "
                                  "{${hexadecimal-digit} ${hexadecimal-digit}^* . }"));
    NCC_addRule(plainRuleData.set(&plainRuleData, "binary-exponent-part", "p|P +|\\-|${ε} ${digit} ${digit}^*"));
    NCC_addRule(plainRuleData.set(&plainRuleData, "hexadecimal-floating-constant",
                                  "${hexadecimal-prefix} ${hexadecimal-fractional-constant}|{${hexadecimal-digit}${hexadecimal-digit}^*} ${binary-exponent-part} ${floating-suffix}|${ε}"));

    // Floating point constant,
    NCC_addRule(pushingRuleData.set(&pushingRuleData, "floating-constant", "${decimal-floating-constant} | ${hexadecimal-floating-constant}"));

    // Enumeration constant,
    NCC_addRule(pushingRuleData.set(&pushingRuleData, "enumeration-constant", "${identifier}"));

    // Character constant (supporting unknown escape sequences which are implementation defined. We'll pass the escaped character like gcc and clang do),
    NCC_addRule(plainRuleData.set(&plainRuleData, "c-char", "\x01-\x09 | \x0b-\x5b | \x5d-\xff")); // All characters except new-line and backslash (\).
    NCC_addRule(plainRuleData.set(&plainRuleData, "c-char-with-backslash-without-uUxX", "\x01-\x09 | \x0b-\x54 | \x56-\x57| \x59-\x74 | \x76-\x77 | \x79-\xff")); // All characters except new-line, 'u', 'U', 'x' and 'X'.
    NCC_addRule(plainRuleData.set(&plainRuleData, "hexadecimal-escape-sequence", "\\\\x ${hexadecimal-digit} ${hexadecimal-digit}^*"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData, "character-constant", "L|u|U|${ε} ' { ${c-char}|${hexadecimal-escape-sequence}|${universal-character-name}|{\\\\${c-char-with-backslash-without-uUxX}} }^* '"));

    // Constant,
    NCC_addRule(pushingRuleData.set(&pushingRuleData, "constant", "#{{integer-constant} {floating-constant} {enumeration-constant} {character-constant}}"));

    // String literal,
    // See: https://stackoverflow.com/a/13087264/1942069   and   https://stackoverflow.com/a/13445170/1942069
    NCC_addRule(pushingRuleData.set(&pushingRuleData, "string-literal-fragment", "{u8}|u|U|L|${ε} \" { ${c-char}|${hexadecimal-escape-sequence}|${universal-character-name}|{\\\\${c-char-with-backslash-without-uUxX}} }^* \""));
    NCC_addRule(pushingRuleData.set(&pushingRuleData, "string-literal", "${string-literal-fragment} {${} ${string-literal-fragment}}|${ε}"));

    // Test document,
    NCC_addRule   (pushingRuleData.set(&pushingRuleData, "TestDocument", "${string-literal}"));
    NCC_setRootRule(ncc, "TestDocument");

    // Cleanup,
    NCC_destroyRuleData(&  plainRuleData);
    NCC_destroyRuleData(&pushingRuleData);
}
