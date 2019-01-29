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
#include "InterpreterRelation.h"

namespace souffle {

class WriteStream {
public:
    static size_t recordCount;
    static size_t pcCount;

    WriteStream(const SymbolMask& symbolMask, const SymbolTable& symbolTable,
                const SymbolTable& featSymT, const bool prov)
            : symbolMask(symbolMask), symbolTable(symbolTable), featSymTable(featSymT), isProvenance(prov) {}
    template <typename T>
    void writeAll(const T& relation) {
        auto lease = symbolTable.acquireLock();
        (void)lease;
        for (const auto& current : relation) {
            recordCount++;
            writeNext(current, relation);
        }
    }

    virtual ~WriteStream() = default;

protected:
    virtual void writeNextTuple(const RamDomain* tuple, const InterpreterRelation& rel) = 0;
    template <typename T, typename Tuple>
    void writeNext(const Tuple tuple, const T& relation) {
        writeNextTuple(tuple.data, relation);
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
inline void WriteStream::writeNext(const RamDomain* tuple, const InterpreterRelation& rel) {
    writeNextTuple(tuple, rel);
}

} /* namespace souffle */
