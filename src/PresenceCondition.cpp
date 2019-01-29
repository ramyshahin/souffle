#include "PresenceCondition.h"
#include "SymbolTable.h"
#include "AstPresenceCondition.h"
#include "WriteStream.h"
#include "ReadStream.h"

#include <cassert>
#include <sstream>
#include <string>

using namespace std;

#include <cudd.h>

namespace souffle {

size_t WriteStream::recordCount;
size_t WriteStream::pcCount;

size_t ReadStream::recordCount;
size_t ReadStream::pcCount;

SymbolTable* PresenceCondition::featSymTab = nullptr;

#ifdef SAT_CHECK
DdManager*   PresenceCondition::bddMgr = nullptr;
PresenceCondition* PresenceCondition::fmPC = nullptr;
DdNode* PresenceCondition::FF;
DdNode* PresenceCondition::TT;
#endif

std::map<MAP_KEY, PresenceCondition*> PresenceCondition::pcMap;

#if 0

void PresenceCondition::init(SymbolTable& st) {
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

PresenceCondition PresenceCondition::makeTrue() {
    return PresenceCondition(TT, "True");
}

PresenceCondition::PresenceCondition() {}

PresenceCondition::PresenceCondition(DdNode* bdd, const std::string& t) :
    pcBDD(bdd), text(t)
{}

PresenceCondition::PresenceCondition(const AstPresenceCondition& pc) {
   pcBDD = pc.toBDD(bddMgr);
   std::stringstream ostr; 
   pc.print(ostr);
   text = ostr.str();
}

PresenceCondition& PresenceCondition::operator=(const PresenceCondition& other) {
    Cudd_RecursiveDeref(bddMgr, pcBDD);
    pcBDD = other.pcBDD;
    Cudd_Ref(pcBDD);
    text = other.text;

    return *this;
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

void PresenceCondition::conjWith(const PresenceCondition& other) {
    if (pcBDD == other.pcBDD) {
        return;
    }

    if (other.pcBDD == TT) {
        return;
    }

    if (pcBDD == TT) {
        pcBDD = other.pcBDD;
        Cudd_Ref(pcBDD);
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

PresenceCondition PresenceCondition::conjoin(const PresenceCondition& other) const {
    DdNode* tmp = Cudd_bddAnd(bddMgr, pcBDD, other.pcBDD);
    Cudd_Ref(tmp);
    return PresenceCondition(tmp, "(" + text + " /\\ " + other.text + ")");
}
bool PresenceCondition::operator==(const PresenceCondition& other) const {
    return pcBDD == other.pcBDD;
}

#define ULONG unsigned long

bool PresenceCondition::operator<(const PresenceCondition& other) const {
    return (((ULONG)pcBDD) < ((ULONG)(other.pcBDD)));
}
    
bool PresenceCondition::operator>(const PresenceCondition& other) const {
    return (((ULONG)pcBDD) > ((ULONG)(other.pcBDD)));
}

bool PresenceCondition::isSAT() const {
    return (pcBDD != FF);
}

bool PresenceCondition::isTrue() const {
    return (pcBDD == TT);
}

std::ostream& operator<<(std::ostream& out, const PresenceCondition& pc) {
    out << pc.text;
    return out;
}
#endif

} // namespace souffle