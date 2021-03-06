/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2018, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/***************************************************************************
 *
 * @file Index.h
 *
 * Computes indexes for relations in a translation unit
 *
 ***************************************************************************/

#pragma once

#include "ram/AbstractExistenceCheck.h"
#include "ram/ExistenceCheck.h"
#include "ram/IndexOperation.h"
#include "ram/ProvenanceExistenceCheck.h"
#include "ram/Relation.h"
#include "ram/TranslationUnit.h"
#include "ram/analysis/Analysis.h"
#include "ram/analysis/Relation.h"
#include "souffle/utility/MiscUtil.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// define if enable unit tests
#define M_UNIT_TEST

namespace souffle::ram::analysis {

enum class AttributeConstraint { None, Equal, Inequal };

/** Search Signature of a RAM Operation
 *    Inequal - The attribute has an inequality constraint i.e. 11 <= x <= 13
 *    Equal   - The attribute has an equality constraint i.e. x = 17
 *    None    - The attribute no constraint
 * */
class SearchSignature {
public:
    class Iterator;

    explicit SearchSignature(size_t arity);

    // array subscript operator
    AttributeConstraint& operator[](std::size_t pos);
    const AttributeConstraint& operator[](std::size_t pos) const;

    // comparison operators
    bool operator==(const SearchSignature& other) const;
    bool operator!=(const SearchSignature& other) const;

    // helper member functions
    bool empty() const;
    bool precedes(const SearchSignature& other) const;
    size_t arity() const;

    // create new signatures from these functions
    static SearchSignature getDelta(const SearchSignature& lhs, const SearchSignature& rhs);
    static SearchSignature getFullSearchSignature(size_t arity);

    // printing
    friend std::ostream& operator<<(std::ostream& out, const SearchSignature& signature);

    // hashing class
    class Hasher {
    public:
        size_t operator()(const SearchSignature& searchSignature) const {
            std::size_t seed = searchSignature.arity();
            for (auto& constraint : searchSignature.constraints) {
                seed ^= static_cast<size_t>(constraint) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            }
            return seed;
        }
    };

    // raw ptr to begin
    Iterator begin() const {
        return Iterator(const_cast<AttributeConstraint*>(constraints.data()));
    }

    // raw ptr to end
    Iterator end() const {
        return Iterator(const_cast<AttributeConstraint*>(constraints.data() + constraints.size()));
    }

    class Iterator : public std::iterator<std::random_access_iterator_tag, AttributeConstraint> {
    public:
        using difference_type =
                typename std::iterator<std::random_access_iterator_tag, AttributeConstraint>::difference_type;

        Iterator() : it(nullptr) {}
        Iterator(AttributeConstraint* rhs) : it(rhs) {}
        Iterator(const Iterator& rhs) : it(rhs.it) {}
        inline Iterator& operator=(AttributeConstraint* rhs) {
            it = rhs;
            return *this;
        }
        inline Iterator& operator=(const Iterator& rhs) {
            it = rhs.it;
            return *this;
        }
        inline Iterator& operator+=(difference_type rhs) {
            it += rhs;
            return *this;
        }
        inline Iterator& operator-=(difference_type rhs) {
            it -= rhs;
            return *this;
        }
        inline AttributeConstraint& operator*() const {
            return *it;
        }
        inline AttributeConstraint operator->() const {
            return *it;
        }
        inline AttributeConstraint& operator[](difference_type rhs) const {
            return it[rhs];
        }

        inline Iterator& operator++() {
            ++it;
            return *this;
        }
        inline Iterator& operator--() {
            --it;
            return *this;
        }

        inline difference_type operator-(const Iterator& rhs) const {
            return {it - rhs.it};
        }
        inline Iterator operator+(difference_type rhs) const {
            return {it + rhs};
        }
        inline Iterator operator-(difference_type rhs) const {
            return {it - rhs};
        }
        friend inline Iterator operator+(difference_type lhs, const Iterator& rhs) {
            return {rhs.it + lhs};
        }
        friend inline Iterator operator-(difference_type lhs, const Iterator& rhs) {
            return {rhs.it - lhs};
        }

        inline bool operator==(const Iterator& rhs) const {
            return it == rhs.it;
        }
        inline bool operator!=(const Iterator& rhs) const {
            return it != rhs.it;
        }
        inline bool operator>(const Iterator& rhs) const {
            return it > rhs.it;
        }
        inline bool operator<(const Iterator& rhs) const {
            return it < rhs.it;
        }
        inline bool operator>=(const Iterator& rhs) const {
            return it >= rhs.it;
        }
        inline bool operator<=(const Iterator& rhs) const {
            return it <= rhs.it;
        }

    private:
        AttributeConstraint* it;
    };

private:
    std::vector<AttributeConstraint> constraints;
};

std::ostream& operator<<(std::ostream& out, const SearchSignature& signature);

/**
 * @class MaxMatching
 * @Brief Computes a maximum matching with Hopcroft-Karp algorithm
 *
 * This class is a helper class for RamIndex.
 *
 * This implements a standard maximum matching algorithm for a bi-partite graph
 * also known as a marriage problem. Given a set of edges in a bi-partite graph
 * select a subset of edges that each node in the bi-partite graph has at most
 * one adjacent edge associated with.
 *
 * The nodes of the bi-partite graph represent index-signatures stemming from
 * RAM operations and RAM existence checks for a relation. A relation between
 * two nodes represent whether an index operation subsumes another operation.
 *
 * Source: http://en.wikipedia.org/wiki/Hopcroft%E2%80%93Karp_algorithm#Pseudocode
 */
class MaxMatching {
public:
    using Node = uint32_t;
    /* The nodes of the bi-partite graph are index signatures of RAM operation */
    using Nodes = std::unordered_set<Node>;
    /* Distance between nodes */
    using Distance = int;
    /**
     * Matching represent a solution of the matching, i.e., which node in the bi-partite
     * graph maps to another node. If no map exist for a node, there is no adjacent edge
     * exists for that node.
     */
    using Matchings = std::unordered_map<Node, Node>;

    /* Node constant representing no match */
    const Node NullVertex = 0;

    /* Infinite distance */
    const Distance InfiniteDistance = -1;

    /**
     * @Brief solve the maximum matching problem
     * @result returns the matching
     */
    const Matchings& solve();

    /**
     * @Brief get number of matches in the solution
     * @return number of matches
     */
    int getNumMatchings() const {
        return match.size() / 2;
    }

    /**
     * @Brief add an edge to the bi-partite graph
     * @param u search signature
     * @param v subsuming search signature
     */
    void addEdge(Node u, Node v);

protected:
    /**
     * @Brief get match for a search signature
     * @param v search signature
     */
    Node getMatch(Node v);

    /**
     * @Brief get distance of a node
     */
    Distance getDistance(Node v);

    /**
     * @Brief perform a breadth first search in the graph
     */
    bool bfSearch();

    /**
     * @Brief perform a depth first search in the graph
     * @param u search signature
     */
    bool dfSearch(Node u);

private:
    /**
     * Edges in the bi-partite graph
     */
    using Edges = std::unordered_set<Node>;
    /**
     * Bi-partite graph of instance
     */
    using Graph = std::unordered_map<Node, Edges>;
    /**
     * distance function of nodes
     */
    using DistanceMap = std::unordered_map<Node, Distance>;

    Matchings match;
    Graph graph;
    DistanceMap distance;
};

/**
 * @class MinIndexSelection
 * @Brief computes the minimal index cover for a relation
 *        in a RAM Program.
 *
 * If the indexes of a relation can cover several searches, the minimal
 * set of indexes is computed by Dilworth's problem. See
 *
 * "Automatic Index Selection for Large-Scale Datalog Computation"
 * http://www.vldb.org/pvldb/vol12/p141-subotic.pdf
 *
 */

using AttributeIndex = uint32_t;
using AttributeSet = std::unordered_set<AttributeIndex>;
using SignatureMap = std::unordered_map<SearchSignature, SearchSignature, SearchSignature::Hasher>;
using SignatureIndexMap = std::unordered_map<SearchSignature, AttributeIndex, SearchSignature::Hasher>;
using IndexSignatureMap = std::unordered_map<AttributeIndex, SearchSignature>;
using DischargeMap = std::unordered_map<SearchSignature, AttributeSet, SearchSignature::Hasher>;
using LexOrder = std::vector<AttributeIndex>;
using OrderCollection = std::vector<LexOrder>;
using SearchCollection = std::vector<SearchSignature>;
using Chain = std::vector<SearchSignature>;
using ChainOrderMap = std::vector<Chain>;
using SignatureOrderMap = std::unordered_map<SearchSignature, LexOrder, SearchSignature::Hasher>;

class SearchComparator {
public:
    bool operator()(const SearchSignature& s1, const SearchSignature& s2) const {
        auto hasher = SearchSignature::Hasher();
        return hasher(s1) < hasher(s2);
    }
};

using SearchSet = std::set<SearchSignature, SearchComparator>;
// SearchSignatures only have a partial order, however we need to produce a unique ordering of searches
// when we output the name of the collection of searches and therefore we order the SearchSignatures
// arbitrarily by their hashes

class MinIndexSelection {
public:
    /** @Brief Add new key to an Index Set */
    inline void addSearch(SearchSignature cols) {
        if (cols.empty()) {
            return;
        }

        searches.insert(cols);
    }

    MinIndexSelection() = default;
    ~MinIndexSelection() = default;

    /** @Brief Get searches **/
    const SearchSet& getSearches() const {
        return searches;
    }

    /** @Brief Get index for a search */
    const LexOrder& getLexOrder(SearchSignature cols) const {
        int idx = map(cols);
        return orders[idx];
    }

    /** @Brief Get all indexes */
    const OrderCollection getAllOrders() const {
        return orders;
    }

    /** @Brief Get all chains */
    const ChainOrderMap getAllChains() const {
        return chainToOrder;
    }

    /**
     * Check whether number of constraints in k is not equal to number of columns in lexicographical
     * order
     * */
    bool isSubset(SearchSignature cols) const {
        int idx = map(cols);
        return card(cols) < orders[idx].size();
    }

    /** @Brief map the keys in the key set to lexicographical order */
    void solve();

    /** @Brief insert a total order index
     *  @param size of the index
     */
    void insertDefaultTotalIndex(size_t arity) {
        Chain chain = std::vector<SearchSignature>();
        SearchSignature fullIndexKey = SearchSignature::getFullSearchSignature(arity);
        chain.push_back(fullIndexKey);
        chainToOrder.push_back(std::move(chain));
        LexOrder totalOrder;
        for (size_t i = 0; i < arity; ++i) {
            totalOrder.push_back(i);
        }
        orders.push_back(std::move(totalOrder));
    }

    void print(std::ostream& os) {
        /* Print searches */
        os << "\tNumber of Searches: " << getSearches().size() << "\n";

        /* Print searches */
        for (auto& search : getSearches()) {
            os << "\t\t";
            os << search;
            os << "\n";
        }

        /* Print chains */
        for (auto& chain : getAllChains()) {
            os << join(chain, "-->") << "\n";
        }
        os << "\n";

        os << "\tNumber of Indexes: " << getAllOrders().size() << "\n";
        for (auto& order : getAllOrders()) {
            os << "\t\t";
            os << join(order, "<") << "\n";
            os << "\n";
        }
    }

protected:
    SignatureIndexMap signatureToIndexA;  // mapping of a SearchSignature on A to its unique index
    SignatureIndexMap signatureToIndexB;  // mapping of a SearchSignature on B to its unique index
    IndexSignatureMap indexToSignature;   // mapping of a unique index to its SearchSignature
    SearchSet searches;                   // set of search patterns on table
    OrderCollection orders;               // collection of lexicographical orders
    ChainOrderMap chainToOrder;           // maps order index to set of searches covered by chain
    MaxMatching matching;                 // matching problem for finding minimal number of orders

    /** @Brief count the number of constraints in key */
    static size_t card(SearchSignature cols) {
        size_t sz = 0;
        for (size_t i = 0; i < cols.arity(); i++) {
            if (cols[i] != AttributeConstraint::None) {
                sz++;
            }
        }
        return sz;
    }

    /** @Brief maps search columns to an lexicographical order (labeled by a number) */
    int map(SearchSignature cols) const {
        assert(orders.size() == chainToOrder.size() && "Order and Chain Sizes do not match!!");
        int i = 0;
        for (auto it = chainToOrder.begin(); it != chainToOrder.end(); ++it, ++i) {
            if (std::find(it->begin(), it->end(), cols) != it->end()) {
                assert((size_t)i < orders.size());
                return i;
            }
        }
        fatal("cannot find matching lexicographical order");
    }

    /** @Brief insert an index based on the delta */
    void insertIndex(LexOrder& ids, SearchSignature delta) {
        LexOrder backlog;  // add inequalities at the end
        for (size_t pos = 0; pos < delta.arity(); pos++) {
            if (delta[pos] == AttributeConstraint::Equal) {
                ids.push_back(pos);
            } else if (delta[pos] == AttributeConstraint::Inequal) {
                backlog.push_back(pos);
            }
        }
        ids.insert(ids.end(), backlog.begin(), backlog.end());
    }

    /** @Brief get a chain from a matching
     *  @param Starting node of a chain
     *  @param Matching
     *  @result A minimal chain
     * given an unmapped node from set A
     * we follow it from set B until it cannot be matched from B
     * if not matched from B then umn is a chain.
     */
    Chain getChain(const SearchSignature umn, const MaxMatching::Matchings& match);

    /** @Brief get all chains from the matching */
    const ChainOrderMap getChainsFromMatching(const MaxMatching::Matchings& match, const SearchSet& nodes);

    /** @Brief get all nodes which are unmatched from A-> B */
    const SearchSet getUnmatchedKeys(const MaxMatching::Matchings& match, const SearchSet& nodes) {
        SearchSet unmatched;

        // For all nodes n such that n is not in match
        for (auto node : nodes) {
            if (match.find(signatureToIndexA[node]) == match.end()) {
                unmatched.insert(node);
            }
        }
        return unmatched;
    }
};

/**
 * @class FinalIndexSelection
 * @Brief Encapsulates the result of the IndexAnalysis
 * i.e. mapping each search (SearchSignature) to a corresponding index (LexOrder)
 */
class FinalIndexSelection {
public:
    FinalIndexSelection(const SignatureOrderMap& indexSelection, const SearchSet& searchSet,
            const OrderCollection& orders)
            : indexSelection(indexSelection), searches(searchSet.begin(), searchSet.end()), orders(orders) {}

    const OrderCollection getAllOrders() const {
        return orders;
    }
    const SearchCollection getSearches() const {
        return searches;
    }
    const LexOrder getLexOrder(SearchSignature cols) const {
        return indexSelection.at(cols);
    }

    int getLexOrderNum(SearchSignature cols) const {
        // get the corresponding order
        auto order = getLexOrder(cols);
        // find the order in the collection
        auto it = std::find(orders.begin(), orders.end(), order);
        // return its relative index
        return std::distance(orders.begin(), it);
    }

private:
    SignatureOrderMap indexSelection;
    SearchCollection searches;
    OrderCollection orders;
};

/**
 * @class RamIndexAnalyis
 * @Brief Analysis pass computing the index sets of RAM relations
 */
class IndexAnalysis : public Analysis {
public:
    IndexAnalysis(const char* id) : Analysis(id), relAnalysis(nullptr) {}

    static constexpr const char* name = "index-analysis";

    void run(const TranslationUnit& translationUnit) override;

    const FinalIndexSelection getIndexSelection(const std::string& relName) const {
        SignatureOrderMap indexSelection = {};
        const auto& cover = minIndexCover.at(relName);
        for (const auto& search : cover.getSearches()) {
            indexSelection.insert({search, cover.getLexOrder(search)});
        }
        return FinalIndexSelection(indexSelection, cover.getSearches(), cover.getAllOrders());
    }

    /**
     * @Brief Get index signature for an Ram IndexOperation operation
     * @param  Index-relation-search operation
     * @result Index signature of operation
     */
    SearchSignature getSearchSignature(const IndexOperation* search) const;

    /**
     * @Brief Get the index signature for an existence check
     * @param Existence check
     * @result index signature of existence check
     */
    SearchSignature getSearchSignature(const ExistenceCheck* existCheck) const;

    /**
     * @Brief Get the index signature for a provenance existence check
     * @param Provenance-existence check
     * @result index signature of provenance-existence check
     */
    SearchSignature getSearchSignature(const ProvenanceExistenceCheck* existCheck) const;

    /**
     * @Brief Get the default index signature for a relation (the total-order index)
     * @param ramRel RAM-relation
     * @result total full-signature of the relation
     */
    SearchSignature getSearchSignature(const Relation* ramRel) const;

    /**
     * @Brief index signature of existence check resembles a total index
     * @param (provenance) existence check
     *
     * isTotalSignature returns true if all elements of a tuple are used for the
     * the existence check.
     */
    bool isTotalSignature(const AbstractExistenceCheck* existCheck) const;

private:
    /**
     * @Brief get the minimal index cover for a relation
     * @param relation name
     * @result set of indexes of the minimal index cover
     */
    MinIndexSelection& getIndexes(const std::string& relName);
    void print(std::ostream& os) const override;

private:
    /** relation analysis for looking up relations by name */
    RelationAnalysis* relAnalysis;

    /**
     * minimal index cover for relations, i.e., maps a relation to a set of indexes
     */
    std::map<std::string, MinIndexSelection> minIndexCover;
};

}  // namespace souffle::ram::analysis
