/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file InterpreterRelation.h
 *
 * Defines Interpreter Relations
 *
 ***********************************************************************/

#pragma once

#include "InterpreterIndex.h"
#include "ParallelUtils.h"
#include "RamTypes.h"
#include "RamRecord.h"

#include <deque>
#include <map>
#include <memory>
#include <vector>
#include <list>

namespace souffle {

/**
 * Interpreter Relation
 */
class InterpreterRelation {
private:
    /** Arity of relation */
    const size_t arity;

    /** Size of blocks containing tuples */
    static const int BLOCK_SIZE = 1024;

    /** Number of tuples in relation */
    size_t num_tuples;

    std::deque<std::unique_ptr<RamDomain[]>> blockList;

    /** List of indices */
    mutable std::map<InterpreterIndexOrder, std::unique_ptr<InterpreterIndex>> indices;

    /** Total index for existence checks */
    mutable InterpreterIndex* totalIndex;

    /** Lock for parallel execution */
    mutable Lock lock;

    /** Relation Presence Conditions */
    std::list<const RamRecord*> records;

public:
    InterpreterRelation(size_t relArity) : arity(relArity), num_tuples(0), totalIndex(nullptr) {}

    InterpreterRelation(const InterpreterRelation& other) = delete;

    virtual ~InterpreterRelation() {
        if (arity == 0) {
            return;
        }
        
        for(const auto* r: records) {
            delete r;
        }
    }

    /** Get arity of relation */
    size_t getArity() const {
        return arity;
    }

    /** Check whether relation is empty */
    bool empty() const {
        return num_tuples == 0;
    }

    /** Gets the number of contained tuples */
    size_t size() const {
        return num_tuples;
    }

    /** Insert tuple */
    virtual void insert(const RamDomain* tuple, const PresenceCondition* pc) {
        // check for null-arity
        if (arity == 0) {
            // set number of tuples to one -- that's it
            records.push_back(nullptr);
            num_tuples = 1;
            return;
        }

        assert(tuple);
        assert(pc);

        if (!pc->isSAT()) {
            return;
        }
        
        // make existence check
        if (exists(tuple, pc)) {
            return;
        }

        int blockIndex = num_tuples / (BLOCK_SIZE / arity);
        int tupleIndex = (num_tuples % (BLOCK_SIZE / arity)) * arity;

        if (tupleIndex == 0) {
            blockList.push_back(std::make_unique<RamDomain[]>(BLOCK_SIZE));
        }

        RamDomain* newTuple = &blockList[blockIndex][tupleIndex];
        for (size_t i = 0; i < arity; ++i) {
            newTuple[i] = tuple[i];
        }

        const RamRecord* rec = new RamRecord(arity, newTuple, pc);
        records.push_back(rec);
        // update all indexes with new tuple
        for (const auto& cur : indices) {
            cur.second->insert(rec);
        }

        // increment relation size
        num_tuples++;
    }

    void insert(const RamRecord* rec) {
        assert(rec);
        insert(rec->field, rec->pc);
    }
    
    /** Insert tuple via arguments */
    /*
    template <typename... Args>
    void insert(RamDomain first, Args... rest) {
        RamDomain tuple[] = {first, RamDomain(rest)...};
        insert(tuple);
    }
    */
    /** Merge another relation into this relation */
    void insert(const InterpreterRelation& other) {
        assert(getArity() == other.getArity());
        size_t count = other.size();
        size_t index = 0;
        for (const auto& cur : other) {
            if (index >= count) {
                break;
            }
            assert(index < count);
            insert(cur->field, cur->pc);
            index++;
        }
    }
    
    /** Purge table */
    void purge() {
        blockList.clear();
        for (const auto& cur : indices) {
            cur.second->purge();
        }
        num_tuples = 0;
    }

    /** get index for a given set of keys using a cached index as a helper. Keys are encoded as bits for each
     * column */
    InterpreterIndex* getIndex(const SearchColumns& key, InterpreterIndex* cachedIndex) const {
        if (!cachedIndex) {
            return getIndex(key);
        }
        return getIndex(cachedIndex->order());
    }

    /** get index for a given set of keys. Keys are encoded as bits for each column */
    InterpreterIndex* getIndex(const SearchColumns& key) const {
        // suffix for order, if no matching prefix exists
        std::vector<unsigned char> suffix;
        suffix.reserve(getArity());

        // convert to order
        InterpreterIndexOrder order;
        for (size_t k = 1, i = 0; i < getArity(); i++, k *= 2) {
            if (key & k) {
                order.append(i);
            } else {
                suffix.push_back(i);
            }
        }

        // see whether there is an order with a matching prefix
        InterpreterIndex* res = nullptr;
        {
            auto lease = lock.acquire();
            (void)lease;

            for (auto it = indices.begin(); !res && it != indices.end(); ++it) {
                if (order.isCompatible(it->first)) {
                    res = it->second.get();
                }
            }
        }
        // if found, use compatible index
        if (res) {
            return res;
        }

        // extend index to full index
        for (auto cur : suffix) {
            order.append(cur);
        }
        assert(order.isComplete());

        // get a new index
        return getIndex(order);
    }

    /** get index for a given order. Keys are encoded as bits for each column */
    InterpreterIndex* getIndex(const InterpreterIndexOrder& order) const {
        // TODO: improve index usage by re-using indices with common prefix
        InterpreterIndex* res = nullptr;
        {
            auto lease = lock.acquire();
            (void)lease;
            auto pos = indices.find(order);
            if (pos == indices.end()) {
                std::unique_ptr<InterpreterIndex>& newIndex = indices[order];
                newIndex = std::make_unique<InterpreterIndex>(order);
                newIndex->insert(this->begin(), this->end());
                res = newIndex.get();
            } else {
                res = pos->second.get();
            }
        }
        return res;
    }

    /** Obtains a full index-key for this relation */
    SearchColumns getTotalIndexKey() const {
        return (1 << (getArity())) - 1;
    }

    /** check whether a tuple exists in the relation */
    bool exists(const RamDomain* tuple, const PresenceCondition* pc) const {
        // handle arity 0
        if (getArity() == 0) {
            return !empty();
        }

        // handle all other arities
        if (!totalIndex) {
            totalIndex = getIndex(getTotalIndexKey());
        }
        RamRecord rec(getArity(), tuple, pc);
        return totalIndex->exists(&rec);
    }
    
    // --- iterator ---

    using iterator = std::list<const RamRecord*>::const_iterator;
    /** Iterator for relation */
    /*
    class iterator : public std::iterator<std::forward_iterator_tag, const RamRecord*> {
        const InterpreterRelation* const relation = nullptr;
        using ttype = std::list<std::unique_ptr<const RamRecord>>::const_iterator;
        ttype tuple_it;
        const RamRecord* tuple;

    public:
        iterator() = default;

        iterator(const InterpreterRelation* const rel,
                 const std::list<std::unique_ptr<const RamRecord>>::const_iterator& it)
                : relation(rel), tuple_it(it), tuple(relation->arity == 0 ? reinterpret_cast<const RamRecord*>(this) : tuple_it->get()) {
                    if (relation->arity != 0 && tuple_it == relation->records.end()) {
                        tuple = nullptr;
                    }
                }

        const RamRecord* operator*() {
            return tuple;
        }

        bool operator==(const iterator& other) const {
            return tuple == other.tuple;
        }

        bool operator!=(const iterator& other) const {
            return tuple != other.tuple;
        }

        iterator& operator++() {
            // support 0-arity
            if (relation->arity == 0) {
                tuple = nullptr;
                return *this;
            }

            tuple_it++;
            if (tuple_it == relation->records.end()) {
                tuple = nullptr; 
            } else {
                tuple = tuple_it->get();
            }

            return *this;
        }
    };
*/
    /** get iterator begin of relation */
    inline iterator begin() const {
        // check for emptiness
        if (empty()) {
            return end();
        }

        return records.begin();
    }

    /** get iterator begin of relation */
    inline iterator end() const {
        return records.end();
    }

    /** Extend tuple */
    virtual std::vector<RamRecord*> extend(const RamDomain* tuple, const PresenceCondition* pc) {
        std::vector<RamRecord*> newTuples;

        // A standard relation does not generate extra new knowledge on insertion.
        newTuples.push_back(new RamRecord(2, new RamDomain[2]{tuple[0], tuple[1]}, pc));

        return newTuples;
    }

    /** Extend relation */
    virtual void extend(const InterpreterRelation& rel) {}
};

/**
 * Interpreter Equivalence Relation
 */

class InterpreterEqRelation : public InterpreterRelation {
public:
    InterpreterEqRelation(size_t relArity) : InterpreterRelation(relArity) {}

    /** Insert tuple */
    void insert(const RamDomain* tuple, const PresenceCondition* pc) override {
        // TODO: (pnappa) an eqrel check here is all that appears to be needed for implicit additions
        // TODO: future optimisation would require this as a member datatype
        // brave soul required to pass this quest
        // // specialisation for eqrel defs
        // std::unique_ptr<binaryrelation> eqreltuples;
        // in addition, it requires insert functions to insert into that, and functions
        // which allow reading of stored values must be changed to accommodate.
        // e.g. insert =>  eqRelTuples->insert(tuple[0], tuple[1]);

        // for now, we just have a naive & extremely slow version, otherwise known as a O(n^2) insertion
        // ):

        for (auto rec : extend(tuple, pc)) {
            InterpreterRelation::insert(rec->field, rec->pc);
            delete[] rec->field;
            delete rec;
        }
    }

    /** Find the new knowledge generated by inserting a tuple */
    std::vector<RamRecord*> extend(const RamDomain* tuple, const PresenceCondition* pc) override {
        std::vector<RamRecord*> newTuples;

        newTuples.push_back(new RamRecord(2, new RamDomain[2]{tuple[0], tuple[0]}, pc));
        newTuples.push_back(new RamRecord(2, new RamDomain[2]{tuple[0], tuple[1]}, pc));
        newTuples.push_back(new RamRecord(2, new RamDomain[2]{tuple[1], tuple[0]}, pc));
        newTuples.push_back(new RamRecord(2, new RamDomain[2]{tuple[1], tuple[1]}, pc));

        std::vector<const RamRecord*> relevantStored;
        for (auto rec : *this) {
            if (!pc->conjSat(rec->pc)) {
                continue;
            }
            const RamDomain* vals = rec->field;
            if (vals[0] == tuple[0] || vals[0] == tuple[1] || vals[1] == tuple[0] || vals[1] == tuple[1]) {
                relevantStored.push_back(rec);
            }
        }

        for (const auto rec : relevantStored) {
            const RamDomain* vals = rec->field;
            auto conjunction = pc->conjoin(rec->pc);
            newTuples.push_back(new RamRecord(2, new RamDomain[2]{vals[0], tuple[0]}, conjunction));
            newTuples.push_back(new RamRecord(2, new RamDomain[2]{vals[0], tuple[1]}, conjunction));
            newTuples.push_back(new RamRecord(2, new RamDomain[2]{vals[1], tuple[0]}, conjunction));
            newTuples.push_back(new RamRecord(2, new RamDomain[2]{vals[1], tuple[1]}, conjunction));
            newTuples.push_back(new RamRecord(2, new RamDomain[2]{tuple[0], vals[0]}, conjunction));
            newTuples.push_back(new RamRecord(2, new RamDomain[2]{tuple[0], vals[1]}, conjunction));
            newTuples.push_back(new RamRecord(2, new RamDomain[2]{tuple[1], vals[0]}, conjunction));
            newTuples.push_back(new RamRecord(2, new RamDomain[2]{tuple[1], vals[1]}, conjunction));
        }

        return newTuples;
    }
    /** Extend this relation with new knowledge generated by inserting all tuples from a relation */
    void extend(const InterpreterRelation& rel) override {
        std::vector<RamRecord*> newTuples;
        // store all values that will be implicitly relevant to the those that we will insert
        for (auto rec : rel) {
            for (auto* newTuple : extend(rec->field, rec->pc)) {
                newTuples.push_back(newTuple);
            }
        }
        for (const auto* newTuple : newTuples) {
            InterpreterRelation::insert(newTuple->field, newTuple->pc);
            delete[] newTuple->field;
            delete newTuple;
        }
    }
};

}  // end of namespace souffle
