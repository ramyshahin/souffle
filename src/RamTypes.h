/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamTypes.h
 *
 * Defines tuple element type and data type for keys on table columns
 *
 ***********************************************************************/

#pragma once

#include <memory>
#include <limits>
#include <cassert>
#include <cstdint>

#include "PresenceCondition.h"

namespace souffle {

/**
 * Type of an element in a tuple.
 *
 * Default type is int32_t; may be overridden by user
 * defining RAM_DOMAIN_TYPE.
 */

#ifndef RAM_DOMAIN_SIZE
#define RAM_DOMAIN_SIZE 32
#endif

#if RAM_DOMAIN_SIZE == 64
using RamDomain = int64_t;
#else
using RamDomain = int32_t;
#endif

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

/** lower and upper boundaries for the ram domain **/
#define MIN_RAM_DOMAIN (std::numeric_limits<RamDomain>::min())
#define MAX_RAM_DOMAIN (std::numeric_limits<RamDomain>::max())

/** type of an index key; each bit represents a column of a table */
using SearchColumns = uint64_t;

}  // end of namespace souffle
