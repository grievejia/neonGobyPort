#ifndef __ICFG_H
#define __ICFG_H

#include "rcs/CFG/MBB.h"

#include "llvm/ADT/GraphTraits.h"

#include <vector>

namespace rcs 
{
class ICFG;

class ICFGNode: public llvm::ilist_node<ICFGNode>
{
private:
  MicroBasicBlock *mbb;

  // Need maintain successors and predecessors in order to implement
  // GraphTraits<Inverse<ICFGNode *> >
  std::vector<ICFGNode *> succs, preds;
  ICFG *parent;
public:
  using iterator = std::vector<ICFGNode *>::iterator;
  using const_iterator = std::vector<ICFGNode *>::const_iterator;

  ICFGNode(): mbb(nullptr), parent(nullptr) {}
  ICFGNode(MicroBasicBlock *m, ICFG *p): mbb(m), parent(p) {}

  iterator succ_begin() { return succs.begin(); }
  iterator succ_end() { return succs.end(); }
  const_iterator succ_begin() const { return succs.begin(); }
  const_iterator succ_end() const { return succs.end(); }
  iterator pred_begin() { return preds.begin(); }
  iterator pred_end() { return preds.end(); }
  const_iterator pred_begin() const { return preds.begin(); }
  const_iterator pred_end() const { return preds.end(); }

  MicroBasicBlock *getMBB() const { return mbb; }
  const ICFG *getParent() const { return parent; }
  ICFG *getParent() { return parent; }
  unsigned size() const { return (unsigned)succs.size(); }
  void addSuccessor(ICFGNode *succ) { succs.push_back(succ); }
  void addPredecessor(ICFGNode *pred) { preds.push_back(pred); }
  void print(llvm::raw_ostream &O) const;
};

class ICFG
{
private:
  using MBBToNode = llvm::DenseMap<const MicroBasicBlock *, ICFGNode *>;
  MBBToNode mbb_to_node;
  llvm::iplist<ICFGNode> nodes;
public:
  using iterator = llvm::iplist<ICFGNode>::iterator;
  using const_iterator = llvm::iplist<ICFGNode>::const_iterator;

  ICFG() {}

  // Returns nullptr if <mbb> is not in the ICFG. 
  const ICFGNode *operator[](const MicroBasicBlock *mbb) const
  {
    return mbb_to_node.lookup(mbb);
  }
  ICFGNode *operator[](const MicroBasicBlock *mbb)
  {
    return mbb_to_node.lookup(mbb);
  }

  iterator begin() { return nodes.begin(); }
  iterator end() { return nodes.end(); }
  const_iterator begin() const { return nodes.begin(); }
  const_iterator end() const { return nodes.end(); }
  ICFGNode &front();
  const ICFGNode &front() const;
  size_t size() const {
    assert(mbb_to_node.size() == nodes.size());
    return nodes.size();
  }

  /**
   * This must be the only interface to create a new node. 
   * <addEdge> calls <getOrInsertMBB>, so it's fine. 
   */
  ICFGNode *getOrInsertMBB(const MicroBasicBlock *mbb);
  void addEdge(const MicroBasicBlock *x, const MicroBasicBlock *y);

  // Print functions. 
  static void printEdge(llvm::raw_ostream &O, const ICFGNode *x, const ICFGNode *y);
  void print(llvm::raw_ostream &O) const;
};
}

namespace llvm
{
template<>
struct GraphTraits<rcs::ICFGNode *>
{
  typedef rcs::ICFGNode NodeType;
  typedef rcs::ICFGNode::iterator ChildIteratorType;

  static NodeType *getEntryNode(NodeType *node) { return node; }
  static ChildIteratorType child_begin(NodeType *x) { return x->succ_begin(); }
  static ChildIteratorType child_end(NodeType *x) { return x->succ_end(); }
};

template<>
struct GraphTraits<const rcs::ICFGNode *>
{
  typedef const rcs::ICFGNode NodeType;
  typedef rcs::ICFGNode::const_iterator ChildIteratorType;

  static NodeType *getEntryNode(NodeType *node) { return node; }
  static ChildIteratorType child_begin(NodeType *x) { return x->succ_begin(); }
  static ChildIteratorType child_end(NodeType *x) { return x->succ_end(); }
};

template<>
struct GraphTraits<Inverse<rcs::ICFGNode *>>
{
  typedef rcs::ICFGNode NodeType;
  typedef rcs::ICFGNode::iterator ChildIteratorType;

  static NodeType *getEntryNode(Inverse<NodeType *> node) {
    return node.Graph;
  }
  static ChildIteratorType child_begin(NodeType *x) { return x->pred_begin(); }
  static ChildIteratorType child_end(NodeType *x) { return x->pred_end(); }
};

template<>
struct GraphTraits<Inverse<const rcs::ICFGNode *> >
{
  typedef const rcs::ICFGNode NodeType;
  typedef rcs::ICFGNode::const_iterator ChildIteratorType;

  static NodeType *getEntryNode(Inverse<NodeType *> node) {
    return node.Graph;
  }
  static ChildIteratorType child_begin(NodeType *x) { return x->pred_begin(); }
  static ChildIteratorType child_end(NodeType *x) { return x->pred_end(); }
};

template<>
struct GraphTraits<rcs::ICFG *>: public GraphTraits<rcs::ICFGNode *>
{
  typedef rcs::ICFG::iterator nodes_iterator;
  static nodes_iterator nodes_begin(rcs::ICFG *icfg) { return icfg->begin(); }
  static nodes_iterator nodes_end(rcs::ICFG *icfg) { return icfg->end(); }
};

template<>
struct GraphTraits<const rcs::ICFG *>: public GraphTraits<const rcs::ICFGNode *>
{
  typedef rcs::ICFG::const_iterator nodes_iterator;
  static nodes_iterator nodes_begin(const rcs::ICFG *icfg) { return icfg->begin(); }
  static nodes_iterator nodes_end(const rcs::ICFG *icfg) { return icfg->end(); }
};
}

#endif
