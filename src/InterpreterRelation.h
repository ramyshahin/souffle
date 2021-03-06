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

#include <deque>
#include <map>
#include <memory>
#include <vector>

namespace souffle {

/**
 * Interpreter Relation
 */
class InterpreterRelationInner {
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

public:
    InterpreterRelationInner(size_t relArity) : arity(relArity), num_tuples(0), totalIndex(nullptr) {}

    InterpreterRelationInner(const InterpreterRelationInner& other) = delete;

    virtual ~InterpreterRelationInner() = default;

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

    bool isNullary() const {
        return (arity == 1);
    }

    /** Insert tuple */
    virtual void insert(const RamDomain* tuple) {
        // check for null-arity
        if (isNullary()) {
            // set number of tuples to one -- that's it
            num_tuples = 1;
            return;
        }

        assert(tuple);

        const size_t pcIndex = arity - 1;
        PresenceCondition* pcTuple = (PresenceCondition*) tuple[pcIndex];
        if (!pcTuple->isSAT()) {
            return;
        }

        // make existence check
        const RamDomain* out = nullptr;
        const PresenceCondition* ff = PresenceCondition::makeFalse();

        if (exists(tuple, out) != ff) {
            assert(out);
            PresenceCondition* pc = (PresenceCondition*) out[pcIndex];
            // check PCs
            if (pcTuple != pc) {
                ((RamDomain*)out)[pcIndex] = (RamDomain)pc->disjoin(pcTuple);
            }
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

        // update all indexes with new tuple
        for (const auto& cur : indices) {
            cur.second->insert(newTuple);
        }

        // increment relation size
        num_tuples++;
    }

    /** Insert tuple via arguments */
    template <typename... Args>
    void insert(RamDomain first, Args... rest) {
        RamDomain tuple[] = {first, RamDomain(rest)...};
        insert(tuple);
    }

    /** Merge another relation into this relation */
    void insert(const InterpreterRelationInner& other) {
        assert(getArity() == other.getArity());
        for (const auto& cur : other) {
            insert(cur);
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
        for (size_t k = 1, i = 0; i < getArity() - 1; i++, k *= 2) {
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
        return (1 << (getArity() - 1)) - 1;
    }

    /** check whether a tuple exists in the relation */
    const PresenceCondition* exists(const RamDomain* tuple, const RamDomain*& out) const {
        const PresenceCondition* tt = PresenceCondition::makeTrue();
        const PresenceCondition* ff = PresenceCondition::makeFalse();

	// handle arity 0
        if (isNullary()) {
            return empty() ? ff : tt;
        }

        // handle all other arities
        if (!totalIndex) {
            totalIndex = getIndex(getTotalIndexKey());
        }

        const RamDomain* d = totalIndex->exists(tuple);

        if (d) {
            out = d;
        }

        return (d != nullptr) ? (const PresenceCondition*) d[arity-1] : ff;
    }

    // --- iterator ---

    /** Iterator for relation */
    class iterator : public std::iterator<std::forward_iterator_tag, RamDomain*> {
        const InterpreterRelationInner* const relation = nullptr;
        size_t index = 0;
        RamDomain* tuple = nullptr;

    public:
        iterator() = default;

        iterator(const InterpreterRelationInner* const relation)
                : relation(relation), tuple(relation->isNullary() ? reinterpret_cast<RamDomain*>(this)
                                                                 : &relation->blockList[0][0]) {}

        const RamDomain* operator*() {
            return tuple;
        }

        bool operator==(const iterator& other) const {
            return tuple == other.tuple;
        }

        bool operator!=(const iterator& other) const {
            return (tuple != other.tuple);
        }

        iterator& operator++() {
            // support 0-arity
            if (relation->isNullary()) {
                tuple = nullptr;
                return *this;
            }

            // support all other arities
            ++index;
            if (index == relation->num_tuples) {
                tuple = nullptr;
                return *this;
            }

            int blockIndex = index / (BLOCK_SIZE / relation->arity);
            int tupleIndex = (index % (BLOCK_SIZE / relation->arity)) * relation->arity;

            tuple = &relation->blockList[blockIndex][tupleIndex];
            return *this;
        }
    };

    /** get iterator begin of relation */
    inline iterator begin() const {
        // check for emptiness
        if (empty()) {
            return end();
        }

        return iterator(this);
    }

    /** get iterator begin of relation */
    inline iterator end() const {
        return iterator();
    }

    /** Extend tuple */
    virtual std::vector<RamDomain*> extend(const RamDomain* tuple) {
        std::vector<RamDomain*> newTuples;

        // A standard relation does not generate extra new knowledge on insertion.
        newTuples.push_back(new RamDomain[2]{tuple[0], tuple[1]});

        return newTuples;
    }

    /** Extend relation */
    virtual void extend(const InterpreterRelationInner& rel) {}
};

/**
 * Interpreter Equivalence Relation
 */

class InterpreterEqRelationInner : public InterpreterRelationInner {
public:
    InterpreterEqRelationInner(size_t relArity) : InterpreterRelationInner(relArity) {}

    /** Insert tuple */
    void insert(const RamDomain* tuple) override {
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

        for (auto* newTuple : extend(tuple)) {
            InterpreterRelationInner::insert(newTuple);
            delete[] newTuple;
        }
    }

    /** Find the new knowledge generated by inserting a tuple */
    std::vector<RamDomain*> extend(const RamDomain* tuple) override {
        std::vector<RamDomain*> newTuples;
        
        size_t arity = getArity();
        assert(arity == 3);
        auto pc = tuple[arity-1];
        
        newTuples.push_back(new RamDomain[3]{tuple[0], tuple[0], pc});
        newTuples.push_back(new RamDomain[3]{tuple[0], tuple[1], pc});
        newTuples.push_back(new RamDomain[3]{tuple[1], tuple[0], pc});
        newTuples.push_back(new RamDomain[3]{tuple[1], tuple[1], pc});

        std::vector<const RamDomain*> relevantStored;
        for (const RamDomain* vals : *this) {
            if (vals[0] == tuple[0] || vals[0] == tuple[1] || vals[1] == tuple[0] || vals[1] == tuple[1]) {
                relevantStored.push_back(vals);
            }
        }

        for (const auto vals : relevantStored) {
            newTuples.push_back(new RamDomain[3]{vals[0], tuple[0], pc});
            newTuples.push_back(new RamDomain[3]{vals[0], tuple[1], pc});
            newTuples.push_back(new RamDomain[3]{vals[1], tuple[0], pc});
            newTuples.push_back(new RamDomain[3]{vals[1], tuple[1], pc});
            newTuples.push_back(new RamDomain[3]{tuple[0], vals[0], pc});
            newTuples.push_back(new RamDomain[3]{tuple[0], vals[1], pc});
            newTuples.push_back(new RamDomain[3]{tuple[1], vals[0], pc});
            newTuples.push_back(new RamDomain[3]{tuple[1], vals[1], pc});
        }

        return newTuples;
    }
    /** Extend this relation with new knowledge generated by inserting all tuples from a relation */
    void extend(const InterpreterRelationInner& rel) override {
        std::vector<RamDomain*> newTuples;
        // store all values that will be implicitly relevant to the those that we will insert
        for (const auto* tuple : rel) {
            for (auto* newTuple : extend(tuple)) {
                newTuples.push_back(newTuple);
            }
        }
        for (const auto* newTuple : newTuples) {
            InterpreterRelationInner::insert(newTuple);
            delete[] newTuple;
        }
    }
};

class InterpreterRelation {
private:
    size_t  arity;
    bool    eqRel;
    InterpreterRelationInner* rel;

public:
    InterpreterRelation(size_t _arity, bool _eqRel) :
        arity(_arity),
        eqRel(_eqRel),
        rel (eqRel ? new InterpreterEqRelationInner(arity + 1) : new InterpreterRelationInner(arity + 1))
        {} 

    InterpreterRelation(const InterpreterRelation& other) = delete;

    ~InterpreterRelation() {
        delete rel;
    }

    bool isEqRel() const {
        return eqRel;
    }

    /** Get arity of relation */
    size_t getArity() const {
        return arity;
    }

    /** Check whether relation is empty */
    bool empty() const {
        return rel->empty();
    }

    /** Gets the number of contained tuples */
    size_t size() const {
        return rel->size();
    }

    /** Insert tuple */
    void insert(const RamDomain* tuple) {
        rel->insert(tuple);
    }

    /** Insert tuple via arguments */
    //template <typename... Args>
    //void insert(RamDomain first, Args... rest) {
    //    RamDomain tuple[] = {first, RamDomain(rest)...};
    //    insert(tuple);
    //}

    /** Merge another relation into this relation */
    void insert(const InterpreterRelation& other) {
        rel->insert(*other.rel);
    }

    /** Purge table */
    void purge() {
        rel->purge();
    }

    /** get index for a given set of keys using a cached index as a helper. Keys are encoded as bits for each
     * column */
    InterpreterIndex* getIndex(const SearchColumns& key, InterpreterIndex* cachedIndex) const {
        return rel->getIndex(key, cachedIndex);
    }

    /** get index for a given set of keys. Keys are encoded as bits for each column */
    InterpreterIndex* getIndex(const SearchColumns& key) const {
        return rel->getIndex(key);
    }

    /** get index for a given order. Keys are encoded as bits for each column */
    InterpreterIndex* getIndex(const InterpreterIndexOrder& order) const {
        return rel->getIndex(order);
    }

    /** Obtains a full index-key for this relation */
    SearchColumns getTotalIndexKey() const {
        return rel->getTotalIndexKey();
    }

    /** check whether a tuple exists in the relation */
    const PresenceCondition* exists(const RamDomain* tuple, const RamDomain* out) const {
        return rel->exists(tuple, out);
    }

    using iterator = InterpreterRelationInner::iterator;

    /** get iterator begin of relation */
    inline iterator begin() const {
        return rel->begin();
    }

    /** get iterator begin of relation */
    inline iterator end() const {
        return rel->end();
    }

    /** Extend tuple */
    std::vector<RamDomain*> extend(const RamDomain* tuple) {
        return rel->extend(tuple);
    }

    void extend(const InterpreterRelation& other) {
        rel->extend(*other.rel);
    }

    const PresenceCondition* getPC(const RamDomain* tuple) const {
        return (arity == 0) ? PresenceCondition::makeTrue() :
                              (const PresenceCondition*) tuple[arity];
    }
}; // InterpreterRelation

}  // end of namespace souffle
