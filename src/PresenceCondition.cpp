#include "PresenceCondition.h"

#include <cassert>

using namespace std;
using namespace souffle;

SymbolTable* PresenceCondition::featSymTab = nullptr;
DdManager*   PresenceCondition::bddMgr = nullptr;

void PresenceCondition::init(SymbolTable& st) {
    featSymTab = &st;

    bddMgr = Cudd_Init(
        featSymTab->size(), 
        0, 
        CUDD_UNIQUE_SLOTS, 
        CUDD_CACHE_SLOTS, 
        0);
    assert(bddMgr);
}

PresenceCondition::PresenceCondition(const AstPresenceCondition& pc) {
   pcBDD = pc.toBDD(bddMgr); 
}

PresenceCondition::~PresenceCondition() {
    Cudd_RecursiveDeref(bddMgr, pcBDD);
}

PresenceCondition& PresenceCondition::operator&&(const PresenceCondition& other) const {
    PresenceCondition ret;
    ret.pcBDD = Cudd_bddAnd(bddMgr, pcBDD, other.pcBDD);
    Cudd_Ref(ret.pcBDD);

    return ret;
}

bool PresenceCondition::isSAT() const {
    return (pcBDD != Cudd_ReadZero(bddMgr));
}