#pragma once

#define SAT_CHECK

#include <string>
#include <cassert>
#include <sstream>
#include <unordered_map>

#ifdef SAT_CHECK
#include <cudd.h>
#endif //SAT_CHECK

#include "SymbolTable.h"
#include "AstPresenceCondition.h"

#ifdef SAT_CHECK
#define MAP_KEY DdNode*
#else
#define MAP_KEY std::string
#define FF "False"
#define TT "True"
#endif //SAT_CHECK

namespace souffle {

class PresenceCondition {
private:
    enum PropType { ATOM, NEG, CONJ, DISJ };
    static SymbolTable* featSymTab;
#ifdef SAT_CHECK
    static DdManager*   bddMgr;
    static DdNode* FF;
    static DdNode* TT;
    DdNode* pcBDD;
    PropType type;
    const PresenceCondition* sub0;
    const PresenceCondition* sub1;
#endif
    static std::unordered_map<MAP_KEY, PresenceCondition*> pcMap;

    std::string text;
protected:
    PresenceCondition() {}

    PresenceCondition(
#ifdef SAT_CHECK
        DdNode* bdd,
        PropType _type,	const PresenceCondition* s0, const PresenceCondition* s1,
#endif
        const std::string& t) :
#ifdef SAT_CHECK
        pcBDD(bdd),
	type(_type), 
	sub0(s0),
	sub1(s1),
#endif
        text(t)
    {}

public:
    static void init(SymbolTable& st) {
        featSymTab = &st;
#ifdef SAT_CHECK
        bddMgr = Cudd_Init(
            featSymTab->size(), 
            0, 
            CUDD_CACHE_SLOTS, 
            CUDD_CACHE_SLOTS, 
            0);
        assert(bddMgr);

        FF = Cudd_ReadLogicZero(bddMgr);
        TT = Cudd_ReadOne(bddMgr);
#endif
        pcMap[FF] = new PresenceCondition(
#ifdef SAT_CHECK
            FF, 
	    ATOM, nullptr, nullptr,
#endif
            "False");

        pcMap[TT] = new PresenceCondition(
#ifdef SAT_CHECK
            TT, 
	    ATOM, nullptr, nullptr,
#endif
            "True");
    }

    static PresenceCondition* makeTrue() {
        return pcMap[TT];
    }

    //PresenceCondition(const PresenceCondition& other) :
    //   pcBDD(other.pcBDD), text(other.text)
    //{
    //    Cudd_Ref(pcBDD);
    //}

    static size_t getPCCount() {
        return pcMap.size();
    }
    static PresenceCondition* parse(const AstPresenceCondition& pc) {
#ifdef SAT_CHECK
        auto pcBDD = const_cast<AstPresenceCondition&>(pc).toBDD(bddMgr);
        auto _pc = pcMap.find(pcBDD);
        if (_pc != pcMap.end()) {
            return _pc->second;
        }
#endif
        std::stringstream ostr; 
        pc.print(ostr);
        std::string text = ostr.str();
        PresenceCondition* newpc = new PresenceCondition(
#ifdef SAT_CHECK
            pcBDD,
	    ATOM, nullptr, nullptr,
#endif
            text);
        pcMap[
#ifdef SAT_CHECK
            pcBDD
#else
            text
#endif
             ] = newpc;
        return newpc;
    }

#ifndef NDEBUG
    void validate() const {
#ifdef SAT_CHECK
        assert (pcBDD);
#endif
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
#ifdef SAT_CHECK
        Cudd_RecursiveDeref(bddMgr, pcBDD);
        #ifndef NDEBUG
        pcBDD = nullptr;
        #endif
#endif
    }

    bool conjSat(const PresenceCondition* other) const {
        assert(other);
#ifdef SAT_CHECK
        DdNode* tmp = Cudd_bddAnd(bddMgr, pcBDD, other->pcBDD);
    
        return tmp != FF;
#else
        return true;
#endif
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
#ifdef SAT_CHECK
        return pcBDD == other.pcBDD;
#else
        return text == other.text;
#endif
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

    const PresenceCondition* conjoin(const PresenceCondition* other) const {
        assert(other);
        if (isTrue()) {
            return other;
        }

        if (other->isTrue()) {
            return this;
        }
        
#ifdef SAT_CHECK
        DdNode* tmp = Cudd_bddAnd(bddMgr, pcBDD, other->pcBDD);
        auto cached = pcMap.find(tmp);
        if (cached != pcMap.end()) {
            return cached->second;
        }
        Cudd_Ref(tmp);
#endif

        //std::string newText = "(" + text + " /\\ " + other->text + ")";
        PresenceCondition *pc = new PresenceCondition(
#ifdef SAT_CHECK
            tmp,
	    CONJ, this, other, 
#endif
            "");

        pcMap[
#ifdef SAT_CHECK
            tmp
#else
            newText
#endif
            ] = pc;

        return pc;
    }

    PresenceCondition* disjoin(const PresenceCondition* other) const {
        assert(other);
#ifdef SAT_CHECK
        DdNode* tmp = Cudd_bddOr(bddMgr, pcBDD, other->pcBDD);
        auto cached = pcMap.find(tmp);
        if (cached != pcMap.end()) {
            return cached->second;
        }
        Cudd_Ref(tmp);
#endif

        //std::string newText = "(" + text + " \\/ " + other->text + ")";
        PresenceCondition *pc = new PresenceCondition(
#ifdef SAT_CHECK
            tmp,
	    DISJ, this, other, 
#endif
            "");
        pcMap[
#ifdef SAT_CHECK
            tmp
#else
            newText
#endif
            ] = pc;
        return pc;
    }

    bool isSAT() const {
#ifdef SAT_CHECK
        return (pcBDD != FF);
#else
        return true;
#endif
    }

    bool isTrue() const {
#ifdef SAT_CHECK
        return (pcBDD == TT);
#else
        return (text == TT);
#endif
    }
    
    friend std::ostream& operator<<(std::ostream& out, const PresenceCondition& pc) {
	switch(pc.type) {
		case ATOM:
		case NEG:
        		out << pc.text;
			break;
		case CONJ:
			out << "(" << *pc.sub0 << " /\\ " << *pc.sub1 << ")";
			break;
		case DISJ:
			out << "(" << *pc.sub0 << " \\/ " << *pc.sub1 << ")";
			break;
	}
        return out;
    }
}; // PresenceCondition

}; // souffle
