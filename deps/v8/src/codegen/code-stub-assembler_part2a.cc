// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/code-stub-assembler.h"

#include "src/codegen/code-factory.h"
#include "src/execution/frames-inl.h"
#include "src/execution/frames.h"
#include "src/heap/heap-inl.h"  // For Page/MemoryChunk. TODO(jkummerow): Drop.
#include "src/logging/counters.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/cell.h"
#include "src/objects/descriptor-array.h"
#include "src/objects/function-kind.h"
#include "src/objects/heap-number.h"
#include "src/objects/oddball.h"
#include "src/objects/ordered-hash-table-inl.h"
#include "src/objects/property-cell.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {

using compiler::Node;
template <class T>
using TNode = compiler::TNode<T>;
template <class T>
using SloppyTNode = compiler::SloppyTNode<T>;


void CodeStubArguments::ForEach(
    const CodeStubAssembler::VariableList& vars,
    const CodeStubArguments::ForEachBodyFunction& body, Node* first, Node* last,
    CodeStubAssembler::ParameterMode mode) {
  assembler_->Comment("CodeStubArguments::ForEach");
  if (first == nullptr) {
    first = assembler_->IntPtrOrSmiConstant(0, mode);
  }
  if (last == nullptr) {
    DCHECK_EQ(mode, argc_mode_);
    last = argc_;
  }
  Node* start = assembler_->IntPtrSub(
      assembler_->UncheckedCast<IntPtrT>(base_),
      assembler_->ElementOffsetFromIndex(first, SYSTEM_POINTER_ELEMENTS, mode));
  Node* end = assembler_->IntPtrSub(
      assembler_->UncheckedCast<IntPtrT>(base_),
      assembler_->ElementOffsetFromIndex(last, SYSTEM_POINTER_ELEMENTS, mode));
  assembler_->BuildFastLoop(
      vars, start, end,
      [this, &body](Node* current) {
        Node* arg = assembler_->Load(MachineType::AnyTagged(), current);
        body(arg);
      },
      -kSystemPointerSize, CodeStubAssembler::INTPTR_PARAMETERS,
      CodeStubAssembler::IndexAdvanceMode::kPost);
}


}  // namespace internal
}  // namespace v8
