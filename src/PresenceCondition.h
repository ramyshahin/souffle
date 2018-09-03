#pragma once

#include "SymbolTable.h"
#include "AstPresenceCondition.h"
#include <cudd.h>

namespace souffle {

class PresenceCondition {
private:
    static SymbolTable* featSymTab;
    static DdManager*   bddMgr;

    DdNode* pcBDD;

protected:
    PresenceCondition();

public:
    static void init(SymbolTable& st);

    PresenceCondition(const AstPresenceCondition& pc);

    ~PresenceCondition();

    PresenceCondition& operator&&(const PresenceCondition& other) const;

    bool isSAT() const;
}; // PresenceCondition

}; // souffle