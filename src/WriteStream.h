/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file WriteStream.h
 *
 ***********************************************************************/

#pragma once

#include "IODirectives.h"
#include "RamTypes.h"
#include "SymbolMask.h"
#include "SymbolTable.h"

namespace souffle {

class WriteStream {
public:
    WriteStream(const SymbolMask& symbolMask, const SymbolTable& symbolTable,
                const SymbolTable& featSymT, const bool prov)
            : symbolMask(symbolMask), symbolTable(symbolTable), featSymTable(featSymT), isProvenance(prov) {}
    template <typename T>
    void writeAll(T& relation) {
        auto lease = symbolTable.acquireLock();
        (void)lease;
        size_t arity = symbolMask.getArity();
        if (isProvenance) {
            arity -=  2;
        }

        if (arity == 0 && relation.size()) {
            writeNullary();
            return;
        }

        for (auto current : relation) {
            writeNext(current);
        }
    }

    virtual void writeNullary() = 0;

    virtual ~WriteStream() = default;

protected:
    virtual void writeNextTuple(const RamRecord* record) = 0;
    template <typename Tuple>
    void writeNext(Tuple tuple) {
        RamRecord rec = tuple.toRecord();
        writeNextTuple(&rec);
    }
    const SymbolMask& symbolMask;
    const SymbolTable& symbolTable;
    const SymbolTable& featSymTable;
    const bool isProvenance;
};

class WriteStreamFactory {
public:
    virtual std::unique_ptr<WriteStream> getWriter(const SymbolMask& symbolMask,
            const SymbolTable& symbolTable, const SymbolTable& featSymT,
            const IODirectives& ioDirectives, const bool provenance) = 0;
    virtual const std::string& getName() const = 0;
    virtual ~WriteStreamFactory() = default;
};

template <>
inline void WriteStream::writeNext(const RamRecord* record) {
    writeNextTuple(record);
}

template <>
inline void WriteStream::writeNext(const RamDomain* d) {
    RamRecord rec(symbolMask.getArity(), d, PresenceCondition::makeTrue());
    writeNextTuple(&rec);
}
} /* namespace souffle */
