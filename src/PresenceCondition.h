#pragma once

#include <string>
#include <cassert>
#include <sstream>
#include <cudd.h>

#include "SymbolTable.h"
#include "AstPresenceCondition.h"

namespace souffle {

class PresenceCondition {
private:
    static SymbolTable* featSymTab;
    static DdManager*   bddMgr;
    static DdNode* FF;
    static DdNode* TT;
    
    DdNode* pcBDD;
    std::string text;
protected:
    PresenceCondition() {}

    PresenceCondition(DdNode* bdd, const std::string& t) :
        pcBDD(bdd), text(t)
    {}

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

    static PresenceCondition makeTrue() {
        return PresenceCondition(TT, "True");
    }

    PresenceCondition(const PresenceCondition& other) :
        pcBDD(other.pcBDD), text(other.text)
    {
        Cudd_Ref(pcBDD);
    }

    PresenceCondition(const AstPresenceCondition& pc) {
        pcBDD = pc.toBDD(bddMgr);
        std::stringstream ostr; 
        pc.print(ostr);
        text = ostr.str();
    }

    PresenceCondition& operator=(const PresenceCondition& other) {
        Cudd_RecursiveDeref(bddMgr, pcBDD);
        pcBDD = other.pcBDD;
        Cudd_Ref(pcBDD);
        text = other.text;

        return *this;
    }

    ~PresenceCondition() {
        Cudd_RecursiveDeref(bddMgr, pcBDD);
    }

    bool conjSat(const PresenceCondition& other) const {
        DdNode* tmp = Cudd_bddAnd(bddMgr, pcBDD, other.pcBDD);
    
        return tmp != Cudd_ReadZero(bddMgr);
    }

    void conjWith(const PresenceCondition& other) {
        if (pcBDD == other.pcBDD) {
            return;
        }

        if (other.pcBDD == TT) {
            return;
        }

        if (pcBDD == TT) {
            pcBDD = other.pcBDD;
            text = other.text;
            return;
        }

        DdNode* tmp = Cudd_bddAnd(bddMgr, pcBDD, other.pcBDD);
        Cudd_Ref(tmp);
        Cudd_RecursiveDeref(bddMgr, pcBDD);
        pcBDD = tmp;
        if (pcBDD == FF) {
            text = "False";
        } else if (pcBDD == TT) {
            text = "True";
        } else {
            text = "(" + text + " /\\ " + other.text + ")";
        }
    }

#ifndef ULONG
#define ULONG unsigned long
#endif

    bool operator==(const PresenceCondition& other) const {
        return pcBDD == other.pcBDD;
    }

    bool operator!=(const PresenceCondition& other) const {
        return !(*this == other);
    }

    bool operator<(const PresenceCondition& other) const {
        return (((ULONG)pcBDD) < ((ULONG)(other.pcBDD)));
    }
    
    bool operator>(const PresenceCondition& other) const {
        return (((ULONG)pcBDD) > ((ULONG)(other.pcBDD)));
    }

    PresenceCondition conjoin(const PresenceCondition& other) const {
        DdNode* tmp = Cudd_bddAnd(bddMgr, pcBDD, other.pcBDD);
        Cudd_Ref(tmp);
        return PresenceCondition(tmp, "(" + text + " /\\ " + other.text + ")");
    }

    bool isSAT() const {
        return (pcBDD != FF);
    }

    bool isTrue() const {
        return (pcBDD == TT);
    }
    
    friend std::ostream& operator<<(std::ostream& out, const PresenceCondition& pc) {
        out << pc.text;
        return out;
    }
}; // PresenceCondition

}; // souffle