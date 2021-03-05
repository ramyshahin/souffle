#pragma once

#include <string>
#include <algorithm>
#include <iostream>
#include <cctype>
#include <list>

#include "SymbolTable.h"
#include "AstPresenceCondition.h"

namespace souffle {

enum TokenType {
    ID,
    AND,
    OR,
    NOT,
    LPAREN,
    RPAREN
}; // TokenType

struct Token {
    TokenType   type;
    const char* begin;
    size_t      length;
}; // Token

inline std::ostream& operator<<(std::ostream& out, const Token& t) {
    switch (t.type) {
        case ID:
            out << "ID(" << std::string(t.begin, t.length) << ")";
            break;
        case AND:
            out << "AND";
            break;
        case OR:
            out << "OR";
            break;
        case NOT:
            out << "NOT";
            break;
        case LPAREN:
            out << "LPAREN";
            break;
        case RPAREN:
            out << "RPAREN";
            break;
    }
    return out;
}

#define EXCEPTION_MSG "Cannot parse presence condition: "

class PresenceConditionParser {
private:
    std::string         pcText;
    std::list<Token>    tokens;

protected:
    bool tokenize(const char* begin) {
        if (!begin) {
            return false;
        }

        Token curToken;
        curToken.begin = begin;
        curToken.length = 1;

        switch (*begin) {
            // end of string
            case '\0':
                return true;
            // whitespaces
            case ' ':
            case '\n':
            case '\r':
            case '\t':
                return tokenize(++begin);
            // LPAREN
            case '(':
                curToken.type = LPAREN;
                break;
            // RPAREN
            case ')':
                curToken.type = RPAREN;
                break;
            // NOT
            case '!':
                curToken.type = NOT;
                break;
            // AND
            case '/':
                if (begin[1] != '\\') {
                    return false;
                }
                curToken.type = AND;
                curToken.length = 2;
                break;
            case '&':
                if (begin[1] != '&') {
                    return false;
                }
                curToken.type = AND;
                curToken.length = 2;
                break; 
            // OR
            case '\\':
                if (begin[1] != '/') {
                    return false;
                }
                curToken.type = OR;
                curToken.length = 2;
                break;
            case '|':
                if (begin[1] != '|') {
                    return false;
                }
                curToken.type = OR;
                curToken.length = 2;
                break;
            // ID
            default:
                if (!isalpha(*begin) && (*begin != '_')) {
                    return false;
                }
                curToken.type = ID;
                size_t index = 1;
                while(isalnum(begin[index]) || begin[index] == '_') {
                    curToken.length++;
                    index++;
                }
        } // switch
        tokens.push_back(curToken);
        return tokenize(begin + curToken.length);
    }

protected:
    AstPresenceCondition* parse_inner(
        SymbolTable& symTable, 
        std::list<Token>::iterator& it,
        AstPresenceCondition* lhs = nullptr) {
        if (it == tokens.end()) {
            return lhs;
        }

        switch (it->type) {
            case ID: {
                if (lhs) {
	            std::cerr << EXCEPTION_MSG << pcText << std::endl; 	
                    return nullptr;
                }
                std::string id(it->begin, it->length);
                AstPresenceCondition* cur;
                if (id == "True") {
                    cur = new AstPresenceConditionPrimitive(true);
                } else if (id == "False") {
                    cur = new AstPresenceConditionPrimitive(false);
                } else {
                    cur = new AstPresenceConditionFeat(symTable, id);
                }
                return cur;
            }
            case AND:
            case OR: {
                if (!lhs) {
                    std::cerr << EXCEPTION_MSG << pcText << std::endl;
                    return nullptr;
                }
                auto type = it->type;
                AstPresenceCondition* rhs = parse_inner(symTable, ++it);
                return new AstPresenceConditionBin(type == AND ? OP_AND : OP_OR, *lhs, *rhs); 
            }
            case NOT: {
                if (lhs) {
                    std::cerr << EXCEPTION_MSG << pcText << std::endl;
                    return nullptr;
                }
                AstPresenceCondition* rhs = parse_inner(symTable, ++it);
                return new AstPresenceConditionNeg(*rhs);
            }
            case LPAREN: {
                if (lhs) {
                    std::cerr << EXCEPTION_MSG << pcText << std::endl;
                    return nullptr;
                }
                bool done = false;
                AstPresenceCondition* rhs = nullptr;
                while (!done) {
                    it++;
                    if (it == tokens.end()) {
                        std::cerr << EXCEPTION_MSG << pcText << std::endl;
                        return nullptr;
                    }
                    if (it->type == RPAREN) {
                        done = true;
                    } else {
                        rhs = parse_inner(symTable, it, rhs);
                    }
                }
                return rhs;
            }
            default:
                std::cerr << EXCEPTION_MSG << pcText << std::endl; 
                return nullptr;
        } // switch
    } // parse_inner

public:
    PresenceConditionParser(const std::string& input) : pcText(input) {
    }

    AstPresenceCondition* parse(SymbolTable& symTable) {
        if (!tokenize(pcText.c_str())) {
            return nullptr;
        }

        auto it = tokens.begin();
        AstPresenceCondition *pc = nullptr;
        while (it != tokens.end()) {
            pc = parse_inner(symTable, it, pc);
            it++;
        }
        return pc;
    }
        
}; // PresenceConditionParser

} // souffle

#if 0
// build using: g++ -std=c++11 -lcudd -g PresenceConditionParser.cpp 
using namespace souffle;
using namespace std;

int main() {
    SymbolTable symT;

    try {
        PresenceConditionParser parser(" A  /\\  !(!B \\/ C)   ");
        AstPresenceCondition* pc = parser.parse(symT);
        pc->print(cout);
        cout << endl;
        delete pc;
    } catch (exception& e) {
        cerr << "Exception: e.what()" << endl;
    }
    return 0;
}
#endif // 0
