#pragma once

#include "PresenceCondition.h"

namespace souffle {

struct RamRecord {
    const RamDomain* field; 
    const std::unique_ptr<const PresenceCondition> pc;
    bool  owned;
#ifdef DEBUG
    std::size_t size;
#endif //DEBUG

    RamRecord(std::size_t s, const RamDomain* f, const PresenceCondition* _pc, bool _owned = false) :
        field(f)
        , pc(_pc)
        , owned(_owned)
#ifdef DEBUG
        , size(s)
#endif
    {
        assert(pc);
    }

    ~RamRecord() {
        if (owned) {
            delete[] field;
        }
    }

    RamRecord(const RamRecord& other) : 
        field(other.field),
        pc(new PresenceCondition(*(other.pc.get()))),
        owned(false)
#ifdef DEBUG
        , size(other.size)
#endif
        {}

    const RamDomain& operator[](std::size_t index) const {
#ifdef DEBUG
        assert(index < size);
#endif
        return field[index];
    }
}; //RamRecord

}