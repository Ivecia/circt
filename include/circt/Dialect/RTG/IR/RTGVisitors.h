//===- RTGVisitors.h - RTG Dialect Visitors ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines visitors that make it easier to work with RTG IR.
//
//===----------------------------------------------------------------------===//

#ifndef CIRCT_DIALECT_RTG_IR_RTGVISITORS_H
#define CIRCT_DIALECT_RTG_IR_RTGVISITORS_H

#include "circt/Dialect/RTG/IR/RTGOps.h"
#include "llvm/ADT/TypeSwitch.h"

namespace circt {
namespace rtg {

/// This helps visit TypeOp nodes.
template <typename ConcreteType, typename ResultType = void,
          typename... ExtraArgs>
class RTGOpVisitor {
public:
  ResultType dispatchOpVisitor(Operation *op, ExtraArgs... args) {
    auto *thisCast = static_cast<ConcreteType *>(this);
    return TypeSwitch<Operation *, ResultType>(op)
        .template Case<SequenceOp, SelectRandomOp, LabelOp, OnContextOp,
                       RenderedContextOp, SelectRandomResourceOp,
                       SetDifferenceResourceOp>([&](auto expr) -> ResultType {
          return thisCast->visitOp(expr, args...);
        })
        .Default([&](auto expr) -> ResultType {
          return thisCast->visitInvalidOp(op, args...);
        });
  }

  /// This callback is invoked on any operations that are not
  /// handled by the concrete visitor.
  ResultType visitUnhandledOp(Operation *op, ExtraArgs... args) {
    return ResultType();
  }

  /// This callback is invoked on any non-expression operations.
  ResultType visitInvalidOp(Operation *op, ExtraArgs... args) {
    op->emitOpError("unknown RTG operation");
    abort();
  }

#define HANDLE(OPTYPE, OPKIND)                                                 \
  ResultType visitOp(OPTYPE op, ExtraArgs... args) {                           \
    return static_cast<ConcreteType *>(this)->visit##OPKIND##Op(op, args...);  \
  }

  HANDLE(SequenceOp, Unhandled);
  HANDLE(SelectRandomOp, Unhandled);
  HANDLE(LabelOp, Unhandled);
  HANDLE(OnContextOp, Unhandled);
  HANDLE(RenderedContextOp, Unhandled);
  HANDLE(SelectRandomResourceOp, Unhandled);
  HANDLE(SetDifferenceResourceOp, Unhandled);
#undef HANDLE
};

} // namespace rtg
} // namespace circt

#endif // CIRCT_DIALECT_RTG_IR_RTGVISITORS_H
