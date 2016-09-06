//===- unittests/ADT/IListTest.cpp - ilist unit tests ---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/ilist.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/ilist_node.h"
#include "gtest/gtest.h"
#include <ostream>

using namespace llvm;

namespace {

struct Node : ilist_node<Node> {
  int Value;

  Node() {}
  Node(int Value) : Value(Value) {}
  Node(const Node&) = default;
  ~Node() { Value = -1; }
};

TEST(IListTest, Basic) {
  ilist<Node> List;
  List.push_back(Node(1));
  EXPECT_EQ(1, List.back().Value);
  EXPECT_EQ(nullptr, List.getPrevNode(List.back()));
  EXPECT_EQ(nullptr, List.getNextNode(List.back()));

  List.push_back(Node(2));
  EXPECT_EQ(2, List.back().Value);
  EXPECT_EQ(2, List.getNextNode(List.front())->Value);
  EXPECT_EQ(1, List.getPrevNode(List.back())->Value);

  const ilist<Node> &ConstList = List;
  EXPECT_EQ(2, ConstList.back().Value);
  EXPECT_EQ(2, ConstList.getNextNode(ConstList.front())->Value);
  EXPECT_EQ(1, ConstList.getPrevNode(ConstList.back())->Value);
}

TEST(IListTest, SpliceOne) {
  ilist<Node> List;
  List.push_back(1);

  // The single-element splice operation supports noops.
  List.splice(List.begin(), List, List.begin());
  EXPECT_EQ(1u, List.size());
  EXPECT_EQ(1, List.front().Value);
  EXPECT_TRUE(std::next(List.begin()) == List.end());

  // Altenative noop. Move the first element behind itself.
  List.push_back(2);
  List.push_back(3);
  List.splice(std::next(List.begin()), List, List.begin());
  EXPECT_EQ(3u, List.size());
  EXPECT_EQ(1, List.front().Value);
  EXPECT_EQ(2, std::next(List.begin())->Value);
  EXPECT_EQ(3, List.back().Value);
}

TEST(IListTest, SpliceSwap) {
  ilist<Node> L;
  Node N0(0);
  Node N1(1);
  L.insert(L.end(), &N0);
  L.insert(L.end(), &N1);
  EXPECT_EQ(0, L.front().Value);
  EXPECT_EQ(1, L.back().Value);

  L.splice(L.begin(), L, ++L.begin());
  EXPECT_EQ(1, L.front().Value);
  EXPECT_EQ(0, L.back().Value);

  L.clearAndLeakNodesUnsafely();
}

TEST(IListTest, SpliceSwapOtherWay) {
  ilist<Node> L;
  Node N0(0);
  Node N1(1);
  L.insert(L.end(), &N0);
  L.insert(L.end(), &N1);
  EXPECT_EQ(0, L.front().Value);
  EXPECT_EQ(1, L.back().Value);

  L.splice(L.end(), L, L.begin());
  EXPECT_EQ(1, L.front().Value);
  EXPECT_EQ(0, L.back().Value);

  L.clearAndLeakNodesUnsafely();
}

TEST(IListTest, UnsafeClear) {
  ilist<Node> List;

  // Before even allocating a sentinel.
  List.clearAndLeakNodesUnsafely();
  EXPECT_EQ(0u, List.size());

  // Empty list with sentinel.
  ilist<Node>::iterator E = List.end();
  List.clearAndLeakNodesUnsafely();
  EXPECT_EQ(0u, List.size());
  // The sentinel shouldn't change.
  EXPECT_TRUE(E == List.end());

  // List with contents.
  List.push_back(1);
  ASSERT_EQ(1u, List.size());
  Node *N = &*List.begin();
  EXPECT_EQ(1, N->Value);
  List.clearAndLeakNodesUnsafely();
  EXPECT_EQ(0u, List.size());
  ASSERT_EQ(1, N->Value);
  delete N;

  // List is still functional.
  List.push_back(5);
  List.push_back(6);
  ASSERT_EQ(2u, List.size());
  EXPECT_EQ(5, List.front().Value);
  EXPECT_EQ(6, List.back().Value);
}

struct Empty {};
TEST(IListTest, HasObsoleteCustomizationTrait) {
  // Negative test for HasObsoleteCustomization.
  static_assert(!ilist_detail::HasObsoleteCustomization<Empty, Node>::value,
                "Empty has no customizations");
}

struct GetNext {
  Node *getNext(Node *);
};
TEST(IListTest, HasGetNextTrait) {
  static_assert(ilist_detail::HasGetNext<GetNext, Node>::value,
                "GetNext has a getNext(Node*)");
  static_assert(ilist_detail::HasObsoleteCustomization<GetNext, Node>::value,
                "Empty should be obsolete because of getNext()");

  // Negative test for HasGetNext.
  static_assert(!ilist_detail::HasGetNext<Empty, Node>::value,
                "Empty does not have a getNext(Node*)");
}

struct CreateSentinel {
  Node *createSentinel();
};
TEST(IListTest, HasCreateSentinelTrait) {
  static_assert(ilist_detail::HasCreateSentinel<CreateSentinel>::value,
                "CreateSentinel has a getNext(Node*)");
  static_assert(
      ilist_detail::HasObsoleteCustomization<CreateSentinel, Node>::value,
      "Empty should be obsolete because of createSentinel()");

  // Negative test for HasCreateSentinel.
  static_assert(!ilist_detail::HasCreateSentinel<Empty>::value,
                "Empty does not have a createSentinel()");
}

struct NodeWithCallback : ilist_node<NodeWithCallback> {
  int Value = 0;
  bool IsInList = false;
  bool WasTransferred = false;

  NodeWithCallback() = default;
  NodeWithCallback(int Value) : Value(Value) {}
  NodeWithCallback(const NodeWithCallback &) = delete;
};

} // end namespace

namespace llvm {
template <> struct ilist_callback_traits<NodeWithCallback> {
  void addNodeToList(NodeWithCallback *N) { N->IsInList = true; }
  void removeNodeFromList(NodeWithCallback *N) { N->IsInList = false; }
  template <class Iterator>
  void transferNodesFromList(ilist_callback_traits &Other, Iterator First,
                             Iterator Last) {
    for (; First != Last; ++First) {
      First->WasTransferred = true;
      Other.removeNodeFromList(&*First);
      addNodeToList(&*First);
    }
  }
};
} // end namespace llvm

namespace {

TEST(IListTest, addNodeToList) {
  ilist<NodeWithCallback> L1, L2;
  NodeWithCallback N(7);
  ASSERT_FALSE(N.IsInList);
  ASSERT_FALSE(N.WasTransferred);

  L1.insert(L1.begin(), &N);
  ASSERT_EQ(1u, L1.size());
  ASSERT_EQ(&N, &L1.front());
  ASSERT_TRUE(N.IsInList);
  ASSERT_FALSE(N.WasTransferred);

  L2.splice(L2.end(), L1);
  ASSERT_EQ(&N, &L2.front());
  ASSERT_TRUE(N.IsInList);
  ASSERT_TRUE(N.WasTransferred);

  L1.remove(&N);
  ASSERT_EQ(0u, L1.size());
  ASSERT_FALSE(N.IsInList);
  ASSERT_TRUE(N.WasTransferred);
}

struct PrivateNode : private ilist_node<PrivateNode> {
  friend struct llvm::ilist_node_access;

  int Value = 0;

  PrivateNode() = default;
  PrivateNode(int Value) : Value(Value) {}
  PrivateNode(const PrivateNode &) = delete;
};

TEST(IListTest, privateNode) {
  // Instantiate various APIs to be sure they're callable when ilist_node is
  // inherited privately.
  ilist<PrivateNode> L;
  PrivateNode N(7);
  L.insert(L.begin(), &N);
  ++L.begin();
  (void)*L.begin();
  (void)(L.begin() == L.end());

  ilist<PrivateNode> L2;
  L2.splice(L2.end(), L);
  L2.remove(&N);
}

} // end namespace