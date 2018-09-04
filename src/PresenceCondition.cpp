#include "PresenceCondition.h"
#include "SymbolTable.h"
#include "AstPresenceCondition.h"

#include <cassert>

using namespace std;

#include <cudd.h>

namespace souffle {

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

PresenceCondition::PresenceCondition() {}

PresenceCondition::PresenceCondition(const AstPresenceCondition& pc) {
   pcBDD = pc.toBDD(bddMgr); 
}

PresenceCondition::~PresenceCondition() {
    Cudd_RecursiveDeref(bddMgr, pcBDD);
    //free(pcBDD);
}

bool PresenceCondition::conjSat(const PresenceCondition& other) const {
    DdNode* tmp = Cudd_bddAnd(bddMgr, pcBDD, other.pcBDD);
    
    return tmp != Cudd_ReadZero(bddMgr);
}

bool PresenceCondition::operator==(const PresenceCondition& other) const {
    return pcBDD == other.pcBDD;
}

bool PresenceCondition::isSAT() const {
    return (pcBDD != Cudd_ReadZero(bddMgr));
}

} // namespace souffle