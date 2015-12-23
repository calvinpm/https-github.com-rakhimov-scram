/*
 * Copyright (C) 2015 Olzhas Rakhimov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/// @file zbdd.h
/// Zero-Suppressed Binary Decision Diagram facilities.

#ifndef SCRAM_SRC_ZBDD_H_
#define SCRAM_SRC_ZBDD_H_

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "bdd.h"
#include "mocus.h"

namespace scram {

/// @class SetNode
/// Representation of non-terminal nodes in ZBDD.
/// Complement variables are represented with negative indices.
/// The order of the complement is higher than the order of the variable.
class SetNode : public NonTerminal {
 public:
  using NonTerminal::NonTerminal;  ///< Constructor with index and order.

  /// @returns true if the ZBDD is minimized.
  bool minimal() const { return minimal_; }

  /// @param[in] flag  A flag for minimized ZBDD.
  void minimal(bool flag) { minimal_ = flag; }

  /// @returns Whatever count is stored in this node.
  int64_t count() const { return count_; }

  /// Stores numerical value for later retrieval.
  /// This is a helper functionality
  /// for counting the number of sets or nodes.
  ///
  /// @param[in] number  A number with a meaning for the caller.
  void count(int64_t number) { count_ = number; }

  /// @returns Cut sets found in the ZBDD represented by this node.
  const std::vector<std::vector<int>>& cut_sets() const { return cut_sets_; }

  /// Sets the cut sets belonging to this ZBDD.
  ///
  /// @param[in] cut_sets  Cut sets calculated from low and high edges.
  void cut_sets(const std::vector<std::vector<int>>& cut_sets) {
    cut_sets_ = cut_sets;
  }

  /// Cuts of this node from its high and low branches.
  /// This is for destructive extraction of cut sets.
  ///
  /// @pre This branches are not going to be used again.
  void CutBranches() {
    high_.reset();
    low_.reset();
  }

  /// Recovers a shared pointer to SetNode from a pointer to Vertex.
  ///
  /// @param[in] vertex  Pointer to a Vertex known to be a SetNode.
  ///
  /// @return Casted pointer to SetNode.
  static std::shared_ptr<SetNode> Ptr(const std::shared_ptr<Vertex>& vertex) {
    return std::static_pointer_cast<SetNode>(vertex);
  }

 private:
  bool minimal_ = false;  ///< A flag for minimized collection of sets.
  std::vector<std::vector<int>> cut_sets_;  ///< Cut sets of this node.
  int64_t count_ = 0;  ///< The number of cut sets, nodes, or anything else.
};

using SetNodePtr = std::shared_ptr<SetNode>;  ///< Shared ZBDD set nodes.
using SetNodeWeakPtr = std::weak_ptr<SetNode>;  ///< Pointer for tables.

/// @class Zbdd
/// Zero-Suppressed Binary Decision Diagrams for set manipulations.
class Zbdd {
 public:
  /// Converts Reduced Ordered BDD
  /// into Zero-Suppressed BDD.
  ///
  /// @param[in] bdd  ROBDD with the ITE vertices.
  /// @param[in] settings  Settings for analysis.
  ///
  /// @pre BDD has attributed edges with only one terminal (1/True).
  Zbdd(const Bdd* bdd, const Settings& settings) noexcept;

  /// Constructor with the analysis target.
  /// ZBDD is directly produced from a Boolean graph.
  ///
  /// @param[in] fault_tree  Preprocessed, partially normalized,
  ///                        and indexed fault tree.
  /// @param[in] settings  The analysis settings.
  ///
  /// @pre The passed Boolean graph already has variable ordering.
  /// @note The construction may take considerable time.
  Zbdd(const BooleanGraph* fault_tree, const Settings& settings) noexcept;

  /// Converts cut sets generated by MOCUS
  /// into a minimized Zbdd.
  ///
  /// @param[in] root_index  Index of the root module.
  /// @param[in] cut_sets  Container with cut sets of modules.
  /// @param[in] settings  Settings for analysis.
  ///
  /// @pre Modules are topologically ordered.
  Zbdd(int root_index,
       const std::vector<std::pair<int, mocus::CutSetContainer>>& cut_sets,
       const Settings& settings) noexcept;

  /// Runs the analysis
  /// with the representation of a Boolean graph as ZBDD.
  ///
  /// @warning The analysis will destroy ZBDD.
  ///
  /// @post Cut sets are generated with ZBDD analysis.
  void Analyze() noexcept;

  /// @returns Cut sets generated by the analysis.
  const std::vector<std::vector<int>>& cut_sets() const { return cut_sets_; }

 protected:
  using UniqueTable = TripletTable<SetNodeWeakPtr>;
  using ComputeTable = TripletTable<VertexPtr>;
  using CutSet = std::vector<int>;

  /// @class GarbageCollector
  /// This garbage collector manages tables of a ZBDD.
  /// The garbage collection is triggered
  /// when the reference count of a ZBDD vertex reaches 0.
  class GarbageCollector {
   public:
    /// @param[in,out] zbdd  ZBDD to manage.
    explicit GarbageCollector(Zbdd* zbdd) noexcept
        : unique_table_(zbdd->unique_table_) {}

    /// Frees the memory
    /// and triggers the garbage collection ONLY if requested.
    ///
    /// @param[in] ptr  Pointer to an SetNode vertex with reference count 0.
    void operator()(SetNode* ptr) noexcept;

   private:
    std::weak_ptr<UniqueTable> unique_table_;  ///< Managed table.
  };

  /// Default constructor to initialize member variables.
  ///
  /// @param[in] settings  Settings that control analysis complexity.
  explicit Zbdd(const Settings& settings) noexcept;

  /// Fetches a unique set node from a hash table.
  /// If the node doesn't exist,
  /// a new node is created.
  ///
  /// @param[in] index  Positive or negative index of the node.
  /// @param[in] high  The high vertex.
  /// @param[in] low  The low vertex.
  /// @param[in] order The order for the vertex variable.
  /// @param[in] module  A flag for the modular ZBDD proxy.
  ///
  /// @returns Set node with the given parameters.
  SetNodePtr FetchUniqueTable(int index, const VertexPtr& high,
                              const VertexPtr& low, int order,
                              bool module) noexcept;

  /// Converts BDD graph into ZBDD graph.
  ///
  /// @param[in] vertex  Vertex of the ROBDD graph.
  /// @param[in] complement  Interpretation of the vertex as complement.
  /// @param[in] bdd_graph  The main ROBDD as helper database.
  /// @param[in] limit_order  The maximum size of requested sets.
  /// @param[in,out] ites  Processed function graphs with ids and limit order.
  ///
  /// @returns Pointer to the root vertex of the ZBDD graph.
  VertexPtr ConvertBdd(const VertexPtr& vertex, bool complement,
                       const Bdd* bdd_graph, int limit_order,
                       PairTable<VertexPtr>* ites) noexcept;

  /// Transforms a Boolean graph gate into a Zbdd set graph.
  ///
  /// @param[in] gate  The root gate of the Boolean graph.
  /// @param[in,out] gates  Processed gates with use counts.
  ///
  /// @returns The top vertex of the Zbdd graph.
  ///
  /// @pre The memoisation container is not used outside of this function.
  VertexPtr ConvertGraph(
      const IGatePtr& gate,
      std::unordered_map<int, std::pair<VertexPtr, int>>* gates) noexcept;

  /// Converts cut sets found by MOCUS into a ZBDD graph.
  ///
  /// @param[in] cut_sets  A set of indices of modules and variables.
  ///
  /// @returns Pointer to the root vertex of ZBDD.
  VertexPtr ConvertCutSets(const mocus::CutSetContainer& cut_sets) noexcept;

  /// Converts MOCUS generated cut set into ZBDD graph nodes.
  ///
  /// @param[in] cut_set  MOCUS specific cut set.
  ///
  /// @returns Pointer to the root ZBDD node.
  ///
  /// @pre Cut sets are passed in increasing size.
  /// @pre The order equals index + 1.
  ///
  /// @post The final ZBDD graph is minimal.
  /// @post Negative literals are discarded.
  VertexPtr EmplaceCutSet(const mocus::CutSetPtr& cut_set) noexcept;

  /// Adds a ZBDD single cut set into a ZBDD cut set database.
  ///
  /// @param[in] root  The root vertex of the ZBDD.
  /// @param[in] set_vertex  The vertex of the cut set.
  ///
  /// @returns Pointer to the resulting ZBDD vertex.
  VertexPtr EmplaceCutSet(const VertexPtr& root,
                          const VertexPtr& set_vertex) noexcept;

  /// Fetches computation tables for results.
  ///
  /// @param[in] type  Boolean operation type.
  /// @param[in] arg_one  First argument.
  /// @param[in] arg_two  Second argument.
  /// @param[in] limit_order  The limit on the order for the computations.
  ///
  /// @returns nullptr reference for uploading the computation results
  ///                  if it doesn't exists.
  /// @returns Pointer to ZBDD root vertex as the computation result.
  ///
  /// @pre The arguments are not the same functions.
  ///      Equal ID functions are handled by the reduction.
  /// @pre Even though the arguments are not SetNodePtr type,
  ///      they are ZBDD SetNode vertices.
  ///
  /// @note The order of input argument vertices does not matter.
  VertexPtr& FetchComputeTable(Operator type, const VertexPtr& arg_one,
                               const VertexPtr& arg_two,
                               int limit_order) noexcept;

  /// Applies Boolean operation to two vertices representing sets.
  ///
  /// @param[in] type  The operator or type of the gate.
  /// @param[in] arg_one  First argument ZBDD set.
  /// @param[in] arg_two  Second argument ZBDD set.
  /// @param[in] limit_order  The limit on the order for the computations.
  ///
  /// @returns The resulting ZBDD vertex.
  ///
  /// @note The limit on the order is not guaranteed.
  ///       It is for optimization purposes only.
  VertexPtr Apply(Operator type, const VertexPtr& arg_one,
                  const VertexPtr& arg_two, int limit_order) noexcept;

  /// Applies the logic of a Boolean operator
  /// to terminal vertices.
  ///
  /// @param[in] type  The operator to apply.
  /// @param[in] term_one  First argument terminal vertex.
  /// @param[in] term_two  Second argument terminal vertex.
  ///
  /// @returns The resulting ZBDD vertex.
  VertexPtr Apply(Operator type, const TerminalPtr& term_one,
                  const TerminalPtr& term_two) noexcept;

  /// Applies the logic of a Boolean operator
  /// to non-terminal and terminal vertices.
  ///
  /// @param[in] type  The operator or type of the gate.
  /// @param[in] set_node  Non-terminal vertex.
  /// @param[in] term  Terminal vertex.
  ///
  /// @returns The resulting ZBDD vertex.
  VertexPtr Apply(Operator type, const SetNodePtr& set_node,
                  const TerminalPtr& term) noexcept;

  /// Applies Boolean operation to ZBDD graph non-terminal vertices.
  ///
  /// @param[in] type  The operator or type of the gate.
  /// @param[in] arg_one  First argument set vertex.
  /// @param[in] arg_two  Second argument set vertex.
  /// @param[in] limit_order  The limit on the order for the computations.
  ///
  /// @returns The resulting ZBDD vertex.
  ///
  /// @pre Argument vertices are ordered.
  VertexPtr Apply(Operator type, const SetNodePtr& arg_one,
                  const SetNodePtr& arg_two, int limit_order) noexcept;

  /// Removes complements of variables from cut sets.
  /// This procedure only needs to be performed for non-coherent graphs
  /// with minimal cut sets as output.
  ///
  /// @param[in] vertex  The variable vertex in the ZBDD.
  /// @param[in,out] wide_results  Memoisation of the processed vertices.
  ///
  /// @returns Processed vertex.
  VertexPtr EliminateComplements(
      const VertexPtr& vertex,
      std::unordered_map<int, VertexPtr>* wide_results) noexcept;

  /// Processes complements in a SetNode with processed high/low edges.
  ///
  /// @param[in] node  SetNode to be processed.
  /// @param[in] high  Processed high edge.
  /// @param[in] low  Processed low edge.
  /// @param[in,out] wide_results  Memoisation of the processed vertices.
  ///
  /// @returns Processed ZBDD vertex without complements.
  VertexPtr EliminateComplement(
      const SetNodePtr& node,
      const VertexPtr& high,
      const VertexPtr& low,
      std::unordered_map<int, VertexPtr>* wide_results) noexcept;

  /// Removes subsets in ZBDD.
  ///
  /// @param[in] vertex  The variable node in the set.
  ///
  /// @returns Processed vertex.
  VertexPtr Minimize(const VertexPtr& vertex) noexcept;

  /// Applies subsume operation on two sets.
  /// Subsume operation removes
  /// paths that exist in Low branch from High branch.
  ///
  /// @param[in] high  True/then/high branch of a variable.
  /// @param[in] low  False/else/low branch of a variable.
  ///
  /// @returns Minimized high branch for a variable.
  VertexPtr Subsume(const VertexPtr& high, const VertexPtr& low) noexcept;

  /// Traverses the reduced ZBDD graph to generate cut sets.
  /// ZBDD is destructively converted into cut sets.
  ///
  /// @param[in] vertex  The root node in traversal.
  ///
  /// @returns A collection of cut sets
  ///          generated from the ZBDD subgraph.
  ///
  /// @warning Cut set generation will destroy ZBDD.
  std::vector<std::vector<int>>
  GenerateCutSets(const VertexPtr& vertex) noexcept;

  /// Counts the number of SetNodes.
  ///
  /// @param[in] vertex  The root vertex to start counting.
  ///
  /// @returns The total number of SetNode vertices
  ///          including vertices in modules.
  ///
  /// @pre SetNode marks are clear (false).
  int CountSetNodes(const VertexPtr& vertex) noexcept;

  /// Counts the total number of sets in ZBDD.
  ///
  /// @param[in] vertex  The root vertex of ZBDD.
  ///
  /// @returns The number of cut sets in ZBDD.
  ///
  /// @pre SetNode marks are clear (false).
  int64_t CountCutSets(const VertexPtr& vertex) noexcept;

  /// Cleans up non-terminal vertex marks
  /// by setting them to "false".
  ///
  /// @param[in] vertex  The root vertex of the graph.
  ///
  /// @pre The graph is marked "true" contiguously.
  void ClearMarks(const VertexPtr& vertex) noexcept;

  /// Checks ZBDD graphs for errors in the structure.
  /// Errors are assertions that fail at runtime.
  ///
  /// @param[in] vertex  The root vertex of ZBDD.
  ///
  /// @pre SetNode marks are clear (false).
  void TestStructure(const VertexPtr& vertex) noexcept;

  const Settings kSettings_;  ///< Analysis settings.
  VertexPtr root_;  ///< The root vertex of ZBDD.

  /// Table of unique SetNodes denoting sets.
  /// The key consists of (index, id_high, id_low) triplet.
  std::shared_ptr<UniqueTable> unique_table_;

  /// Table of processed computations over sets.
  /// The argument sets are recorded with their IDs (not vertex indices).
  /// In order to keep only unique computations,
  /// the argument IDs must be ordered.
  /// The key is {min_id, max_id, max_order}.
  ComputeTable and_table_;  ///< Table of processed AND computations over sets.
  ComputeTable or_table_;  ///< Table of processed OR computations over sets.

  /// Memoisation of minimal ZBDD vertices.
  std::unordered_map<int, VertexPtr> minimal_results_;
  /// The results of subsume operations over sets.
  PairTable<VertexPtr> subsume_table_;

  std::unordered_map<int, VertexPtr> modules_;  ///< Module graphs.
  const TerminalPtr kBase_;  ///< Terminal Base (Unity/1) set.
  const TerminalPtr kEmpty_;  ///< Terminal Empty (Null/0) set.
  int set_id_;  ///< Identification assignment for new set graphs.
  std::vector<CutSet> cut_sets_;  ///< Generated cut sets.
};

namespace zbdd {

/// @class CutSetContainer
/// Storage for generated cut sets in MOCUS.
/// The semantics is similar to a set of cut sets.
class CutSetContainer : public Zbdd {
 public:
  /// Default constructor to initialize member variables.
  ///
  /// @param[in] settings  Settings that control analysis complexity.
  /// @param[in] gate_index_bound  The exclusive lower bound for gate indices.
  ///
  /// @pre No complements of gates.
  /// @pre Gates are indexed sequentially
  ///      starting from a number larger than the lower bound.
  /// @pre Basic events are indexed sequentially
  ///      up to a number less than or equal to the given lower bound.
  CutSetContainer(const Settings& settings, int gate_index_bound) noexcept;

  /// Converts a Boolean graph gate into intermediate cut sets.
  ///
  /// @param[in] gate  The target AND/OR gate with arguments.
  ///
  /// @returns The root vertex of the ZBDD representing the gate cut sets.
  VertexPtr ConvertGate(const IGatePtr& gate) noexcept;

  /// Finds a gate in intermediate cut sets.
  ///
  /// @param[in] vertex  The root vertex of ZBDD to search for.
  ///
  /// @returns The index of the gate in intermediate cut sets.
  /// @returns 0 if no gates are found.
  ///
  /// @post The path to the target vertex is marked.
  int GetNextGate(const VertexPtr& vertex) noexcept;

  /// Extracts (removes!) intermediate cut sets
  /// containing a node with a given index.
  ///
  /// @param[in] index  The index of the gate.
  ///
  /// @returns The root of the ZBDD containing the intermediate cut sets.
  ///
  /// @pre The path to the target vertex is marked.
  /// @pre Not all nodes containing the index may be extracted.
  ///
  /// @post The path to the target vertex is cleaned.
  /// @post The extracted cut sets are pre-processed
  ///       by removing the vertex with the index of the gate.
  VertexPtr ExtractIntermediateCutSets(int index) noexcept;

  /// Expands the intermediate ZBDD representation of a gate
  /// in intermediate cut sets containing the gate.
  ///
  /// @param[in] gate_zbdd  The intermediate ZBDD of the gate.
  /// @param[in] cut_sets  A collection of cut sets.
  ///
  /// @returns The root vertex of the resulting ZBDD.
  ///
  /// @pre The intermediate cut sets are pre-processed
  ///      by removing the vertex with the index of the gate.
  VertexPtr ExpandGate(const VertexPtr& gate_zbdd,
                       const VertexPtr& cut_sets) noexcept;

  /// Merges a set of cut sets into the main container.
  ///
  /// @param[in] vertex  The root ZBDD vertex representing the cut sets.
  ///
  /// @pre The argument ZBDD cut sets are managed by this container.
  void Merge(const VertexPtr& vertex) noexcept;

  /// Eliminates all complements from cut sets.
  /// This can only be done
  /// if the cut set generation is certain not to have conflicts.
  ///
  /// @pre The cut sets have negative literals, i.e., non-coherent.
  void EliminateComplements() noexcept;

  /// Joins a ZBDD representing a module gate.
  ///
  /// @param[in] index  The index of the module.
  /// @param[in] container  The container of the module cut sets.
  ///
  /// @pre The module cut sets are final,
  ///      and no more processing or sanitizing is needed.
  void JoinModule(int index, const CutSetContainer& container) noexcept;

  /// Sanitizes the container
  /// after finishing all the cut set generation operations.
  ///
  /// @pre No complements in the container.
  ///
  /// @post No constant modules in the container.
  void Sanitize() noexcept;

 private:
  /// Checks if a set node represents a gate.
  ///
  /// @param[in] node  A node to be tested.
  ///
  /// @returns true if the index of the node belongs to a gate.
  ///
  /// @pre There are no complements of gates.
  /// @pre Gate indexation has a lower bound.
  bool IsGate(const SetNodePtr& node) noexcept {
    return node->index() > gate_index_bound_;
  }

  /// Extracts intermediate cut set representation from a given ZBDD.
  ///
  /// @param[in] node  The root vertex of the ZBDD.
  /// @param[in] index  The index of the target gate.
  ///
  /// @returns A pair of vertices representing the target cut sets
  ///          and the remaining ZBDD cut sets.
  ///
  /// @pre The path to the target vertex is marked.
  ///
  /// @post The path to the target vertex is cleaned.
  std::pair<VertexPtr, VertexPtr>
  ExtractIntermediateCutSets(const SetNodePtr& node, int index) noexcept;

  int gate_index_bound_;  ///< The exclusive lower bound for the gate indices.
};

}  // namespace zbdd

}  // namespace scram

#endif  // SCRAM_SRC_ZBDD_H_
