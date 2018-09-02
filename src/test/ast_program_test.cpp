/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file type_system_test.h
 *
 * Tests souffle's type system operations.
 *
 ***********************************************************************/

#include "AstProgram.h"
#include "AstTranslationUnit.h"
#include "ParserDriver.h"
#include "SymbolTable.h"
#include "test.h"

namespace souffle {

namespace test {

TEST(AstProgram, Parse) {
    SymbolTable sym;
    SymbolTable featSymTab;
    ErrorReport e;
    DebugReport d;
    // check the empty program
    std::unique_ptr<AstTranslationUnit> empty = ParserDriver::parseTranslationUnit("", sym, featSymTab, e, d);

    EXPECT_TRUE(empty->getProgram()->getTypes().empty());
    EXPECT_TRUE(empty->getProgram()->getRelations().empty());

    // check something simple
    std::unique_ptr<AstTranslationUnit> prog = ParserDriver::parseTranslationUnit(
            R"(
                   .type Node
                   .decl e ( a : Node , b : Node )
                   .decl r ( from : Node , to : Node )

                   r(X,Y) :- e(X,Y).
                   r(X,Z) :- r(X,Y), r(Y,Z).
            )",
            sym, featSymTab, e, d);

    std::cout << prog->getProgram() << "\n";

    EXPECT_EQ(1, prog->getProgram()->getTypes().size());
    EXPECT_EQ(2, prog->getProgram()->getRelations().size());

    EXPECT_TRUE(prog->getProgram()->getRelation("e"));
    EXPECT_TRUE(prog->getProgram()->getRelation("r"));
    EXPECT_FALSE(prog->getProgram()->getRelation("n"));
}

TEST(AstProgram, ParseWithPCs) {
    SymbolTable sym;
    SymbolTable featSymTab;
    ErrorReport e;
    DebugReport d;

    // check something simple
    std::unique_ptr<AstTranslationUnit> prog = ParserDriver::parseTranslationUnit(
            R"(
                .type D = number
                .decl a(a:D,b:D)
                .decl b(a:D,b:D)
                .decl c(a:D,b:D) output
                .decl d(a:D,b:D)

                a(1,2) @ A /\ B.
                b(x,y) :- a(x,y).
                c(x,y) :- b(x,y).

                d(x,y) :- b(x,y), c(y,x).

            )",
            sym, featSymTab, e, d);

    std::cout << prog->getProgram() << "\n";

    AstProgram& program = *prog->getProgram();

    EXPECT_EQ(4, program.getRelations().size());
}

}  // end namespace test
}  // end namespace souffle
