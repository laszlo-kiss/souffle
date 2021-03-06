/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file TranslatorContext.cpp
 *
 ***********************************************************************/

#include "ast2ram/utility/TranslatorContext.h"
#include "Global.h"
#include "ast/Atom.h"
#include "ast/Directive.h"
#include "ast/QualifiedName.h"
#include "ast/TranslationUnit.h"
#include "ast/analysis/AuxArity.h"
#include "ast/analysis/Functor.h"
#include "ast/analysis/IOType.h"
#include "ast/analysis/RecursiveClauses.h"
#include "ast/analysis/RelationDetailCache.h"
#include "ast/analysis/RelationSchedule.h"
#include "ast/analysis/SCCGraph.h"
#include "ast/analysis/TypeEnvironment.h"
#include "ast/analysis/TypeSystem.h"
#include "ast/utility/SipsMetric.h"
#include "ast/utility/Utils.h"
#include "souffle/utility/FunctionalUtil.h"
#include "souffle/utility/StringUtil.h"
#include <set>

namespace souffle::ast2ram {

TranslatorContext::TranslatorContext(const ast::TranslationUnit& tu) {
    program = &tu.getProgram();

    // Set up analyses
    auxArityAnalysis = tu.getAnalysis<ast::analysis::AuxiliaryArityAnalysis>();
    functorAnalysis = tu.getAnalysis<ast::analysis::FunctorAnalysis>();
    recursiveClauses = tu.getAnalysis<ast::analysis::RecursiveClausesAnalysis>();
    sccGraph = tu.getAnalysis<ast::analysis::SCCGraphAnalysis>();
    relationSchedule = tu.getAnalysis<ast::analysis::RelationScheduleAnalysis>();
    relationDetail = tu.getAnalysis<ast::analysis::RelationDetailCacheAnalysis>();
    ioType = tu.getAnalysis<ast::analysis::IOTypeAnalysis>();
    typeEnv = &tu.getAnalysis<ast::analysis::TypeEnvironmentAnalysis>()->getTypeEnvironment();

    // Set up SIPS metric
    std::string sipsChosen = "all-bound";
    if (Global::config().has("RamSIPS")) {
        sipsChosen = Global::config().get("RamSIPS");
    }
    sipsMetric = ast::SipsMetric::create(sipsChosen, tu);
}

TranslatorContext::~TranslatorContext() = default;

bool TranslatorContext::isRecursiveClause(const ast::Clause* clause) const {
    return recursiveClauses->recursive(clause);
}

std::string TranslatorContext::getAttributeTypeQualifier(const ast::QualifiedName& name) const {
    return getTypeQualifier(typeEnv->getType(name));
}

size_t TranslatorContext::getNumberOfSCCs() const {
    return sccGraph->getNumberOfSCCs();
}

bool TranslatorContext::isRecursiveSCC(size_t scc) const {
    return sccGraph->isRecursive(scc);
}

std::vector<ast::Directive*> TranslatorContext::getStoreDirectives(const ast::QualifiedName& name) const {
    return filter(getDirectives(*program, name), [&](const ast::Directive* dir) {
        return dir->getType() == ast::DirectiveType::printsize ||
               dir->getType() == ast::DirectiveType::output;
    });
}

std::vector<ast::Directive*> TranslatorContext::getLoadDirectives(const ast::QualifiedName& name) const {
    return filter(getDirectives(*program, name),
            [&](const ast::Directive* dir) { return dir->getType() == ast::DirectiveType::input; });
}

bool TranslatorContext::hasSizeLimit(const ast::Relation* relation) const {
    return ioType->isLimitSize(relation);
}

size_t TranslatorContext::getSizeLimit(const ast::Relation* relation) const {
    assert(hasSizeLimit(relation) && "relation does not have a size limit");
    return ioType->getLimitSize(relation);
}

const ast::Relation* TranslatorContext::getAtomRelation(const ast::Atom* atom) const {
    return ast::getAtomRelation(atom, program);
}

std::set<const ast::Relation*> TranslatorContext::getRelationsInSCC(size_t scc) const {
    return sccGraph->getInternalRelations(scc);
}

std::set<const ast::Relation*> TranslatorContext::getInputRelationsInSCC(size_t scc) const {
    return sccGraph->getInternalInputRelations(scc);
}

std::set<const ast::Relation*> TranslatorContext::getOutputRelationsInSCC(size_t scc) const {
    return sccGraph->getInternalOutputRelations(scc);
}

std::set<const ast::Relation*> TranslatorContext::getExpiredRelations(size_t scc) const {
    return relationSchedule->schedule().at(scc).expired();
}

std::set<ast::Clause*> TranslatorContext::getClauses(const ast::QualifiedName& name) const {
    return relationDetail->getClauses(name);
}

ast::Relation* TranslatorContext::getRelation(const ast::QualifiedName& name) const {
    return relationDetail->getRelation(name);
}

TypeAttribute TranslatorContext::getFunctorReturnType(const ast::Functor* functor) const {
    return functorAnalysis->getReturnType(functor);
}

TypeAttribute TranslatorContext::getFunctorArgType(const ast::Functor* functor, size_t idx) const {
    return functorAnalysis->getArgType(functor, idx);
}

const std::vector<TypeAttribute>& TranslatorContext::getFunctorArgTypes(
        const ast::UserDefinedFunctor& udf) const {
    return functorAnalysis->getArgTypes(udf);
}

bool TranslatorContext::isStatefulFunctor(const ast::UserDefinedFunctor* udf) const {
    return functorAnalysis->isStateful(udf);
}

size_t TranslatorContext::getAuxiliaryArity(const ast::Atom* atom) const {
    return auxArityAnalysis->getArity(atom);
}

size_t TranslatorContext::getAuxiliaryArity(const ast::Relation* relation) const {
    return auxArityAnalysis->getArity(relation);
}

size_t TranslatorContext::getEvaluationArity(const ast::Atom* atom) const {
    std::string relName = atom->getQualifiedName().toString();
    if (isPrefix("@info_", relName)) return 0;

    // Get the original relation name
    if (isPrefix("@delta_", relName)) {
        relName = stripPrefix("@delta_", relName);
    } else if (isPrefix("@new_", relName)) {
        relName = stripPrefix("@new_", relName);
    }

    const auto* originalRelation = getRelation(ast::QualifiedName(relName));
    return auxArityAnalysis->getArity(originalRelation);
}

}  // namespace souffle::ast2ram
