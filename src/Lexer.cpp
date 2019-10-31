//
// Created by lee on 2019-10-28.
//

#include "llvm/ADT/STLExtras.h"
#include <string>

/**
 * @brief Define reversed words or something else mentioned in grammar.txt
 */
enum Token {
    tok_eof = -1,   ///< EOF means the end of files.

    // commands
    tok_def = -2,   ///< def used to define a function
    tok_extern = -3,    ///< extern linkeage

    // primary
    tok_identifier = -4,    ///< words expect for reserved words
    tok_number = -5,    ///< number include int or double

    tok_return = -6,    ///< return for a function
    tok_double = - 7   ///< define a new variable
};

std::string IdentifierStr;  ///@var IdentifierStr will always point to the current token in std::string

double NumVal;
/**
 * @brief gettok() will skip whitespace and comments, and simply separate out each word.
 * @param
 *
 * @return token number
 */
int gettok() {
    static int LastChar = ' ';

    ///< Skip any whitespace.
    while (isspace(LastChar))
        LastChar = getchar();

    if (isalpha(LastChar)) { ///< identifier: [a-zA-Z][a-zA-Z0-9]*
        IdentifierStr = LastChar;
        while (isalnum((LastChar = getchar())))
            IdentifierStr += LastChar;

        if (IdentifierStr == "def")
            return tok_def;
        if (IdentifierStr == "extern")
            return tok_extern;
        if (IdentifierStr == "return"){
            return tok_return;
        }
        if (IdentifierStr == "double"){
            return tok_double;
        }
        return tok_identifier;
    }
    if (isdigit(LastChar) || LastChar == '.') { ///< Number: [0-9.]+
        std::string NumStr;
        do {
            NumStr += LastChar;
            LastChar = getchar();
        } while (isdigit(LastChar) || LastChar == '.');
        NumVal = strtod(NumStr.c_str(), nullptr);
        return tok_number;
    }

    if (LastChar == '#') {
        // Comment until end of line.
        do
            LastChar = getchar();
        while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

        if (LastChar != EOF)
            return gettok();
    }

    // Check for end of file.  Don't eat the EOF.
    if (LastChar == EOF)
        return tok_eof;

    // Otherwise, just return the character as its ascii value.
    int ThisChar = LastChar;
    LastChar = getchar();
    return ThisChar;
}