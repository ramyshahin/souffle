#pragma once

#define SAT_CHECK

#include <string>
#include <cassert>
#include <sstream>
#include <fstream>
#include <map>

#ifdef SAT_CHECK
#include <cudd.h>
#endif //SAT_CHECK

#include "SymbolTable.h"
#include "AstPresenceCondition.h"
#include "PresenceConditionParser.h"

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
    static PresenceCondition* fmPC;
    static std::map<MAP_KEY, PresenceCondition*> pcMap;

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
#endif

        std::string fmPath = "/home/ramy/model.prop";
        std::ifstream in(fmPath.c_str());
        if (in.good()) {
            std::string fm;
            getline(in, fm);
            std::cout << "Using Feature Model: " << fm << std::endl;
#ifdef SAT_CHECK
            PresenceConditionParser parser(fm);
            auto ast = parser.parse(st);
            fmPC = parse(*ast);
#else
            fmPC = new PresenceCondition(fm);
#endif
        }

#ifdef SAT_CHECK
        FF = Cudd_ReadLogicZero(bddMgr);
        TT = Cudd_ReadOne(bddMgr);
#endif
        pcMap[FF] = new PresenceCondition(
#ifdef SAT_CHECK
            FF, ATOM, nullptr, nullptr,
#endif
            "False");

        pcMap[TT] = new PresenceCondition(
#ifdef SAT_CHECK
            TT, ATOM, nullptr, nullptr,
#endif
            "True");

        assert(pcMap[FF] != nullptr);
        assert(pcMap[TT] != nullptr);
    }

    static PresenceCondition* makeTrue() {
        auto ret = fmPC ? fmPC : pcMap[TT];
        assert(ret);

        return ret;
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
        std::stringstream ostr; 
        pc.print(ostr);
        std::string text = ostr.str();
#ifdef SAT_CHECK
        auto pcBDD = const_cast<AstPresenceCondition&>(pc).toBDD(bddMgr);
#endif
        auto _pc = pcMap.find(
#ifdef SAT_CHECK
            pcBDD
#else
            text
#endif
            );
        if (_pc != pcMap.end()) {
            return _pc->second;
        }
        
        PresenceCondition* newpc = new PresenceCondition(
#ifdef SAT_CHECK
            pcBDD, ATOM, nullptr, nullptr,
#endif
            text);

        assert(newpc);

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

    const PresenceCondition* conjoin(const PresenceCondition* other) const {
        assert(other);
        if (isTrue()) {
            return other;
        }

        if (other->isTrue()) {
            return this;
        }
        
        if (*this == *other) {
            return this;
        }

#ifdef SAT_CHECK
        DdNode* tmp = Cudd_bddAnd(bddMgr, pcBDD, other->pcBDD);
#else
        std::string tmp = "(" + text + " /\\ " + other->text + ")";
#endif
        auto cached = pcMap.find(tmp);
        if (cached != pcMap.end()) {
            return cached->second;
        }

#ifdef SAT_CHECK
        Cudd_Ref(tmp);
#endif

        PresenceCondition *pc = new PresenceCondition(
            tmp
#ifdef SAT_CHECK
            , CONJ, this, other, ""
#endif
            );

        assert(pc);

        pcMap[tmp] = pc;

        return pc;
    }

    const PresenceCondition* disjoin(const PresenceCondition* other) const {
        assert(other);

        if (isTrue()) {
            return this;
        }

        if (other->isTrue()) {
            return other;
        }
        
        if (*this == *other) {
            return this;
        }

#ifdef SAT_CHECK
        DdNode* tmp = Cudd_bddOr(bddMgr, pcBDD, other->pcBDD);
#else
        std::string tmp = "(" + text + " \\/ " + other->text + ")";
#endif
        auto cached = pcMap.find(tmp);
        if (cached != pcMap.end()) {
            return cached->second;
        }

#ifdef SAT_CHECK
        Cudd_Ref(tmp);
#endif

        PresenceCondition *pc = new PresenceCondition(
            tmp
#ifdef SAT_CHECK
            ,DISJ, this, other, ""
#endif
        );

        assert(pc);

        pcMap[tmp] = pc;
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
        return (text == std::string(TT));
#endif
    }
    
    friend std::ostream& operator<<(std::ostream& out, const PresenceCondition& pc) {
#ifdef SAT_CHECK
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
#else
    out << pc.text;
#endif
        return out;
    }
}; // PresenceCondition

}; // souffle
