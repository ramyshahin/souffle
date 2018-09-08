#pragma once

#include <string>

class DdManager;
class DdNode;

namespace souffle {

class SymbolTable;
class AstPresenceCondition;

class PresenceCondition {
private:
    static SymbolTable* featSymTab;
    static DdManager*   bddMgr;

    DdNode* pcBDD;
    std::string text;
protected:
    PresenceCondition();
    PresenceCondition(DdNode* bdd, const std::string& t);

public:
    static void init(SymbolTable& st);
    static PresenceCondition makeTrue();

    PresenceCondition(const PresenceCondition& other);
    PresenceCondition(const AstPresenceCondition& pc);
    
    ~PresenceCondition();

    bool conjSat(const PresenceCondition& other) const;

    bool operator==(const PresenceCondition& other) const;

    bool operator!=(const PresenceCondition& other) const {
        return !(*this == other);
    }
    
    bool operator<(const PresenceCondition& other) const;
    
    bool operator>(const PresenceCondition& other) const;

    PresenceCondition conjoin(const PresenceCondition& other) const;

    bool isSAT() const;

    friend std::ostream& operator<<(std::ostream& out, const PresenceCondition& pc);
}; // PresenceCondition

}; // souffle