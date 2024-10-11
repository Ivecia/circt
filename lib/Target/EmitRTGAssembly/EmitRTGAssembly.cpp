//===- EmitRTGAssembly.cpp - RTG Assembly Emitter -------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This is the main Assembly emitter implementation for the RTG dialect.
//
//===----------------------------------------------------------------------===//

#include "circt/Target/EmitRTGAssembly.h"
#include "circt/Dialect/RTG/IR/RTGVisitors.h"
#include "circt/Dialect/RTGTest/IR/RTGTestDialect.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Support/IndentedOstream.h"
#include "mlir/Tools/mlir-translate/Translation.h"
#include "llvm/ADT/ScopedHashTable.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>

using namespace circt;
using namespace rtg;
using namespace EmitRTGAssembly;

#define DEBUG_TYPE "emit-rtg-assembly"

static void printValue(Value val, raw_ostream &stream) {
  if (auto constOp = val.getDefiningOp<mlir::arith::ConstantOp>())
    if (auto integerAttr = dyn_cast<IntegerAttr>(constOp.getValue()))
      stream << integerAttr.getValue().getZExtValue();

  if (auto resourceOp = val.getDefiningOp<ResourceOpInterface>())
    resourceOp.printAssembly(stream);
}

static FailureOr<APInt> getBinary(Value val) {
  if (auto labelDeclOp = val.getDefiningOp<LabelDeclOp>())
    return labelDeclOp->emitError(
        "binary representation cannot be computed for labels");

  if (auto constOp = val.getDefiningOp<mlir::arith::ConstantOp>())
    if (auto integerAttr = dyn_cast<IntegerAttr>(constOp.getValue()))
      return integerAttr.getValue();

  if (auto resourceOp = val.getDefiningOp<ResourceOpInterface>())
    return resourceOp.getBinary();

  return failure();
}

namespace {
  class EmitRTGToElf : public RTGOpVisitor<EmitRTGToElf, LogicalResult, int> {
    public:
  
    LogicalResult visitUnhandledOp(Operation *op, int ctx) {
      op->emitError("unknown RTG operation");
      return failure();
    }

    LogicalResult visitRenderedContextOp(RenderedContextOp op, int ctx) {
      assert(ctx == -1);
      for (auto [index, region] : llvm::zip(op.getSelectors(), op.getRegions())) {
        auto &body = region->getBlocks().front();
        for (auto &op : body.getOperations()) {
          if (failed(dispatchOpVisitor(&op, index)))
            return failure();
        }
      }
      return success();
    }

  };
}

LogicalResult EmitRTGAssembly::emitRTGAssembly(Operation *module,
                                         llvm::raw_ostream &os,
                                         const EmitRTGAssemblyOptions &options) {
  if (module->getNumRegions() != 1)
    return module->emitError("must have exactly one region");
  if (!module->getRegion(0).hasOneBlock())
    return module->emitError("op region must have exactly one block");

  mlir::raw_indented_ostream ios(os);
  for (auto snippet : module->getRegion(0).getOps<rtg::SequenceOp>()) {
    for (auto &op : snippet.getBody()->getOperations()) {
      if (auto instr = dyn_cast<InstructionOpInterface>(&op)) {
        os << llvm::indent(4);
        if (llvm::is_contained(options.supportedInstructions,
                               instr->getName().getStringRef())) {
          instr.printAssembly(ios,
                              [&](Value value) { printValue(value, ios); });
          ios << "\n";
          continue;
        }
        SmallVector<APInt> operands;
        for (auto operand : instr->getOperands()) {
          auto res = getBinary(operand);
          if (failed(res))
            return failure();
          operands.push_back(*res);
        }
        SmallVector<char> str;
        instr.getBinary(operands).toString(str, 16, false);
        ios << ".word 0x" << str << "\n";
      }
      if (auto label = dyn_cast<LabelOp>(&op)) {
        if (label.getGlobal()) {
          os << ".global ";
          printValue(label.getLabel(), os);
          os << "\n";
        }
        printValue(label.getLabel(), os);
        os << ":\n";
        continue;
      }
    }
  }

  return success();
}

//===----------------------------------------------------------------------===//
// circt-translate registration
//===----------------------------------------------------------------------===//

void EmitRTGAssembly::registerEmitRTGAssemblyTranslation() {
  static llvm::cl::opt<std::string> allowedTextualInstructionsFile(
      "emit-assembly-allowed-instr-file",
      llvm::cl::desc("File with a comma-separated list of instructions "
                     "supported by the assembler."),
      llvm::cl::init(""));

  static llvm::cl::list<std::string> allowedInstructions(
      "emit-assembly-allowed-instr",
      llvm::cl::desc(
          "Comma-separated list of instructions supported by the assembler."),
      llvm::cl::MiscFlags::CommaSeparated);

  auto getOptions = [] {
    EmitRTGAssemblyOptions opts;
    SmallVector<std::string> instrs;
    for (auto &instr : allowedInstructions) {
      llvm::errs() << instr << "\n";
      instrs.push_back(instr);
    }

    if (!allowedTextualInstructionsFile.empty()) {
      std::ifstream input(allowedTextualInstructionsFile.getValue());
      std::string token;
      while (std::getline(input, token, ','))
        instrs.push_back(token);
    }
    opts.supportedInstructions = instrs;
    return opts;
  };

  static mlir::TranslateFromMLIRRegistration toAssembly(
      "emit-assembly", "emit assembly",
      [=](Operation *moduleOp, raw_ostream &output) {
        return EmitRTGAssembly::emitRTGAssembly(moduleOp, output, getOptions());
      },
      [](mlir::DialectRegistry &registry) {
        registry.insert<rtgtest::RTGTestDialect, rtg::RTGDialect,
                        mlir::arith::ArithDialect>();
      });
}
