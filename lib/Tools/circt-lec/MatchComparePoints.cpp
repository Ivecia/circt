//===- MatchComparePoints.cpp
//---------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "circt/Dialect/HW/HWOps.h"
#include "circt/Dialect/Verif/VerifOps.h"
#include "circt/Support/InstanceGraph.h"
#include "circt/Tools/circt-lec/Passes.h"
#include "mlir/Analysis/TopologicalSortUtils.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/LLVMIR/FunctionCallUtils.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Transforms/DialectConversion.h"

using namespace mlir;
using namespace circt;
using namespace circt::igraph;
using namespace hw;

namespace circt {
#define GEN_PASS_DEF_MATCHCOMPAREPOINTS
#include "circt/Tools/circt-lec/Passes.h.inc"
} // namespace circt

//===----------------------------------------------------------------------===//
// MatchComparePoints pass
//===----------------------------------------------------------------------===//

namespace {
struct MatchComparePointsPass
    : public circt::impl::MatchComparePointsBase<MatchComparePointsPass> {
  using circt::impl::MatchComparePointsBase<
      MatchComparePointsPass>::MatchComparePointsBase;
  void runOnOperation() override;
  hw::HWModuleOp lookupModule(StringRef name);
};
} // namespace

hw::HWModuleOp MatchComparePointsPass::lookupModule(StringRef name) {
  Operation *expectedModule = SymbolTable::lookupNearestSymbolFrom(
      getOperation(), StringAttr::get(&getContext(), name));
  if (!expectedModule || !isa<hw::HWModuleOp>(expectedModule)) {
    getOperation().emitError("module named '") << name << "' not found";
    return {};
  }
  return cast<hw::HWModuleOp>(expectedModule);
}

struct ComparePointsMatcher {
  HWModuleOp lhs, rhs;
  InstanceGraph &instanceGraph;
  ComparePointsMatcher(HWModuleOp lhs, HWModuleOp rhs,
                       InstanceGraph &instanceGraph)
      : lhs(lhs), rhs(rhs), instanceGraph(instanceGraph) {}
  using MatchingOperands = SmallVector<std::pair<OpOperand, OpOperand>>;
  using MatchingResults = SmallVector<std::pair<OpResult, OpResult>>;
  struct ComparePoint {
    ArrayAttr instancePath;
    StringAttr originalName;
    Type type;
  };

  using MatchingOps = SmallVector<Operation *>;

  SmallVector<Operation *> match();

  struct PortMutation {
    SmallVector<Value> inputPorts;
    SmallVector<Value> outputPorts;
  };

  DenseMap<HWModuleOp, PortMutation> mutations;
  void applyPortMutations();
};

ComparePointsMatcher::MatchingOps ComparePointsMatcher::match() {
  struct StackElement {
    HWModuleOp lhsCurrent, rhsCurrent;
    StringRef instanceName;
  };
  SmallVector<StackElement> worklist;
  worklist.push_back(StackElement{lhs, rhs, ""});
  while (!worklist.empty()) {
    auto element = worklist.pop_back_val();
  }
}

void MatchComparePointsPass::runOnOperation() {
  // Create necessary function declarations and globals
  // Lookup the modules.
  auto moduleA = lookupModule(firstModule);
  if (!moduleA)
    return signalPassFailure();
  auto moduleB = lookupModule(secondModule);
  if (!moduleB)
    return signalPassFailure();

  if (moduleA.getModuleType() != moduleB.getModuleType()) {
    moduleA.emitError("module's IO types don't match second modules: ")
        << moduleA.getModuleType() << " vs " << moduleB.getModuleType();
    return signalPassFailure();
  }
  // Bottom-up.

  // Regalize instances of HWModuleExtern, registers, memories.

  ComparePointsMatcher matcher(moduleA, moduleB, getAnalysis<InstanceGraph>());
  // SmallVector<std::pair<Operand, Operand>> operandComparePoints =
  //     getMatching(moduleA, moduleB);

  /*
    auto current = module;
    auto regs = module.getOps<RegOp>;
    // This will be wired to output ports.
    SmallVector<std::pair<Operand, Operand>> operandComparePoints =
    getMatching(circuit);

    // This will be wired to input ports.
    SmallVector<std::pair<OpResult, OpResult>> resultComparePoints =
    getMatching(circuit);

    for(auto [lhs, rhs]: operandComparePoints){
      while(!wokrlist.empty()){
        auto cur = worklist.pop_back_val();
        addPort(module, reg); // Add input ports with results.
      }
    }
  */
}
