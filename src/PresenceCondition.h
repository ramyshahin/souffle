#pragma once
#include "SymbolTable.h"
#include "AstPresenceCondition.h"

#include <cassert>

#include <cudd.h>

namespace souffle {

class SymbolTable;
class AstPresenceCondition;

class PresenceCondition {
private:
    static SymbolTable* featSymTab;
    static DdManager*   bddMgr;
    static DdNode*      TT;
    static DdNode*      FF;

    DdNode* pcBDD;

protected:
    PresenceCondition() {}

public:
    static void init(SymbolTable& st) {
        featSymTab = &st;

        bddMgr = Cudd_Init(
            featSymTab->size(), 
            0, 
            CUDD_UNIQUE_SLOTS, 
            CUDD_CACHE_SLOTS, 
            0);
        assert(bddMgr);

        FF = Cudd_ReadLogicZero(bddMgr);
        TT = Cudd_ReadOne(bddMgr);
    }

    PresenceCondition(const AstPresenceCondition& pc) {
       pcBDD = pc.toBDD(bddMgr); 
    }
    
    ~PresenceCondition() {
        Cudd_RecursiveDeref(bddMgr, pcBDD);
    }

    bool conjSat(const PresenceCondition& other) const {
        DdNode* tmp = Cudd_bddAnd(bddMgr, pcBDD, other.pcBDD);
        bool ret = (tmp != FF);
        Cudd_RecursiveDeref(bddMgr, tmp);
        return ret;
    }

    bool operator==(const PresenceCondition& other) const {
        return pcBDD == other.pcBDD;
    }

    bool isSAT() const {
        return (pcBDD != FF);
    }

    bool isTrue() const {
        return (pcBDD == TT);
    }
}; // PresenceCondition

}; // souffle