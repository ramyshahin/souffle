#include "PresenceCondition.h"
#include "SymbolTable.h"
#include "AstPresenceCondition.h"

#include <cassert>
#include <strstream>
#include <string>

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
   std::stringstream ostr; 
   pc.print(ostr);
   text = ostr.str();
}

PresenceCondition::PresenceCondition(const PresenceCondition& other) :
    pcBDD(other.pcBDD), text(other.text)
{
    Cudd_Ref(pcBDD);
}

PresenceCondition::~PresenceCondition() {
    Cudd_RecursiveDeref(bddMgr, pcBDD);
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

std::ostream& operator<<(std::ostream& out, const PresenceCondition& pc) {
    out << pc.text;
}
} // namespace souffle