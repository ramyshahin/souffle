#pragma once

#include <string>
namespace souffle {

class PresenceCondition {
public:
    enum {
        TT,
        FF
    };
    
    PresenceCondition(const std::string& atom);

}; // PresenceCondition

}; // souffle