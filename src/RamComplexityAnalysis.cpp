/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2019, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamComplexityAnalysis.cpp
 *
 * Implementation of RAM Complexity Analysis
 *
 ***********************************************************************/

#include "RamComplexityAnalysis.h"
#include "RamVisitor.h"
#include <algorithm>

namespace souffle {

int RamComplexityAnalysis::getComplexity(const RamNode* node) const {
    // visitor
    class ValueComplexityVisitor : public RamVisitor<int> {
    public:
        // conjunction
        int visitConjunction(const RamConjunction& conj) override {
            return visit(conj.getLHS()) + visit(conj.getRHS());
        }

        // negation
        int visitNegation(const RamNegation& neg) override {
            return visit(neg.getOperand());
        }

        // existence check
        int visitExistenceCheck(const RamExistenceCheck& exists) override {
            return 2;
        }

        // provenance existence check
        int visitProvenanceExistenceCheck(const RamProvenanceExistenceCheck& provExists) override {
            return 2;
        }

        // emptiness check
        int visitEmptinessCheck(const RamEmptinessCheck& emptiness) override {
            // emptiness check for nullary relations is for free; others have weight one
            return (emptiness.getRelation().getArity()> 0)? 1:0;
        }

        // default rule
        int visitNode(const RamNode& node) override {
            return 0;
        }
    };

    assert((dynamic_cast<const RamExpression*>(node) != nullptr ||
                   dynamic_cast<const RamCondition*>(node) != nullptr) &&
            "not an expression/condition/operation");
    return ValueComplexityVisitor().visit(node);
}

}  // end of namespace souffle
