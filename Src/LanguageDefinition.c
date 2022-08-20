
//
// Addaat language syntax.
//
// By Omar El Sayyed.
// The 8th of August, 2022.
//

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

    // Notes:
    // ======
    //  Leave right recursion as is.
    //  Convert left recursion into repeat or right recursion (note that right recursion inverses the order of operations).
    //    Example:
    //    ========
    //      Rule:
    //      -----
    //         shift-expression:
    //            additive-expression
    //            shift-expression << additive-expression
    //            shift-expression >> additive-expression
    //      Becomes:
    //      --------
    //         shift-expression:
    //            ${additive-expression} {
    //               { << ${additive-expression}} |
    //               { >> ${additive-expression}}
    //            }^*
    //      Or:
    //      --
    //         shift-expression:
    //            ${additive-expression} |
    //            { ${additive-expression} << ${shift-expression}} |
    //            { ${additive-expression} >> ${shift-expression}}
    //

    // TODO: do we need a ${} when all unnecessary whitespaces should be removed during pre-processing?
    //       ${} could necessary for code coloring, and not for compiling. This should be more obvious
    //       upon implementation.

    struct NCC_RuleData plainRuleData, pushingRuleData;
    NCC_initializeRuleData(&  plainRuleData, ncc, "", "",                 0,                 0,                0);
    NCC_initializeRuleData(&pushingRuleData, ncc, "", "", NCC_createASTNode, NCC_deleteASTNode, NCC_matchASTNode);

    // =====================================
    // Lexical rules,
    // =====================================

    // Tokens,
    NCC_addRule(pushingRuleData.set(&pushingRuleData,              "+",              "+"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,              "-",            "\\-"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,              "*",            "\\*"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,              "/",              "/"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,              "%",              "%"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,              "!",              "!"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,              "~",              "~"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,              "&",              "&"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,              "|",            "\\|"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,              "^",            "\\^"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,             "<<",             "<<"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,             ">>",             ">>"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,              "=",              "="));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,             "+=",             "+="));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,             "-=",        "\\-\\-="));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,             "*=",           "\\*="));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,             "/=",             "/="));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,             "%=",             "%="));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,            "<<=",            "<<="));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,            ">>=",            ">>="));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,             "^=",           "\\^="));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,             "&=",             "&="));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,             "|=",           "\\|="));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,             "==",             "=="));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,             "!=",             "!="));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,              "<",              "<"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,              ">",              ">"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,             "<=",             "<="));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,             ">=",             ">="));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,             "&&",             "&&"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,             "||",         "\\|\\|"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,              "(",              "("));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,              ")",              ")"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,              "[",              "["));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,              "]",              "]"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,             "OB",            "\\{"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,             "CB",            "\\}"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,              ":",              ":"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,              ";",              ";"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,              "?",              "?"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,              ",",              ","));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,              ".",              "."));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,             "++",             "++"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,             "--",         "\\-\\-"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,            "...",            "..."));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,          "class",          "class"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,           "enum",           "enum"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,             "if",             "if"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,           "else",           "else"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,          "while",          "while"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,             "do",             "do"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,            "for",            "for"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,       "continue",       "continue"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,          "break",          "break"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,         "return",         "return"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,         "switch",         "switch"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,           "case",           "case"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,        "default",        "default"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,           "goto",           "goto"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,           "void",           "void"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,           "char",           "char"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,          "short",          "short"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,            "int",            "int"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,           "long",           "long"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,          "float",          "float"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,         "double",         "double"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,         "signed",         "signed"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,       "unsigned",       "unsigned"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData,         "static",         "static"));

    // Space markers (forward declaration),
    NCC_addRule(pushingRuleData.set(&pushingRuleData, "insert space", ""));

    // Spaces and comments,
    NCC_addRule(  plainRuleData.set(&  plainRuleData, "ε", ""));
    NCC_addRule(pushingRuleData.set(&pushingRuleData, "line-cont", "\\\\\n"));
    NCC_addRule(  plainRuleData.set(&  plainRuleData, "white-space", "{\\ |\t|\r|\n|${line-cont}} {\\ |\t|\r|\n|${line-cont}}^*"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData, "line-comment", "${white-space} // {{* \\\\\n}^*} * \n|${ε}"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData, "block-comment", "${white-space} /\\* * \\*/"));
    NCC_addRule(  plainRuleData.set(&  plainRuleData, "ignorable", "#{{white-space} {line-comment} {block-comment}}"));
    NCC_addRule(  plainRuleData.set(&  plainRuleData,  "",              "${ignorable}^*"));
    NCC_addRule(  plainRuleData.set(&  plainRuleData, " ", "${ignorable} ${ignorable}^*"));

    // Space markers (implementation),
    NCC_addRule(  plainRuleData.set(&  plainRuleData, "+ ", "${} ${insert space}"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData, "insert \n" , ""));
    NCC_addRule(pushingRuleData.set(&pushingRuleData, "insert \ns", ""));
    NCC_addRule(  plainRuleData.set(&  plainRuleData, "+\n" , "${} ${insert \n}"));
    NCC_addRule(  plainRuleData.set(&  plainRuleData, "+\ns", "${} ${insert \ns}"));

    // TODO: use the non-ignorable white-spaces where they should be (like, between "int" and "a" in "int a;").

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
    //NCC_addRule(pushingRuleData.set(&pushingRuleData, "constant", "${integer-constant} | ${floating-constant} | ${enumeration-constant} | ${character-constant}"));
    //NCC_addRule(pushingRuleData.set(&pushingRuleData, "constant", "#{{integer-constant} {floating-constant} {enumeration-constant} {character-constant}}"));
    NCC_addRule(pushingRuleData.set(&pushingRuleData, "constant", "#{{integer-constant} {floating-constant} {enumeration-constant} {character-constant}}"));

    // String literal,
    // See: https://stackoverflow.com/a/13087264/1942069   and   https://stackoverflow.com/a/13445170/1942069
    NCC_addRule(pushingRuleData.set(&pushingRuleData, "string-literal-fragment", "{u8}|u|U|L|${ε} \" { ${c-char}|${hexadecimal-escape-sequence}|${universal-character-name}|{\\\\${c-char-with-backslash-without-uUxX}} }^* \""));
    NCC_addRule(pushingRuleData.set(&pushingRuleData, "string-literal", "${string-literal-fragment} {${} ${string-literal-fragment}}|${ε}"));

    // =====================================
    // Phrase structure,
    // =====================================

    // -------------------------------------
    // Expressions,
    // -------------------------------------

    // Primary expression,
    NCC_addRule   (  plainRuleData.set(&  plainRuleData, "expression", "STUB!"));
    NCC_addRule   (pushingRuleData.set(&pushingRuleData, "primary-expression",
                                       "${identifier} | "
                                       "${constant} | "
                                       "${string-literal} | "
                                       "{ ${(} ${} ${expression} ${} ${)} }"));

    // Postfix expression,
    NCC_addRule   (  plainRuleData.set(&plainRuleData, "type-name", "STUB!"));
    NCC_addRule   (  plainRuleData.set(&  plainRuleData, "argument-expression-list", "STUB!"));
    NCC_addRule   (pushingRuleData.set(&pushingRuleData, "postfix-expression",
                                       "${primary-expression} {"
                                       "   {${} ${[}  ${} ${expression} ${} ${]} } | "
                                       "   {${} ${(}  ${} ${argument-expression-list}|${ε} ${} ${)} } | "
                                       "   {${} ${.}  ${} ${identifier}} | "
                                       "   {${} ${++} } | "
                                       "   {${} ${--} }"
                                       "}^*"));

    // Argument expression list,
    NCC_addRule   (plainRuleData.set(&plainRuleData, "assignment-expression", "STUB!"));
    NCC_updateRule(plainRuleData.set(&plainRuleData, "argument-expression-list",
                                     "${assignment-expression} {"
                                     "   ${} ${,} ${+ } ${assignment-expression}"
                                     "}^*"));

    // Unary expression,
    NCC_addRule   (  plainRuleData.set(&  plainRuleData, "unary-expression", "STUB!"));
    NCC_addRule   (  plainRuleData.set(&  plainRuleData, "unary-operator", "STUB!"));
    NCC_addRule   (  plainRuleData.set(&  plainRuleData, "cast-expression", "STUB!"));
    NCC_updateRule(pushingRuleData.set(&pushingRuleData, "unary-expression",
                                       "${postfix-expression} | "
                                       "{ ${++}             ${} ${unary-expression} } | "
                                       "{ ${--}             ${} ${unary-expression} } | "
                                       "{ ${unary-operator} ${} ${cast-expression}  }"));

    // Unary operator,
    NCC_updateRule(  plainRuleData.set(&  plainRuleData, "unary-operator", "#{{+}{-}{~}{!} {++}{--} != {++}{--}}"));

    // Cast expression,
    NCC_updateRule(pushingRuleData.set(&pushingRuleData, "cast-expression",
                                       "${unary-expression} | "
                                       "{ ${(} ${} ${type-name} ${} ${)} ${} ${cast-expression} }"));

    // Multiplicative expression,
    NCC_addRule   (pushingRuleData.set(&pushingRuleData, "multiplicative-expression",
                                       "${cast-expression} {"
                                       "   ${+ } ${*}|${/}|${%} ${+ } ${cast-expression}"
                                       "}^*"));

    // Additive expression,
    NCC_addRule   (pushingRuleData.set(&pushingRuleData, "additive-expression",
                                       "${multiplicative-expression} {"
                                       "   ${+ } ${+}|${-} ${+ } ${multiplicative-expression}"
                                       "}^*"));

    // Shift expression,
    NCC_addRule   (pushingRuleData.set(&pushingRuleData, "shift-expression",
                                       "${additive-expression} {"
                                       "   ${+ } ${<<}|${>>} ${+ } ${additive-expression}"
                                       "}^*"));

    // Relational expression,
    NCC_addRule   (pushingRuleData.set(&pushingRuleData, "relational-expression",
                                       "${shift-expression} {"
                                       "   ${+ } #{{<} {>} {<=} {>=}} ${+ } ${shift-expression}"
                                       "}^*"));

    // Equality expression,
    NCC_addRule   (pushingRuleData.set(&pushingRuleData, "equality-expression",
                                       "${relational-expression} {"
                                       "   ${+ } ${==}|${!=} ${+ } ${relational-expression}"
                                       "}^*"));

    // AND expression,
    NCC_addRule   (pushingRuleData.set(&pushingRuleData, "and-expression",
                                       "${equality-expression} {"
                                       "   ${+ } #{{&} {&&} != {&&}} ${+ } ${equality-expression}"
                                       "}^*"));

    // Exclusive OR expression,
    NCC_addRule   (pushingRuleData.set(&pushingRuleData, "xor-expression",
                                       "${and-expression} {"
                                       "   ${+ } ${^} ${+ } ${and-expression}"
                                       "}^*"));

    // Inclusive OR expression,
    NCC_addRule   (pushingRuleData.set(&pushingRuleData, "or-expression",
                                       "${xor-expression} {"
                                       "   ${+ } #{{|} {||} != {||}} ${+ } ${xor-expression}"
                                       "}^*"));

    // Logical AND expression,
    NCC_addRule   (pushingRuleData.set(&pushingRuleData, "logical-and-expression",
                                       "${or-expression} {"
                                       "   ${+ } ${&&} ${+ } ${or-expression}"
                                       "}^*"));

    // Logical OR expression,
    NCC_addRule   (pushingRuleData.set(&pushingRuleData, "logical-or-expression",
                                       "${logical-and-expression} {"
                                       "   ${+ } ${||} ${+ } ${logical-and-expression}"
                                       "}^*"));

    // Conditional expression,
    NCC_addRule   (  plainRuleData.set(&  plainRuleData, "conditional-expression", "STUB!"));
    NCC_updateRule(pushingRuleData.set(&pushingRuleData, "conditional-expression",
                                       "${logical-or-expression} | "
                                       "{${logical-or-expression} ${+ } ${?} ${+ } ${expression} ${+ } ${:} ${+ } ${conditional-expression}}"));

    // Assignment expression,
    NCC_addRule   (  plainRuleData.set(&  plainRuleData, "assignment-operator", "STUB!"));
    NCC_updateRule(pushingRuleData.set(&pushingRuleData, "assignment-expression",
                                       "${conditional-expression} | "
                                       "{${unary-expression} ${+ } ${assignment-operator} ${+ } ${assignment-expression}}"));

    // Assignment operator,
    NCC_updateRule(  plainRuleData.set(&  plainRuleData, "assignment-operator", "#{{=} {*=} {/=} {%=} {+=} {-=} {<<=} {>>=} {&=} {^=} {|=}}"));

    // Expression,
    NCC_updateRule(pushingRuleData.set(&pushingRuleData, "expression",
                                       "${assignment-expression} {"
                                       "   ${} ${,} ${} ${assignment-expression}"
                                       "}^*"));

    NCC_addRule   (pushingRuleData.set(&pushingRuleData, "constant-expression", "${conditional-expression}"));

    // -------------------------------------
    // Declarations,
    // -------------------------------------

    // Declaration,
    NCC_addRule   (  plainRuleData.set(&  plainRuleData, "declaration-specifiers", "STUB!"));
    NCC_addRule   (  plainRuleData.set(&  plainRuleData, "identifier-list", "STUB!"));
    NCC_addRule   (pushingRuleData.set(&pushingRuleData, "declaration",
                                       "${declaration-specifiers} ${+ } ${identifier-list} ${} ${;}"));

    // Identifier list,
    NCC_updateRule(  plainRuleData.set(&  plainRuleData, "identifier-list",
                                       "${identifier} {"
                                       "   ${} ${,} ${+ } ${identifier}"
                                       "}^*"));

    // Declaration specifiers,
    NCC_addRule   (  plainRuleData.set(&  plainRuleData, "storage-class-specifier", "STUB!"));
    NCC_addRule   (  plainRuleData.set(&  plainRuleData, "type-specifier", "STUB!"));
    NCC_updateRule(  plainRuleData.set(&  plainRuleData, "declaration-specifiers",
                                       "${storage-class-specifier}|${ε} ${+ } ${type-specifier}"));

    // Storage class specifier,
    NCC_updateRule(  plainRuleData.set(&  plainRuleData, "storage-class-specifier",
                                       "#{{static} {identifier} != {identifier}}"));

    // Type specifier,
    NCC_addRule   (  plainRuleData.set(&  plainRuleData, "class-specifier", "STUB!"));
    NCC_addRule   (  plainRuleData.set(&  plainRuleData, "enum-specifier", "STUB!"));
    NCC_addRule   (  plainRuleData.set(&  plainRuleData, "array-specifier", "STUB!"));
    // TODO: Fix enum specifiers...
    NCC_updateRule(pushingRuleData.set(&pushingRuleData, "type-specifier",
                                       "#{{void}     {char}            "
                                       "  {short}    {int}      {long} "
                                       "  {float}    {double}          "
                                       "  {class-specifier}            "
                                       "  {enum-specifier}             "
                                       "  {identifier} != {identifier}}"
                                       "{${} ${array-specifier}}^*"));

    // Array specifier,
    NCC_updateRule(pushingRuleData.set(&pushingRuleData, "array-specifier",
                                       "${[} ${} ${]}"));

    // Class specifier,
    NCC_updateRule(pushingRuleData.set(&pushingRuleData, "class-specifier",
                                       "${identifier}"));

    // Class declaration,
    NCC_addRule   (  plainRuleData.set(&  plainRuleData, "declaration-list", "STUB!"));
    NCC_addRule   (pushingRuleData.set(&pushingRuleData, "class-declaration",
                                       "${class} ${+ } ${identifier} "
                                       "{${} ${;} ${+\n}} |"
                                       "{${+ } ${OB} ${+\n} ${declaration-list} ${} ${CB} ${+\n}}"));

    // Declaration list,
    NCC_updateRule(  plainRuleData.set(&  plainRuleData, "declaration-list",
                                       "${declaration} ${+\n} ${declaration-list}|${ε}"));

    // Enum specifier,
    NCC_addRule   (  plainRuleData.set(&  plainRuleData, "enumerator-list", "STUB!"));
    NCC_updateRule(  plainRuleData.set(&  plainRuleData, "enum-specifier",
                                       "{ ${enum} ${} ${identifier}|${ε} ${} ${OB} ${enumerator-list} ${} ${,}|${ε} ${} ${CB} } | "
                                       "{ ${enum} ${} ${identifier} }"));

    // Enumerator list,
    NCC_addRule   (  plainRuleData.set(&  plainRuleData, "enumerator", "STUB!"));
    NCC_updateRule(  plainRuleData.set(&  plainRuleData, "enumerator-list",
                                       "${enumerator} {"
                                       "   ${} ${,} ${+ } ${enumerator}"
                                       "}^*"));

    // Enumerator,
    NCC_updateRule(  plainRuleData.set(&  plainRuleData, "enumerator",
                                       "${enumeration-constant} { ${} = ${} ${constant-expression} }|${ε}"));

    // -------------------------------------
    // Statements,
    // -------------------------------------

    // Statement,
    NCC_addRule   (  plainRuleData.set(&  plainRuleData,    "labeled-statement", "STUB!"));
    NCC_addRule   (  plainRuleData.set(&  plainRuleData,   "compound-statement", "STUB!"));
    NCC_addRule   (  plainRuleData.set(&  plainRuleData, "expression-statement", "STUB!"));
    NCC_addRule   (  plainRuleData.set(&  plainRuleData,  "selection-statement", "STUB!"));
    NCC_addRule   (  plainRuleData.set(&  plainRuleData,  "iteration-statement", "STUB!"));
    NCC_addRule   (  plainRuleData.set(&  plainRuleData,       "jump-statement", "STUB!"));
    NCC_addRule   (pushingRuleData.set(&pushingRuleData, "statement",
                                       "#{   {labeled-statement}"
                                       "    {compound-statement}"
                                       "  {expression-statement}"
                                       "   {selection-statement}"
                                       "   {iteration-statement}"
                                       "        {jump-statement}}"));

    // Labeled statement,
    NCC_updateRule(pushingRuleData.set(&pushingRuleData, "labeled-statement",
                                       "{${identifier}                      ${} ${:} ${} ${statement}} | "
                                       "{${case} ${} ${constant-expression} ${} ${:} ${} ${statement}} | "
                                       "{${default}                         ${} ${:} ${} ${statement}}"));

    // Compound statement,
    NCC_addRule   (pushingRuleData.set(&pushingRuleData, "block-item-list", "STUB!"));
    NCC_updateRule(pushingRuleData.set(&pushingRuleData, "compound-statement",
                                       "${OB} ${} ${block-item-list}|${ε} ${} ${CB}"));

    // Block item list,
    NCC_addRule   (  plainRuleData.set(&  plainRuleData, "block-item", "STUB!"));
    NCC_updateRule(  plainRuleData.set(&  plainRuleData, "block-item-list",
                                       "${+\n} ${block-item} {{"
                                       "   ${+\n} ${block-item}"
                                       "}^*} ${+\n}"));

    // Block item,
    NCC_updateRule(  plainRuleData.set(&  plainRuleData, "block-item",
                                       "#{{declaration} {statement}}"));

    // Expression statement,
    NCC_updateRule(pushingRuleData.set(&pushingRuleData, "expression-statement",
                                       "${expression}|${ε} ${} ${;}"));

    // Selection statement,
    NCC_updateRule(pushingRuleData.set(&pushingRuleData, "selection-statement",
                                       "{ ${if}     ${} ${(} ${} ${expression} ${} ${)} ${} ${statement} {${} ${else} ${} ${statement}}|${ε} } | "
                                       "{ ${switch} ${} ${(} ${} ${expression} ${} ${)} ${} ${statement}                                     }"));

    // Iteration statement,
    NCC_updateRule(pushingRuleData.set(&pushingRuleData, "iteration-statement",
                                       "{ ${while} ${+ }                           ${(} ${} ${expression} ${} ${)} ${} ${;}|{${+ } ${statement}} } | "
                                       "{ ${do}    ${+ } ${statement} ${} ${while} ${(} ${} ${expression} ${} ${)} ${} ${;}                      } | "
                                       "{ ${for}   ${+ } ${(} ${} ${expression}|${ε} ${} ${;} ${+ } ${expression}|${ε} ${} ${;} ${+ } ${expression}|${ε} ${} ${)} ${} ${;}|{${+ } ${statement}} } | "
                                       "{ ${for}   ${+ } ${(} ${} ${declaration}              ${+ } ${expression}|${ε} ${} ${;} ${+ } ${expression}|${ε} ${} ${)} ${} ${;}|{${+ } ${statement}} }"));

    // Jump statement,
    NCC_updateRule(pushingRuleData.set(&pushingRuleData, "jump-statement",
                                       "{ ${goto}     ${} ${identifier}      ${} ${;} } | "
                                       "{ ${continue} ${}                        ${;} } | "
                                       "{ ${break}    ${}                        ${;} } | "
                                       "{ ${return}   ${} ${expression}|${ε} ${} ${;} }"));

    // -------------------------------------
    // External definitions,
    // -------------------------------------

    // Translation unit,
    NCC_addRule   (  plainRuleData.set(&  plainRuleData, "external-declaration", "STUB!"));
    NCC_addRule   (pushingRuleData.set(&pushingRuleData, "translation-unit",
                                       "${} ${external-declaration} {{"
                                       "   ${} ${+\ns} ${external-declaration}"
                                       "}^*} ${}")); // Encapsulated the repeat in a sub-rule to avoid early termination. Can \
                                                        we consider early termination a feature now?

    // External declaration,
    NCC_addRule   (  plainRuleData.set(&  plainRuleData, "function-definition", "STUB!"));
    NCC_updateRule(  plainRuleData.set(&  plainRuleData, "external-declaration",
                                       "#{{function-definition} {declaration} {class-declaration}}"));

    // Parameter declaration,
    NCC_addRule   (pushingRuleData.set(&pushingRuleData, "parameter-declaration",
                                       "${type-specifier} ${+ } ${identifier}"));

    // Parameter list,
    NCC_addRule   (  plainRuleData.set(&  plainRuleData, "parameter-list",
                                       "${parameter-declaration} {"
                                       "   ${} ${,} ${+ } ${parameter-declaration}"
                                       "}^*"));

    // Function definition,
    NCC_updateRule(pushingRuleData.set(&pushingRuleData, "function-definition",
                                       "${declaration-specifiers} ${+ } "
                                       "${identifier} ${+ } "
                                       "${(} ${} ${parameter-list} ${} ${)} ${+ } "
                                       "${compound-statement} ${+\n}"));

    // Test document,
    NCC_addRule   (pushingRuleData.set(&pushingRuleData, "TestDocument",
                                       "#{                          "
                                       "        {primary-expression}"
                                       "        {postfix-expression}"
                                       "          {unary-expression}"
                                       "           {cast-expression}"
                                       " {multiplicative-expression}"
                                       "       {additive-expression}"
                                       "          {shift-expression}"
                                       "     {relational-expression}"
                                       "       {equality-expression}"
                                       "            {and-expression}"
                                       "            {xor-expression}"
                                       "             {or-expression}"
                                       "    {logical-and-expression}"
                                       "     {logical-or-expression}"
                                       "    {conditional-expression}"
                                       "     {assignment-expression}"
                                       "                {expression}"
                                       "       {constant-expression}"
                                       "               {declaration}"
                                       "          {translation-unit}"
                                       "}                           "));
    NCC_setRootRule(ncc, "TestDocument");

    // Cleanup,
    NCC_destroyRuleData(&  plainRuleData);
    NCC_destroyRuleData(&pushingRuleData);
}
