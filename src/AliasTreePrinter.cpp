//===-- AliasTreePrinter.cpp - Memory Hierarchy Printer ---------*- C++ -*-===//
//
//                       Traits Static Analyzer (SAPFOR)
//
//===----------------------------------------------------------------------===//
//
// This file implements a 'dot-em' analysis pass, which emits the
// em.<fnname>.dot file for each function in the program in the program,
// with an alias tree for that function. This file also implements a 'view-em'
// analysis pass which display this graph.
//
//===----------------------------------------------------------------------===//

#include "EstimateMemory.h"
#include "tsar_dbg_output.h"
#include "tsar_pass.h"
#include <llvm/Analysis/DOTGraphTraitsPass.h>
#include <llvm/IR/CallSite.h>
#include <llvm/Support/GraphWriter.h>

using namespace llvm;
using namespace tsar;

namespace llvm {
template<> struct DOTGraphTraits<AliasTree *> :
    public DefaultDOTGraphTraits {

  using GT = GraphTraits<AliasTree *>;
  using EdgeItr = typename GT::ChildIteratorType;

  explicit DOTGraphTraits(bool IsSimple = false) :
    DefaultDOTGraphTraits(IsSimple) {}

  static std::string getGraphName(const AliasTree */*G*/) {
    return "Alias Tree";
  }

  std::string getNodeLabel(AliasTopNode */*N*/, AliasTree */*G*/) {
      return "Whole Memory";
  }

  std::string getNodeLabel(AliasEstimateNode *N, AliasTree */*G*/) {
    std::string Str;
    llvm::raw_string_ostream OS(Str);
    for (auto &EM : *N) {
      if (isSimple()) {
        printLocationSource(OS,
          MemoryLocation(EM.front(), EM.getSize(), EM.getAAInfo()));
        OS << ' ';
      } else if (EM.isAmbiguous()) {
        OS << "Ambiguous, size ";
        if (EM.getSize() == MemoryLocation::UnknownSize)
          OS << "unknown\\l";
        else
          OS << EM.getSize() << "\\l";
        for (auto Ptr : EM) {
          OS << "  ";
          if (isa<Function>(Ptr))
            Ptr->printAsOperand(OS);
          else
            Ptr->print(OS, true);
          OS << "\\l";
        }
      } else {
        if (isa<Function>(EM.front()))
          EM.front()->printAsOperand(OS);
        else
          EM.front()->print(OS, true);
        OS << ", size ";
        if (EM.getSize() == MemoryLocation::UnknownSize)
          OS << "unknown\\l";
        else
          OS << EM.getSize() << "\\l";
      }
    }
    return OS.str();
  }

  std::string getNodeLabel(AliasUnknownNode *N, AliasTree */*G*/) {
    std::string Str;
    llvm::raw_string_ostream OS(Str);
    OS << "Unknown Memory\n";
    for (auto &Unknown : *N) {
      if (isSimple()) {
        ImmutableCallSite CS(Unknown);
        if (auto Callee = [CS]() {
          return !CS ? nullptr : dyn_cast<Function>(
            CS.getCalledValue()->stripPointerCasts());
        }())
          Callee->printAsOperand(OS, false);
        else
          Unknown->printAsOperand(OS, false);
        OS << ' ';
      } else if (isa<Function>(*Unknown)) {
        Unknown->printAsOperand(OS, true);
        OS << "\\l";
      }
      else {
        Unknown->print(OS, true);
        OS << "\\l";
      }
    }
   return OS.str();
  }

  struct GetNodeLabelSwitch {
    template<class Ty> void operator()() {
      if (!isa<Ty>(Node))
        return;
      Result = This->getNodeLabel(cast<Ty>(Node), Graph);
    }

    DOTGraphTraits<AliasTree *> *This;
    AliasNode *Node;
    AliasTree *Graph;
    std::string Result;
  };

  std::string getNodeLabel(AliasNode *N, AliasTree *G) {
    GetNodeLabelSwitch Switch{ this, N, G };
    AliasNode::KindList::for_each_type(Switch);
    return std::move(Switch.Result);
  }

  static std::string getEdgeAttributes(
    AliasNode *N, EdgeItr /*E*/, const AliasTree */*G*/) {
    if (isa<AliasUnknownNode>(N))
      return "style=dashed";
    return "";
  }
};

struct EstimateMemoryPassGraphTraits {
  static AliasTree * getGraph(EstimateMemoryPass *EMP) {
    return &EMP->getAliasTree();
  }
};
}

namespace {
struct AliasTreePrinter:
  public DOTGraphTraitsPrinter<EstimateMemoryPass,
    false, tsar::AliasTree *, EstimateMemoryPassGraphTraits> {
  static char ID;
  AliasTreePrinter() :
    DOTGraphTraitsPrinter<EstimateMemoryPass,
      false, tsar::AliasTree *, EstimateMemoryPassGraphTraits>("em", ID) {
    initializeAliasTreePrinterPass(*PassRegistry::getPassRegistry());
  }
};

char AliasTreePrinter::ID = 0;

struct AliasTreeOnlyPrinter :
  public DOTGraphTraitsPrinter<EstimateMemoryPass,
    true, tsar::AliasTree *, EstimateMemoryPassGraphTraits> {
  static char ID;
  AliasTreeOnlyPrinter() :
    DOTGraphTraitsPrinter<EstimateMemoryPass,
      true, tsar::AliasTree *, EstimateMemoryPassGraphTraits>("emonly", ID) {
    initializeAliasTreeOnlyPrinterPass(*PassRegistry::getPassRegistry());
  }
};

char AliasTreeOnlyPrinter::ID = 0;

struct AliasTreeViewer :
  public DOTGraphTraitsViewer<EstimateMemoryPass,
    false, tsar::AliasTree *, EstimateMemoryPassGraphTraits> {
  static char ID;
  AliasTreeViewer() :
    DOTGraphTraitsViewer<EstimateMemoryPass,
      false, tsar::AliasTree *, EstimateMemoryPassGraphTraits>("em", ID) {
    initializeAliasTreeViewerPass(*PassRegistry::getPassRegistry());
  }
};

char AliasTreeViewer::ID = 0;

struct AliasTreeOnlyViewer :
  public DOTGraphTraitsViewer<EstimateMemoryPass,
    true, tsar::AliasTree *, EstimateMemoryPassGraphTraits> {
  static char ID;
  AliasTreeOnlyViewer() :
    DOTGraphTraitsViewer<EstimateMemoryPass,
      true, tsar::AliasTree *, EstimateMemoryPassGraphTraits>("emonly", ID) {
    initializeAliasTreeOnlyViewerPass(*PassRegistry::getPassRegistry());
  }
};

char AliasTreeOnlyViewer::ID = 0;
}

INITIALIZE_PASS(AliasTreeViewer, "view-em",
  "View alias tree of a function", true, true)

INITIALIZE_PASS(AliasTreeOnlyViewer, "view-em-only",
  "View alias tree of a function (alias summary only)", true, true)

INITIALIZE_PASS(AliasTreePrinter, "dot-em",
  "Print alias tree to 'dot' file", true, true)

INITIALIZE_PASS(AliasTreeOnlyPrinter, "dot-em-only",
  "Print alias tree to 'dot' file (alias summary only)", true, true)

FunctionPass * llvm::createAliasTreeViewerPass() {
  return new AliasTreeViewer();
}

FunctionPass * llvm::createAliasTreeOnlyViewerPass() {
  return new AliasTreeOnlyViewer();
}

FunctionPass * llvm::createAliasTreePrinterPass() {
  return new AliasTreePrinter();
}

FunctionPass * llvm::createAliasTreeOnlyPrinterPass() {
  return new AliasTreeOnlyPrinter();
}

void AliasTree::view() const {
  llvm::ViewGraph(this, "em", true,
    llvm::DOTGraphTraits<AliasTree *>::getGraphName(this));
}

void AliasTree::viewOnly() const {
  llvm::ViewGraph(this, "emonly", false,
    llvm::DOTGraphTraits<AliasTree *>::getGraphName(this));
}