#pragma once

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

protected:
    PresenceCondition();

public:
    static void init(SymbolTable& st);

    PresenceCondition(const AstPresenceCondition& pc);
    
    ~PresenceCondition();

    bool conjSat(const PresenceCondition& other) const;

    bool operator==(const PresenceCondition& other) const;

    bool isSAT() const;
}; // PresenceCondition

}; // souffle