/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file TranslatorContext.h
 *
 ***********************************************************************/

#pragma once

#include "souffle/TypeAttribute.h"
#include "souffle/utility/ContainerUtil.h"
#include <cstddef>
#include <set>

namespace souffle::ast {
class Atom;
class Clause;
class Directive;
class Functor;
class Program;
class QualifiedName;
class Relation;
class SipsMetric;
class TranslationUnit;
class UserDefinedFunctor;
}  // namespace souffle::ast

namespace souffle::ast::analysis {
class AuxiliaryArityAnalysis;
class FunctorAnalysis;
class IOTypeAnalysis;
class RecursiveClausesAnalysis;
class RelationDetailCacheAnalysis;
class RelationScheduleAnalysis;
class SCCGraphAnalysis;
class TypeEnvironment;
}  // namespace souffle::ast::analysis

namespace souffle::ast2ram {

class TranslatorContext {
public:
    TranslatorContext(const ast::TranslationUnit& tu);
    ~TranslatorContext();

    const ast::Program* getProgram() const {
        return program;
    }

    /** Relation methods */
    ast::Relation* getRelation(const ast::QualifiedName& name) const;
    const ast::Relation* getAtomRelation(const ast::Atom* atom) const;
    std::vector<ast::Directive*> getStoreDirectives(const ast::QualifiedName& name) const;
    std::vector<ast::Directive*> getLoadDirectives(const ast::QualifiedName& name) const;
    std::string getAttributeTypeQualifier(const ast::QualifiedName& name) const;
    bool hasSizeLimit(const ast::Relation* relation) const;
    size_t getSizeLimit(const ast::Relation* relation) const;

    /** Clause methods */
    std::set<ast::Clause*> getClauses(const ast::QualifiedName& name) const;
    bool isRecursiveClause(const ast::Clause* clause) const;

    /** SCC methods */
    size_t getNumberOfSCCs() const;
    bool isRecursiveSCC(size_t scc) const;
    std::set<const ast::Relation*> getExpiredRelations(size_t scc) const;
    std::set<const ast::Relation*> getRelationsInSCC(size_t scc) const;
    std::set<const ast::Relation*> getInputRelationsInSCC(size_t scc) const;
    std::set<const ast::Relation*> getOutputRelationsInSCC(size_t scc) const;

    /** Functor methods */
    TypeAttribute getFunctorReturnType(const ast::Functor* functor) const;
    TypeAttribute getFunctorArgType(const ast::Functor* functor, size_t idx) const;
    const std::vector<TypeAttribute>& getFunctorArgTypes(const ast::UserDefinedFunctor& udf) const;
    bool isStatefulFunctor(const ast::UserDefinedFunctor* functor) const;

    /** Analyses */
    const ast::SipsMetric* getSipsMetric() const {
        return sipsMetric.get();
    }

    size_t getAuxiliaryArity(const ast::Atom* atom) const;
    size_t getAuxiliaryArity(const ast::Relation* relation) const;
    size_t getEvaluationArity(const ast::Atom* atom) const;

private:
    const ast::Program* program;
    const ast::analysis::AuxiliaryArityAnalysis* auxArityAnalysis;
    const ast::analysis::RecursiveClausesAnalysis* recursiveClauses;
    const ast::analysis::RelationScheduleAnalysis* relationSchedule;
    const ast::analysis::SCCGraphAnalysis* sccGraph;
    const ast::analysis::RelationDetailCacheAnalysis* relationDetail;
    const ast::analysis::FunctorAnalysis* functorAnalysis;
    const ast::analysis::IOTypeAnalysis* ioType;
    const ast::analysis::TypeEnvironment* typeEnv;
    Own<ast::SipsMetric> sipsMetric;
};

}  // namespace souffle::ast2ram
