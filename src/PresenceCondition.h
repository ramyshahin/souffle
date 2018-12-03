#pragma once

#include <string>
#include <cassert>
#include <sstream>
#include <map>
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
    static std::map<DdNode*, PresenceCondition*> pcMap;

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

        pcMap[FF] = new PresenceCondition(FF, "False");
        pcMap[TT] = new PresenceCondition(TT, "True");
    }

    static PresenceCondition* makeTrue() {
        return pcMap[TT];
    }

    //PresenceCondition(const PresenceCondition& other) :
    //   pcBDD(other.pcBDD), text(other.text)
    //{
    //    Cudd_Ref(pcBDD);
    //}

    static PresenceCondition* parse(const AstPresenceCondition& pc) {
        auto pcBDD = pc.toBDD(bddMgr);
        auto _pc = pcMap.find(pcBDD);
        if (_pc != pcMap.end()) {
            return _pc->second;
        }
        std::stringstream ostr; 
        pc.print(ostr);
        std::string text = ostr.str();
        PresenceCondition* newpc = new PresenceCondition(pcBDD, text);
        pcMap[pcBDD] = newpc;
        return newpc;
    }

#ifndef NDEBUG
    void validate() const {
        assert (pcBDD);
    }
#endif

    //PresenceCondition& operator=(const PresenceCondition& other) {
    //    Cudd_RecursiveDeref(bddMgr, pcBDD);
    //    pcBDD = other.pcBDD;
    //    Cudd_Ref(pcBDD);
    //    text = other.text;
    //
    //    return *this;
    //}

    ~PresenceCondition() {
        Cudd_RecursiveDeref(bddMgr, pcBDD);
        #ifndef NDEBUG
        pcBDD = nullptr;
        #endif
    }

    bool conjSat(const PresenceCondition* other) const {
        assert(other);
        DdNode* tmp = Cudd_bddAnd(bddMgr, pcBDD, other->pcBDD);
    
        return tmp != FF;
    }

/*
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
*/

#ifndef ULONG
#define ULONG unsigned long
#endif

    bool operator==(const PresenceCondition& other) const {
        return pcBDD == other.pcBDD;
    }

    bool operator!=(const PresenceCondition& other) const {
        return !(*this == other);
    }

    //bool operator<(const PresenceCondition& other) const {
    //    return (((ULONG)pcBDD) < ((ULONG)(other.pcBDD)));
    //}
    
    //bool operator>(const PresenceCondition& other) const {
    //    return (((ULONG)pcBDD) > ((ULONG)(other.pcBDD)));
    //}

    PresenceCondition* conjoin(const PresenceCondition* other) const {
        assert(other);
        DdNode* tmp = Cudd_bddAnd(bddMgr, pcBDD, other->pcBDD);
        auto cached = pcMap.find(tmp);
        if (cached != pcMap.end()) {
            return cached->second;
        }
        Cudd_Ref(tmp);
        PresenceCondition *pc = new PresenceCondition(tmp, "(" + text + " /\\ " + other->text + ")");
        pcMap[tmp] = pc;
        return pc;
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