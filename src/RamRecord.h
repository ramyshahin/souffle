#pragma once

#include "PresenceCondition.h"

namespace souffle {

struct RamRecord {
    const RamDomain* field; 
    const std::unique_ptr<const PresenceCondition> pc;

#ifdef DEBUG
    std::size_t size;
#endif //DEBUG

    RamRecord(std::size_t s, const RamDomain* f, const PresenceCondition* _pc) :
        field(f)
        , pc(_pc)
#ifdef DEBUG
        , size(s)
#endif
    {
        assert(pc);
    }

    RamDomain operator[](std::size_t index) const {
#ifdef DEBUG
        assert(index < size);
#endif
        return field[index];
    }
}; //RamRecord

}