/// @file indexed_fault_tree.h
/// A fault tree analysis facility with event and gate indices instead
/// of id names. This facility is designed to work with FaultTreeAnalysis class.
#ifndef SCRAM_SRC_INDEXED_FAULT_TREE_H_
#define SCRAM_SRC_INDEXED_FAULT_TREE_H_

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>

#include "indexed_gate.h"

class IndexedFaultTreeTest;

namespace scram {

class Gate;
class Formula;
class Mocus;

/// @class IndexedFaultTree
/// This class provides simpler representation of a fault tree
/// that takes into account the indices of events instead of ids and pointers.
/// The class provides main preprocessing operations over a fault tree
/// to generate minimal cut sets.
class IndexedFaultTree {
  friend class ::IndexedFaultTreeTest;
  friend class Mocus;

 public:
  typedef boost::shared_ptr<Gate> GatePtr;

  /// Constructs a simplified fault tree with indices of nodes.
  /// @param[in] top_event_id The index of the top event of this tree.
  explicit IndexedFaultTree(int top_event_id);

  /// Creates indexed gates with basic and house event indices as children.
  /// Nested gates are flattened and given new indices.
  /// It is assumed that indices are sequential starting from 1.
  /// @param[in] int_to_inter Container of gates and their indices including
  ///                         the top gate.
  /// @param[in] ccf_basic_to_gates CCF basic events that are converted to
  ///                               gates.
  /// @param[in] all_to_int Container of all events in this tree to index
  ///                       children of the gates.
  void InitiateIndexedFaultTree(
      const boost::unordered_map<int, GatePtr>& int_to_inter,
      const std::map<std::string, int>& ccf_basic_to_gates,
      const boost::unordered_map<std::string, int>& all_to_int);

  /// Remove all house events by propagating them as constants in Boolean
  /// equation.
  /// @param[in] true_house_events House events with true state.
  /// @param[in] false_house_events House events with false state.
  void PropagateConstants(const std::set<int>& true_house_events,
                          const std::set<int>& false_house_events);

  /// Performs processing of a fault tree to simplify the structure to
  /// normalized (OR/AND gates only), modular, positive-gate-only indexed fault
  /// tree.
  /// @param[in] num_basic_events The number of basic events. This information
  ///                             is needed to optimize the tree traversal
  ///                             with certain expectation.
  void ProcessIndexedFaultTree(int num_basic_events);

 private:
  typedef boost::shared_ptr<IndexedGate> IndexedGatePtr;
  typedef boost::shared_ptr<Formula> FormulaPtr;

  /// Mapping to string gate types to enum gate types.
  static const std::map<std::string, GateType> kStringToType_;

  /// Processes a formula into a new indexed gates.
  /// @param[in] index The index to be assigned to the new indexed gate.
  /// @param[in] formula The formula to be converted into a gate.
  /// @param[in] ccf_basic_to_gates CCF basic events that are converted to
  ///                               gates.
  /// @param[in] all_to_int Container of all events in this tree to index
  ///                       children of the gates.
  void ProcessFormula(int index,
                      const FormulaPtr& formula,
                      const std::map<std::string, int>& ccf_basic_to_gates,
                      const boost::unordered_map<std::string, int>& all_to_int);

  /// Starts normalizing gates to simplify gates to OR, AND gates.
  /// NOT and NUll are dealt with specifically.
  /// This function uses parent information of each gate, so the tree must
  /// be initialized before a call of this function.
  /// New gates are created upon normalizing complex gates, such as XOR.
  void NormalizeGates();

  /// Traverses the tree to gather information about parents of indexed gates.
  /// This information might be needed for other algorithms because
  /// due to processing of the tree, the shape and nodes may change.
  /// @param[in] parent_gate The parent to start information gathering.
  /// @param[in,out] processed_gates The gates that have already been processed.
  void GatherParentInformation(const IndexedGatePtr& parent_gate,
                               std::set<int>* processed_gates);

  /// Notifies all parents of negative gates, such as NOR and NAND before
  /// transforming these gates into basic gates of OR and AND.
  /// The parent information should be available. This function does not
  /// change the type of the given gate.
  /// @param[in] gate The gate to be start processing.
  void NotifyParentsOfNegativeGates(const IndexedGatePtr& gate);

  /// Normalizes a gate to make OR, AND gates. The parents of the
  /// gates are not notified. This means that negative gates must be dealt
  /// separately. However, NOT and NULL gates are left untouched for later
  /// special processing.
  /// @param[in,out] gate The gate to be processed.
  void NormalizeGate(const IndexedGatePtr& gate);

  /// Normalizes a gate with XOR logic.
  /// @param[in,out] gate The gate to normalize.
  void NormalizeXorGate(const IndexedGatePtr& gate);

  /// Normalizes an ATLEAST gate with a vote number.
  /// @param[in,out] gate The atleast gate to normalize.
  void NormalizeAtleastGate(const IndexedGatePtr& gate);

  /// Remove all house events from a given gate according to the Boolean logic.
  /// The structure of the tree should not be pre-processed before this
  /// operation; that is, this is the first operation that is done after
  /// creation of an indexed fault tree.
  /// After this function, there should not be any unity or null gates because
  /// of house events.
  /// @param[in] true_house_events House events with true state.
  /// @param[in] false_house_events House events with false state.
  /// @param[in,out] gate The final resultant processed gate.
  /// @param[in,out] processed_gates The gates that have already been processed.
  void PropagateConstants(const std::set<int>& true_house_events,
                          const std::set<int>& false_house_events,
                          const IndexedGatePtr& gate,
                          std::set<int>* processed_gates);

  /// Changes the state of a gate or passes a constant child to be removed
  /// later. The function determines its actions depending on the type of
  /// a gate and state of a child; however, the sign of the index is ignored.
  /// The caller of this function must ensure that the state corresponds to the
  /// sign of the child index.
  /// The type of the gate may change, but it will only be valid after the
  /// to-be-erased children are handled properly.
  /// @param[in,out] gate The parent gate that contains the children.
  /// @param[in] child The constant child under consideration.
  /// @param[in] state False or True constant state of the child.
  /// @param[in,out] to_erase The set of children to erase from the parent gate.
  /// @returns true if the passed gate has become constant due to its child.
  /// @returns false if the parent still valid for further operations.
  bool ProcessConstantChild(const IndexedGatePtr& gate, int child,
                            bool state, std::vector<int>* to_erase);

  /// Removes a set of children from a gate taking into account the logic.
  /// This is a helper function for NULL and UNITY propagation on the tree.
  /// If the final gate is empty, its state is turned into NULL or UNITY
  /// depending on the logic of the gate and the logic of the constant
  /// propagation.
  /// The parent information is not updated for the child.
  /// @param[in,out] gate The gate that contains the children to be removed.
  /// @param[in] to_erase The set of children to erase from the parent gate.
  void RemoveChildren(const IndexedGatePtr& gate,
                      const std::vector<int>& to_erase);

  /// Removes null and unity gates. There should not be negative gate children.
  /// After this function, there should not be null or unity gates resulting
  /// from previous processing steps.
  /// @param[in,out] gate The starting gate to traverse the tree. This is for
  ///                     recursive purposes.
  /// @param[in,out] processed_gates The gates that have already been processed.
  /// @returns true if the given tree has been changed by this function.
  /// @returns false if no change has been made.
  bool ProcessConstGates(const IndexedGatePtr& gate,
                         std::set<int>* processed_gates);

  /// Propagates complements of child gates down to basic events
  /// in order to remove any NOR or NAND logic from the tree.
  /// This function also processes NOT and NULL gates.
  /// The resulting tree will contain only positive gates, OR and AND.
  /// @param[in,out] gate The starting gate to traverse the tree. This is for
  ///                     recursive purposes. The sign of this passed gate
  ///                     is unknown for the function, so it must be sanitized
  ///                     for a top event to function correctly.
  /// @param[in,out] gate_complements The processed complements of gates.
  /// @param[in,out] processed_gates The gates that have already been processed.
  void PropagateComplements(const IndexedGatePtr& gate,
                            std::map<int, int>* gate_complements,
                            std::set<int>* processed_gates);

  /// Pre-processes the tree by doing simple Boolean algebra.
  /// At this point all gates are expected to be either OR or AND.
  /// There should not be negative gate children.
  /// This function merges similar gates and may produce null or unity gates.
  /// @param[in,out] gate The starting gate to traverse the tree. This is for
  ///                     recursive purposes. This gate must be AND or OR.
  /// @param[in,out] processed_gates The gates that have already been processed.
  /// @returns true if the given tree has been changed by this function.
  /// @returns false if no change has been made.
  bool JoinGates(const IndexedGatePtr& gate, std::set<int>* processed_gates);

  /// Traverses the indexed fault tree to detect modules.
  /// @param[in] num_basic_events The number of basic events in the tree.
  void DetectModules(int num_basic_events);

  /// Traverses the given gate and assigns time of visit to nodes.
  /// @param[in] time The current time.
  /// @param[in,out] gate The gate to traverse and assign time to.
  /// @param[in,out] visit_basics The recordings for basic events.
  /// @returns The final time of traversing.
  int AssignTiming(int time, const IndexedGatePtr& gate, int visit_basics[][2]);

  /// Determines modules from original gates that have been already timed.
  /// This function can also create new modules from the existing tree.
  /// @param[in,out] gate The gate to test for modularity.
  /// @param[in] visit_basics The recordings for basic events.
  /// @param[in,out] visited_gates Container of visited gates with
  ///                              min and max time of visits of the subtree.
  void FindOriginalModules(const IndexedGatePtr& gate,
                           const int visit_basics[][2],
                           std::map<int, std::pair<int, int> >* visited_gates);

  /// Creates a new module as a child of an existing gate. The existing
  /// children of the original gate are used to create the new module.
  /// The module is added in the module and gate databases.
  /// If the new module must contain all the children, the original gate is
  /// turned into a module.
  /// @param[in,out] gate The parent gate for a module.
  /// @param[in] children Modular children to be added into the new module.
  void CreateNewModule(const IndexedGatePtr& gate,
                       const std::vector<int>& children);

  /// Checks if a group of modular children share anything with non-modular
  /// children. If so, then the modular children are not actually modular, and
  /// that children are removed from modular containers.
  /// This is due to chain of events that are shared between modular and
  /// non-modular children.
  /// @param[in] visit_basics The recordings for basic events.
  /// @param[in] visited_gates Visit max and min time recordings for gates.
  /// @param[in,out] modular_children Candidates for modular grouping.
  /// @param[in,out] non_modular_children Non modular children.
  void FilterModularChildren(
      const int visit_basics[][2],
      const std::map<int, std::pair<int, int> >& visited_gates,
      std::vector<int>* modular_children,
      std::vector<int>* non_modular_children);

  int top_event_index_;  ///< The index of the top gate of this tree.
  int gate_index_;  ///< The starting gate index for gate identification.
  /// All gates of this tree including newly created ones.
  boost::unordered_map<int, IndexedGatePtr> indexed_gates_;
  std::set<int> modules_;  ///< Modules in the tree.
  int top_event_sign_;  ///< The negative or positive sign of the top event.
  int new_gate_index_;  ///< Index for a new gate.
};

}  // namespace scram

#endif  // SCRAM_SRC_INDEXED_FAULT_TREE_H_
