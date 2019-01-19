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
#include <tuple>

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

public:
    InterpreterRelation(size_t relArity) : arity(relArity), num_tuples(0), totalIndex(nullptr) {}

    InterpreterRelation(const InterpreterRelation& other) = delete;

    virtual ~InterpreterRelation() = default;

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
    virtual void insert(const RamDomain* tuple) {
        // check for null-arity
        if (arity == 0) {
            // set number of tuples to one -- that's it
            num_tuples = 1;
            return;
        }

        assert(tuple);

        // make existence check
        if (exists(tuple)) {
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
    void insert(const InterpreterRelation& other) {
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
    bool exists(const RamDomain* tuple) const {
        // handle arity 0
        if (getArity() == 0) {
            return !empty();
        }

        // handle all other arities
        if (!totalIndex) {
            totalIndex = getIndex(getTotalIndexKey());
        }
        return totalIndex->exists(tuple);
    }

    // --- iterator ---

    /** Iterator for relation */
    class iterator : public std::iterator<std::forward_iterator_tag, RamDomain*> {
        const InterpreterRelation* relation = nullptr;
        size_t index = 0;
        RamDomain* tuple = nullptr;

    public:
        iterator() = default;
        iterator& operator=(const iterator&) = default;

        iterator(const InterpreterRelation* const relation)
                : relation(relation), tuple(relation->arity == 0 ? reinterpret_cast<RamDomain*>(this)
                                                                 : &relation->blockList[0][0]) 
                                                                 {assert(relation);}

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
            if (relation->arity == 0) {
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
    virtual void extend(const InterpreterRelation& rel) {}
};

/**
 * Interpreter Equivalence Relation
 */

class InterpreterEqRelation : public InterpreterRelation {
public:
    InterpreterEqRelation(size_t relArity) : InterpreterRelation(relArity) {}

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
            InterpreterRelation::insert(newTuple);
            delete[] newTuple;
        }
    }

    /** Find the new knowledge generated by inserting a tuple */
    std::vector<RamDomain*> extend(const RamDomain* tuple) override {
        std::vector<RamDomain*> newTuples;

        newTuples.push_back(new RamDomain[2]{tuple[0], tuple[0]});
        newTuples.push_back(new RamDomain[2]{tuple[0], tuple[1]});
        newTuples.push_back(new RamDomain[2]{tuple[1], tuple[0]});
        newTuples.push_back(new RamDomain[2]{tuple[1], tuple[1]});

        std::vector<const RamDomain*> relevantStored;
        for (const RamDomain* vals : *this) {
            if (vals[0] == tuple[0] || vals[0] == tuple[1] || vals[1] == tuple[0] || vals[1] == tuple[1]) {
                relevantStored.push_back(vals);
            }
        }

        for (const auto vals : relevantStored) {
            newTuples.push_back(new RamDomain[2]{vals[0], tuple[0]});
            newTuples.push_back(new RamDomain[2]{vals[0], tuple[1]});
            newTuples.push_back(new RamDomain[2]{vals[1], tuple[0]});
            newTuples.push_back(new RamDomain[2]{vals[1], tuple[1]});
            newTuples.push_back(new RamDomain[2]{tuple[0], vals[0]});
            newTuples.push_back(new RamDomain[2]{tuple[0], vals[1]});
            newTuples.push_back(new RamDomain[2]{tuple[1], vals[0]});
            newTuples.push_back(new RamDomain[2]{tuple[1], vals[1]});
        }

        return newTuples;
    }
    /** Extend this relation with new knowledge generated by inserting all tuples from a relation */
    void extend(const InterpreterRelation& rel) override {
        std::vector<RamDomain*> newTuples;
        // store all values that will be implicitly relevant to the those that we will insert
        for (const auto* tuple : rel) {
            for (auto* newTuple : extend(tuple)) {
                newTuples.push_back(newTuple);
            }
        }
        for (const auto* newTuple : newTuples) {
            InterpreterRelation::insert(newTuple);
            delete[] newTuple;
        }
    }
};

class LiftedInterpreterRelation {
private:
    std::map<const PresenceCondition*, InterpreterRelation*> rels;
    size_t arity;
    bool   eqRel;

public:
    LiftedInterpreterRelation(bool _eqRel, size_t _arity) : arity(_arity), eqRel(_eqRel) {
    }

    ~LiftedInterpreterRelation() {
        for (auto& r: rels) {
            delete r.second;
        }
    }

    bool isEqRel() const {
        return eqRel;
    }

    /** Insert tuple */
    void insert(const RamDomain* tuple, const PresenceCondition* pc) {
        assert(tuple);
        assert(pc);
        if (!pc->isSAT()) {
            return;
        }

        auto it = rels.find(pc);
        if (it == rels.end()) {
            InterpreterRelation* r = eqRel ? new InterpreterEqRelation(arity) : new InterpreterRelation(arity);
            auto ret = rels.emplace(pc, r);
            assert(ret.second);
            it = ret.first;
        }
        auto& rel = it->second;
        assert(rel);

        rel->insert(tuple);
    }

    /** Insert tuple via arguments */
    //template <typename... Args>
    //void insert(RamDomain first, Args... rest) {
    //    rel->insert(first, rest...);
    //}

    void insert(const RamRecord* rec) {
        assert(rec);
        insert(rec->field, rec->pc);
    }

    /** Merge another relation into this relation */
    void insert(const LiftedInterpreterRelation& other) {
        for (const auto& r: other.rels) {
            const PresenceCondition* pc = r.first;

            auto it = rels.find(pc);
            if (it == rels.end()) {
                InterpreterRelation* r = eqRel ? new InterpreterEqRelation(arity) : new InterpreterRelation(arity);
                auto ret = rels.emplace(pc, r);
                assert(ret.second);
                it = ret.first;
            }

            auto& rel = it->second;
            assert(rel);

            rel->insert(*(r.second));
        }
    }

    /** Get arity of relation */
    size_t getArity() const {
        return arity;
    }

    /** Check whether relation is empty */
    bool empty() const {
        bool empty = true;
        for(const auto& r: rels) {
            if (!r.second->empty()) {
                empty = false;
                break;
            }
        }
        return empty;
    }

    /** check whether a tuple exists in the relation */
    bool exists(const RamDomain* tuple, const PresenceCondition* pc) const {
        const auto r = rels.find(pc);
        if (r == rels.end()) {
            return false;
        }
        return (r->second->exists(tuple));
    }

    /** Gets the number of contained tuples */
    size_t size() const {
        // not: size is not lifted (i.e. we don't return different sizes with corresponding presence conditions)
        size_t size = 0;
        for(const auto& r: rels) {
            size += r.second->size();
        }

        return size;
    }

    /** Purge table */
    void purge() {
        for(const auto& r: rels) {
            r.second->purge();
        }
    }

    /** Extend tuple */
    std::vector<RamDomain*> extend(const RamDomain* tuple) {
        for (auto& rel: rels) {
            return rel.second->extend(tuple);
        }
    }

    /** Extend relation */
    void extend(const LiftedInterpreterRelation& r) {
        for (auto& rel: r.rels) {
            const PresenceCondition* pc = rel.first;

            auto it = rels.find(pc);
            if (it == rels.end()) {
                InterpreterRelation* r = eqRel ? new InterpreterEqRelation(arity) : new InterpreterRelation(arity);
                auto ret = rels.emplace(pc, r);
                assert(ret.second);
                it = ret.first;
            }

            auto& thisRel = it->second;
            assert(thisRel);

            thisRel->extend(*(rel.second));
        }
    }

    using IndexRange = std::tuple<const PresenceCondition*, InterpreterIndex::iterator, InterpreterIndex::iterator>;

    /** get index for a given set of keys using a cached index as a helper. Keys are encoded as bits for each
     * column */
    std::vector<IndexRange> getRange(const SearchColumns& key, InterpreterIndex* cachedIndex, const RamDomain* low, const RamDomain* high) const {
        std::vector<IndexRange> ret;
        ret.reserve(rels.size());

        for (const auto& rel: rels) {
            InterpreterIndex* index = cachedIndex ? rel.second->getIndex(key, cachedIndex) : rel.second->getIndex(key);
            auto range = index->lowerUpperBound(low, high);
            
            // only include non-empty ranges
            if (range.first != range.second) {
                ret.push_back(std::make_tuple(rel.first, range.first, range.second));
            }
        }

        return ret;
    }

    /** get index for a given set of keys. Keys are encoded as bits for each column */
    std::vector<IndexRange> getRange(const SearchColumns& key, const RamDomain* low, const RamDomain* high) const {
        return getRange(key, nullptr, low, high);
    }

    class iterator : public std::iterator<std::forward_iterator_tag, RamDomain*> {
        std::map<const PresenceCondition*, InterpreterRelation*>::const_iterator mapItr;
        std::map<const PresenceCondition*, InterpreterRelation*>::const_iterator endItr;
        const PresenceCondition* curPC;
        InterpreterRelation::iterator curRelEnd;
        InterpreterRelation::iterator curItr;
        bool  end;

        void resetCur(const std::map<const PresenceCondition*, InterpreterRelation*>::const_iterator& itr) {
            mapItr = itr;
            if (itr == endItr) {
                return;
            }

            curPC = itr->first;
            assert(curPC);
            assert(itr->second);
            curItr = itr->second->begin();
            curRelEnd = itr->second->end();

            if (curItr == curRelEnd) {
                resetCur(++mapItr);
            }
        }

    public:
        iterator(const std::map<const PresenceCondition*, InterpreterRelation*>::const_iterator& itr, 
                 const std::map<const PresenceCondition*, InterpreterRelation*>::const_iterator& eItr)
                : mapItr(itr), endItr(eItr), end(itr == eItr) 
        {
            if (end) {
                curPC = nullptr;
            } else {
                resetCur(itr);
            }
        }

        const RamDomain* operator*() {
            return (*curItr);
        }

        bool operator==(const iterator& other) const {
            return (mapItr == other.mapItr && (end || curItr == other.curItr));
        }

        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }

        iterator& operator++() {
            ++curItr;
            if (curItr == curRelEnd) {
                mapItr++;
                resetCur(mapItr);
            }
            return *this;
        }

        const PresenceCondition* getCurPC() const {
            return curPC;
        }
    };

#if 0 // these functions don't seem to be used anywhere
    /** get index for a given order. Keys are encoded as bits for each column */
    InterpreterIndex* getIndex(const InterpreterIndexOrder& order) const {
        return rel->getIndex(order);
    }

    /** Obtains a full index-key for this relation */
    SearchColumns getTotalIndexKey() const {
        return rel->getTotalIndexKey();
    }
#endif

    /** get iterator begin of relation */
    inline iterator begin() const {
        return iterator(rels.cbegin(), rels.cend());
    }

    /** get iterator end of relation */
    inline iterator end() const {
        return iterator(rels.cend(), rels.cend());
    }
}; // LiftedInterpreterRelation

} // end of namespace souffle
