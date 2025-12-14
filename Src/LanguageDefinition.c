
//
// Addaat language syntax.
//
// By Omar El Sayyed.
// The 8th of August, 2022.
//

#include <LanguageDefinition.h>

#include <NCC.h>
#include <NSystemUtils.h>

// TODO: (performance improvement) reduce pushing rules as much as possible (like parenthesis, commas and such, they are not useful). \
         Remember to update code-generation to reflect changes...

static boolean printListener(NCC_MatchingData* matchingData) {
    NLOGI("HelloCC", "ruleName: %s", NString.get(&matchingData->node.rule->ruleName));
    NLOGI("HelloCC", "        Match length: %s%d%s", NTCOLOR(HIGHLIGHT), matchingData->matchLength, NTCOLOR(STREAM_DEFAULT));
    NLOGI("HelloCC", "        Matched text: %s%s%s", NTCOLOR(HIGHLIGHT), matchingData->matchedText, NTCOLOR(STREAM_DEFAULT));
    return True;
}

typedef struct RuleDefinitionData {
    struct NCC* ncc;
    NCC_RuleData plainRuleData, pushingRuleData;
} RuleDefinitionData;

static void addRule(RuleDefinitionData* rdd, const char* ruleName, const char* ruleText) {
    NCC_addRule(rdd->ncc, rdd->plainRuleData.set(&rdd->plainRuleData, ruleName, ruleText));
}

static void addPushingRule(RuleDefinitionData* rdd, const char* ruleName, const char* ruleText) {
    NCC_addRule(rdd->ncc, rdd->pushingRuleData.set(&rdd->pushingRuleData, ruleName, ruleText));
}

static void updateRule(RuleDefinitionData* rdd, const char* ruleName, const char* ruleText) {
    NCC_Rule* rule = NCC_getRule(rdd->ncc, ruleName);
    NCC_updateRuleText(rdd->ncc, rule, ruleText);
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

    RuleDefinitionData rdd = { .ncc = ncc };
    NCC_initializeRuleData(&rdd.  plainRuleData, "", "",                 0,                 0,                0);
    NCC_initializeRuleData(&rdd.pushingRuleData, "", "", NCC_createASTNode, NCC_deleteASTNode, NCC_matchASTNode);

    // =====================================
    // Lexical rules,
    // =====================================

    // TODO: add boolean, True and False...

    // Tokens,
    addPushingRule(&rdd,        "+",        "+");
    addPushingRule(&rdd,        "-",      "\\-");
    addPushingRule(&rdd,        "*",      "\\*");
    addPushingRule(&rdd,        "/",        "/");
    addPushingRule(&rdd,        "%",        "%");
    addPushingRule(&rdd,        "!",        "!");
    addPushingRule(&rdd,        "~",        "~");
    addPushingRule(&rdd,        "&",        "&");
    addPushingRule(&rdd,        "|",      "\\|");
    addPushingRule(&rdd,        "^",      "\\^");
    addPushingRule(&rdd,       "<<",       "<<");
    addPushingRule(&rdd,       ">>",       ">>");
    addPushingRule(&rdd,        "=",        "=");
    addPushingRule(&rdd,       "+=",       "+=");
    addPushingRule(&rdd,       "-=",  "\\-\\-=");
    addPushingRule(&rdd,       "*=",     "\\*=");
    addPushingRule(&rdd,       "/=",       "/=");
    addPushingRule(&rdd,       "%=",       "%=");
    addPushingRule(&rdd,      "<<=",      "<<=");
    addPushingRule(&rdd,      ">>=",      ">>=");
    addPushingRule(&rdd,       "^=",     "\\^=");
    addPushingRule(&rdd,       "&=",       "&=");
    addPushingRule(&rdd,       "|=",     "\\|=");
    addPushingRule(&rdd,       "==",       "==");
    addPushingRule(&rdd,       "!=",       "!=");
    addPushingRule(&rdd,        "<",        "<");
    addPushingRule(&rdd,        ">",        ">");
    addPushingRule(&rdd,       "<=",       "<=");
    addPushingRule(&rdd,       ">=",       ">=");
    addPushingRule(&rdd,       "&&",       "&&");
    addPushingRule(&rdd,       "||",   "\\|\\|");
    addRule       (&rdd,        "(",        "(");
    addRule       (&rdd,        ")",        ")");
    addPushingRule(&rdd,        "[",        "[");
    addPushingRule(&rdd,        "]",        "]");
    addPushingRule(&rdd,       "OB",      "\\{");
    addPushingRule(&rdd,       "CB",      "\\}");
    addRule       (&rdd,        ":",        ":");
    addPushingRule(&rdd,        ";",        ";");
    addRule       (&rdd,        "?",        "?");
    addPushingRule(&rdd,        ",",        ",");
    addPushingRule(&rdd,        ".",        ".");
    addPushingRule(&rdd,       "++",       "++");
    addPushingRule(&rdd,       "--",   "\\-\\-");
    addPushingRule(&rdd,      "...",      "...");
    addPushingRule(&rdd,    "class",    "class");
    addPushingRule(&rdd,     "enum",     "enum");
    addPushingRule(&rdd,       "if",       "if");
    addPushingRule(&rdd,     "else",     "else");
    addPushingRule(&rdd,    "while",    "while");
    addPushingRule(&rdd,       "do",       "do");
    addPushingRule(&rdd,      "for",      "for");
    addPushingRule(&rdd, "continue", "continue");
    addPushingRule(&rdd,    "break",    "break");
    addPushingRule(&rdd,   "return",   "return");
    addPushingRule(&rdd,   "switch",   "switch");
    addPushingRule(&rdd,     "case",     "case");
    addPushingRule(&rdd,  "default",  "default");
    addPushingRule(&rdd,     "goto",     "goto");
    addPushingRule(&rdd,     "void",     "void");
    addPushingRule(&rdd,     "char",     "char");
    addPushingRule(&rdd,    "short",    "short");
    addPushingRule(&rdd,      "int",      "int");
    addPushingRule(&rdd,     "long",     "long");
    addPushingRule(&rdd,    "float",    "float");
    addPushingRule(&rdd,   "double",   "double");
    addPushingRule(&rdd,   "signed",   "signed");
    addPushingRule(&rdd, "unsigned", "unsigned");
    addPushingRule(&rdd,   "static",   "static");

    // Keywords,
    addPushingRule(&rdd, "keyword", "#{{class} {enum} {if} {else} {while} {do} {for} {continue} {break} {return} {switch} {case} {default} {goto} {void} {char} {short} {int} {long} {float} {double} {signed} {unsigned} {static}}");

    // Spaces and comments,
    addRule       (&rdd, "ε", "");
    addRule       (&rdd, "line-cont", "\\\\\n");
    addRule       (&rdd, "white-space", "{\\ |\\\t|\r|\n|${line-cont}} {\\ |\\\t|\r|\n|${line-cont}}^*");
    addRule       (&rdd, "line-comment", "${white-space} // {{* \\\\\n}^*} * \n|${ε}");
    addRule       (&rdd, "block-comment", "${white-space} /\\* * \\*/");
    addRule       (&rdd, "ignorable", "#{{white-space} {line-comment} {block-comment}}");
    addRule       (&rdd,  "",              "${ignorable}^*");
    addRule       (&rdd, " ", "${ignorable} ${ignorable}^*"); // If we need to force matching at least 1 ignorable.

    // TODO: use the non-ignorable white-spaces where they should be (like, between "int" and "a" in "int a;").

    addRule       (&rdd, "digit", "0-9");
    addRule       (&rdd, "non-zero-digit", "1-9");
    addRule       (&rdd, "non-digit", "_|a-z|A-Z");
    addRule       (&rdd, "hexadecimal-prefix", "0x|X");
    addRule       (&rdd, "hexadecimal-digit", "0-9|a-f|A-F");
    addRule       (&rdd, "hex-quad", "${hexadecimal-digit}${hexadecimal-digit}${hexadecimal-digit}${hexadecimal-digit}");
    addRule       (&rdd, "universal-character-name", "{\\\\u ${hex-quad}} | {\\\\U ${hex-quad} ${hex-quad}}");

    // Identifier,
    addRule       (&rdd, "identifier-non-digit", "${non-digit} | ${universal-character-name}"); // TODO: This doesn't look right! This won't identify true unicode characters.
    addPushingRule(&rdd, "identifier-content", "${identifier-non-digit} {${digit} | ${identifier-non-digit}}^*");
    addPushingRule(&rdd, "identifier", "#{{keyword} {identifier-content} == {identifier-content}}");

    // Constants,
    // Integer constant,
    addRule       (&rdd, "decimal-constant", "${non-zero-digit} ${digit}^*");  // 0 is not a decimal-constant. 0 is recognized as an octal-constant.
    addRule       (&rdd, "octal-constant", "0 0-7^*");
    addRule       (&rdd, "hexadecimal-constant", "${hexadecimal-prefix} ${hexadecimal-digit} ${hexadecimal-digit}^*");
    addRule       (&rdd, "integer-suffix", "{ u|U l|L|{ll}|{LL}|${ε} } | { l|L|{ll}|{LL} u|U|${ε} }");
    addPushingRule(&rdd, "integer-constant", "${decimal-constant}|${octal-constant}|${hexadecimal-constant} ${integer-suffix}|${ε}");

    // Decimal floating point,
    addRule       (&rdd, "fractional-constant", "{${digit}^* . ${digit} ${digit}^*} | {${digit} ${digit}^* . }");
    addRule       (&rdd, "exponent-part", "e|E +|\\-|${ε} ${digit} ${digit}^*");
    addRule       (&rdd, "floating-suffix", "f|l|F|L");
    addRule       (&rdd, "decimal-floating-constant",
                             "{${fractional-constant} ${exponent-part}|${ε} ${floating-suffix}|${ε}} | "
                             "{${digit} ${digit}^* ${exponent-part} ${floating-suffix}|${ε}}");

    // Hexadecimal floating point,
    addRule       (&rdd, "hexadecimal-fractional-constant",
                             "{${hexadecimal-digit}^* . ${hexadecimal-digit} ${hexadecimal-digit}^*} | "
                             "{${hexadecimal-digit} ${hexadecimal-digit}^* . }");
    addRule       (&rdd, "binary-exponent-part", "p|P +|\\-|${ε} ${digit} ${digit}^*");
    addRule       (&rdd, "hexadecimal-floating-constant",
                             "${hexadecimal-prefix} ${hexadecimal-fractional-constant}|{${hexadecimal-digit}${hexadecimal-digit}^*} ${binary-exponent-part} ${floating-suffix}|${ε}");

    // Floating point constant,
    addPushingRule(&rdd, "floating-constant", "${decimal-floating-constant} | ${hexadecimal-floating-constant}");

    // Enumeration constant,
    addPushingRule(&rdd, "enumeration-constant", "${identifier}");

    // Character constant (supporting unknown escape sequences which are implementation defined. We'll pass the escaped character like gcc and clang do),
    addRule       (&rdd, "c-char", "\x01-\\\x09 | \x0b-\x5b | \x5d-\xff"); // All characters except new-line and backslash (\). "\x09" is "\t", and is reserved, hence we needed to escape it.
    addRule       (&rdd, "c-char-with-backslash-without-uUxX", "\x01-\\\x09 | \x0b-\x54 | \x56-\x57| \x59-\x74 | \x76-\x77 | \x79-\xff"); // All characters except new-line, 'u', 'U', 'x' and 'X'.
    addRule       (&rdd, "hexadecimal-escape-sequence", "\\\\x ${hexadecimal-digit} ${hexadecimal-digit}^*");
    addPushingRule(&rdd, "character-constant", "L|u|U|${ε} ' { ${c-char}|${hexadecimal-escape-sequence}|${universal-character-name}|{\\\\${c-char-with-backslash-without-uUxX}} }^* '");

    // Constant,
    //addPushingRule(&rdd, "constant", "${integer-constant} | ${floating-constant} | ${enumeration-constant} | ${character-constant}");
    //addPushingRule(&rdd, "constant", "#{{integer-constant} {floating-constant} {enumeration-constant} {character-constant}}");
    addPushingRule(&rdd, "constant", "#{{integer-constant} {floating-constant} {enumeration-constant} {character-constant}}");

    // String literal,
    // See: https://stackoverflow.com/a/13087264/1942069   and   https://stackoverflow.com/a/13445170/1942069
    addPushingRule(&rdd, "string-literal-fragment", "{u8}|u|U|L|${ε} \" { ${c-char}|${hexadecimal-escape-sequence}|${universal-character-name}|{\\\\${c-char-with-backslash-without-uUxX}} }^* \"");
    addPushingRule(&rdd, "string-literal", "${string-literal-fragment} {${} ${string-literal-fragment}}^*");

    // =====================================
    // Phrase structure,
    // =====================================

    // -------------------------------------
    // Expressions,
    // -------------------------------------

    // Primary expression,
    // You can think of primary expression as the smallest unit that could be considered an expression
    // on its own. Other more complex expressions are built on top of the primary expression,
    addPushingRule(&rdd, "expression", "STUB!");
    addPushingRule(&rdd, "primary-expression",
                             "${identifier} | "
                             "${constant} | "
                             "${string-literal} | "
                             "{ ${(} ${} ${expression} ${} ${)} }");

    // Postfix expression,
    addPushingRule(&rdd, "argument-expression-list", "STUB!");
    addPushingRule(&rdd, "postfix-expression",
                             "${primary-expression} {"
                             "   {${} ${[}  ${} ${expression} ${} ${]} } | "
                             "   {${} ${(}  ${} ${argument-expression-list}|${ε} ${} ${)} } | "
                             "   {${} ${.}  ${} ${identifier}} | "
                             "   {${} ${++} } | "
                             "   {${} ${--} }"
                             "}^*");

    // Argument expression list,
    addPushingRule(&rdd, "assignment-expression", "STUB!");
    updateRule    (&rdd, "argument-expression-list",
                             "${assignment-expression} {"
                             "   ${} ${,} ${} ${assignment-expression}"
                             "}^*");

    // Unary expression,
    addPushingRule(&rdd, "unary-expression", "STUB!");
    addRule       (&rdd, "unary-operator", "STUB!");
    addPushingRule(&rdd, "cast-expression", "STUB!");
    updateRule    (&rdd, "unary-expression",
                             "${postfix-expression} | "
                             "{ ${++}             ${} ${unary-expression} } | "
                             "{ ${--}             ${} ${unary-expression} } | "
                             "{ ${unary-operator} ${} ${cast-expression}  }");

    // Unary operator,
    updateRule    (&rdd, "unary-operator", "#{{+}{-}{~}{!} {++}{--} != {++}{--}}");

    // Cast expression,
    updateRule    (&rdd, "cast-expression",
                             "${unary-expression} | "
                             "{ ${(} ${} ${identifier} ${} ${)} ${} ${cast-expression} }");

    // Multiplicative expression,
    addPushingRule(&rdd, "multiplicative-expression",
                             "${cast-expression} {"
                             "   ${} ${*}|${/}|${%} ${} ${cast-expression}"
                             "}^*");

    // Additive expression,
    addPushingRule(&rdd, "additive-expression",
                             "${multiplicative-expression} {"
                             "   ${} ${+}|${-} ${} ${multiplicative-expression}"
                             "}^*");

    // Shift expression,
    addPushingRule(&rdd, "shift-expression",
                             "${additive-expression} {"
                             "   ${} ${<<}|${>>} ${} ${additive-expression}"
                             "}^*");

    // Relational expression,
    addPushingRule(&rdd, "relational-expression",
                             "${shift-expression} {"
                             "   ${} #{{<} {>} {<=} {>=}} ${} ${shift-expression}"
                             "}^*");

    // Equality expression,
    addPushingRule(&rdd, "equality-expression",
                             "${relational-expression} {"
                             "   ${} ${==}|${!=} ${} ${relational-expression}"
                             "}^*");

    // AND expression,
    addPushingRule(&rdd, "and-expression",
                             "${equality-expression} {"
                             "   ${} #{{&} {&&} != {&&}} ${} ${equality-expression}"
                             "}^*");

    // Exclusive OR expression,
    addPushingRule(&rdd, "xor-expression",
                             "${and-expression} {"
                             "   ${} ${^} ${} ${and-expression}"
                             "}^*");

    // Inclusive OR expression,
    addPushingRule(&rdd, "or-expression",
                             "${xor-expression} {"
                             "   ${} #{{|} {||} != {||}} ${} ${xor-expression}"
                             "}^*");

    // Logical AND expression,
    addPushingRule(&rdd, "logical-and-expression",
                             "${or-expression} {"
                             "   ${} ${&&} ${} ${or-expression}"
                             "}^*");

    // Logical OR expression,
    addPushingRule(&rdd, "logical-or-expression",
                             "${logical-and-expression} {"
                             "   ${} ${||} ${} ${logical-and-expression}"
                             "}^*");

    // Conditional expression,
    addPushingRule(&rdd, "conditional-expression", "STUB!");
    updateRule    (&rdd, "conditional-expression",
                             "${logical-or-expression} | "
                             "{${logical-or-expression} ${} ${?} ${} ${expression} ${} ${:} ${} ${conditional-expression}}");

    // Assignment expression,
    addRule       (&rdd, "assignment-operator", "STUB!");
    updateRule    (&rdd, "assignment-expression",
                             "${conditional-expression} | "
                             "{${unary-expression} ${} ${assignment-operator} ${} ${assignment-expression}}");

    // Assignment operator,
    updateRule    (&rdd, "assignment-operator",
                            "#{{=} {*=} {/=} {%=} {+=} {-=} {<<=} {>>=} {&=} {^=} {|=}}");

    // Expression,
    updateRule    (&rdd, "expression",
                             "${assignment-expression} {"
                             "   ${} ${,} ${} ${assignment-expression}"
                             "}^*");

    addPushingRule(&rdd, "constant-expression",
                            "${conditional-expression}");

    // -------------------------------------
    // Declarations,
    // -------------------------------------

    // Declaration,
    addRule       (&rdd, "declaration-specifiers", "STUB!");
    addRule       (&rdd, "identifier-list", "STUB!");
    // TODO: add initialization support...
    addPushingRule(&rdd, "declaration",
                             "${declaration-specifiers} ${} ${identifier-list} ${} ${;}");

    // Identifier list,
    updateRule    (&rdd, "identifier-list",
                             "${identifier} {"
                             "   ${} ${,} ${} ${identifier}"
                             "}^*");

    // Declaration specifiers,
    addRule       (&rdd, "storage-class-specifier", "STUB!");
    addPushingRule(&rdd, "type-specifier", "STUB!");
    updateRule    (&rdd, "declaration-specifiers",
                             "{${storage-class-specifier} ${}}|${ε} ${type-specifier}");

    // Storage class specifier,
    updateRule    (&rdd, "storage-class-specifier",
                             "#{{static} {identifier} != {identifier}}");

    // Type specifier,
    addPushingRule(&rdd, "class-specifier", "STUB!");
    addRule       (&rdd, "enum-specifier", "STUB!");
    addPushingRule(&rdd, "array-specifier", "STUB!");
    // TODO: Fix enum specifiers...
    updateRule    (&rdd, "type-specifier",
                             "#{{void}     {char}            "
                             "  {short}    {int}      {long} "
                             "  {float}    {double}          "
                             "  {class-specifier}            "
                             "  {enum-specifier}             "
                             "  {identifier} != {identifier}}"    // To prevent identifiers that start with a type name from being mistakenly matched as type-specifier. For example, the identifier "structure" would match "struct", but not when we match and reject identifiers.
                             "{${} ${array-specifier}}^*");

    // Array specifier,
    updateRule    (&rdd, "array-specifier",
                             "${[} ${} ${]}");

    // Class specifier,
    updateRule    (&rdd, "class-specifier",
                             "${identifier}");

    // Class declaration,
    addRule       (&rdd, "declaration-list", "STUB!");
    addPushingRule(&rdd, "class-declaration",
                            "${class} ${} ${identifier} "
                            "{${} ${;}} |"
                            "{${} ${OB} {${} ${declaration-list}}|${ε} ${} ${CB}}");

    // Declaration list,
    updateRule    (&rdd, "declaration-list",
                            "${declaration} ${} ${declaration-list}|${ε}");

    // Enum specifier,
    addRule       (&rdd, "enumerator-list", "STUB!");
    updateRule    (&rdd, "enum-specifier",
                            "{ ${enum} ${} ${identifier}|${ε} ${} ${OB} ${enumerator-list} ${} ${,}|${ε} ${} ${CB} } | "
                            "{ ${enum} ${} ${identifier} }");

    // Enumerator list,
    addRule       (&rdd, "enumerator", "STUB!");
    updateRule    (&rdd, "enumerator-list",
                            "${enumerator} {"
                            "   ${} ${,} ${} ${enumerator}"
                            "}^*");

    // Enumerator,
    updateRule    (&rdd, "enumerator",
                            "${enumeration-constant} { ${} = ${} ${constant-expression} }|${ε}");

    // -------------------------------------
    // Statements,
    // -------------------------------------

    // Statement,
    addPushingRule(&rdd,    "labeled-statement", "STUB!");
    addPushingRule(&rdd,   "compound-statement", "STUB!");
    addPushingRule(&rdd, "expression-statement", "STUB!");
    addPushingRule(&rdd,  "selection-statement", "STUB!");
    addPushingRule(&rdd,  "iteration-statement", "STUB!");
    addPushingRule(&rdd,       "jump-statement", "STUB!");
    addPushingRule(&rdd, "statement",
                            "#{   {labeled-statement}"
                            "    {compound-statement}"
                            "  {expression-statement}"
                            "   {selection-statement}"
                            "   {iteration-statement}"
                            "        {jump-statement}}");

    // Labeled statement,
    updateRule    (&rdd, "labeled-statement",
                            "{${identifier}                      ${} ${:} ${} ${statement}} | "
                            "{${case} ${} ${constant-expression} ${} ${:} ${} ${statement}} | "
                            "{${default}                         ${} ${:} ${} ${statement}}");

    // Compound statement,
    addRule       (&rdd, "block-item-list", "STUB!");
    updateRule    (&rdd, "compound-statement",
                            "${OB} ${} ${block-item-list}|${ε} ${} ${CB}");

    // Block item list,
    addRule       (&rdd, "block-item", "STUB!");
    updateRule    (&rdd, "block-item-list",
                            "${block-item} {"
                            "   ${} ${block-item}"
                            "}^*");

    // Block item,
    updateRule    (&rdd, "block-item",
                            "#{{declaration} {statement}}");

    // Expression statement,
    updateRule    (&rdd, "expression-statement",
                            "${expression}|${ε} ${} ${;}");

    // Selection statement,
    updateRule    (&rdd, "selection-statement",
                            "{ ${if}     ${} ${(} ${} ${expression} ${} ${)} ${} ${statement} {${} ${else} ${} ${statement}}|${ε} } | "
                            "{ ${switch} ${} ${(} ${} ${expression} ${} ${)} ${} ${statement}                                     }");

    // Iteration statement,
    updateRule    (&rdd, "iteration-statement",
                            "{ ${while} ${}                           ${(} ${} ${expression} ${} ${)} ${} ${statement} } | "
                            "{ ${do}    ${} ${statement} ${} ${while} ${(} ${} ${expression} ${} ${)} ${} ${;}         } | "
                            "{ ${for}   ${} ${(} ${} ${expression}|${ε} ${} ${;} ${} ${expression}|${ε} ${} ${;} ${} ${expression}|${ε} ${} ${)} ${} ${statement} } | "
                            "{ ${for}   ${} ${(} ${} ${declaration}              ${} ${expression}|${ε} ${} ${;} ${} ${expression}|${ε} ${} ${)} ${} ${statement} }");

    // Jump statement,
    updateRule    (&rdd, "jump-statement",
                            "{ ${goto}     ${} ${identifier}      ${} ${;} } | "
                            "{ ${continue} ${}                        ${;} } | "
                            "{ ${break}    ${}                        ${;} } | "
                            "{ ${return}   ${} ${expression}|${ε} ${} ${;} }");

    // -------------------------------------
    // External definitions,
    // -------------------------------------

    // Translation unit,
    addPushingRule(&rdd, "external-declaration", "STUB!");
    addPushingRule(&rdd, "translation-unit",
                            "${} ${external-declaration} {{"
                            "   ${} ${external-declaration}"
                            "}^*} ${}"); // Encapsulated the repeat in a sub-rule to avoid early termination. Can \
                                            we consider early termination a feature now?

    // External declaration,
    addPushingRule(&rdd, "function-declaration", "STUB!");
    addPushingRule(&rdd, "function-definition", "STUB!");
    updateRule    (&rdd, "external-declaration",
                            "#{{function-declaration} {function-definition} {declaration} {class-declaration}}");

    // Parameter declaration,
    addPushingRule(&rdd, "parameter-declaration",
                            "${type-specifier} ${} ${identifier}");

    // Parameter list,
    addRule       (&rdd, "parameter-list",
                            "${parameter-declaration} {"
                            "   ${} ${,} ${} ${parameter-declaration}"
                            "}^*");

    // Function head,
    addPushingRule(&rdd, "function-head",
                            "${declaration-specifiers} ${} "
                            "${identifier} ${} "
                            "${(} ${} ${parameter-list}|${ε} ${} ${)}");

    // Function declaration,
    updateRule    (&rdd, "function-declaration",
                            "${function-head} ${} ${;}");

    // Function definition,
    updateRule    (&rdd, "function-definition",
                            "${function-head} ${} ${compound-statement}");

    // Cleanup,
    NCC_destroyRuleData(&rdd.  plainRuleData);
    NCC_destroyRuleData(&rdd.pushingRuleData);
}

NCC_Rule *getRootRule(struct NCC* ncc) {
    return NCC_getRule(ncc, "translation-unit");
}