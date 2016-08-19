// Copyright 2012 the V8 project authors. All rights reserved.
//
// Copyright IBM Corp. 2012-2014. All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>  // For assert
#include <limits.h>  // For LONG_MIN, LONG_MAX.

#include "src/v8.h"

#if V8_TARGET_ARCH_S390

#include "src/bootstrapper.h"
#include "src/codegen.h"
#include "src/cpu-profiler.h"
#include "src/debug.h"
#include "src/isolate-inl.h"
#include "src/runtime.h"

namespace v8 {
namespace internal {

MacroAssembler::MacroAssembler(Isolate* arg_isolate, void* buffer, int size)
    : Assembler(arg_isolate, buffer, size),
      generating_stub_(false),
      has_frame_(false) {
  if (isolate() != NULL) {
    code_object_ = Handle<Object>(isolate()->heap()->undefined_value(),
                                  isolate());
  }
}


void MacroAssembler::Jump(Register target) {
  b(target);
}


void MacroAssembler::JumpToJSEntry(Register target) {
  Move(ip, target);
  Jump(ip);
}


void MacroAssembler::Jump(intptr_t target, RelocInfo::Mode rmode,
                          Condition cond, CRegister) {
  Label skip;

  if (cond != al) b(NegateCondition(cond), &skip);

  DCHECK(rmode == RelocInfo::CODE_TARGET ||
         rmode == RelocInfo::RUNTIME_ENTRY);

  mov(ip, Operand(target, rmode));
  b(ip);

  bind(&skip);
}


void MacroAssembler::Jump(Address target, RelocInfo::Mode rmode,
                          Condition cond, CRegister cr) {
  DCHECK(!RelocInfo::IsCodeTarget(rmode));
  Jump(reinterpret_cast<intptr_t>(target), rmode, cond, cr);
}


void MacroAssembler::Jump(Handle<Code> code, RelocInfo::Mode rmode,
                          Condition cond) {
  DCHECK(RelocInfo::IsCodeTarget(rmode));
  // 'code' is always generated s390 code, never THUMB code
  AllowDeferredHandleDereference embedding_raw_address;
  Jump(reinterpret_cast<intptr_t>(code.location()), rmode, cond);
}


int MacroAssembler::CallSize(Register target) {
  // 2-byte BASR is used to dispatch.
  return 2;
}


void MacroAssembler::CallC(Register target) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Label start;
  bind(&start);

  // Statement positions are expected to be recorded when the target
  // address is loaded.
  positions_recorder()->WriteRecordedPositions();
#ifdef V8_OS_ZOS
  // Branch to target via indirect branch
  basr(r7, target);
  nop(BASR_CALL_TYPE_NOP);
  DCHECK_EQ(CallSize(target) + 2, SizeOfCodeGeneratedSince(&start));
#else
  // Branch to target via indirect branch
  basr(r14, target);
  DCHECK_EQ(CallSize(target), SizeOfCodeGeneratedSince(&start));
#endif
}


void MacroAssembler::Call(Register target) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  Label start;
  bind(&start);

  // Statement positions are expected to be recorded when the target
  // address is loaded.
  positions_recorder()->WriteRecordedPositions();

  // Branch to target via indirect branch
  basr(r14, target);

  DCHECK_EQ(CallSize(target), SizeOfCodeGeneratedSince(&start));
}


void MacroAssembler::CallJSEntry(Register target) {
  DCHECK(target.is(ip));
  Call(target);
}


int MacroAssembler::CallSize(
    Address target, RelocInfo::Mode rmode, Condition cond) {
  // S390 Assembler::move sequence are IILF / IIHF
  int size;
#if V8_TARGET_ARCH_S390X
  size = 2 + 12;  // IILF + IIHF + BASR
#else
  size = 2 + 6;   // IILF + BASR
#endif
  return size;
}


int MacroAssembler::CallSizeNotPredictableCodeSize(
    Address target, RelocInfo::Mode rmode, Condition cond) {
  // S390 Assembler::move sequence are IILF / IIHF
  int size;
#if V8_TARGET_ARCH_S390X
  size = 2 + 12;  // IILF + IIHF + BASR
#else
  size = 2 + 6;  // IILF + BASR
#endif
  return size;
}


void MacroAssembler::Call(Address target,
                          RelocInfo::Mode rmode,
                          Condition cond) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  DCHECK(cond == al);

#ifdef DEBUG
  // Check the expected size before generating code to ensure we assume the same
  // constant pool availability (e.g., whether constant pool is full or not).
  int expected_size = CallSize(target, rmode, cond);
  Label start;
  bind(&start);
#endif

  // Statement positions are expected to be recorded when the target
  // address is loaded.
  positions_recorder()->WriteRecordedPositions();

  mov(ip, Operand(reinterpret_cast<intptr_t>(target), rmode));
  basr(r14, ip);

  DCHECK_EQ(expected_size, SizeOfCodeGeneratedSince(&start));
}


int MacroAssembler::CallSize(Handle<Code> code,
                             RelocInfo::Mode rmode,
                             TypeFeedbackId ast_id,
                             Condition cond) {
  AllowDeferredHandleDereference using_raw_address;
  return CallSize(reinterpret_cast<Address>(code.location()), rmode, cond);
}


void MacroAssembler::Call(Handle<Code> code,
                          RelocInfo::Mode rmode,
                          TypeFeedbackId ast_id,
                          Condition cond) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  DCHECK(RelocInfo::IsCodeTarget(rmode));

#ifdef DEBUG
  // Check the expected size before generating code to ensure we assume the same
  // constant pool availability (e.g., whether constant pool is full or not).
  int expected_size = CallSize(code, rmode, ast_id, cond);
  Label start;
  bind(&start);
#endif

  if (rmode == RelocInfo::CODE_TARGET && !ast_id.IsNone()) {
    SetRecordedAstId(ast_id);
    rmode = RelocInfo::CODE_TARGET_WITH_ID;
  }
  AllowDeferredHandleDereference using_raw_address;
  Call(reinterpret_cast<Address>(code.location()), rmode, cond);
  DCHECK_EQ(expected_size, SizeOfCodeGeneratedSince(&start));
}

void MacroAssembler::RetC() {
#ifdef V8_OS_ZOS
    b(r7);
#else
    b(r14);
#endif
}

void MacroAssembler::Ret() {
    b(r14);
}


void MacroAssembler::Drop(int count) {
  if (count > 0) {
    la(sp, MemOperand(sp, count * kPointerSize));
  }
}


void MacroAssembler::Ret(int drop) {
  Drop(drop);
  Ret();
}


void MacroAssembler::Call(Label* target) {
  b(r14, target);
}


void MacroAssembler::Push(Handle<Object> handle) {
  mov(r0, Operand(handle));
  push(r0);
}


void MacroAssembler::Move(Register dst, Handle<Object> value) {
  AllowDeferredHandleDereference smi_check;
  if (value->IsSmi()) {
    LoadSmiLiteral(dst, reinterpret_cast<Smi *>(*value));
  } else {
    DCHECK(value->IsHeapObject());
    if (isolate()->heap()->InNewSpace(*value)) {
      Handle<Cell> cell = isolate()->factory()->NewCell(value);
      mov(dst, Operand(cell));
      LoadP(dst, FieldMemOperand(dst, Cell::kValueOffset));
    } else {
      mov(dst, Operand(value));
    }
  }
}


void MacroAssembler::Move(Register dst, Register src, Condition cond) {
  if (!dst.is(src)) {
    LoadRR(dst, src);
  }
}


void MacroAssembler::Move(DoubleRegister dst, DoubleRegister src) {
  if (!dst.is(src)) {
    ldr(dst, src);
  }
}


void MacroAssembler::MultiPush(RegList regs) {
  int16_t num_to_push = NumberOfBitsSet(regs);
  int16_t stack_offset = num_to_push * kPointerSize;

  lay(sp, MemOperand(sp, -stack_offset));
  for (int16_t i = kNumRegisters - 1; i >= 0; i--) {
    if ((regs & (1 << i)) != 0) {
      stack_offset -= kPointerSize;
      StoreP(ToRegister(i), MemOperand(sp, stack_offset));
    }
  }
}


void MacroAssembler::MultiPop(RegList regs) {
  int16_t stack_offset = 0;

  for (int16_t i = 0; i < kNumRegisters; i++) {
    if ((regs & (1 << i)) != 0) {
      LoadP(ToRegister(i), MemOperand(sp, stack_offset));
      stack_offset += kPointerSize;
    }
  }
  la(sp, MemOperand(sp, stack_offset));
}


void MacroAssembler::LoadRoot(Register destination,
                              Heap::RootListIndex index,
                              Condition) {
  LoadP(destination, MemOperand(kRootRegister,
                                index << kPointerSizeLog2), r0);
}


void MacroAssembler::StoreRoot(Register source,
                               Heap::RootListIndex index,
                               Condition) {
  StoreP(source, MemOperand(kRootRegister, index << kPointerSizeLog2));
}


void MacroAssembler::InNewSpace(Register object,
                                Register scratch,
                                Condition cond,
                                Label* branch) {
  // N.B. scratch may be same register as object
  DCHECK(cond == eq || cond == ne);
  // TODO(joransiu): check if we can merge mov Operand into AndP.
  mov(r0, Operand(ExternalReference::new_space_mask(isolate())));

  AndP(scratch, object, r0);
  CmpP(scratch, Operand(ExternalReference::new_space_start(isolate())));
  b(cond, branch);
}


void MacroAssembler::RecordWriteField(
    Register object,
    int offset,
    Register value,
    Register dst,
    LinkRegisterStatus lr_status,
    SaveFPRegsMode save_fp,
    RememberedSetAction remembered_set_action,
    SmiCheck smi_check,
    PointersToHereCheck pointers_to_here_check_for_value) {
  // First, check if a write barrier is even needed. The tests below
  // catch stores of Smis.
  Label done;

  // Skip barrier if writing a smi.
  if (smi_check == INLINE_SMI_CHECK) {
    JumpIfSmi(value, &done);
  }

  // Although the object register is tagged, the offset is relative to the start
  // of the object, so so offset must be a multiple of kPointerSize.
  DCHECK(IsAligned(offset, kPointerSize));

  lay(dst, MemOperand(object, offset - kHeapObjectTag));
  if (emit_debug_code()) {
    Label ok;
    AndP(r0, dst, Operand((1 << kPointerSizeLog2) - 1));
    beq(&ok, Label::kNear);
    stop("Unaligned cell in write barrier");
    bind(&ok);
  }

  RecordWrite(object,
              dst,
              value,
              lr_status,
              save_fp,
              remembered_set_action,
              OMIT_SMI_CHECK,
              pointers_to_here_check_for_value);

  bind(&done);

  // Clobber clobbered input registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(value, Operand(BitCast<intptr_t>(kZapValue + 4)));
    mov(dst, Operand(BitCast<intptr_t>(kZapValue + 8)));
  }
}


// Will clobber 4 registers: object, map, dst, ip.  The
// register 'object' contains a heap object pointer.
void MacroAssembler::RecordWriteForMap(Register object,
                                       Register map,
                                       Register dst,
                                       LinkRegisterStatus lr_status,
                                       SaveFPRegsMode fp_mode) {
  if (emit_debug_code()) {
    LoadP(dst, FieldMemOperand(map, HeapObject::kMapOffset));
    CmpP(dst, Operand(isolate()->factory()->meta_map()));
    Check(eq, kWrongAddressOrValuePassedToRecordWrite);
  }

  if (!FLAG_incremental_marking) {
    return;
  }

  if (emit_debug_code()) {
    CmpP(map, FieldMemOperand(object, HeapObject::kMapOffset));
    Check(eq, kWrongAddressOrValuePassedToRecordWrite);
  }

  Label done;

  // A single check of the map's pages interesting flag suffices, since it is
  // only set during incremental collection, and then it's also guaranteed that
  // the from object's page's interesting flag is also set.  This optimization
  // relies on the fact that maps can never be in new space.
  CheckPageFlag(map,
                map,  // Used as scratch.
                MemoryChunk::kPointersToHereAreInterestingMask,
                eq,
                &done);

  lay(dst, MemOperand(object, HeapObject::kMapOffset - kHeapObjectTag));
  if (emit_debug_code()) {
    Label ok;
    AndP(r0, dst, Operand((1 << kPointerSizeLog2) - 1));
    beq(&ok, Label::kNear);
    stop("Unaligned cell in write barrier");
    bind(&ok);
  }

  // Record the actual write.
  if (lr_status == kLRHasNotBeenSaved) {
    push(r14);
  }
  RecordWriteStub stub(isolate(), object, map, dst, OMIT_REMEMBERED_SET,
                       fp_mode);
  CallStub(&stub);
  if (lr_status == kLRHasNotBeenSaved) {
    pop(r14);
  }

  bind(&done);

  // Count number of write barriers in generated code.
  isolate()->counters()->write_barriers_static()->Increment();
  IncrementCounter(isolate()->counters()->write_barriers_dynamic(), 1, ip, dst);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(dst, Operand(BitCast<intptr_t>(kZapValue + 12)));
    mov(map, Operand(BitCast<intptr_t>(kZapValue + 16)));
  }
}


// Will clobber 4 registers: object, address, scratch, ip.  The
// register 'object' contains a heap object pointer.  The heap object
// tag is shifted away.
void MacroAssembler::RecordWrite(
    Register object,
    Register address,
    Register value,
    LinkRegisterStatus lr_status,
    SaveFPRegsMode fp_mode,
    RememberedSetAction remembered_set_action,
    SmiCheck smi_check,
    PointersToHereCheck pointers_to_here_check_for_value) {
  DCHECK(!object.is(value));
  if (emit_debug_code()) {
    CmpP(value, MemOperand(address));
    Check(eq, kWrongAddressOrValuePassedToRecordWrite);
  }

  if (remembered_set_action == OMIT_REMEMBERED_SET &&
      !FLAG_incremental_marking) {
    return;
  }
  // First, check if a write barrier is even needed. The tests below
  // catch stores of smis and stores into the young generation.
  Label done;

  if (smi_check == INLINE_SMI_CHECK) {
    JumpIfSmi(value, &done);
  }

  if (pointers_to_here_check_for_value != kPointersToHereAreAlwaysInteresting) {
    CheckPageFlag(value,
                  value,  // Used as scratch.
                  MemoryChunk::kPointersToHereAreInterestingMask,
                  eq,
                  &done);
  }
  CheckPageFlag(object,
                value,  // Used as scratch.
                MemoryChunk::kPointersFromHereAreInterestingMask,
                eq,
                &done);

  // Record the actual write.
  if (lr_status == kLRHasNotBeenSaved) {
    push(r14);
  }
  RecordWriteStub stub(isolate(), object, value, address, remembered_set_action,
                       fp_mode);
  CallStub(&stub);
  if (lr_status == kLRHasNotBeenSaved) {
    pop(r14);
  }

  bind(&done);

  // Count number of write barriers in generated code.
  isolate()->counters()->write_barriers_static()->Increment();
  IncrementCounter(isolate()->counters()->write_barriers_dynamic(), 1, ip,
                   value);

  // Clobber clobbered registers when running with the debug-code flag
  // turned on to provoke errors.
  if (emit_debug_code()) {
    mov(address, Operand(BitCast<intptr_t>(kZapValue + 12)));
    mov(value, Operand(BitCast<intptr_t>(kZapValue + 16)));
  }
}


void MacroAssembler::RememberedSetHelper(Register object,  // For debug tests.
                                         Register address,
                                         Register scratch,
                                         SaveFPRegsMode fp_mode,
                                         RememberedSetFinalAction and_then) {
  Label done;
  if (emit_debug_code()) {
    Label ok;
    JumpIfNotInNewSpace(object, scratch, &ok);
    stop("Remembered set pointer is in new space");
    bind(&ok);
  }
  // Load store buffer top.
  ExternalReference store_buffer =
      ExternalReference::store_buffer_top(isolate());
  mov(ip, Operand(store_buffer));
  LoadP(scratch, MemOperand(ip));
  // Store pointer to buffer and increment buffer top.
  StoreP(address, MemOperand(scratch));
  AddP(scratch, Operand(kPointerSize));
  // Write back new top of buffer.
  StoreP(scratch, MemOperand(ip));
  // Call stub on end of buffer.
  // Check for end of buffer.
  AndP(scratch, Operand(StoreBuffer::kStoreBufferOverflowBit));

  if (and_then == kFallThroughAtEnd) {
    beq(&done, Label::kNear);
  } else {
    DCHECK(and_then == kReturnAtEnd);
    beq(&done, Label::kNear);
  }
  push(r14);
  StoreBufferOverflowStub store_buffer_overflow =
      StoreBufferOverflowStub(isolate(), fp_mode);
  CallStub(&store_buffer_overflow);
  pop(r14);
  bind(&done);
  if (and_then == kReturnAtEnd) {
    Ret();
  }
}


void MacroAssembler::PushFixedFrame(Register marker_reg) {
  CleanseP(r14);
  if (marker_reg.is_valid()) {
    Push(r14, fp, cp, marker_reg);
  } else {
    Push(r14, fp, cp);
  }
}


void MacroAssembler::PopFixedFrame(Register marker_reg) {
  if (marker_reg.is_valid()) {
    Pop(r14, fp, cp, marker_reg);
  } else {
    Pop(r14, fp, cp);
  }
}


// Push and pop all registers that can hold pointers.
void MacroAssembler::PushSafepointRegisters() {
  // Safepoints expect a block of kNumSafepointRegisters values on the
  // stack, so adjust the stack for unsaved registers.
  const int num_unsaved = kNumSafepointRegisters - kNumSafepointSavedRegisters;
  DCHECK(num_unsaved >= 0);
  if (num_unsaved > 0) {
    lay(sp, MemOperand(sp, -(num_unsaved * kPointerSize)));
  }
  MultiPush(kSafepointSavedRegisters);
}


void MacroAssembler::PopSafepointRegisters() {
  const int num_unsaved = kNumSafepointRegisters - kNumSafepointSavedRegisters;
  MultiPop(kSafepointSavedRegisters);
  if (num_unsaved > 0) {
    la(sp, MemOperand(sp, num_unsaved * kPointerSize));
  }
}




void MacroAssembler::StoreToSafepointRegisterSlot(Register src, Register dst) {
  StoreP(src, SafepointRegisterSlot(dst));
}


void MacroAssembler::LoadFromSafepointRegisterSlot(Register dst, Register src) {
  LoadP(dst, SafepointRegisterSlot(src));
}


int MacroAssembler::SafepointRegisterStackIndex(int reg_code) {
  // The registers are pushed starting with the highest encoding,
  // which means that lowest encodings are closest to the stack pointer.
  RegList regs = kSafepointSavedRegisters;
  int index = 0;

  DCHECK(reg_code >= 0 && reg_code < kNumRegisters);

  for (int16_t i = 0; i < reg_code; i++) {
    if ((regs & (1 << i)) != 0) {
      index++;
    }
  }

  return index;
}


MemOperand MacroAssembler::SafepointRegisterSlot(Register reg) {
  return MemOperand(sp, SafepointRegisterStackIndex(reg.code()) * kPointerSize);
}


MemOperand MacroAssembler::SafepointRegistersAndDoublesSlot(Register reg) {
  // General purpose registers are pushed last on the stack.
  int doubles_size = DoubleRegister::NumAllocatableRegisters() * kDoubleSize;
  int register_offset = SafepointRegisterStackIndex(reg.code()) * kPointerSize;
  return MemOperand(sp, doubles_size + register_offset);
}

void MacroAssembler::CanonicalizeNaN(const DoubleRegister dst,
                                     const DoubleRegister src) {
  Label done;

  // Test for NaN
  cdbr(src, src);

  if (dst.is(src)) {
    bordered(&done, Label::kNear);
  } else {
    Label is_nan;
    bunordered(&is_nan, Label::kNear);
    ldr(dst, src);
    b(&done, Label::kNear);
    bind(&is_nan);
  }

  // Replace with canonical NaN.
  uint64_t nan_int64 = BitCast<uint64_t>(
    FixedDoubleArray::canonical_not_the_hole_nan_as_double());
  iihf(r0, Operand(nan_int64 >> 32));
  iilf(r0, Operand(static_cast<int32_t>(nan_int64)));
  ldgr(dst, r0);
  bind(&done);
}


void MacroAssembler::ConvertIntToDouble(Register src,
                                        DoubleRegister double_dst) {
  cdfbr(double_dst, src);
}


void MacroAssembler::ConvertUnsignedIntToDouble(Register src,
                                                DoubleRegister double_dst) {
  if (CpuFeatures::IsSupported(FLOATING_POINT_EXT)) {
    cdlfbr(Condition(5), Condition(5), double_dst, src);
  } else {
    // zero-extend src
    llgfr(src, src);
    // convert to double
    cdgbr(double_dst, src);
  }
}


void MacroAssembler::ConvertIntToFloat(const DoubleRegister dst,
                                       const Register src,
                                       const Register int_scratch) {
  cefbr(dst, src);
}


void MacroAssembler::ConvertDoubleToInt64(const DoubleRegister double_input,
                                          const Register dst,
                                          FPRoundingMode rounding_mode) {
  Condition m = Condition(0);
  switch (rounding_mode) {
    case kRoundToZero:
      m = Condition(5);
      break;
    case kRoundToNearest:
      // ToDo(Zen): 1 or 3?
      UNIMPLEMENTED();
      break;
    case kRoundToPlusInf:
      m = Condition(6);
      break;
    case kRoundToMinusInf:
      m = Condition(7);
      break;
    default:
      UNIMPLEMENTED();
      break;
  }
  cgdbr(m, dst, double_input);
}


void MacroAssembler::StubPrologue(int prologue_offset) {
  PushFixedFrame();
  Push(Smi::FromInt(StackFrame::STUB));
  // Adjust FP to point to saved FP.
  la(fp, MemOperand(sp, StandardFrameConstants::kFixedFrameSizeFromFp));
}


void MacroAssembler::Prologue(bool code_pre_aging,
                              int prologue_offset) {
  { PredictableCodeSizeScope predictible_code_size_scope(
      this, kNoCodeAgeSequenceLength);
    Assembler::BlockTrampolinePoolScope block_trampoline_pool(this);
    // The following instructions must remain together and unmodified
    // for code aging to work properly.
    if (code_pre_aging) {
      // Pre-age the code.
      // This matches the code found in PatchPlatformCodeAge()
      Code* stub = Code::GetPreAgedCodeAgeStub(isolate());
      intptr_t target = reinterpret_cast<intptr_t>(stub->instruction_start());
      nop();
      mov(r2, Operand(target));
      Jump(r2);
      for (int i = 0;
           i < kNoCodeAgeSequenceLength - kCodeAgingSequenceLength; i += 2) {
        // TODO(joransiu): Create nop function to pad
        //         (kNoCodeAgeSequenceLength - kCodeAgingSequenceLength) bytes.
        nop();   // 2-byte nops().
      }
    } else {
      // This matches the code found in GetNoCodeAgeSequence()
      PushFixedFrame(r3);
      // Adjust fp to point to saved fp.
      la(fp, MemOperand(sp, StandardFrameConstants::kFixedFrameSizeFromFp));
    }
  }
}


// Used by FrameScope constructor to enter frame.
void MacroAssembler::EnterFrame(StackFrame::Type type,
                                bool load_constant_pool) {
  // We create a stack frame with:
  //    Return Addr <-- old sp
  //    Old FP      <-- new fp
  //    CP
  //    type
  //    CodeObject  <-- new sp
  if (FLAG_enable_ool_constant_pool && load_constant_pool) {
    PushFixedFrame();

    LoadSmiLiteral(ip, Smi::FromInt(type));
    push(ip);
  } else {
    LoadSmiLiteral(ip, Smi::FromInt(type));
    PushFixedFrame(ip);
  }


  mov(r0, Operand(CodeObject()));
  push(r0);
  // Adjust FP to point to saved FP
  la(fp, MemOperand(sp,
         StandardFrameConstants::kFixedFrameSizeFromFp + kPointerSize));
}


int MacroAssembler::LeaveFrame(StackFrame::Type type,
                               int stack_adjustment) {
  // Drop the execution stack down to the frame pointer and restore
  // the caller frame pointer and return address.
  LoadP(r14, MemOperand(fp, StandardFrameConstants::kCallerPCOffset));
  lay(r1, MemOperand(fp,
      StandardFrameConstants::kCallerSPOffset + stack_adjustment));
  LoadP(fp, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  LoadRR(sp, r1);
  int frame_ends = pc_offset();
  return frame_ends;
}


// ExitFrame layout (probably wrongish.. needs updating)
//
//  SP -> previousSP
//        LK reserved
//        code
//        sp_on_exit (for debug?)
// oldSP->prev SP
//        LK
//        <parameters on stack>

// Prior to calling EnterExitFrame, we've got a bunch of parameters
// on the stack that we need to wrap a real frame around.. so first
// we reserve a slot for LK and push the previous SP which is captured
// in the fp register (r11)
// Then - we buy a new frame

void MacroAssembler::EnterExitFrame(bool save_doubles, int stack_space) {
  // Set up the frame structure on the stack.
  DCHECK_EQ(2 * kPointerSize, ExitFrameConstants::kCallerSPDisplacement);
  DCHECK_EQ(1 * kPointerSize, ExitFrameConstants::kCallerPCOffset);
  DCHECK_EQ(0 * kPointerSize, ExitFrameConstants::kCallerFPOffset);
  DCHECK(stack_space > 0);

  // This is an opportunity to build a frame to wrap
  // all of the pushes that have happened inside of V8
  // since we were called from C code

  // replicate ARM frame - TODO make this more closely follow S390 ABI


  CleanseP(r14);
  Push(r14, fp);
  LoadRR(fp, sp);
  // Reserve room for saved entry sp and code object.
  lay(sp, MemOperand(sp, - ExitFrameConstants::kFrameSize));

  if (emit_debug_code()) {
    StoreP(MemOperand(fp, ExitFrameConstants::kSPOffset),
           Operand::Zero(), r1);
  }
  mov(r1, Operand(CodeObject()));
  StoreP(r1, MemOperand(fp, ExitFrameConstants::kCodeOffset));

  // Save the frame pointer and the context in top.
  mov(r1, Operand(ExternalReference(Isolate::kCEntryFPAddress, isolate())));
  StoreP(fp, MemOperand(r1));
  mov(r1, Operand(ExternalReference(Isolate::kContextAddress, isolate())));
  StoreP(cp, MemOperand(r1));

  // Optionally save all volatile double registers.
  if (save_doubles) {
    const int kNumRegs = DoubleRegister::kNumVolatileRegisters;
    lay(sp, MemOperand(sp, -kNumRegs * kDoubleSize));
#define StoreFloatingPointRegisterToStack(reg, offset) \
    StoreF(DoubleRegister::from_code(reg), \
      MemOperand(sp, (offset) * kDoubleSize));
#ifdef V8_TARGET_ARCH_S390X
    for (int i = 0; i < 7; i++) {
      StoreFloatingPointRegisterToStack(i, i);
    }
#else
    StoreFloatingPointRegisterToStack(0, 0);
    StoreFloatingPointRegisterToStack(1, 1);
    StoreFloatingPointRegisterToStack(2, 2);
    StoreFloatingPointRegisterToStack(3, 3);
    StoreFloatingPointRegisterToStack(5, 4);
    int offset = 5;
    for (int i = 7; i < DoubleRegister::kNumRegisters; i++, offset++) {
      StoreFloatingPointRegisterToStack(i, offset);
    }
#endif
#undef StoreFloatingPointRegisterToStack
  }

  lay(sp, MemOperand(sp, -stack_space * kPointerSize));


  // Allocate and align the frame preparing for calling the runtime
  // function.
  const int frame_alignment = MacroAssembler::ActivationFrameAlignment();
  if (frame_alignment > 0) {
    DCHECK(frame_alignment == 8);
    ClearRightImm(sp, sp, Operand(3));  // equivalent to &= -8
  }
  StoreP(MemOperand(sp, -kNumRequiredStackFrameSlots * kPointerSize),
         Operand::Zero(), r0);
  lay(sp, MemOperand(sp, -kNumRequiredStackFrameSlots * kPointerSize));
  // Set the exit frame sp value to point just before the return address
  // location.
  lay(r1, MemOperand(sp, kStackFrameSPSlot * kPointerSize));
  StoreP(r1, MemOperand(fp, ExitFrameConstants::kSPOffset));
}


void MacroAssembler::InitializeNewString(Register string,
                                         Register length,
                                         Heap::RootListIndex map_index,
                                         Register scratch1,
                                         Register scratch2) {
  SmiTag(scratch1, length);
  LoadRoot(scratch2, map_index);
  StoreP(scratch1, FieldMemOperand(string, String::kLengthOffset));
  StoreP(FieldMemOperand(string, String::kHashFieldSlot),
         Operand(String::kEmptyHashField), scratch1);
  StoreP(scratch2, FieldMemOperand(string, HeapObject::kMapOffset));
}


int MacroAssembler::ActivationFrameAlignment() {
#if !defined(USE_SIMULATOR)
  // Running on the real platform. Use the alignment as mandated by the local
  // environment.
  // Note: This will break if we ever start generating snapshots on one S390
  // platform for another S390 platform with a different alignment.
  return base::OS::ActivationFrameAlignment();
#else  // Simulated
  // If we are using the simulator then we should always align to the expected
  // alignment. As the simulator is used to generate snapshots we do not know
  // if the target platform will need alignment, so this is controlled from a
  // flag.
  return FLAG_sim_stack_alignment;
#endif
}


void MacroAssembler::LeaveExitFrame(bool save_doubles,
                                    Register argument_count,
                                    bool restore_context) {
  // Optionally restore all double registers.
  if (save_doubles) {
    // Calculate the stack location of the saved doubles and restore them.
    const int kNumRegs = DoubleRegister::kNumVolatileRegisters;
    lay(sp, MemOperand(fp, -(2 * kPointerSize + kNumRegs * kDoubleSize)));
#define LoadFloatingPointRegisterToStack(reg, offset) \
    LoadF(DoubleRegister::from_code(reg), \
      MemOperand(sp, (offset) * kDoubleSize));
#ifdef V8_TARGET_ARCH_S390X
    for (int i = 0; i < 7; i++) {
      LoadFloatingPointRegisterToStack(i, i);
    }
#else
    LoadFloatingPointRegisterToStack(0, 0);
    LoadFloatingPointRegisterToStack(1, 1);
    LoadFloatingPointRegisterToStack(2, 2);
    LoadFloatingPointRegisterToStack(3, 3);
    LoadFloatingPointRegisterToStack(5, 4);
    int offset = 5;
    for (int i = 7; i < DoubleRegister::kNumRegisters; i++, offset++) {
      LoadFloatingPointRegisterToStack(i, offset);
    }
#endif
#undef LoadFloatingPointRegisterToStack
  }

  // Clear top frame.
  mov(ip, Operand(ExternalReference(Isolate::kCEntryFPAddress, isolate())));
  StoreP(MemOperand(ip), Operand(0, kRelocInfo_NONEPTR), r0);

  // Restore current context from top and clear it in debug mode.
  if (restore_context) {
    mov(ip, Operand(ExternalReference(Isolate::kContextAddress, isolate())));
    LoadP(cp, MemOperand(ip));
  }
#ifdef DEBUG
  mov(ip, Operand(ExternalReference(Isolate::kContextAddress, isolate())));
  StoreP(MemOperand(ip), Operand(0, kRelocInfo_NONEPTR), r0);
#endif

  // Tear down the exit frame, pop the arguments, and return.
  LeaveFrame(StackFrame::EXIT);

  if (argument_count.is_valid()) {
    ShiftLeftP(argument_count, argument_count, Operand(kPointerSizeLog2));
    la(sp, MemOperand(sp, argument_count));
  }
}


void MacroAssembler::MovFromFloatResult(const DoubleRegister dst) {
  Move(dst, d0);
}


void MacroAssembler::MovFromFloatParameter(const DoubleRegister dst) {
  Move(dst, d0);
}


void MacroAssembler::InvokePrologue(const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    Handle<Code> code_constant,
                                    Register code_reg,
                                    Label* done,
                                    bool* definitely_mismatches,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  bool definitely_matches = false;
  *definitely_mismatches = false;
  Label regular_invoke;

  // Check whether the expected and actual arguments count match. If not,
  // setup registers according to contract with ArgumentsAdaptorTrampoline:
  //  r2: actual arguments count
  //  r3: function (passed through to callee)
  //  r4: expected arguments count

  // The code below is made a lot easier because the calling code already sets
  // up actual and expected registers according to the contract if values are
  // passed in registers.

  // roohack - remove these 3 checks temporarily
  //  DCHECK(actual.is_immediate() || actual.reg().is(r2));
  //  DCHECK(expected.is_immediate() || expected.reg().is(r4));
  //  DCHECK((!code_constant.is_null() && code_reg.is(no_reg))
  //          || code_reg.is(r5));

  if (expected.is_immediate()) {
    DCHECK(actual.is_immediate());
    if (expected.immediate() == actual.immediate()) {
      definitely_matches = true;
    } else {
      mov(r2, Operand(actual.immediate()));
      const int sentinel = SharedFunctionInfo::kDontAdaptArgumentsSentinel;
      if (expected.immediate() == sentinel) {
        // Don't worry about adapting arguments for builtins that
        // don't want that done. Skip adaption code by making it look
        // like we have a match between expected and actual number of
        // arguments.
        definitely_matches = true;
      } else {
        *definitely_mismatches = true;
        mov(r4, Operand(expected.immediate()));
      }
    }
  } else {
    if (actual.is_immediate()) {
      CmpPH(expected.reg(), Operand(actual.immediate()));
      beq(&regular_invoke);
      mov(r2, Operand(actual.immediate()));
    } else {
      CmpP(expected.reg(), actual.reg());
      beq(&regular_invoke);
    }
  }

  if (!definitely_matches) {
    if (!code_constant.is_null()) {
      mov(r5, Operand(code_constant));
      AddP(r5, Operand(Code::kHeaderSize - kHeapObjectTag));
    }

    Handle<Code> adaptor =
        isolate()->builtins()->ArgumentsAdaptorTrampoline();
    if (flag == CALL_FUNCTION) {
      call_wrapper.BeforeCall(CallSize(adaptor));
      Call(adaptor);
      call_wrapper.AfterCall();
      if (!*definitely_mismatches) {
        b(done);
      }
    } else {
      Jump(adaptor, RelocInfo::CODE_TARGET);
    }
    bind(&regular_invoke);
  }
}


void MacroAssembler::InvokeCode(Register code,
                                const ParameterCount& expected,
                                const ParameterCount& actual,
                                InvokeFlag flag,
                                const CallWrapper& call_wrapper) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  Label done;
  bool definitely_mismatches = false;
  InvokePrologue(expected, actual, Handle<Code>::null(), code,
                 &done, &definitely_mismatches, flag,
                 call_wrapper);
  if (!definitely_mismatches) {
    if (flag == CALL_FUNCTION) {
      call_wrapper.BeforeCall(CallSize(code));
      CallJSEntry(code);
      call_wrapper.AfterCall();
    } else {
      DCHECK(flag == JUMP_FUNCTION);
      JumpToJSEntry(code);
    }

    // Continue here if InvokePrologue does handle the invocation due to
    // mismatched parameter counts.
    bind(&done);
  }
}


void MacroAssembler::InvokeFunction(Register fun,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  // Contract with called JS functions requires that function is passed in r3.
  DCHECK(fun.is(r3));

  Register expected_reg = r4;
  Register code_reg = ip;
  LoadP(code_reg, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
  LoadP(cp, FieldMemOperand(r3, JSFunction::kContextOffset));
  LoadW(expected_reg, FieldMemOperand(code_reg,
                      SharedFunctionInfo::kFormalParameterCountOffset));
#if !defined(V8_TARGET_ARCH_S390X)
  SmiUntag(expected_reg);
#endif
  LoadP(code_reg,
        FieldMemOperand(r3, JSFunction::kCodeEntryOffset));

  ParameterCount expected(expected_reg);
  InvokeCode(code_reg, expected, actual, flag, call_wrapper);
}


void MacroAssembler::InvokeFunction(Register function,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  // You can't call a function without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  // Contract with called JS functions requires that function is passed in r3.
  DCHECK(function.is(r3));

  // Get the function and setup the context.
  LoadP(cp, FieldMemOperand(r3, JSFunction::kContextOffset));

  // We call indirectly through the code field in the function to
  // allow recompilation to take effect without changing any of the
  // call sites.
  LoadP(ip, FieldMemOperand(r3, JSFunction::kCodeEntryOffset));
  InvokeCode(ip, expected, actual, flag, call_wrapper);
}

void MacroAssembler::InvokeFunction(Handle<JSFunction> function,
                                    const ParameterCount& expected,
                                    const ParameterCount& actual,
                                    InvokeFlag flag,
                                    const CallWrapper& call_wrapper) {
  Move(r3, function);
  InvokeFunction(r3, expected, actual, flag, call_wrapper);
}

void MacroAssembler::IsObjectJSObjectType(Register heap_object,
                                          Register map,
                                          Register scratch,
                                          Label* fail) {
  LoadP(map, FieldMemOperand(heap_object, HeapObject::kMapOffset));
  IsInstanceJSObjectType(map, scratch, fail);
}


void MacroAssembler::IsInstanceJSObjectType(Register map,
                                            Register scratch,
                                            Label* fail) {
  LoadlB(scratch, FieldMemOperand(map, Map::kInstanceTypeOffset));
  CmpP(scratch, Operand(FIRST_NONCALLABLE_SPEC_OBJECT_TYPE));
  blt(fail, Label::kNear);
  CmpP(scratch, Operand(LAST_NONCALLABLE_SPEC_OBJECT_TYPE));
  bgt(fail);
}


void MacroAssembler::IsObjectJSStringType(Register object,
                                          Register scratch,
                                          Label* fail) {
  DCHECK(kNotStringTag != 0);

  LoadP(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
  LoadlB(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
  mov(r0, Operand(kIsNotStringMask));
  AndP(r0, scratch);
  bne(fail /*, cr0*/);
}


void MacroAssembler::IsObjectNameType(Register object,
                                      Register scratch,
                                      Label* fail) {
  LoadP(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
  LoadlB(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
  CmpP(scratch, Operand(LAST_NAME_TYPE));
  bgt(fail);
}


void MacroAssembler::DebugBreak() {
  LoadImmP(r2, Operand::Zero());
  mov(r3, Operand(ExternalReference(Runtime::kDebugBreak, isolate())));
  CEntryStub ces(isolate(), 1);
  DCHECK(AllowThisStubCall(&ces));
  Call(ces.GetCode(), RelocInfo::DEBUG_BREAK);
}


void MacroAssembler::PushTryHandler(StackHandler::Kind kind,
                                    int handler_index) {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 5 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kCodeOffset == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kStateOffset == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kContextOffset == 3 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kFPOffset == 4 * kPointerSize);

  // For the JSEntry handler, we must preserve r2-r6,
  //   r0,r1,r7-r9 are available.
  //
  // We want the stack to look like
  // sp -> NextOffset
  //       CodeObject
  //       state
  //       context
  //       frame pointer

  // Link the current handler as the next handler.
  mov(r7, Operand(ExternalReference(Isolate::kHandlerAddress, isolate())));

  // Buy the full stack frame for 5 slots.
  lay(sp, MemOperand(sp,  -StackHandlerConstants::kSize));

  // Copy the old handler into the next handler slot.
  mvc(MemOperand(sp, StackHandlerConstants::kNextOffset),
      MemOperand(r7), kPointerSize);
  // Set this new handler as the current one.
  StoreP(sp, MemOperand(r7));

  unsigned state =
      StackHandler::IndexField::encode(handler_index) |
      StackHandler::KindField::encode(kind);

  if (kind == StackHandler::JS_ENTRY) {
    // R7: state, R8: Context, R9: FP Offset
    LoadIntLiteral(r7, state);
    LoadImmP(r8, Operand::Zero());  // NULL frame pointer.
    // @TODO Potential Bug here as r10 is roots register.
    LoadSmiLiteral(r9, Smi::FromInt(0));    // Indicates no context.
    StoreMultipleP(r7, r9, MemOperand(sp, StackHandlerConstants::kStateOffset));
  } else {
    // still not sure if fp is right
    StoreP(fp, MemOperand(sp, StackHandlerConstants::kFPOffset));
    StoreP(cp, MemOperand(sp, StackHandlerConstants::kContextOffset));
    LoadIntLiteral(r7, state);
    StoreP(r7, MemOperand(sp, StackHandlerConstants::kStateOffset));
  }
  mov(r7, Operand(CodeObject()));
  StoreP(r7, MemOperand(sp, StackHandlerConstants::kCodeOffset));
}


void MacroAssembler::PopTryHandler() {
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  // Pop the Next Handler into r3 and store it into Handler Address reference.
  LoadP(r3, MemOperand(sp, StackHandlerConstants::kNextOffset));
  mov(ip, Operand(ExternalReference(Isolate::kHandlerAddress, isolate())));
  // Restore previous stack frame.
  lay(sp, MemOperand(sp, StackHandlerConstants::kSize));
  StoreP(r3, MemOperand(ip));
}


// Make use of ip as a temporary register
void MacroAssembler::JumpToHandlerEntry() {
  // Compute the handler entry address and jump to it.  The handler table is
  // a fixed array of (smi-tagged) code offsets.
  // r2 = exception, r3 = code object, r4 = state.
  LoadP(r5,
        FieldMemOperand(r3, Code::kHandlerTableOffset));  // Handler table.
  AddP(r5, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  srl(r4, Operand(StackHandler::kKindWidth));  // Handler index.
  LoadRR(ip, r4);
  sll(ip, Operand(kPointerSizeLog2));
  AddP(ip, r5);
  LoadP(r4, MemOperand(ip));  // Smi-tagged offset.
  AddP(r3, Operand(Code::kHeaderSize - kHeapObjectTag));  // Code start.
  SmiUntag(ip, r4);
  AddP(r0, r3, ip);
  LoadRR(r14, r0);
  Ret();
}


void MacroAssembler::Throw(Register value) {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 5 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0);
  STATIC_ASSERT(StackHandlerConstants::kCodeOffset == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kStateOffset == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kContextOffset == 3 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kFPOffset == 4 * kPointerSize);
  Label skip;

  // The exception is expected in r2.
  if (!value.is(r2)) {
    LoadRR(r2, value);
  }
  // Drop the stack pointer to the top of the top handler.
  mov(r5, Operand(ExternalReference(Isolate::kHandlerAddress, isolate())));
  LoadP(sp, MemOperand(r5));
  // Restore the next handler.
  pop(r4);
  StoreP(r4, MemOperand(r5));

  // Get the code object (r3) and state (r4).  Restore the context and frame
  // pointer.
  pop(r3);
  pop(r4);
  pop(cp);
  pop(fp);

  // If the handler is a JS frame, restore the context to the frame.
  // (kind == ENTRY) == (fp == 0) == (cp == 0), so we could test either fp
  // or cp.
  CmpP(cp, Operand::Zero());
  beq(&skip, Label::kNear);
  StoreP(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  bind(&skip);

  JumpToHandlerEntry();
}


void MacroAssembler::ThrowUncatchable(Register value) {
  // Adjust this code if not the case.
  STATIC_ASSERT(StackHandlerConstants::kSize == 5 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kNextOffset == 0 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kCodeOffset == 1 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kStateOffset == 2 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kContextOffset == 3 * kPointerSize);
  STATIC_ASSERT(StackHandlerConstants::kFPOffset == 4 * kPointerSize);

  // The exception is expected in r2.
  if (!value.is(r2)) {
    LoadRR(r2, value);
  }
  // Drop the stack pointer to the top of the top stack handler.
  mov(r5, Operand(ExternalReference(Isolate::kHandlerAddress, isolate())));
  LoadP(sp, MemOperand(r5));

  // Unwind the handlers until the ENTRY handler is found.
  Label fetch_next, check_kind;
  b(&check_kind, Label::kNear);
  bind(&fetch_next);
  LoadP(sp, MemOperand(sp, StackHandlerConstants::kNextOffset));

  bind(&check_kind);
  STATIC_ASSERT(StackHandler::JS_ENTRY == 0);
  LoadP(r4, MemOperand(sp, StackHandlerConstants::kStateOffset));
  mov(r0, Operand(StackHandler::KindField::kMask));
  AndP(r0, r4);
  bne(&fetch_next /*, cr0*/);

  // Set the top handler address to next handler past the top ENTRY handler.
  pop(r4);
  StoreP(r4, MemOperand(r5));
  // Get the code object (r3) and state (r4).  Clear the context and frame
  // pointer (0 was saved in the handler).
  pop(r3);
  pop(r4);
  pop(cp);
  pop(fp);

  JumpToHandlerEntry();
}


void MacroAssembler::CheckAccessGlobalProxy(Register holder_reg,
                                            Register scratch,
                                            Label* miss) {
  Label same_contexts;

  DCHECK(!holder_reg.is(scratch));
  DCHECK(!holder_reg.is(ip));
  DCHECK(!scratch.is(ip));

  // Load current lexical context from the stack frame.
  LoadP(scratch, MemOperand(fp, StandardFrameConstants::kContextOffset));
  // In debug mode, make sure the lexical context is set.
#ifdef DEBUG
  CmpP(scratch, Operand::Zero());
  Check(ne, kWeShouldNotHaveAnEmptyLexicalContext);
#endif

  // Load the native context of the current context.
  int offset =
      Context::kHeaderSize + Context::GLOBAL_OBJECT_INDEX * kPointerSize;
  LoadP(scratch, FieldMemOperand(scratch, offset));
  LoadP(scratch, FieldMemOperand(scratch, GlobalObject::kNativeContextOffset));

  // Check the context is a native context.
  if (emit_debug_code()) {
    // Cannot use ip as a temporary in this verification code. Due to the fact
    // that ip is clobbered as part of cmp with an object Operand.
    push(holder_reg);  // Temporarily save holder on the stack.
    // Read the first word and compare to the native_context_map.
    LoadP(holder_reg, FieldMemOperand(scratch, HeapObject::kMapOffset));
    CompareRoot(holder_reg, Heap::kNativeContextMapRootIndex);
    Check(eq, kJSGlobalObjectNativeContextShouldBeANativeContext);
    pop(holder_reg);  // Restore holder.
  }

  // Check if both contexts are the same.
  LoadP(ip, FieldMemOperand(holder_reg, JSGlobalProxy::kNativeContextOffset));
  CmpP(scratch, ip);
  beq(&same_contexts, Label::kNear);

  // Check the context is a native context.
  if (emit_debug_code()) {
    // TODO(119): avoid push(holder_reg)/pop(holder_reg)
    // Cannot use ip as a temporary in this verification code. Due to the fact
    // that ip is clobbered as part of cmp with an object Operand.
    push(holder_reg);  // Temporarily save holder on the stack.
    LoadRR(holder_reg, ip);  // Move ip to its holding place.
    CompareRoot(holder_reg, Heap::kNullValueRootIndex);
    Check(ne, kJSGlobalProxyContextShouldNotBeNull);

    LoadP(holder_reg, FieldMemOperand(holder_reg, HeapObject::kMapOffset));
    CompareRoot(holder_reg, Heap::kNativeContextMapRootIndex);
    Check(eq, kJSGlobalObjectNativeContextShouldBeANativeContext);
    // Restore ip is not needed. ip is reloaded below.
    pop(holder_reg);  // Restore holder.
    // Restore ip to holder's context.
    LoadP(ip, FieldMemOperand(holder_reg, JSGlobalProxy::kNativeContextOffset));
  }

  // Check that the security token in the calling global object is
  // compatible with the security token in the receiving global
  // object.
  int token_offset = Context::kHeaderSize +
                     Context::SECURITY_TOKEN_INDEX * kPointerSize;

  LoadP(scratch, FieldMemOperand(scratch, token_offset));
  LoadP(ip, FieldMemOperand(ip, token_offset));
  CmpP(scratch, ip);
  bne(miss);

  bind(&same_contexts);
}


// Compute the hash code from the untagged key.  This must be kept in sync with
// ComputeIntegerHash in utils.h and KeyedLoadGenericStub in
// code-stub-hydrogen.cc
void MacroAssembler::GetNumberHash(Register t0, Register scratch) {
  // First of all we assign the hash seed to scratch.
  LoadRoot(scratch, Heap::kHashSeedRootIndex);
  SmiUntag(scratch);

  // Xor original key with a seed.
  XorP(t0, scratch);

  // Compute the hash code from the untagged key.  This must be kept in sync
  // with ComputeIntegerHash in utils.h.
  //
  // hash = ~hash + (hash << 15);
  LoadRR(scratch, t0);
  NotP(scratch);
  sll(t0, Operand(15));
  AddP(t0, scratch, t0);
  // hash = hash ^ (hash >> 12);
  ShiftRight(scratch, t0, Operand(12));
  XorP(t0, scratch);
  // hash = hash + (hash << 2);
  ShiftLeft(scratch, t0, Operand(2));
  AddP(t0, t0, scratch);
  // hash = hash ^ (hash >> 4);
  ShiftRight(scratch, t0, Operand(4));
  XorP(t0, scratch);
  // hash = hash * 2057;
  LoadRR(r0, t0);
  ShiftLeft(scratch, t0, Operand(3));
  AddP(t0, t0, scratch);
  ShiftLeft(scratch, r0, Operand(11));
  AddP(t0, t0, scratch);
  // hash = hash ^ (hash >> 16);
  ShiftRight(scratch, t0, Operand(16));
  XorP(t0, scratch);
}


void MacroAssembler::LoadFromNumberDictionary(Label* miss,
                                              Register elements,
                                              Register key,
                                              Register result,
                                              Register t0,
                                              Register t1,
                                              Register t2) {
  // Register use:
  //
  // elements - holds the slow-case elements of the receiver on entry.
  //            Unchanged unless 'result' is the same register.
  //
  // key      - holds the smi key on entry.
  //            Unchanged unless 'result' is the same register.
  //
  // result   - holds the result on exit if the load succeeded.
  //            Allowed to be the same as 'key' or 'result'.
  //            Unchanged on bailout so 'key' or 'result' can be used
  //            in further computation.
  //
  // Scratch registers:
  //
  // t0 - holds the untagged key on entry and holds the hash once computed.
  //
  // t1 - used to hold the capacity mask of the dictionary
  //
  // t2 - used for the index into the dictionary.
  Label done;

  GetNumberHash(t0, t1);

  // Compute the capacity mask.
  LoadP(t1, FieldMemOperand(elements, SeededNumberDictionary::kCapacityOffset));
  SmiUntag(t1);
  SubP(t1, Operand(1));

  // Generate an unrolled loop that performs a few probes before giving up.
  for (int i = 0; i < kNumberDictionaryProbes; i++) {
    // Use t2 for index calculations and keep the hash intact in t0.
    LoadRR(t2, t0);
    // Compute the masked index: (hash + i + i * i) & mask.
    if (i > 0) {
      AddP(t2, Operand(SeededNumberDictionary::GetProbeOffset(i)));
    }
    AndP(t2, t1);

    // Scale the index by multiplying by the element size.
    DCHECK(SeededNumberDictionary::kEntrySize == 3);
    LoadRR(ip, t2);
    sll(ip, Operand(1));
    AddP(t2, ip);  // t2 = t2 * 3

    // Check if the key is identical to the name.
    sll(t2, Operand(kPointerSizeLog2));
    AddP(t2, elements);
    LoadP(ip,
          FieldMemOperand(t2, SeededNumberDictionary::kElementsStartOffset));
    CmpP(key, ip);
    if (i != kNumberDictionaryProbes - 1) {
      beq(&done, Label::kNear);
    } else {
      bne(miss);
    }
  }

  bind(&done);
  // Check that the value is a normal property.
  // t2: elements + (index * kPointerSize)
  const int kDetailsOffset =
      SeededNumberDictionary::kElementsStartOffset + 2 * kPointerSize;
  LoadP(t1, FieldMemOperand(t2, kDetailsOffset));
  LoadSmiLiteral(ip, Smi::FromInt(PropertyDetails::TypeField::kMask));
  AndP(r0, ip, t1/*, SetRC*/);  // Should be okay to remove rc
  bne(miss /*, cr0*/);

  // Get the value at the masked, scaled index and return.
  const int kValueOffset =
      SeededNumberDictionary::kElementsStartOffset + kPointerSize;
  LoadP(result, FieldMemOperand(t2, kValueOffset));
}


void MacroAssembler::Allocate(int object_size,
                              Register result,
                              Register scratch1,
                              Register scratch2,
                              Label* gc_required,
                              AllocationFlags flags) {
  DCHECK(object_size <= Page::kMaxRegularHeapObjectSize);
  if (!FLAG_inline_new) {
    if (emit_debug_code()) {
      // Trash the registers to simulate an allocation failure.
      LoadImmP(result, Operand(0x7091));
      LoadImmP(scratch1, Operand(0x7191));
      LoadImmP(scratch2, Operand(0x7291));
    }
    b(gc_required);
    return;
  }

  DCHECK(!result.is(scratch1));
  DCHECK(!result.is(scratch2));
  DCHECK(!scratch1.is(scratch2));
  DCHECK(!scratch1.is(ip));
  DCHECK(!scratch2.is(ip));

  // Make object size into bytes.
  if ((flags & SIZE_IN_WORDS) != 0) {
    object_size *= kPointerSize;
  }
  DCHECK_EQ(0, static_cast<int>(object_size & kObjectAlignmentMask));

  // Check relative positions of allocation top and limit addresses.
  ExternalReference allocation_top =
      AllocationUtils::GetAllocationTopReference(isolate(), flags);
  ExternalReference allocation_limit =
      AllocationUtils::GetAllocationLimitReference(isolate(), flags);
  intptr_t top   =
      reinterpret_cast<intptr_t>(allocation_top.address());
  intptr_t limit =
      reinterpret_cast<intptr_t>(allocation_limit.address());
  DCHECK((limit - top) == kPointerSize);

  // Set up allocation top address and object size registers.
  Register topaddr = scratch1;
  mov(topaddr, Operand(allocation_top));

  intptr_t limitOffset = 0;
  if ((flags & RESULT_CONTAINS_TOP) == 0) {
    // Load allocation top into result
    LoadP(result, MemOperand(topaddr));
    limitOffset = kPointerSize;
  } else {
    if (emit_debug_code()) {
      // Assert that result actually contains top on entry.
      CmpP(result, MemOperand(topaddr));
      Check(eq, kUnexpectedAllocationTop);
    }
    // Result already contains allocation top.
    limitOffset = limit - top;
  }
  MemOperand limitMemOperand = MemOperand(topaddr, limitOffset);

  if ((flags & DOUBLE_ALIGNMENT) != 0) {
    // Align the next allocation. Storing the filler map without checking top is
    // safe in new-space because the limit of the heap is aligned there.
    DCHECK((flags & PRETENURE_OLD_POINTER_SPACE) == 0);
#if V8_TARGET_ARCH_S390X
    STATIC_ASSERT(kPointerAlignment == kDoubleAlignment);
#else
    STATIC_ASSERT(kPointerAlignment * 2 == kDoubleAlignment);
    AndP(scratch2, result, Operand(kDoubleAlignmentMask));
    Label aligned;
    beq(&aligned, Label::kNear);
    if ((flags & PRETENURE_OLD_DATA_SPACE) != 0) {
      CmpLogicalP(result, limitMemOperand);
      bge(gc_required);
    }
    mov(scratch2, Operand(isolate()->factory()->one_pointer_filler_map()));
    StoreW(scratch2, MemOperand(result));
    la(result, MemOperand(result, kDoubleSize / 2));
    bind(&aligned);
#endif
  }

  // Calculate new top and bail out if new space is exhausted. Use result
  // to calculate the new top.
  AddP(scratch2, result, Operand(object_size));
  b(Condition(CC_OF), gc_required);  // Detect overflow
  CmpLogicalP(scratch2, limitMemOperand);
  bgt(gc_required);
  StoreP(scratch2, MemOperand(topaddr));

  // Tag object if requested.
  if ((flags & TAG_OBJECT) != 0) {
    la(result, MemOperand(result, kHeapObjectTag));
  }
}


void MacroAssembler::Allocate(Register object_size,
                              Register result,
                              Register scratch1,
                              Register scratch2,
                              Label* gc_required,
                              AllocationFlags flags) {
  if (!FLAG_inline_new) {
    if (emit_debug_code()) {
      // Trash the registers to simulate an allocation failure.
      LoadImmP(result, Operand(0x7091));
      LoadImmP(scratch1, Operand(0x7191));
      LoadImmP(scratch2, Operand(0x7291));
    }
    b(gc_required);
    return;
  }

  // Assert that the register arguments are different and that none of
  // them are ip. ip is used explicitly in the code generated below.
  DCHECK(!result.is(scratch1));
  DCHECK(!result.is(scratch2));
  DCHECK(!scratch1.is(scratch2));
  DCHECK(!object_size.is(ip));
  DCHECK(!result.is(ip));
  DCHECK(!scratch1.is(ip));
  DCHECK(!scratch2.is(ip));

  // Check relative positions of allocation top and limit addresses.
  ExternalReference allocation_top =
      AllocationUtils::GetAllocationTopReference(isolate(), flags);
  ExternalReference allocation_limit =
      AllocationUtils::GetAllocationLimitReference(isolate(), flags);

  intptr_t top =
      reinterpret_cast<intptr_t>(allocation_top.address());
  intptr_t limit =
      reinterpret_cast<intptr_t>(allocation_limit.address());
  DCHECK((limit - top) == kPointerSize);

  // Set up allocation top address.
  Register topaddr = scratch1;
  mov(topaddr, Operand(allocation_top));

  intptr_t limitOffset = 0;
  if ((flags & RESULT_CONTAINS_TOP) == 0) {
    // Load allocation top into result and allocation limit into ip.
    LoadP(result, MemOperand(topaddr));
    limitOffset = kPointerSize;
  } else {
    if (emit_debug_code()) {
      // Assert that result actually contains top on entry.
      CmpP(result, MemOperand(topaddr));
      Check(eq, kUnexpectedAllocationTop);
    }
    // Result already contains allocation top.
    limitOffset = limit - top;
  }
  MemOperand limitMemOperand = MemOperand(topaddr, limitOffset);

  if ((flags & DOUBLE_ALIGNMENT) != 0) {
    // Align the next allocation. Storing the filler map without checking top is
    // safe in new-space because the limit of the heap is aligned there.
    DCHECK((flags & PRETENURE_OLD_POINTER_SPACE) == 0);
#if V8_TARGET_ARCH_S390X
    STATIC_ASSERT(kPointerAlignment == kDoubleAlignment);
#else
    STATIC_ASSERT(kPointerAlignment * 2 == kDoubleAlignment);
    AndP(scratch2, result, Operand(kDoubleAlignmentMask));
    Label aligned;
    beq(&aligned, Label::kNear);
    if ((flags & PRETENURE_OLD_DATA_SPACE) != 0) {
      CmpLogicalP(result, limitMemOperand);
      bge(gc_required);
    }
    mov(scratch2, Operand(isolate()->factory()->one_pointer_filler_map()));
    StoreW(scratch2, MemOperand(result));
    la(result, MemOperand(result, kDoubleSize / 2));
    bind(&aligned);
#endif
  }

  // Calculate new top and bail out if new space is exhausted. Use result
  // to calculate the new top. Object size may be in words so a shift is
  // required to get the number of bytes.
  if ((flags & SIZE_IN_WORDS) != 0) {
    ShiftLeftP(scratch2, object_size, Operand(kPointerSizeLog2));
    AddP(scratch2, result);
  } else {
    AddP(scratch2, result, object_size);
  }
  b(Condition(CC_OF), gc_required);
  CmpLogicalP(scratch2, limitMemOperand);
  bgt(gc_required);

  // Update allocation top. result temporarily holds the new top.
  if (emit_debug_code()) {
    AndP(r0, scratch2, Operand(kObjectAlignmentMask));
    Check(eq, kUnalignedAllocationInNewSpace);
  }
  StoreP(scratch2, MemOperand(topaddr));

  // Tag object if requested.
  if ((flags & TAG_OBJECT) != 0) {
    la(result, MemOperand(result, kHeapObjectTag));
  }
}


void MacroAssembler::UndoAllocationInNewSpace(Register object,
                                              Register scratch) {
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address(isolate());

  // Make sure the object has no tag before resetting top.
  ClearRightImm(object, object, Operand(kHeapObjectTagSize));
  // was.. and_(object, object, Operand(~kHeapObjectTagMask));
#ifdef DEBUG
  // Check that the object un-allocated is below the current top.
  mov(scratch, Operand(new_space_allocation_top));
  LoadP(scratch, MemOperand(scratch));
  CmpP(object, scratch);
  Check(lt, kUndoAllocationOfNonAllocatedMemory);
#endif
  // Write the address of the object to un-allocate as the current top.
  mov(scratch, Operand(new_space_allocation_top));
  StoreP(object, MemOperand(scratch));
}


void MacroAssembler::AllocateTwoByteString(Register result,
                                           Register length,
                                           Register scratch1,
                                           Register scratch2,
                                           Register scratch3,
                                           Label* gc_required) {
  // Calculate the number of bytes needed for the characters in the string while
  // observing object alignment.
  DCHECK((SeqTwoByteString::kHeaderSize & kObjectAlignmentMask) == 0);

  ShiftLeft(scratch1, length, Operand(1));  // Length in bytes, not chars.
  AddP(scratch1,
       Operand(kObjectAlignmentMask + SeqTwoByteString::kHeaderSize));

  AndP(scratch1, Operand(~kObjectAlignmentMask));

  // Allocate two-byte string in new space.
  Allocate(scratch1,
           result,
           scratch2,
           scratch3,
           gc_required,
           TAG_OBJECT);

  // Set the map, length and hash field.
  InitializeNewString(result,
                      length,
                      Heap::kStringMapRootIndex,
                      scratch1,
                      scratch2);
}


void MacroAssembler::AllocateAsciiString(Register result,
                                         Register length,
                                         Register scratch1,
                                         Register scratch2,
                                         Register scratch3,
                                         Label* gc_required) {
  // Calculate the number of bytes needed for the characters in the string while
  // observing object alignment.
  DCHECK((SeqOneByteString::kHeaderSize & kObjectAlignmentMask) == 0);
  DCHECK(kCharSize == 1);
  AddP(scratch1, length,
                 Operand(kObjectAlignmentMask + SeqOneByteString::kHeaderSize));
  AndP(scratch1, Operand(~kObjectAlignmentMask));

  // Allocate ASCII string in new space.
  Allocate(scratch1,
           result,
           scratch2,
           scratch3,
           gc_required,
           TAG_OBJECT);

  // Set the map, length and hash field.
  InitializeNewString(result,
                      length,
                      Heap::kAsciiStringMapRootIndex,
                      scratch1,
                      scratch2);
}


void MacroAssembler::AllocateTwoByteConsString(Register result,
                                               Register length,
                                               Register scratch1,
                                               Register scratch2,
                                               Label* gc_required) {
  Allocate(ConsString::kSize, result, scratch1, scratch2, gc_required,
           TAG_OBJECT);

  InitializeNewString(result,
                      length,
                      Heap::kConsStringMapRootIndex,
                      scratch1,
                      scratch2);
}


void MacroAssembler::AllocateAsciiConsString(Register result,
                                             Register length,
                                             Register scratch1,
                                             Register scratch2,
                                             Label* gc_required) {
  Allocate(ConsString::kSize,
           result,
           scratch1,
           scratch2,
           gc_required,
           TAG_OBJECT);

  InitializeNewString(result,
                      length,
                      Heap::kConsAsciiStringMapRootIndex,
                      scratch1,
                      scratch2);
}


void MacroAssembler::AllocateTwoByteSlicedString(Register result,
                                                 Register length,
                                                 Register scratch1,
                                                 Register scratch2,
                                                 Label* gc_required) {
  Allocate(SlicedString::kSize, result, scratch1, scratch2, gc_required,
           TAG_OBJECT);

  InitializeNewString(result,
                      length,
                      Heap::kSlicedStringMapRootIndex,
                      scratch1,
                      scratch2);
}


void MacroAssembler::AllocateAsciiSlicedString(Register result,
                                               Register length,
                                               Register scratch1,
                                               Register scratch2,
                                               Label* gc_required) {
  Allocate(SlicedString::kSize, result, scratch1, scratch2, gc_required,
           TAG_OBJECT);

  InitializeNewString(result,
                      length,
                      Heap::kSlicedAsciiStringMapRootIndex,
                      scratch1,
                      scratch2);
}


void MacroAssembler::CompareObjectType(Register object,
                                       Register map,
                                       Register type_reg,
                                       InstanceType type) {
  const Register temp = type_reg.is(no_reg) ? r0 : type_reg;

  LoadP(map, FieldMemOperand(object, HeapObject::kMapOffset));
  CompareInstanceType(map, temp, type);
}


void MacroAssembler::CheckObjectTypeRange(Register object,
                                          Register map,
                                          InstanceType min_type,
                                          InstanceType max_type,
                                          Label* false_label) {
  STATIC_ASSERT(Map::kInstanceTypeOffset < 4096);
  STATIC_ASSERT(LAST_TYPE < 256);
  LoadP(map, FieldMemOperand(object, HeapObject::kMapOffset));
  LoadlB(ip, FieldMemOperand(map, Map::kInstanceTypeOffset));
  SubP(ip, Operand(min_type));
  CmpLogicalP(ip, Operand(max_type - min_type));
  bgt(false_label);
}


void MacroAssembler::CompareInstanceType(Register map,
                                         Register type_reg,
                                         InstanceType type) {
  STATIC_ASSERT(Map::kInstanceTypeOffset < 4096);
  STATIC_ASSERT(LAST_TYPE < 256);
  LoadlB(type_reg, FieldMemOperand(map, Map::kInstanceTypeOffset));
  CmpP(type_reg, Operand(type));
}


void MacroAssembler::CompareRoot(Register obj,
                                 Heap::RootListIndex index) {
  CmpP(obj, MemOperand(kRootRegister, index << kPointerSizeLog2));
}


void MacroAssembler::CheckFastElements(Register map,
                                       Register scratch,
                                       Label* fail) {
  STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
  STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
  STATIC_ASSERT(FAST_ELEMENTS == 2);
  STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
  STATIC_ASSERT(Map::kMaximumBitField2FastHoleyElementValue < 0x8000);
  CmpLogicalByte(FieldMemOperand(map, Map::kBitField2Offset),
      Operand(Map::kMaximumBitField2FastHoleyElementValue));
  bgt(fail);
}


void MacroAssembler::CheckFastObjectElements(Register map,
                                             Register scratch,
                                             Label* fail) {
  STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
  STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
  STATIC_ASSERT(FAST_ELEMENTS == 2);
  STATIC_ASSERT(FAST_HOLEY_ELEMENTS == 3);
  CmpLogicalByte(FieldMemOperand(map, Map::kBitField2Offset),
      Operand(Map::kMaximumBitField2FastHoleySmiElementValue));
  ble(fail);
  CmpLogicalByte(FieldMemOperand(map, Map::kBitField2Offset),
      Operand(Map::kMaximumBitField2FastHoleyElementValue));
  bgt(fail);
}


void MacroAssembler::CheckFastSmiElements(Register map,
                                          Register scratch,
                                          Label* fail) {
  STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
  STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
  CmpLogicalByte(FieldMemOperand(map, Map::kBitField2Offset),
      Operand(Map::kMaximumBitField2FastHoleySmiElementValue));
  bgt(fail);
}


void MacroAssembler::SmiToDouble(DoubleRegister value, Register smi) {
  SmiUntag(ip, smi);
  ConvertIntToDouble(ip, value);
}

void MacroAssembler::StoreNumberToDoubleElements(
                                      Register value_reg,
                                      Register key_reg,
                                      Register elements_reg,
                                      Register scratch1,
                                      DoubleRegister double_scratch,
                                      Label* fail,
                                      int elements_offset) {
  Label smi_value, store;

  // Handle smi values specially.
  JumpIfSmi(value_reg, &smi_value);

  // Ensure that the object is a heap number
  CheckMap(value_reg,
           scratch1,
           isolate()->factory()->heap_number_map(),
           fail,
           DONT_DO_SMI_CHECK);

  LoadF(double_scratch, FieldMemOperand(value_reg, HeapNumber::kValueOffset));
  // Force a canonical NaN.
  CanonicalizeNaN(double_scratch);
  b(&store);

  bind(&smi_value);
  SmiToDouble(double_scratch, value_reg);

  bind(&store);
  SmiToDoubleArrayOffset(scratch1, key_reg);
  StoreF(double_scratch,
       FieldMemOperand(elements_reg, scratch1,
                       FixedDoubleArray::kHeaderSize - elements_offset));
}

void MacroAssembler::AddAndCheckForOverflow(Register dst,
                                            Register left,
                                            Register right,
                                            Register overflow_dst,
                                            Register scratch) {
  DCHECK(!dst.is(overflow_dst));
  DCHECK(!dst.is(scratch));
  DCHECK(!overflow_dst.is(scratch));
  DCHECK(!overflow_dst.is(left));
  DCHECK(!overflow_dst.is(right));

  // C = A+B; C overflows if A/B have same sign and C has diff sign than A
  if (dst.is(left)) {
    LoadRR(scratch, left);            // Preserve left.
    AddP(dst, left, right);            // Left is overwritten.
    XorP(scratch, dst);               // Original left.
    XorP(overflow_dst, dst, right);
    AndP(overflow_dst, scratch/*, SetRC*/);
    LoadAndTestRR(overflow_dst, overflow_dst);
    // Should be okay to remove rc
  } else if (dst.is(right)) {
    LoadRR(scratch, right);           // Preserve right.
    AddP(dst, left, right);            // Right is overwritten.
    XorP(scratch, dst);               // Original right.
    XorP(overflow_dst, dst, left);
    AndP(overflow_dst, scratch/*, SetRC*/);
    LoadAndTestRR(overflow_dst, overflow_dst);
    // Should be okay to remove rc
  } else {
    AddP(dst, left, right);
    XorP(overflow_dst, dst, left);
    XorP(scratch, dst, right);
    AndP(overflow_dst, scratch/*, SetRC*/);
    LoadAndTestRR(overflow_dst, overflow_dst);
  }
}


void MacroAssembler::AddAndCheckForOverflow(Register dst,
                                            Register left,
                                            intptr_t right,
                                            Register overflow_dst,
                                            Register scratch) {
  Register original_left = left;
  DCHECK(!dst.is(overflow_dst));
  DCHECK(!dst.is(scratch));
  DCHECK(!overflow_dst.is(scratch));
  DCHECK(!overflow_dst.is(left));

  // C = A+B; C overflows if A/B have same sign and C has diff sign than A
  if (dst.is(left)) {
    // Preserve left.
    original_left = overflow_dst;
    LoadRR(original_left, left);
  }
  AddP(dst, left, Operand(right));
  XorP(overflow_dst, dst, original_left);
  if (right >= 0) {
    AndP(overflow_dst, overflow_dst, dst);
  } else {
    DCHECK(0);
    // andc(overflow_dst, overflow_dst, dst, SetRC);
    AndP(overflow_dst, overflow_dst, dst);
  }
}


void MacroAssembler::SubAndCheckForOverflow(Register dst,
                                            Register left,
                                            Register right,
                                            Register overflow_dst,
                                            Register scratch) {
  DCHECK(!dst.is(overflow_dst));
  DCHECK(!dst.is(scratch));
  DCHECK(!overflow_dst.is(scratch));
  DCHECK(!overflow_dst.is(left));
  DCHECK(!overflow_dst.is(right));

  // C = A-B; C overflows if A/B have diff signs and C has diff sign than A
  if (dst.is(left)) {
    LoadRR(scratch, left);            // Preserve left.
    SubP(dst, left, right);           // Left is overwritten.
    XorP(overflow_dst, dst, scratch);
    XorP(scratch, right);
    AndP(overflow_dst, scratch/*, SetRC*/);
    LoadAndTestRR(overflow_dst, overflow_dst);
    // Should be okay to remove rc
  } else if (dst.is(right)) {
    LoadRR(scratch, right);           // Preserve right.
    SubP(dst, left, right);           // Right is overwritten.
    XorP(overflow_dst, dst, left);
    XorP(scratch, left);
    AndP(overflow_dst, scratch/*, SetRC*/);
    LoadAndTestRR(overflow_dst, overflow_dst);
    // Should be okay to remove rc
  } else {
    SubP(dst, left, right);
    XorP(overflow_dst, dst, left);
    XorP(scratch, left, right);
    AndP(overflow_dst, scratch/*, SetRC*/);
    LoadAndTestRR(overflow_dst, overflow_dst);
    // Should be okay to remove rc
  }
}


void MacroAssembler::CompareMap(Register obj,
                                Register scratch,
                                Handle<Map> map,
                                Label* early_success) {
  LoadP(scratch, FieldMemOperand(obj, HeapObject::kMapOffset));
  CompareMap(obj, map, early_success);
}


void MacroAssembler::CompareMap(Register obj,
                                Handle<Map> map,
                                Label* early_success) {
  mov(r0, Operand(map));
  CmpP(r0, FieldMemOperand(obj, HeapObject::kMapOffset));
}


void MacroAssembler::CheckMap(Register obj,
                              Register scratch,
                              Handle<Map> map,
                              Label* fail,
                              SmiCheckType smi_check_type) {
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, fail);
  }

  Label success;
  CompareMap(obj, scratch, map, &success);
  bne(fail);
  bind(&success);
}


void MacroAssembler::CheckMap(Register obj,
                              Register scratch,
                              Heap::RootListIndex index,
                              Label* fail,
                              SmiCheckType smi_check_type) {
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, fail);
  }
  LoadP(scratch, FieldMemOperand(obj, HeapObject::kMapOffset));
  CompareRoot(scratch, index);
  bne(fail);
}


void MacroAssembler::DispatchMap(Register obj,
                                 Register scratch,
                                 Handle<Map> map,
                                 Handle<Code> success,
                                 SmiCheckType smi_check_type) {
  Label fail;
  if (smi_check_type == DO_SMI_CHECK) {
    JumpIfSmi(obj, &fail);
  }
  LoadP(scratch, FieldMemOperand(obj, HeapObject::kMapOffset));
  CmpP(scratch, Operand(map));
  bne(&fail);
  Jump(success, RelocInfo::CODE_TARGET, al);
  bind(&fail);
}


void MacroAssembler::TryGetFunctionPrototype(Register function,
                                             Register result,
                                             Register scratch,
                                             Label* miss,
                                             bool miss_on_bound_function) {
  Label non_instance;
  if (miss_on_bound_function) {
    // Check that the receiver isn't a smi.
    JumpIfSmi(function, miss);

    // Check that the function really is a function.  Load map into result reg.
    CompareObjectType(function, result, scratch, JS_FUNCTION_TYPE);
    bne(miss);

    LoadP(scratch,
          FieldMemOperand(function, JSFunction::kSharedFunctionInfoOffset));
    LoadlW(scratch,
           FieldMemOperand(scratch, SharedFunctionInfo::kCompilerHintsOffset));
    TestBit(scratch,
#if V8_TARGET_ARCH_S390X
            SharedFunctionInfo::kBoundFunction,
#else
            SharedFunctionInfo::kBoundFunction + kSmiTagSize,
#endif
            r0);
    bne(miss /*, cr0*/);

    // Make sure that the function has an instance prototype.
    LoadlB(scratch, FieldMemOperand(result, Map::kBitFieldOffset));
    AndP(r0, scratch, Operand(1 << Map::kHasNonInstancePrototype));
    bne(&non_instance, Label::kNear);
  }

  // Get the prototype or initial map from the function.
  LoadP(result,
        FieldMemOperand(function, JSFunction::kPrototypeOrInitialMapOffset));

  // If the prototype or initial map is the hole, don't return it and
  // simply miss the cache instead. This will allow us to allocate a
  // prototype object on-demand in the runtime system.
  CompareRoot(result, Heap::kTheHoleValueRootIndex);
  beq(miss);

  // If the function does not have an initial map, we're done.
  Label done;
  CompareObjectType(result, scratch, scratch, MAP_TYPE);
  bne(&done, Label::kNear);

  // Get the prototype from the initial map.
  LoadP(result, FieldMemOperand(result, Map::kPrototypeOffset));

  if (miss_on_bound_function) {
    b(&done, Label::kNear);

    // Non-instance prototype: Fetch prototype from constructor field
    // in initial map.
    bind(&non_instance);
    LoadP(result, FieldMemOperand(result, Map::kConstructorOffset));
  }

  // All done.
  bind(&done);
}


void MacroAssembler::CallStub(CodeStub* stub,
                              TypeFeedbackId ast_id,
                              Condition cond) {
  DCHECK(AllowThisStubCall(stub));  // Stub calls are not allowed in some stubs.
  Call(stub->GetCode(), RelocInfo::CODE_TARGET, ast_id, cond);
}


void MacroAssembler::TailCallStub(CodeStub* stub, Condition cond) {
    Jump(stub->GetCode(), RelocInfo::CODE_TARGET, cond);
}


static int AddressOffset(ExternalReference ref0, ExternalReference ref1) {
  return ref0.address() - ref1.address();
}


void MacroAssembler::CallApiFunctionAndReturn(
    Register function_address,
    ExternalReference thunk_ref,
    int stack_space,
    MemOperand return_value_operand,
    MemOperand* context_restore_operand) {
  ExternalReference next_address =
    ExternalReference::handle_scope_next_address(isolate());
  const int kNextOffset = 0;
  const int kLimitOffset = AddressOffset(
    ExternalReference::handle_scope_limit_address(isolate()),
    next_address);
  const int kLevelOffset = AddressOffset(
    ExternalReference::handle_scope_level_address(isolate()),
    next_address);

  // Additional parameter is the address of the actual callback.
  DCHECK(function_address.is(r3) || function_address.is(r4));
  Register scratch = r5;
  mov(scratch, Operand(ExternalReference::is_profiling_address(isolate())));
  LoadlB(scratch, MemOperand(scratch, 0));
  CmpP(scratch, Operand::Zero());

  Label profiler_disabled;
  Label end_profiler_check;
  beq(&profiler_disabled, Label::kNear);
  mov(scratch, Operand(thunk_ref));
  b(&end_profiler_check, Label::kNear);
  bind(&profiler_disabled);
  LoadRR(scratch, function_address);
  bind(&end_profiler_check);

  // Allocate HandleScope in callee-save registers.
  // r9 - next_address
  // r6(linux) / r14(zOS) - next_address->kNextOffset
  // r7 - next_address->kLimitOffset
  // r8 - next_address->kLevelOffset
#ifdef V8_OS_ZOS
  Register prev_next_ = r14;
#else
  Register prev_next_ = r6;
#endif
  mov(r9, Operand(next_address));
  LoadP(prev_next_, MemOperand(r9, kNextOffset));
  LoadP(r7, MemOperand(r9, kLimitOffset));
  LoadlW(r8, MemOperand(r9, kLevelOffset));
  AddP(r8, Operand(1));
  StoreW(r8, MemOperand(r9, kLevelOffset));

  if (FLAG_log_timer_events) {
    FrameScope frame(this, StackFrame::MANUAL);
    PushSafepointRegisters();
    PrepareCallCFunction(1, r2);
    mov(r2, Operand(ExternalReference::isolate_address(isolate())));
    CallCFunction(ExternalReference::log_enter_external_function(isolate()), 1);
    PopSafepointRegisters();
  }

  // Native call returns to the DirectCEntry stub which redirects to the
  // return address pushed on stack (could have moved after GC).
  // DirectCEntry stub itself is generated early and never moves.
#ifdef V8_OS_ZOS
  // Shuffle the arguments from Linux arg registers to XPLINK arg regs.
  // Reassign stack pointer to r4.
  LoadRR(r1, r2);
  if (function_address.is(r3)) {
    LoadRR(r2, r3);
  } else {
    LoadRR(r2, r3);
    LoadRR(r3, r4);
  }
  lay(r4, MemOperand(sp, -(kStackPointerBias + 18 * kPointerSize)));
  LoadRR(r10, r7);  // Clobbered root register.
#endif

  DirectCEntryStub stub(isolate());
  stub.GenerateCall(this, scratch);
#ifdef V8_OS_ZOS
  LoadRR(r7, r10);
  InitializeRootRegister();
#endif
  if (FLAG_log_timer_events) {
    FrameScope frame(this, StackFrame::MANUAL);
    PushSafepointRegisters();
    PrepareCallCFunction(1, r2);
    mov(r2, Operand(ExternalReference::isolate_address(isolate())));
    CallCFunction(ExternalReference::log_leave_external_function(isolate()), 1);
    PopSafepointRegisters();
  }

  Label promote_scheduled_exception;
  Label exception_handled;
  Label delete_allocated_handles;
  Label leave_exit_frame;
  Label return_value_loaded;

  // load value from ReturnValue
  LoadP(r2, return_value_operand);
  bind(&return_value_loaded);
  // No more valid handles (the result handle was the last one). Restore
  // previous handle scope.
  StoreP(prev_next_, MemOperand(r9, kNextOffset));
  if (emit_debug_code()) {
    LoadlW(r3, MemOperand(r9, kLevelOffset));
    CmpP(r3, r8);
    Check(eq, kUnexpectedLevelAfterReturnFromApiCall);
  }
  SubP(r8, Operand(1));
  StoreW(r8, MemOperand(r9, kLevelOffset));

  CmpP(r7, MemOperand(r9, kLimitOffset));
  bne(&delete_allocated_handles, Label::kNear);

  // Check if the function scheduled an exception.
  bind(&leave_exit_frame);
  mov(r7, Operand(ExternalReference::scheduled_exception_address(isolate())));
  LoadP(r7, MemOperand(r7));
  CompareRoot(r7, Heap::kTheHoleValueRootIndex);
  bne(&promote_scheduled_exception, Label::kNear);

  bind(&exception_handled);

  bool restore_context = context_restore_operand != NULL;
  if (restore_context) {
    LoadP(cp, *context_restore_operand);
  }
  // LeaveExitFrame expects unwind space to be in a register.
  mov(r6, Operand(stack_space));
  LeaveExitFrame(false, r6, !restore_context);
  Ret();

  bind(&promote_scheduled_exception);
  {
    FrameScope frame(this, StackFrame::INTERNAL);
    CallExternalReference(
        ExternalReference(Runtime::kPromoteScheduledException, isolate()),
        0);
  }
  jmp(&exception_handled);

  // HandleScope limit has changed. Delete allocated extensions.
  bind(&delete_allocated_handles);
  StoreP(r7, MemOperand(r9, kLimitOffset));
#ifndef V8_OS_ZOS  
  LoadRR(r6, r2);
#endif
  PrepareCallCFunction(1, r7);
#ifdef V8_OS_ZOS
  mov(r1, Operand(ExternalReference::isolate_address(isolate())));
#else
  mov(r2, Operand(ExternalReference::isolate_address(isolate())));
#endif 
  CallCFunction(
      ExternalReference::delete_handle_scope_extensions(isolate()), 1);
#ifndef V8_OS_ZOS  
  LoadRR(r2, r6);
#endif 
  b(&leave_exit_frame);
}


bool MacroAssembler::AllowThisStubCall(CodeStub* stub) {
  return has_frame_ || !stub->SometimesSetsUpAFrame();
}


void MacroAssembler::IndexFromHash(Register hash, Register index) {
  // If the hash field contains an array index pick it out. The assert checks
  // that the constants for the maximum number of digits for an array index
  // cached in the hash field and the number of bits reserved for it does not
  // conflict.
  DCHECK(TenToThe(String::kMaxCachedArrayIndexLength) <
         (1 << String::kArrayIndexValueBits));
  DecodeFieldToSmi<String::ArrayIndexValueBits>(index, hash);
}



void MacroAssembler::TestDoubleIsInt32(DoubleRegister double_input,
                                       Register scratch1,
                                       Register scratch2,
                                       DoubleRegister double_scratch) {
  TryDoubleToInt32Exact(scratch1, double_input, scratch2, double_scratch);
}


void MacroAssembler::TryDoubleToInt32Exact(Register result,
                                           DoubleRegister double_input,
                                           Register,
                                           DoubleRegister double_scratch) {
  Label done;
  DCHECK(!double_input.is(double_scratch));

  ConvertDoubleToInt64(double_input, result);

  TestIfInt32(result, r0);
  bne(&done, Label::kNear);

  // convert back and compare
  cdfbr(double_scratch, result);
  cdbr(double_scratch, double_input);
  bind(&done);
}

void MacroAssembler::TryInt32Floor(Register result,
                                   DoubleRegister double_input,
                                   Register input_gpr64,
                                   Register,
                                   DoubleRegister double_scratch,
                                   Label* done,
                                   Label* exact) {
  DCHECK(!result.is(input_gpr64));
  DCHECK(!double_input.is(double_scratch));
  Label exception;

  // Move double input into input_gpr64
  lgdr(input_gpr64, double_input);

  // Test for NaN/Inf
  DCHECK(0x7ff00000u == HeapNumber::kExponentMask);
  ExtractBitRange(result, input_gpr64, 62, 52);
  CmpLogicalP(result, Operand(0x7ff));
  beq(&exception, Label::kNear);

  // Convert (rounding to -Inf)
  ConvertDoubleToInt64(double_input, result, kRoundToMinusInf);

  // Test for overflow
  TestIfInt32(result, r0);
  bne(&exception, Label::kNear);

  // Test for exactness
  cdfbr(double_scratch, result);
  cdbr(double_scratch, double_input);
  beq(exact);
  b(done);

  bind(&exception);
}

void MacroAssembler::TryInlineTruncateDoubleToI(Register result,
                                                DoubleRegister double_input,
                                                Label* done) {
  ConvertDoubleToInt64(double_input, result);
  // Test for overflow
  TestIfInt32(result, r0);
  beq(done);
}

void MacroAssembler::TruncateDoubleToI(Register result,
                                       DoubleRegister double_input) {
  Label done;

  TryInlineTruncateDoubleToI(result, double_input, &done);

  // If we fell through then inline version didn't succeed - call stub instead.
  push(r14);
  // Put input on stack.
  StoreF(double_input, MemOperand(sp, -kDoubleSize));
  lay(sp, MemOperand(sp, -kDoubleSize));

  DoubleToIStub stub(isolate(), sp, result, 0, true, true);
  CallStub(&stub);

  la(sp, MemOperand(sp, kDoubleSize));
  pop(r14);

  bind(&done);
}


void MacroAssembler::TruncateHeapNumberToI(Register result,
                                           Register object) {
  Label done;
  DoubleRegister double_scratch = kScratchDoubleReg;
  DCHECK(!result.is(object));

  LoadF(double_scratch, FieldMemOperand(object, HeapNumber::kValueOffset));
  TryInlineTruncateDoubleToI(result, double_scratch, &done);

  // If we fell through then inline version didn't succeed - call stub instead.
  push(r14);
  DoubleToIStub stub(isolate(),
                     object,
                     result,
                     HeapNumber::kValueOffset - kHeapObjectTag,
                     true,
                     true);
  CallStub(&stub);
  pop(r14);

  bind(&done);
}


void MacroAssembler::TruncateNumberToI(Register object,
                                       Register result,
                                       Register heap_number_map,
                                       Register scratch1,
                                       Label* not_number) {
  Label done;
  DCHECK(!result.is(object));

  UntagAndJumpIfSmi(result, object, &done);
  JumpIfNotHeapNumber(object, heap_number_map, scratch1, not_number);
  TruncateHeapNumberToI(result, object);

  bind(&done);
}


void MacroAssembler::GetLeastBitsFromSmi(Register dst,
                                         Register src,
                                         int num_least_bits) {
  if (CpuFeatures::IsSupported(GENERAL_INSTR_EXT)) {
    // We rotate by kSmiShift amount, and extract the num_least_bits
    risbg(dst, src, Operand(64 - num_least_bits), Operand(63),
                    Operand(64 - kSmiShift), true);
  } else {
    SmiUntag(dst, src);
    AndP(dst, Operand((1 << num_least_bits) - 1));
  }
}


void MacroAssembler::GetLeastBitsFromInt32(Register dst,
                                           Register src,
                                           int num_least_bits) {
  AndP(dst, src, Operand((1 << num_least_bits) - 1));
}


void MacroAssembler::CallRuntime(const Runtime::Function* f,
                                 int num_arguments,
                                 SaveFPRegsMode save_doubles) {
  // All parameters are on the stack.  r2 has the return value after call.

  // If the expected number of arguments of the runtime function is
  // constant, we check that the actual number of arguments match the
  // expectation.
  CHECK(f->nargs < 0 || f->nargs == num_arguments);

  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  mov(r2, Operand(num_arguments));
  mov(r3, Operand(ExternalReference(f, isolate())));
  CEntryStub stub(isolate(),
#if V8_TARGET_ARCH_S390X
                  f->result_size,
#else
                  1,
#endif
                  save_doubles);
  CallStub(&stub);
}


void MacroAssembler::CallExternalReference(const ExternalReference& ext,
                                           int num_arguments) {
  mov(r2, Operand(num_arguments));
  mov(r3, Operand(ext));

  CEntryStub stub(isolate(), 1);
  CallStub(&stub);
}


void MacroAssembler::TailCallExternalReference(const ExternalReference& ext,
                                               int num_arguments,
                                               int result_size) {
  // TODO(1236192): Most runtime routines don't need the number of
  // arguments passed in because it is constant. At some point we
  // should remove this need and make the runtime routine entry code
  // smarter.
  mov(r2, Operand(num_arguments));
  JumpToExternalReference(ext);
}


void MacroAssembler::TailCallRuntime(Runtime::FunctionId fid,
                                     int num_arguments,
                                     int result_size) {
  TailCallExternalReference(ExternalReference(fid, isolate()),
                            num_arguments,
                            result_size);
}


void MacroAssembler::JumpToExternalReference(const ExternalReference& builtin) {
  mov(r3, Operand(builtin));
  CEntryStub stub(isolate(), 1);
  Jump(stub.GetCode(), RelocInfo::CODE_TARGET);
}


void MacroAssembler::InvokeBuiltin(Builtins::JavaScript id,
                                   InvokeFlag flag,
                                   const CallWrapper& call_wrapper) {
  // You can't call a builtin without a valid frame.
  DCHECK(flag == JUMP_FUNCTION || has_frame());

  GetBuiltinEntry(ip, id);
  if (flag == CALL_FUNCTION) {
    call_wrapper.BeforeCall(CallSize(ip));
    CallJSEntry(ip);
    call_wrapper.AfterCall();
  } else {
    DCHECK(flag == JUMP_FUNCTION);
    Jump(ip);
  }
}


void MacroAssembler::GetBuiltinFunction(Register target,
                                        Builtins::JavaScript id) {
  // Load the builtins object into target register.
  LoadP(target,
        MemOperand(cp, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  LoadP(target, FieldMemOperand(target, GlobalObject::kBuiltinsOffset));
  // Load the JavaScript builtin function from the builtins object.
  LoadP(target,
        FieldMemOperand(target,
                        JSBuiltinsObject::OffsetOfFunctionWithId(id)), r0);
}


void MacroAssembler::GetBuiltinEntry(Register target, Builtins::JavaScript id) {
  DCHECK(!target.is(r3));
  GetBuiltinFunction(r3, id);
  // Load the code entry point from the builtins object.
  LoadP(target, FieldMemOperand(r3, JSFunction::kCodeEntryOffset));
}


void MacroAssembler::SetCounter(StatsCounter* counter, int value,
                                Register scratch1, Register scratch2) {
  if (FLAG_native_code_counters && counter->Enabled()) {
    mov(scratch1, Operand(value));
    mov(scratch2, Operand(ExternalReference(counter)));
    StoreW(scratch1, MemOperand(scratch2));
  }
}


void MacroAssembler::IncrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  DCHECK(value > 0 && is_int8(value));
  if (FLAG_native_code_counters && counter->Enabled()) {
    mov(scratch1, Operand(ExternalReference(counter)));
    // @TODO(JOHN): can be optimized by asi()
    LoadW(scratch2, MemOperand(scratch1));
    AddP(scratch2, Operand(value));
    StoreW(scratch2, MemOperand(scratch1));
  }
}


void MacroAssembler::DecrementCounter(StatsCounter* counter, int value,
                                      Register scratch1, Register scratch2) {
  DCHECK(value > 0 && is_int8(value));
  if (FLAG_native_code_counters && counter->Enabled()) {
    mov(scratch1, Operand(ExternalReference(counter)));
    // @TODO(JOHN): can be optimized by asi()
    LoadW(scratch2, MemOperand(scratch1));
    AddP(scratch2, Operand(-value));
    StoreW(scratch2, MemOperand(scratch1));
  }
}

void MacroAssembler::Assert(Condition cond, BailoutReason reason,
                            CRegister cr) {
  if (emit_debug_code())
    Check(cond, reason, cr);
}


void MacroAssembler::AssertFastElements(Register elements) {
  if (emit_debug_code()) {
    DCHECK(!elements.is(r0));
    Label ok;
    push(elements);
    LoadP(elements, FieldMemOperand(elements, HeapObject::kMapOffset));
    CompareRoot(elements, Heap::kFixedArrayMapRootIndex);
    beq(&ok, Label::kNear);
    CompareRoot(elements, Heap::kFixedDoubleArrayMapRootIndex);
    beq(&ok, Label::kNear);
    CompareRoot(elements, Heap::kFixedCOWArrayMapRootIndex);
    beq(&ok, Label::kNear);
    Abort(kJSObjectWithFastElementsMapHasSlowElements);
    bind(&ok);
    pop(elements);
  }
}


void MacroAssembler::Check(Condition cond, BailoutReason reason, CRegister cr) {
  Label L;
  b(cond, &L);
  Abort(reason);
  // will not return here
  bind(&L);
}


void MacroAssembler::Abort(BailoutReason reason) {
  Label abort_start;
  bind(&abort_start);
#ifdef DEBUG
  const char* msg = GetBailoutReason(reason);
  if (msg != NULL) {
    RecordComment("Abort message: ");
    RecordComment(msg);
  }

  if (FLAG_trap_on_abort) {
    stop(msg);
    return;
  }
#endif

  LoadSmiLiteral(r0, Smi::FromInt(reason));
  push(r0);
  // Disable stub call restrictions to always allow calls to abort.
  if (!has_frame_) {
    // We don't actually want to generate a pile of code for this, so just
    // claim there is a stack frame, without generating one.
    FrameScope scope(this, StackFrame::NONE);
    CallRuntime(Runtime::kAbort, 1);
  } else {
    CallRuntime(Runtime::kAbort, 1);
  }
  // will not return here
}


void MacroAssembler::LoadContext(Register dst, int context_chain_length) {
  if (context_chain_length > 0) {
    // Move up the chain of contexts to the context containing the slot.
    LoadP(dst, MemOperand(cp, Context::SlotOffset(Context::PREVIOUS_INDEX)));
    for (int i = 1; i < context_chain_length; i++) {
      LoadP(dst, MemOperand(dst, Context::SlotOffset(Context::PREVIOUS_INDEX)));
    }
  } else {
    // Slot is in the current function context.  Move it into the
    // destination register in case we store into it (the write barrier
    // cannot be allowed to destroy the context in esi).
    LoadRR(dst, cp);
  }
}


void MacroAssembler::LoadTransitionedArrayMapConditional(
    ElementsKind expected_kind,
    ElementsKind transitioned_kind,
    Register map_in_out,
    Register scratch,
    Label* no_map_match) {
  // Load the global or builtins object from the current context.
  LoadP(scratch,
      MemOperand(cp, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  LoadP(scratch, FieldMemOperand(scratch, GlobalObject::kNativeContextOffset));

  // Check that the function's map is the same as the expected cached map.
  LoadP(scratch,
      MemOperand(scratch,
                 Context::SlotOffset(Context::JS_ARRAY_MAPS_INDEX)));
  size_t offset = expected_kind * kPointerSize +
      FixedArrayBase::kHeaderSize;
  CmpP(map_in_out, FieldMemOperand(scratch, offset));
  bne(no_map_match);

  // Use the transitioned cached map.
  offset = transitioned_kind * kPointerSize +
      FixedArrayBase::kHeaderSize;
  LoadP(map_in_out, FieldMemOperand(scratch, offset));
}


void MacroAssembler::LoadGlobalFunction(int index, Register function) {
  // Load the global or builtins object from the current context.
  LoadP(function,
        MemOperand(cp, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  // Load the native context from the global or builtins object.
  LoadP(function, FieldMemOperand(function,
                                  GlobalObject::kNativeContextOffset));
  // Load the function from the native context.
  LoadP(function, MemOperand(function, Context::SlotOffset(index)), r0);
}


void MacroAssembler::LoadGlobalFunctionInitialMap(Register function,
                                                  Register map,
                                                  Register scratch) {
  // Load the initial map. The global functions all have initial maps.
  LoadP(map,
        FieldMemOperand(function, JSFunction::kPrototypeOrInitialMapOffset));
  if (emit_debug_code()) {
    Label ok, fail;
    CheckMap(map, scratch, Heap::kMetaMapRootIndex, &fail, DO_SMI_CHECK);
    b(&ok);
    bind(&fail);
    Abort(kGlobalFunctionsMustHaveInitialMap);
    bind(&ok);
  }
}


void MacroAssembler::JumpIfNotPowerOfTwoOrZero(
    Register reg,
    Register scratch,
    Label* not_power_of_two_or_zero) {
  SubP(scratch, reg, Operand(1));
  CmpP(scratch, Operand::Zero());
  blt(not_power_of_two_or_zero);
  AndP(r0, reg, scratch/*, SetRC*/);  // Should be okay to remove rc
  bne(not_power_of_two_or_zero /*, cr0*/);
}


void MacroAssembler::JumpIfNotPowerOfTwoOrZeroAndNeg(
    Register reg,
    Register scratch,
    Label* zero_and_neg,
    Label* not_power_of_two) {
  SubP(scratch, reg, Operand(1));
  CmpP(scratch, Operand::Zero());
  blt(zero_and_neg);
  AndP(r0, reg, scratch/*, SetRC*/);  // Should be okay to remove rc
  bne(not_power_of_two /*, cr0*/);
}

#if !V8_TARGET_ARCH_S390X
void MacroAssembler::SmiTagCheckOverflow(Register reg, Register overflow) {
  DCHECK(!reg.is(overflow));
  LoadRR(overflow, reg);  // Save original value.
  SmiTag(reg);
  XorP(overflow, overflow, reg);  // Overflow if (value ^ 2 * value) < 0.
  LoadAndTestRR(overflow, overflow);
}


void MacroAssembler::SmiTagCheckOverflow(Register dst,
                                         Register src,
                                         Register overflow) {
  if (dst.is(src)) {
    // Fall back to slower case.
    SmiTagCheckOverflow(dst, overflow);
  } else {
    DCHECK(!dst.is(src));
    DCHECK(!dst.is(overflow));
    DCHECK(!src.is(overflow));
    SmiTag(dst, src);
    XorP(overflow, dst, src);  // Overflow if (value ^ 2 * value) < 0.
    LoadAndTestRR(overflow, overflow);
  }
}
#endif

void MacroAssembler::JumpIfNotBothSmi(Register reg1,
                                      Register reg2,
                                      Label* on_not_both_smi) {
  STATIC_ASSERT(kSmiTag == 0);
  OrP(r0, reg1, reg2/*, LeaveRC*/);  // should be okay to remove LeaveRC
  JumpIfNotSmi(r0, on_not_both_smi);
}


void MacroAssembler::UntagAndJumpIfSmi(
    Register dst, Register src, Label* smi_case) {
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);
  // this won't work if src == dst
  DCHECK(src.code() != dst.code());
  SmiUntag(dst, src);
  TestIfSmi(src);
  beq(smi_case);
}


void MacroAssembler::UntagAndJumpIfNotSmi(
    Register dst, Register src, Label* non_smi_case) {
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagSize == 1);

  // We can more optimally use TestIfSmi if dst != src
  // otherwise, the UnTag operation will kill the CC and we cannot
  // test the Tag bit.
  if (src.code() != dst.code()) {
    SmiUntag(dst, src);
    TestIfSmi(src);
  } else {
    TestBit(src, 0, r0);
    SmiUntag(dst, src);
    LoadAndTestRR(r0, r0);
  }
  bne(non_smi_case);
}


void MacroAssembler::JumpIfEitherSmi(Register reg1,
                                     Register reg2,
                                     Label* on_either_smi) {
  STATIC_ASSERT(kSmiTag == 0);
  JumpIfSmi(reg1, on_either_smi);
  JumpIfSmi(reg2, on_either_smi);
}


void MacroAssembler::AssertNotSmi(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object);
    Check(ne, kOperandIsASmi, cr0);
  }
}


void MacroAssembler::AssertSmi(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object);
    Check(eq, kOperandIsNotSmi, cr0);
  }
}


void MacroAssembler::AssertString(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object);
    Check(ne, kOperandIsASmiAndNotAString, cr0);
    push(object);
    LoadP(object, FieldMemOperand(object, HeapObject::kMapOffset));
    CompareInstanceType(object, object, FIRST_NONSTRING_TYPE);
    pop(object);
    Check(lt, kOperandIsNotAString);
  }
}


void MacroAssembler::AssertName(Register object) {
  if (emit_debug_code()) {
    STATIC_ASSERT(kSmiTag == 0);
    TestIfSmi(object);
    Check(ne, kOperandIsASmiAndNotAName, cr0);
    push(object);
    LoadP(object, FieldMemOperand(object, HeapObject::kMapOffset));
    CompareInstanceType(object, object, LAST_NAME_TYPE);
    pop(object);
    Check(le, kOperandIsNotAName);
  }
}


void MacroAssembler::AssertUndefinedOrAllocationSite(Register object,
                                                     Register scratch) {
  if (emit_debug_code()) {
    Label done_checking;
    AssertNotSmi(object);
    CompareRoot(object, Heap::kUndefinedValueRootIndex);
    beq(&done_checking, Label::kNear);
    LoadP(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
    CompareRoot(scratch, Heap::kAllocationSiteMapRootIndex);
    Assert(eq, kExpectedUndefinedOrCell);
    bind(&done_checking);
  }
}


void MacroAssembler::AssertIsRoot(Register reg, Heap::RootListIndex index) {
  if (emit_debug_code()) {
    CompareRoot(reg, index);
    Check(eq, kHeapNumberMapRegisterClobbered);
  }
}


void MacroAssembler::JumpIfNotHeapNumber(Register object,
                                         Register heap_number_map,
                                         Register scratch,
                                         Label* on_not_heap_number) {
  LoadP(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
  AssertIsRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
  CmpP(scratch, heap_number_map);
  bne(on_not_heap_number);
}

void MacroAssembler::LookupNumberStringCache(Register object,
                                             Register result,
                                             Register scratch1,
                                             Register scratch2,
                                             Register scratch3,
                                             Label* not_found) {
  // Use of registers. Register result is used as a temporary.
  Register number_string_cache = result;
  Register mask = scratch3;

  // Load the number string cache.
  LoadRoot(number_string_cache, Heap::kNumberStringCacheRootIndex);

  // Make the hash mask from the length of the number string cache. It
  // contains two elements (number and string) for each cache entry.
  LoadP(mask, FieldMemOperand(number_string_cache,
                              FixedArray::kLengthOffset));
  // Divide length by two (length is a smi).
  ShiftRightArithP(mask, mask, Operand(kSmiTagSize + kSmiShiftSize + 1));
  SubP(mask, Operand(1));  // Make mask.

  // Calculate the entry in the number string cache. The hash value in the
  // number string cache for smis is just the smi value, and the hash for
  // doubles is the xor of the upper and lower words. See
  // Heap::GetNumberStringCache.
  Label is_smi;
  Label load_result_from_cache;
  JumpIfSmi(object, &is_smi);
  CheckMap(object,
           scratch1,
           Heap::kHeapNumberMapRootIndex,
           not_found,
           DONT_DO_SMI_CHECK);

  STATIC_ASSERT(8 == kDoubleSize);
  LoadlW(scratch1, FieldMemOperand(object, HeapNumber::kExponentOffset));
  LoadlW(scratch2, FieldMemOperand(object, HeapNumber::kMantissaOffset));
  XorP(scratch1, scratch2);
  AndP(scratch1, mask);

  // Calculate address of entry in string cache: each entry consists
  // of two pointer sized fields.
  ShiftLeftP(scratch1, scratch1, Operand(kPointerSizeLog2 + 1));
  AddP(scratch1, number_string_cache);

  Register probe = mask;
  LoadP(probe, FieldMemOperand(scratch1, FixedArray::kHeaderSize));
  JumpIfSmi(probe, not_found);
  LoadF(d0, FieldMemOperand(object, HeapNumber::kValueOffset));
  LoadF(d1, FieldMemOperand(probe, HeapNumber::kValueOffset));
  cdbr(d0, d1);
  bne(not_found);  // The cache did not contain this value.
  b(&load_result_from_cache);

  bind(&is_smi);
  Register scratch = scratch1;
  SmiUntag(scratch, object);
  AndP(scratch, mask);
  // Calculate address of entry in string cache: each entry consists
  // of two pointer sized fields.
  ShiftLeftP(scratch, scratch, Operand(kPointerSizeLog2 + 1));
  AddP(scratch, number_string_cache, scratch);

  // Check if the entry is the smi we are looking for.
  LoadP(probe, FieldMemOperand(scratch, FixedArray::kHeaderSize));
  CmpP(object, probe);
  bne(not_found);

  // Get the result from the cache.
  bind(&load_result_from_cache);
  LoadP(result,
        FieldMemOperand(scratch, FixedArray::kHeaderSize + kPointerSize));
  IncrementCounter(isolate()->counters()->number_to_string_native(),
                   1,
                   scratch1,
                   scratch2);
}


void MacroAssembler::JumpIfNonSmisNotBothSequentialAsciiStrings(
    Register first,
    Register second,
    Register scratch1,
    Register scratch2,
    Label* failure) {
  // Test that both first and second are sequential ASCII strings.
  // Assume that they are non-smis.
  LoadP(scratch1, FieldMemOperand(first, HeapObject::kMapOffset));
  LoadP(scratch2, FieldMemOperand(second, HeapObject::kMapOffset));
  LoadlB(scratch1, FieldMemOperand(scratch1, Map::kInstanceTypeOffset));
  LoadlB(scratch2, FieldMemOperand(scratch2, Map::kInstanceTypeOffset));

  JumpIfBothInstanceTypesAreNotSequentialAscii(scratch1,
                                               scratch2,
                                               scratch1,
                                               scratch2,
                                               failure);
}


void MacroAssembler::JumpIfNotBothSequentialAsciiStrings(Register first,
                                                         Register second,
                                                         Register scratch1,
                                                         Register scratch2,
                                                         Label* failure) {
  // Check that neither is a smi.
  AndP(scratch1, first, second);
  JumpIfSmi(scratch1, failure);
  JumpIfNonSmisNotBothSequentialAsciiStrings(first,
                                             second,
                                             scratch1,
                                             scratch2,
                                             failure);
}


void MacroAssembler::JumpIfNotUniqueName(Register reg,
                                         Label* not_unique_name) {
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  Label succeed;
  AndP(r0, reg, Operand(kIsNotStringMask | kIsNotInternalizedMask));
  beq(&succeed, Label::kNear);
  CmpP(reg, Operand(SYMBOL_TYPE));
  bne(not_unique_name);

  bind(&succeed);
}


// Allocates a heap number or jumps to the need_gc label if the young space
// is full and a scavenge is needed.
void MacroAssembler::AllocateHeapNumber(Register result,
                                        Register scratch1,
                                        Register scratch2,
                                        Register heap_number_map,
                                        Label* gc_required,
                                        TaggingMode tagging_mode,
                                        MutableMode mode) {
  // Allocate an object in the heap for the heap number and tag it as a heap
  // object.
  Allocate(HeapNumber::kSize, result, scratch1, scratch2, gc_required,
           tagging_mode == TAG_RESULT ? TAG_OBJECT : NO_ALLOCATION_FLAGS);

  Heap::RootListIndex map_index = mode == MUTABLE
      ? Heap::kMutableHeapNumberMapRootIndex
      : Heap::kHeapNumberMapRootIndex;
  AssertIsRoot(heap_number_map, map_index);

  // Store heap number map in the allocated object.
  if (tagging_mode == TAG_RESULT) {
    StoreP(heap_number_map, FieldMemOperand(result, HeapObject::kMapOffset));
  } else {
    StoreP(heap_number_map, MemOperand(result, HeapObject::kMapOffset));
  }
}


void MacroAssembler::AllocateHeapNumberWithValue(Register result,
                                                 DoubleRegister value,
                                                 Register scratch1,
                                                 Register scratch2,
                                                 Register heap_number_map,
                                                 Label* gc_required) {
  AllocateHeapNumber(result, scratch1, scratch2, heap_number_map, gc_required);
  StoreF(value, FieldMemOperand(result, HeapNumber::kValueOffset));
}


// Copies a fixed number of fields of heap objects from src to dst.
void MacroAssembler::CopyFields(Register dst,
                                Register src,
                                RegList temps,
                                int field_count) {
  // At least one bit set in the first 15 registers.
  DCHECK((temps & ((1 << 15) - 1)) != 0);
  DCHECK((temps & dst.bit()) == 0);
  DCHECK((temps & src.bit()) == 0);
  // Primitive implementation using only one temporary register.

  Register tmp = no_reg;
  // Find a temp register in temps list.
  for (int i = 0; i < 15; i++) {
    if ((temps & (1 << i)) != 0) {
      tmp.set_code(i);
      break;
    }
  }
  DCHECK(!tmp.is(no_reg));

  for (int i = 0; i < field_count; i++) {
    LoadP(tmp, FieldMemOperand(src, i * kPointerSize), r0);
    StoreP(tmp, FieldMemOperand(dst, i * kPointerSize));
  }
}


void MacroAssembler::CopyBytes(Register src,
                               Register dst,
                               Register length,
                               Register scratch) {
  Label big_loop, left_bytes, done, fake_call;

  DCHECK(!scratch.is(r0));

  // big loop moves 256 bytes at a time
  bind(&big_loop);
  CmpP(length, Operand(static_cast<intptr_t>(0x100)));
  blt(&left_bytes);

  mvc(MemOperand(dst), MemOperand(src), 0x100);

  AddP(src, Operand(static_cast<intptr_t>(0x100)));
  AddP(dst, Operand(static_cast<intptr_t>(0x100)));
  SubP(length, Operand(static_cast<intptr_t>(0x100)));
  b(&big_loop);

  bind(&left_bytes);
  CmpP(length, Operand::Zero());
  beq(&done);

  // TODO(JOHN): The full optimized version with unknown problem.
  /*
  b(scratch, &fake_call);  // use brasl to Save mvc addr to scratch
  mvc(MemOperand(dst), MemOperand(src), 1);
  bind(&fake_call);
  SubP(length, Operand(static_cast<intptr_t>(-1)));
  ex(length, MemOperand(scratch));  // execute mvc instr above
  AddP(src, length);
  AddP(dst, length);
  AddP(src, Operand(static_cast<intptr_t>(0x1)));
  AddP(dst, Operand(static_cast<intptr_t>(0x1)));
  */

  mvc(MemOperand(dst), MemOperand(src), 1);
  AddP(src, Operand(static_cast<intptr_t>(0x1)));
  AddP(dst, Operand(static_cast<intptr_t>(0x1)));
  SubP(length, Operand(static_cast<intptr_t>(0x1)));

  b(&left_bytes);
  bind(&done);
}


void MacroAssembler::InitializeNFieldsWithFiller(Register start_offset,
                                                Register count,
                                                Register filler) {
  Label loop;
  bind(&loop);
  StoreP(filler, MemOperand(start_offset));
  AddP(start_offset, Operand(kPointerSize));
  SubP(count, Operand(1));
  CmpP(count, Operand::Zero());
  bne(&loop);
}



void MacroAssembler::InitializeFieldsWithFiller(Register start_offset,
                                                Register end_offset,
                                                Register filler) {
  Label done;
  SubP(r0, end_offset, start_offset /*, LeaveOE, SetRC*/);
  beq(&done, Label::kNear);
  ShiftRightP(r0, r0, Operand(kPointerSizeLog2));
  InitializeNFieldsWithFiller(start_offset, r0, filler);
  bind(&done);
}


void MacroAssembler::SaveFPRegs(Register location, int first, int count) {
  DCHECK(count > 0);
  int cur = first;
  SubP(location, Operand(count * kDoubleSize));
  for (int i = 0; i < count; i++) {
    DoubleRegister reg = DoubleRegister::from_code(cur++);
    StoreF(reg, MemOperand(location, i * kDoubleSize));
  }
}


void MacroAssembler::RestoreFPRegs(Register location, int first, int count) {
  DCHECK(count > 0);
  int cur = first + count - 1;
  for (int i = count - 1; i >= 0; i--) {
    DoubleRegister reg = DoubleRegister::from_code(cur--);
    LoadF(reg, MemOperand(location, i * kDoubleSize));
  }
  AddP(location, Operand(count * kDoubleSize));
}


void MacroAssembler::JumpIfBothInstanceTypesAreNotSequentialAscii(
    Register first,
    Register second,
    Register scratch1,
    Register scratch2,
    Label* failure) {
  const int kFlatAsciiStringMask =
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask;
  const int kFlatAsciiStringTag =
      kStringTag | kOneByteStringTag | kSeqStringTag;
  if (!scratch1.is(first)) LoadRR(scratch1, first);
  if (!scratch2.is(second)) LoadRR(scratch2, second);
  nilf(scratch1, Operand(kFlatAsciiStringMask));
  CmpP(scratch1, Operand(kFlatAsciiStringTag));
  bne(failure);
  nilf(scratch2, Operand(kFlatAsciiStringMask));
  CmpP(scratch2, Operand(kFlatAsciiStringTag));
  bne(failure);

  /*
  CmpP(scratch1, Operand(kFlatAsciiStringTag));
  nilf(scratch2, Operand(kFlatAsciiStringMask));
  mov(scratch1, Operand(kFlatAsciiStringMask));
  AndP(scratch1, first);
  mov(scratch2, Operand(kFlatAsciiStringMask));
  AndP(scratch2, second);
  CmpP(scratch1, Operand(kFlatAsciiStringTag));
  bne(failure);
  CmpP(scratch2, Operand(kFlatAsciiStringTag));
  bne(failure);
  */
}


void MacroAssembler::JumpIfInstanceTypeIsNotSequentialAscii(Register type,
                                                            Register scratch,
                                                            Label* failure) {
  const int kFlatAsciiStringMask =
      kIsNotStringMask | kStringEncodingMask | kStringRepresentationMask;
  const int kFlatAsciiStringTag =
      kStringTag | kOneByteStringTag | kSeqStringTag;

  if (!scratch.is(type)) LoadRR(scratch, type);
  nilf(scratch, Operand(kFlatAsciiStringMask));
  CmpP(scratch, Operand(kFlatAsciiStringTag));
  bne(failure);
  /*
  mov(scratch, Operand(kFlatAsciiStringMask));
  AndP(scratch, type);
  CmpP(scratch, Operand(kFlatAsciiStringTag));
  bne(failure);
  */
}


static const int kRegisterPassedArguments = 5;


int MacroAssembler::CalculateStackPassedWords(int num_reg_arguments,
                                              int num_double_arguments) {
  int stack_passed_words = 0;
#ifdef V8_OS_ZOS
  // XPLINK Linkage reserves space for arguments on the stack
  // even when passing them via registers.
  stack_passed_words = num_reg_arguments + num_double_arguments;
#else
  if (num_double_arguments > DoubleRegister::kNumRegisters) {
      stack_passed_words +=
        2 * (num_double_arguments - DoubleRegister::kNumRegisters);
  }
  // Up to five simple arguments are passed in registers r2..r6
  if (num_reg_arguments > kRegisterPassedArguments) {
    stack_passed_words += num_reg_arguments - kRegisterPassedArguments;
  }
#endif
  return stack_passed_words;
}

void MacroAssembler::EmitSeqStringSetCharCheck(Register string,
                                               Register index,
                                               Register value,
                                               uint32_t encoding_mask) {
  Label is_object;
  TestIfSmi(string);
  Check(ne, kNonObject, cr0);

  LoadP(ip, FieldMemOperand(string, HeapObject::kMapOffset));
  LoadlB(ip, FieldMemOperand(ip, Map::kInstanceTypeOffset));

  AndP(ip, Operand(kStringRepresentationMask | kStringEncodingMask));
  CmpP(ip, Operand(encoding_mask));
  Check(eq, kUnexpectedStringType);

  // The index is assumed to be untagged coming in, tag it to compare with the
  // string length without using a temp register, it is restored at the end of
  // this function.
#if !V8_TARGET_ARCH_S390X
  Label index_tag_ok, index_tag_bad;
  JumpIfNotSmiCandidate(index, r0, &index_tag_bad);
#endif
  SmiTag(index, index);
#if !V8_TARGET_ARCH_S390X
  b(&index_tag_ok);
  bind(&index_tag_bad);
  Abort(kIndexIsTooLarge);
  bind(&index_tag_ok);
#endif

  LoadP(ip, FieldMemOperand(string, String::kLengthOffset));
  CmpP(index, ip);
  Check(lt, kIndexIsTooLarge);

  DCHECK(Smi::FromInt(0) == 0);
  CmpP(index, Operand::Zero());
  Check(ge, kIndexIsNegative);

  SmiUntag(index, index);
}

void MacroAssembler::PrepareCallCFunction(int num_reg_arguments,
                                          int num_double_arguments,
                                          Register scratch) {
  int frame_alignment = ActivationFrameAlignment();
  int stack_passed_arguments = CalculateStackPassedWords(
      num_reg_arguments, num_double_arguments);
#ifdef V8_OS_ZOS
  Register c_sp   = r4;  // Stack pointer in C/C++.
  int stack_space = 16;  // Save area + debug area + reserved space.
  LoadRR(c_sp , sp);
#else
  int stack_space = kNumRequiredStackFrameSlots;
  Register c_sp   = sp;
#endif
  if (frame_alignment > kPointerSize) {
    // Make stack end at alignment and make room for stack arguments
    // -- preserving original value of sp.
    LoadRR(scratch, c_sp);
    lay(c_sp, MemOperand(c_sp, -(stack_passed_arguments + 1) * kPointerSize));
    DCHECK(IsPowerOf2(frame_alignment));
    ClearRightImm(c_sp, c_sp, Operand(WhichPowerOf2(frame_alignment)));
    StoreP(scratch, MemOperand(c_sp, (stack_passed_arguments) * kPointerSize));
  } else {
    stack_space += stack_passed_arguments;
  }
  lay(c_sp,
      MemOperand(c_sp, -((stack_space * kPointerSize) + kStackPointerBias)));
}


void MacroAssembler::PrepareCallCFunction(int num_reg_arguments,
                                          Register scratch) {
  PrepareCallCFunction(num_reg_arguments, 0, scratch);
}


void MacroAssembler::MovToFloatParameter(DoubleRegister src) {
  Move(d0, src);
}


void MacroAssembler::MovToFloatResult(DoubleRegister src) {
  Move(d0, src);
}


void MacroAssembler::MovToFloatParameters(DoubleRegister src1,
                                          DoubleRegister src2) {
  if (src2.is(d0)) {
    DCHECK(!src1.is(d2));
    Move(d2, src2);
    Move(d0, src1);
  } else {
    Move(d0, src1);
    Move(d2, src2);
  }
}


void MacroAssembler::CallCFunction(ExternalReference function,
                                   int num_reg_arguments,
                                   int num_double_arguments) {
  mov(ip, Operand(function));
  CallCFunctionHelper(ip, num_reg_arguments, num_double_arguments);
}


void MacroAssembler::CallCFunction(Register function,
                                   int num_reg_arguments,
                                   int num_double_arguments) {
  CallCFunctionHelper(function, num_reg_arguments, num_double_arguments);
}


void MacroAssembler::CallCFunction(ExternalReference function,
                                   int num_arguments) {
  CallCFunction(function, num_arguments, 0);
}


void MacroAssembler::CallCFunction(Register function,
                                   int num_arguments) {
  CallCFunction(function, num_arguments, 0);
}


void MacroAssembler::CallCFunctionHelper(Register function,
                                         int num_reg_arguments,
                                         int num_double_arguments) {
  DCHECK(has_frame());
  // Just call directly. The function called cannot cause a GC, or
  // allow preemption, so the return address in the link register
  // stays correct.
#if ABI_USES_FUNCTION_DESCRIPTORS && !defined(USE_SIMULATOR)
  // z/OS uses a function descriptor. When calling C code be aware
  // of this descriptor and pick up values from it
  LoadMultipleP(r5, r6, MemOperand(function, 0));
  Register dest = r6;
#elif ABI_TOC_ADDRESSABILITY_VIA_IP
  Move(ip, function);
  Register dest = ip;
#else
  Register dest = function;
#endif

#ifdef V8_OS_ZOS
  CallC(dest);
#else
  Call(dest);
#endif
  // Javascript stack pointer will be restored by the callee's epilogue.
#ifndef V8_OS_ZOS
  int stack_passed_arguments = CalculateStackPassedWords(
      num_reg_arguments, num_double_arguments);
  int stack_space = kNumRequiredStackFrameSlots + stack_passed_arguments;
  if (ActivationFrameAlignment() > kPointerSize) {
    // Load the original stack pointer (pre-alignment) from the stack.
    LoadP(sp, MemOperand(sp, stack_space * kPointerSize));
  } else {
    la(sp, MemOperand(sp, stack_space * kPointerSize));
  }
#endif
}


void MacroAssembler::FlushICache(Register address, size_t size,
                                 Register scratch) {
  // S390 memory model does not require us to flush icache
  return;
}


// This code assumes a FIXED_SEQUENCE for iilf on 31-bit
// and iihf/iilf on 64-bit
void MacroAssembler::SetRelocatedValue(Register patch_location,
                                         Register scratch,
                                         Register new_value) {
  int32_t offset = 0;

#if V8_TARGET_ARCH_S390X
  // On 64-bit, we expect a IIHF instruction here.
  if (emit_debug_code()) {
#if V8_TARGET_LITTLE_ENDIAN
    // Instructions are stored in Big Endian format
    lrvh(scratch, MemOperand(patch_location));
#else
    llh(scratch, MemOperand(patch_location));
#endif
    nilf(scratch, Operand(0xFF0F));
    // IIHF Opcode with extra zero in 3rd nibble
    cfi(scratch, Operand(0xC008));
    // TODO(Zen): Fix this check to work
    // Check(eq, kTheInstructionToPatchShouldBeAnOri);
  }

  srlg(scratch, new_value, Operand(32));
  // insert new high word into iihf instruction
#if V8_TARGET_LITTLE_ENDIAN
  // Instructions are stored in Big Endian format
  strv(scratch, MemOperand(patch_location, 2));
#else
  st(scratch, MemOperand(patch_location, 2));
#endif
  offset += 6;
#endif  // V8_TARGET_ARCH_S390X


  // At this point scratch is a iilf instruction.
  if (emit_debug_code()) {
#if V8_TARGET_LITTLE_ENDIAN
    // Instructions are stored in Big Endian format
    lrvh(scratch, MemOperand(patch_location, offset));
#else
    llh(scratch, MemOperand(patch_location, offset));
#endif
    nilf(scratch, Operand(0xFF0F));
    // IILF Opcode with extra zero in 3rd nibble
    cfi(scratch, Operand(0xC009));
    // TODO(Zen): Fix this check to work
    // Check(eq, kTheInstructionToPatchShouldBeAnIilf);
  }

  // insert low word into iilf instruction
#if V8_TARGET_LITTLE_ENDIAN
  // Instructions are stored in Big Endian format
  strv(new_value, MemOperand(patch_location, 2 + offset));
#else
  st(new_value, MemOperand(patch_location, 2 + offset));
#endif

  // Update the I-cache so the new lis and addic can be executed.
#if V8_TARGET_ARCH_S390X
  FlushICache(patch_location, 12, scratch);
#else
  FlushICache(patch_location, 6, scratch);
#endif
}


// This code assumes a FIXED_SEQUENCE for iilf on 31-bit
// and iihf/iilf on 64-bit
void MacroAssembler::GetRelocatedValue(Register patch_location,
                                               Register result,
                                               Register scratch) {
  int32_t offset = 0;

#if V8_TARGET_ARCH_S390X
  // On 64-bit, we expect a IIHF instruction here.
  if (emit_debug_code()) {
#if V8_TARGET_LITTLE_ENDIAN
    // Instructions are stored in Big Endian format
    lrvh(scratch, MemOperand(patch_location));
#else
    llh(scratch, MemOperand(patch_location));
#endif
    nilf(scratch, Operand(0xFF0F));
    // IIHF Opcode with extra zero in 3rd nibble
    cfi(scratch, Operand(0xC008));
    // TODO(Zen): Fix this check to work
    // Check(eq, "The instruction to patch should be a iihf.");
  }

  // load high word from iihf instruction
#if V8_TARGET_LITTLE_ENDIAN
  // Instructions are stored in Big Endian format
  lrv(result, MemOperand(patch_location, 2));
#else
  l(result, MemOperand(patch_location, 2));
#endif
  sllg(result, result, Operand(32));

  offset += 6;
#endif  // V8_TARGET_ARCH_S390X


  // At this point scratch is a iilf instruction.
  if (emit_debug_code()) {
#if V8_TARGET_LITTLE_ENDIAN
    // Instructions are stored in Big Endian format
    lrvh(scratch, MemOperand(patch_location, offset));
#else
    llh(scratch, MemOperand(patch_location, offset));
#endif
    nilf(scratch, Operand(0xFF0F));
    // IILF Opcode with extra zero in 3rd nibble
    cfi(scratch, Operand(0xC009));
    // TODO(Zen): Fix this check to work
    // Check(eq, "The instruction to patch should be a iilf.");
  }

  // load low word from iilf instruction
#if V8_TARGET_LITTLE_ENDIAN
  // Instructions are stored in Big Endian format
  lrv(result, MemOperand(patch_location, 2 + offset));
#else
  l(result, MemOperand(patch_location, 2 + offset));
#endif
}


void MacroAssembler::CheckPageFlag(
    Register object,
    Register scratch,  // scratch may be same register as object
    int mask,
    Condition cc,
    Label* condition_met) {
  DCHECK(cc == ne || cc == eq);
  ClearRightImm(scratch, object, Operand(kPageSizeBits));

  if (IsPowerOf2(mask)) {
    // If it's a power of two, we can use Test-Under-Mask Memory-Imm form
    // which allows testing of a single byte in memory.
    int32_t byte_offset = 4;
    uint32_t shifted_mask = mask;
    // Determine the byte offset to be tested
    if (mask <= 0x80) {
      byte_offset = kPointerSize - 1;
    } else if (mask < 0x8000) {
      byte_offset = kPointerSize - 2;
      shifted_mask = mask >> 8;
    } else if (mask < 0x800000) {
      byte_offset = kPointerSize - 3;
      shifted_mask = mask >> 16;
    } else {
      byte_offset = kPointerSize - 4;
      shifted_mask = mask >> 24;
    }
#if V8_TARGET_LITTLE_ENDIAN
    // Reverse the byte_offset if emulating on little endian platform
    byte_offset = kPointerSize - byte_offset - 1;
#endif
    tm(MemOperand(scratch, MemoryChunk::kFlagsOffset + byte_offset),
       Operand(shifted_mask));
  } else {
    LoadP(scratch, MemOperand(scratch, MemoryChunk::kFlagsOffset));
    AndP(r0, scratch, Operand(mask));
  }
  // Should be okay to remove rc

  if (cc == ne) {
    bne(condition_met, Label::kNear);
  }
  if (cc == eq) {
    beq(condition_met, Label::kNear);
  }
}

void MacroAssembler::CheckMapDeprecated(Handle<Map> map,
                                        Register scratch,
                                        Label* if_deprecated) {
  if (map->CanBeDeprecated()) {
    mov(scratch, Operand(map));
    LoadlW(scratch, FieldMemOperand(scratch, Map::kBitField3Offset));
    ExtractBitMask(scratch, scratch, Map::Deprecated::kMask/*, SetRC*/);
    bne(if_deprecated);
  }
}

void MacroAssembler::JumpIfBlack(Register object,
                                 Register scratch0,
                                 Register scratch1,
                                 Label* on_black) {
  HasColor(object, scratch0, scratch1, on_black, 1, 0);  // kBlackBitPattern.
  DCHECK(strcmp(Marking::kBlackBitPattern, "10") == 0);
}


void MacroAssembler::HasColor(Register object,
                              Register bitmap_scratch,
                              Register mask_scratch,
                              Label* has_color,
                              int first_bit,
                              int second_bit) {
  DCHECK(!AreAliased(object, bitmap_scratch, mask_scratch, no_reg));

  GetMarkBits(object, bitmap_scratch, mask_scratch);

  Label other_color, word_boundary;
  LoadlW(ip, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize));
  // Test the first bit
  AndP(r0, ip, mask_scratch/*, SetRC*/);  // Should be okay to remove rc
  b(first_bit == 1 ? eq : ne, &other_color, Label::kNear);
  // Shift left 1
  // May need to load the next cell
  sll(mask_scratch, Operand(1)/*, SetRC*/);
  beq(&word_boundary, Label::kNear);
  // Test the second bit
  AndP(r0, ip, mask_scratch/*, SetRC*/);  // Should be okay to remove rc
  b(second_bit == 1 ? ne : eq, has_color);
  b(&other_color, Label::kNear);

  bind(&word_boundary);
  LoadlW(ip, MemOperand(bitmap_scratch,
                     MemoryChunk::kHeaderSize + kIntSize));
  AndP(r0, ip, Operand(1));
  b(second_bit == 1 ? ne : eq, has_color);
  bind(&other_color);
}


// Detect some, but not all, common pointer-free objects.  This is used by the
// incremental write barrier which doesn't care about oddballs (they are always
// marked black immediately so this code is not hit).
void MacroAssembler::JumpIfDataObject(Register value,
                                      Register scratch,
                                      Label* not_data_object) {
  Label is_data_object;
  LoadP(scratch, FieldMemOperand(value, HeapObject::kMapOffset));
  CompareRoot(scratch, Heap::kHeapNumberMapRootIndex);
  beq(&is_data_object, Label::kNear);
  DCHECK(kIsIndirectStringTag == 1 && kIsIndirectStringMask == 1);
  DCHECK(kNotStringTag == 0x80 && kIsNotStringMask == 0x80);
  // If it's a string and it's not a cons string then it's an object containing
  // no GC pointers.
  LoadlB(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
  STATIC_ASSERT((kIsIndirectStringMask | kIsNotStringMask) == 0x81);
  nilf(scratch, Operand(kIsIndirectStringMask | kIsNotStringMask));
  bne(not_data_object /*, cr0*/);
  bind(&is_data_object);
}


void MacroAssembler::GetMarkBits(Register addr_reg,
                                 Register bitmap_reg,
                                 Register mask_reg) {
  DCHECK(!AreAliased(addr_reg, bitmap_reg, mask_reg, no_reg));
  LoadRR(bitmap_reg, addr_reg);
  nilf(bitmap_reg, Operand(~Page::kPageAlignmentMask));
  const int kLowBits = kPointerSizeLog2 + Bitmap::kBitsPerCellLog2;
  ExtractBitRange(mask_reg, addr_reg,
                  kLowBits - 1,
                  kPointerSizeLog2);
  ExtractBitRange(ip, addr_reg,
                  kPageSizeBits - 1,
                  kLowBits);
  ShiftLeftP(ip, ip, Operand(Bitmap::kBytesPerCellLog2));
  AddP(bitmap_reg, ip);
  LoadRR(ip, mask_reg);   // Have to do some funky reg shuffling as
                          // 31-bit shift left clobbers on s390.
  LoadImmP(mask_reg, Operand(1));
  ShiftLeftP(mask_reg, mask_reg, ip);
}


void MacroAssembler::EnsureNotWhite(
    Register value,
    Register bitmap_scratch,
    Register mask_scratch,
    Register load_scratch,
    Label* value_is_white_and_not_data) {
  DCHECK(!AreAliased(value, bitmap_scratch, mask_scratch, ip));
  GetMarkBits(value, bitmap_scratch, mask_scratch);

  // If the value is black or grey we don't need to do anything.
  DCHECK(strcmp(Marking::kWhiteBitPattern, "00") == 0);
  DCHECK(strcmp(Marking::kBlackBitPattern, "10") == 0);
  DCHECK(strcmp(Marking::kGreyBitPattern, "11") == 0);
  DCHECK(strcmp(Marking::kImpossibleBitPattern, "01") == 0);

  Label done;

  // Since both black and grey have a 1 in the first position and white does
  // not have a 1 there we only need to check one bit.
  LoadlW(load_scratch, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize));
  LoadRR(r0, load_scratch);
  AndP(r0, mask_scratch/*, SetRC*/);
  // Should be okay to remove rc
  bne(&done /*, cr0*/);

  if (emit_debug_code()) {
    // Check for impossible bit pattern.
    Label ok;
    // LSL may overflow, making the check conservative.
    LoadRR(r0, mask_scratch);
    sll(r0, Operand(1));
    AndP(r0, load_scratch/*, SetRC*/);  // Should be okay to remove rc
    beq(&ok /*, cr0*/);
    stop("Impossible marking bit pattern");
    bind(&ok);
  }

  // Value is white.  We check whether it is data that doesn't need scanning.
  // Currently only checks for HeapNumber and non-cons strings.
  Register map = load_scratch;  // Holds map while checking type.
  Register length = load_scratch;  // Holds length of object after testing type.
  Label is_data_object, maybe_string_object, is_string_object, is_encoded;
#if V8_TARGET_ARCH_S390X
  Label length_computed;
#endif


  // Check for heap-number
  LoadP(map, FieldMemOperand(value, HeapObject::kMapOffset));
  CompareRoot(map, Heap::kHeapNumberMapRootIndex);
  bne(&maybe_string_object, Label::kNear);
  LoadImmP(length, Operand(HeapNumber::kSize));
  b(&is_data_object);
  bind(&maybe_string_object);

  // Check for strings.
  DCHECK(kIsIndirectStringTag == 1 && kIsIndirectStringMask == 1);
  DCHECK(kNotStringTag == 0x80 && kIsNotStringMask == 0x80);
  // If it's a string and it's not a cons string then it's an object containing
  // no GC pointers.
  Register instance_type = load_scratch;
  LoadlB(instance_type, FieldMemOperand(map, Map::kInstanceTypeOffset));
  mov(r0, Operand(kIsIndirectStringMask | kIsNotStringMask));
  AndP(r0, instance_type);
  bne(value_is_white_and_not_data /*, cr0*/);
  // It's a non-indirect (non-cons and non-slice) string.
  // If it's external, the length is just ExternalString::kSize.
  // Otherwise it's String::kHeaderSize + string->length() * (1 or 2).
  // External strings are the only ones with the kExternalStringTag bit
  // set.
  DCHECK_EQ(0, kSeqStringTag & kExternalStringTag);
  DCHECK_EQ(0, kConsStringTag & kExternalStringTag);
  mov(r0, Operand(kExternalStringTag));
  AndP(r0, instance_type);
  beq(&is_string_object, Label::kNear/*, cr0*/);
  LoadImmP(length, Operand(ExternalString::kSize));
  b(&is_data_object, Label::kNear);
  bind(&is_string_object);

  // Sequential string, either ASCII or UC16.
  // For ASCII (char-size of 1) we untag the smi to get the length.
  // For UC16 (char-size of 2):
  //   - (32-bit) we just leave the smi tag in place, thereby getting
  //              the length multiplied by 2.
  //   - (64-bit) we compute the offset in the 2-byte array
  DCHECK(kOneByteStringTag == 4 && kStringEncodingMask == 4);
  LoadP(ip, FieldMemOperand(value, String::kLengthOffset));
  mov(r0, Operand(kStringEncodingMask));
  AndP(r0, instance_type);
  beq(&is_encoded, Label::kNear);
  SmiUntag(ip);
#if V8_TARGET_ARCH_S390X
  b(&length_computed, Label::kNear);
#endif
  bind(&is_encoded);
#if V8_TARGET_ARCH_S390X
  SmiToShortArrayOffset(ip, ip);
  bind(&length_computed);
#else
  DCHECK(kSmiShift == 1);
#endif
  AddP(length, ip, Operand(SeqString::kHeaderSize + kObjectAlignmentMask));
  LoadImmP(r0, Operand(~kObjectAlignmentMask));
  AndP(length, r0);

  bind(&is_data_object);
  // Value is a data object, and it is white.  Mark it black.  Since we know
  // that the object is white we can make it black by flipping one bit.
  LoadlW(ip, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize));
  OrP(ip, mask_scratch);
  StoreW(ip, MemOperand(bitmap_scratch, MemoryChunk::kHeaderSize));

  mov(ip, Operand(~Page::kPageAlignmentMask));
  AndP(bitmap_scratch, ip);
  LoadlW(ip, MemOperand(bitmap_scratch, MemoryChunk::kLiveBytesOffset));
  AddP(ip, length);
  StoreW(ip, MemOperand(bitmap_scratch, MemoryChunk::kLiveBytesOffset));

  bind(&done);
}


// Saturate a value into 8-bit unsigned integer
//   if input_value < 0, output_value is 0
//   if input_value > 255, output_value is 255
//   otherwise output_value is the input_value
void MacroAssembler::ClampUint8(Register output_reg, Register input_reg) {
  Label done, negative_label, overflow_label;
  int satval = (1 << 8) - 1;

  CmpP(input_reg, Operand::Zero());
  blt(&negative_label, Label::kNear);

  CmpP(input_reg, Operand(satval));
  bgt(&overflow_label, Label::kNear);
  if (!output_reg.is(input_reg)) {
    LoadRR(output_reg, input_reg);
  }
  b(&done, Label::kNear);

  bind(&negative_label);
  LoadImmP(output_reg, Operand::Zero());  // set to 0 if negative
  b(&done, Label::kNear);


  bind(&overflow_label);  // set to satval if > satval
  LoadImmP(output_reg, Operand(satval));

  bind(&done);
}


void MacroAssembler::ClampDoubleToUint8(Register result_reg,
                                        DoubleRegister input_reg,
                                        DoubleRegister double_scratch) {
  Label above_zero;
  Label done;
  Label in_bounds;

  LoadDoubleLiteral(double_scratch, 0.0, result_reg);
  cdbr(input_reg, double_scratch);
  bgt(&above_zero, Label::kNear);

  // Double value is less than zero, NaN or Inf, return 0.
  LoadIntLiteral(result_reg, 0);
  b(&done, Label::kNear);

  // Double value is >= 255, return 255.
  bind(&above_zero);
  LoadDoubleLiteral(double_scratch, 255.0, result_reg);
  cdbr(input_reg, double_scratch);
  ble(&in_bounds, Label::kNear);
  LoadIntLiteral(result_reg, 255);
  b(&done, Label::kNear);

  // In 0-255 range, round and truncate.
  bind(&in_bounds);

  // round to nearest (default rounding mode)
  cfdbr(ROUND_TO_NEAREST_WITH_TIES_TO_EVEN, result_reg, input_reg);
  bind(&done);
}


void MacroAssembler::LoadInstanceDescriptors(Register map,
                                             Register descriptors) {
  LoadP(descriptors, FieldMemOperand(map, Map::kDescriptorsOffset));
}


void MacroAssembler::NumberOfOwnDescriptors(Register dst, Register map) {
  LoadlW(dst, FieldMemOperand(map, Map::kBitField3Offset));
  DecodeField<Map::NumberOfOwnDescriptorsBits>(dst);
}


void MacroAssembler::EnumLength(Register dst, Register map) {
  STATIC_ASSERT(Map::EnumLengthBits::kShift == 0);
  LoadW(dst, FieldMemOperand(map, Map::kBitField3Offset));
  And(dst, Operand(Map::EnumLengthBits::kMask));
  SmiTag(dst);
}


void MacroAssembler::CheckEnumCache(Register null_value, Label* call_runtime) {
  Register  empty_fixed_array_value = r8;
  LoadRoot(empty_fixed_array_value, Heap::kEmptyFixedArrayRootIndex);
  Label next, start;
  LoadRR(r4, r2);

  // Check if the enum length field is properly initialized, indicating that
  // there is an enum cache.
  LoadP(r3, FieldMemOperand(r4, HeapObject::kMapOffset));

  EnumLength(r5, r3);
  CmpSmiLiteral(r5, Smi::FromInt(kInvalidEnumCacheSentinel), r0);
  beq(call_runtime);

  b(&start, Label::kNear);

  bind(&next);
  LoadP(r3, FieldMemOperand(r4, HeapObject::kMapOffset));

  // For all objects but the receiver, check that the cache is empty.
  EnumLength(r5, r3);
  CmpSmiLiteral(r5, Smi::FromInt(0), r0);
  bne(call_runtime);

  bind(&start);

  // Check that there are no elements. Register r4 contains the current JS
  // object we've reached through the prototype chain.
  Label no_elements;
  LoadP(r4, FieldMemOperand(r4, JSObject::kElementsOffset));
  CmpP(r4, empty_fixed_array_value);
  beq(&no_elements, Label::kNear);

  // Second chance, the object may be using the empty slow element dictionary.
  CompareRoot(r5, Heap::kEmptySlowElementDictionaryRootIndex);
  bne(call_runtime);

  bind(&no_elements);
  LoadP(r4, FieldMemOperand(r3, Map::kPrototypeOffset));
  CmpP(r4, null_value);
  bne(&next);
}


////////////////////////////////////////////////////////////////////////////////
//
// New MacroAssembler Interfaces added for S390
//
////////////////////////////////////////////////////////////////////////////////
// Primarily used for loading constants
// This should really move to be in macro-assembler as it
// is really a pseudo instruction
// Some usages of this intend for a FIXED_SEQUENCE to be used
// @TODO - break this dependency so we can optimize mov() in general
// and only use the generic version when we require a fixed sequence

// New MacroAssembler Interfaces from Power
// TODO(Zen): Not sure if we need this

void MacroAssembler::LoadRepresentation(Register dst,
                                        const MemOperand& mem,
                                        Representation r,
                                        Register scratch) {
  DCHECK(!r.IsDouble());
  if (r.IsInteger8()) {
    LoadB(dst, mem);
    lgbr(dst, dst);
  } else if (r.IsUInteger8()) {
    LoadB(dst, mem);
  } else if (r.IsInteger16()) {
    LoadHalfWordP(dst, mem, scratch);
    lghr(dst, dst);
  } else if (r.IsUInteger16()) {
    LoadHalfWordP(dst, mem, scratch);
#if V8_TARGET_ARCH_S390X
  } else if (r.IsInteger32()) {
    LoadW(dst, mem, scratch);
#endif
  } else {
    LoadP(dst, mem, scratch);
  }
}


void MacroAssembler::StoreRepresentation(Register src,
                                         const MemOperand& mem,
                                         Representation r,
                                         Register scratch) {
  DCHECK(!r.IsDouble());
  if (r.IsInteger8() || r.IsUInteger8()) {
    StoreByte(src, mem, scratch);
  } else if (r.IsInteger16() || r.IsUInteger16()) {
    StoreHalfWord(src, mem, scratch);
#if V8_TARGET_ARCH_S390X
  } else if (r.IsInteger32()) {
    StoreW(src, mem, scratch);
#endif
  } else {
    if (r.IsHeapObject()) {
      AssertNotSmi(src);
    } else if (r.IsSmi()) {
      AssertSmi(src);
    }
    StoreP(src, mem, scratch);
  }
}


void MacroAssembler::TestJSArrayForAllocationMemento(
    Register receiver_reg,
    Register scratch_reg,
    Label* no_memento_found) {
  ExternalReference new_space_start =
      ExternalReference::new_space_start(isolate());
  ExternalReference new_space_allocation_top =
      ExternalReference::new_space_allocation_top_address(isolate());
  AddP(scratch_reg, receiver_reg,
       Operand(JSArray::kSize + AllocationMemento::kSize - kHeapObjectTag));
  CmpP(scratch_reg, Operand(new_space_start));
  blt(no_memento_found);
  mov(ip, Operand(new_space_allocation_top));
  LoadP(ip, MemOperand(ip));
  CmpP(scratch_reg, ip);
  bgt(no_memento_found);
  LoadP(scratch_reg, MemOperand(scratch_reg, -AllocationMemento::kSize));
  CmpP(scratch_reg,
       Operand(isolate()->factory()->allocation_memento_map()));
}


Register GetRegisterThatIsNotOneOf(Register reg1,
                                   Register reg2,
                                   Register reg3,
                                   Register reg4,
                                   Register reg5,
                                   Register reg6) {
  RegList regs = 0;
  if (reg1.is_valid()) regs |= reg1.bit();
  if (reg2.is_valid()) regs |= reg2.bit();
  if (reg3.is_valid()) regs |= reg3.bit();
  if (reg4.is_valid()) regs |= reg4.bit();
  if (reg5.is_valid()) regs |= reg5.bit();
  if (reg6.is_valid()) regs |= reg6.bit();

  for (int i = 0; i < Register::NumAllocatableRegisters(); i++) {
    Register candidate = Register::FromAllocationIndex(i);
    if (regs & candidate.bit()) continue;
    return candidate;
  }
  UNREACHABLE();
  return no_reg;
}


void MacroAssembler::JumpIfDictionaryInPrototypeChain(
    Register object,
    Register scratch0,
    Register scratch1,
    Label* found) {
  DCHECK(!scratch1.is(scratch0));
  Factory* factory = isolate()->factory();
  Register current = scratch0;
  Label loop_again;

  // scratch contained elements pointer.
  LoadRR(current, object);

  // Loop based on the map going up the prototype chain.
  bind(&loop_again);
  LoadP(current, FieldMemOperand(current, HeapObject::kMapOffset));
  LoadlB(scratch1, FieldMemOperand(current, Map::kBitField2Offset));
  DecodeField<Map::ElementsKindBits>(scratch1);
  CmpP(scratch1, Operand(DICTIONARY_ELEMENTS));
  beq(found);
  LoadP(current, FieldMemOperand(current, Map::kPrototypeOffset));
  CmpP(current, Operand(factory->null_value()));
  bne(&loop_again);
}


void MacroAssembler::mov(Register dst, const Operand& src) {
  BlockTrampolinePoolScope block_trampoline_pool(this);
  if (src.rmode_ != kRelocInfo_NONEPTR) {
    // some form of relocation needed
    RecordRelocInfo(src.rmode_, src.imm_);
  }

#if V8_TARGET_ARCH_S390X
  int64_t value = src.immediate();
  int32_t hi_32 = static_cast<int64_t>(value) >> 32;
  int32_t lo_32 = static_cast<int32_t>(value);

  iihf(dst, Operand(hi_32));
  iilf(dst, Operand(lo_32));
#else
  int value = src.immediate();
  iilf(dst, Operand(value));
#endif
}


void MacroAssembler::Mul(Register dst, Register src1, Register src2) {
  Move(dst, src1);
  MulP(dst, src2);
}


void MacroAssembler::DivP(Register dividend, Register divider) {
  // have to make sure the src and dst are reg pairs
  DCHECK(dividend.code() % 2 == 0);
#if V8_TARGET_ARCH_S390X
  dsgr(dividend, divider);
#else
  dr(dividend, divider);
#endif
}


void MacroAssembler::MulP(Register dst, const Operand& opnd) {
#if V8_TARGET_ARCH_S390X
  msgfi(dst, opnd);
#else
  msfi(dst, opnd);
#endif
}


void MacroAssembler::MulP(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
  msgr(dst, src);
#else
  msr(dst, src);
#endif
}


void MacroAssembler::MulP(Register dst, const MemOperand& opnd) {
#if V8_TARGET_ARCH_S390X
  if (is_uint16(opnd.offset())) {
    ms(dst, opnd);
  } else if (is_int20(opnd.offset())) {
    msy(dst, opnd);
  } else {
    UNIMPLEMENTED();
  }
#else
  if (is_int20(opnd.offset())) {
    msg(dst, opnd);
  } else {
    UNIMPLEMENTED();
  }
#endif
}


//----------------------------------------------------------------------------
//  Add Instructions
//----------------------------------------------------------------------------

// Add 32-bit (Register dst = Register dst + Immediate opnd)
void MacroAssembler::Add32(Register dst, const Operand& opnd) {
  if (is_int16(opnd.immediate()))
    ahi(dst, opnd);
  else
    afi(dst, opnd);
}


// Add Pointer Size (Register dst = Register dst + Immediate opnd)
void MacroAssembler::AddP(Register dst, const Operand& opnd) {
#if V8_TARGET_ARCH_S390X
  if (is_int16(opnd.immediate()))
    aghi(dst, opnd);
  else
    agfi(dst, opnd);
#else
  Add32(dst, opnd);
#endif
}


// Add 32-bit (Register dst = Register src + Immediate opnd)
void MacroAssembler::Add32(Register dst, Register src, const Operand& opnd) {
  if (!dst.is(src)) {
    if (CpuFeatures::IsSupported(DISTINCT_OPS) && is_int16(opnd.immediate())) {
      ahik(dst, src, opnd);
      return;
    }
    lr(dst, src);
  }
  Add32(dst, opnd);
}


// Add Pointer Size (Register dst = Register src + Immediate opnd)
void MacroAssembler::AddP(Register dst, Register src, const Operand& opnd) {
  if (!dst.is(src)) {
    if (CpuFeatures::IsSupported(DISTINCT_OPS) && is_int16(opnd.immediate())) {
      AddPImm_RRI(dst, src, opnd);
      return;
    }
    LoadRR(dst, src);
  }
  AddP(dst, opnd);
}


// Add 32-bit (Register dst = Register dst + Register src)
void MacroAssembler::Add32(Register dst, Register src) {
  ar(dst, src);
}


// Add Pointer Size (Register dst = Register dst + Register src)
void MacroAssembler::AddP(Register dst, Register src) {
  AddRR(dst, src);
}


// Add Pointer Size with src extension
//     (Register dst(ptr) = Register dst (ptr) + Register src (32 | 32->64))
// src is treated as a 32-bit signed integer, which is sign extended to
// 64-bit if necessary.
void MacroAssembler::AddP_ExtendSrc(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
  agfr(dst, src);
#else
  ar(dst, src);
#endif
}


// Add 32-bit (Register dst = Register src1 + Register src2)
void MacroAssembler::Add32(Register dst, Register src1, Register src2) {
  if (!dst.is(src1) && !dst.is(src2)) {
    // We prefer to generate AR/AGR, over the non clobbering ARK/AGRK
    // as AR is a smaller instruction
    if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
      ark(dst, src1, src2);
      return;
    } else {
      lr(dst, src1);
    }
  } else if (dst.is(src2)) {
    src2 = src1;
  }
  ar(dst, src2);
}


// Add Pointer Size (Register dst = Register src1 + Register src2)
void MacroAssembler::AddP(Register dst, Register src1, Register src2) {
  if (!dst.is(src1) && !dst.is(src2)) {
    // We prefer to generate AR/AGR, over the non clobbering ARK/AGRK
    // as AR is a smaller instruction
    if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
      AddP_RRR(dst, src1, src2);
      return;
    } else {
      LoadRR(dst, src1);
    }
  } else if (dst.is(src2)) {
    src2 = src1;
  }
  AddRR(dst, src2);
}


// Add Pointer Size with src extension
//      (Register dst (ptr) = Register dst (ptr) + Register src1 (ptr) +
//                            Register src2 (32 | 32->64))
// src is treated as a 32-bit signed integer, which is sign extended to
// 64-bit if necessary.
void MacroAssembler::AddP_ExtendSrc(Register dst, Register src1,
                                    Register src2) {
#if V8_TARGET_ARCH_S390X
  if (dst.is(src2)) {
    // The source we need to sign extend is the same as result.
    lgfr(dst, src2);
    agr(dst, src1);
  } else {
    if (!dst.is(src1))
      LoadRR(dst, src1);
    agfr(dst, src2);
  }
#else
  AddP(dst, src1, src2);
#endif
}


// Add 32-bit (Register-Memory)
void MacroAssembler::Add32(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
  if (is_uint12(opnd.offset()))
    a(dst, opnd);
  else
    ay(dst, opnd);
}


// Add Pointer Size (Register-Memory)
void MacroAssembler::AddP(Register dst, const MemOperand& opnd) {
#if V8_TARGET_ARCH_S390X
  DCHECK(is_int20(opnd.offset()));
  ag(dst, opnd);
#else
  Add32(dst, opnd);
#endif
}


// Add Pointer Size with src extension
//      (Register dst (ptr) = Register dst (ptr) + Mem opnd (32 | 32->64))
// src is treated as a 32-bit signed integer, which is sign extended to
// 64-bit if necessary.
void MacroAssembler::AddP_ExtendSrc(Register dst, const MemOperand& opnd) {
#if V8_TARGET_ARCH_S390X
  DCHECK(is_int20(opnd.offset()));
  agf(dst, opnd);
#else
  Add32(dst, opnd);
#endif
}


// Add 32-bit (Memory - Immediate)
void MacroAssembler::Add32(const MemOperand& opnd, const Operand& imm) {
  DCHECK(is_int8(imm.immediate()));
  DCHECK(is_int20(opnd.offset()));
  DCHECK(CpuFeatures::IsSupported(GENERAL_INSTR_EXT));
  asi(opnd, imm);
}


// Add Pointer-sized (Memory - Immediate)
void MacroAssembler::AddP(const MemOperand& opnd, const Operand& imm) {
  DCHECK(is_int8(imm.immediate()));
  DCHECK(is_int20(opnd.offset()));
  DCHECK(CpuFeatures::IsSupported(GENERAL_INSTR_EXT));
#if V8_TARGET_ARCH_S390X
  agsi(opnd, imm);
#else
  asi(opnd, imm);
#endif
}




//----------------------------------------------------------------------------
//  Add Logical Instructions
//----------------------------------------------------------------------------

// Add Logical 32-bit (Register dst = Register dst + Immediate opnd)
void MacroAssembler::AddLogical(Register dst, const Operand& imm) {
  alfi(dst, imm);
}


// Add Logical Pointer Size (Register dst = Register dst + Immediate opnd)
void MacroAssembler::AddLogicalP(Register dst, const Operand& imm) {
#ifdef V8_TARGET_ARCH_S390X
  algfi(dst, imm);
#else
  AddLogical(dst, imm);
#endif
}


// Add Logical 32-bit (Register-Memory)
void MacroAssembler::AddLogical(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
  if (is_uint12(opnd.offset()))
    al_z(dst, opnd);
  else
    aly(dst, opnd);
}


// Add Logical Pointer Size (Register-Memory)
void MacroAssembler::AddLogicalP(Register dst, const MemOperand& opnd) {
#if V8_TARGET_ARCH_S390X
  DCHECK(is_int20(opnd.offset()));
  alg(dst, opnd);
#else
  AddLogical(dst, opnd);
#endif
}


//----------------------------------------------------------------------------
//  Subtract Instructions
//----------------------------------------------------------------------------

// Subtract 32-bit (Register dst = Register dst - Immediate opnd)
void MacroAssembler::Sub32(Register dst, const Operand& imm) {
  Add32(dst, Operand(-(imm.imm_)));
}


// Subtract Pointer Size (Register dst = Register dst - Immediate opnd)
void MacroAssembler::SubP(Register dst, const Operand& imm) {
  AddP(dst, Operand(-(imm.imm_)));
}


// Subtract 32-bit (Register dst = Register src - Immediate opnd)
void MacroAssembler::Sub32(Register dst, Register src, const Operand& imm) {
  Add32(dst, src, Operand(-(imm.imm_)));
}


// Subtract Pointer Sized (Register dst = Register src - Immediate opnd)
void MacroAssembler::SubP(Register dst, Register src, const Operand& imm) {
  AddP(dst, src, Operand(-(imm.imm_)));
}


// Subtract 32-bit (Register dst = Register dst - Register src)
void MacroAssembler::Sub32(Register dst, Register src) {
  sr(dst, src);
}


// Subtract Pointer Size (Register dst = Register dst - Register src)
void MacroAssembler::SubP(Register dst, Register src) {
  SubRR(dst, src);
}


// Subtract Pointer Size with src extension
//     (Register dst(ptr) = Register dst (ptr) - Register src (32 | 32->64))
// src is treated as a 32-bit signed integer, which is sign extended to
// 64-bit if necessary.
void MacroAssembler::SubP_ExtendSrc(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
  sgfr(dst, src);
#else
  sr(dst, src);
#endif
}


// Subtract 32-bit (Register = Register - Register)
void MacroAssembler::Sub32(Register dst, Register src1, Register src2) {
  // Use non-clobbering version if possible
  if (CpuFeatures::IsSupported(DISTINCT_OPS) && !dst.is(src1)) {
    srk(dst, src1, src2);
    return;
  }
  if (!dst.is(src1) && !dst.is(src2))
    lr(dst, src1);
  // In scenario where we have dst = src - dst, we need to swap and negate
  if (!dst.is(src1) && dst.is(src2)) {
    sr(dst, src1);  // dst = (dst - src)
    lcr(dst, dst);  // dst = -dst
  } else {
    sr(dst, src2);
  }
}


// Subtract Pointer Sized (Register = Register - Register)
void MacroAssembler::SubP(Register dst, Register src1, Register src2) {
  // Use non-clobbering version if possible
  if (CpuFeatures::IsSupported(DISTINCT_OPS) && !dst.is(src1)) {
    SubP_RRR(dst, src1, src2);
    return;
  }
  if (!dst.is(src1) && !dst.is(src2))
    LoadRR(dst, src1);
  // In scenario where we have dst = src - dst, we need to swap and negate
  if (!dst.is(src1) && dst.is(src2)) {
    SubP(dst, src1);  // dst = (dst - src)
    LoadComplementRR(dst, dst);  // dst = -dst
  } else {
    SubP(dst, src2);
  }
}


// Subtract Pointer Size with src extension
//     (Register dst(ptr) = Register dst (ptr) - Register src (32 | 32->64))
// src is treated as a 32-bit signed integer, which is sign extended to
// 64-bit if necessary.
void MacroAssembler::SubP_ExtendSrc(Register dst, Register src1,
                                    Register src2) {
#if V8_TARGET_ARCH_S390X
  if (!dst.is(src1) && !dst.is(src2))
    LoadRR(dst, src1);

  // In scenario where we have dst = src - dst, we need to swap and negate
  if (!dst.is(src1) && dst.is(src2)) {
    lgfr(dst, dst);  // Sign extend this operand first.
    SubP(dst, src1);  // dst = (dst - src)
    LoadComplementRR(dst, dst);  // dst = -dst
  } else {
    sgfr(dst, src2);
  }
#else
  SubP(dst, src1, src2);
#endif
}


// Subtract 32-bit (Register-Memory)
void MacroAssembler::Sub32(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
  if (is_uint12(opnd.offset()))
    s(dst, opnd);
  else
    sy(dst, opnd);
}


// Subtract Pointer Sized (Register - Memory)
void MacroAssembler::SubP(Register dst, const MemOperand& opnd) {
#if V8_TARGET_ARCH_S390X
  sg(dst, opnd);
#else
  Sub32(dst, opnd);
#endif
}


// Subtract Pointer Size with src extension
//      (Register dst (ptr) = Register dst (ptr) - Mem opnd (32 | 32->64))
// src is treated as a 32-bit signed integer, which is sign extended to
// 64-bit if necessary.
void MacroAssembler::SubP_ExtendSrc(Register dst, const MemOperand& opnd) {
#if V8_TARGET_ARCH_S390X
  DCHECK(is_int20(opnd.offset()));
  sgf(dst, opnd);
#else
  Sub32(dst, opnd);
#endif
}


//----------------------------------------------------------------------------
//  Subtract Logical Instructions
//----------------------------------------------------------------------------

// Subtract Logical 32-bit (Register - Memory)
void MacroAssembler::SubLogical(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
  if (is_uint12(opnd.offset()))
    sl(dst, opnd);
  else
    sly(dst, opnd);
}


// Subtract Logical Pointer Sized (Register - Memory)
void MacroAssembler::SubLogicalP(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
#if V8_TARGET_ARCH_S390X
  slgf(dst, opnd);
#else
  SubLogical(dst, opnd);
#endif
}


// Subtract Logical Pointer Size with src extension
//      (Register dst (ptr) = Register dst (ptr) - Mem opnd (32 | 32->64))
// src is treated as a 32-bit signed integer, which is sign extended to
// 64-bit if necessary.
void MacroAssembler::SubLogicalP_ExtendSrc(Register dst,
                                           const MemOperand& opnd) {
#if V8_TARGET_ARCH_S390X
  DCHECK(is_int20(opnd.offset()));
  slgf(dst, opnd);
#else
  SubLogical(dst, opnd);
#endif
}


//----------------------------------------------------------------------------
//  Bitwise Operations
//----------------------------------------------------------------------------

// AND 32-bit - dst = dst & src
void MacroAssembler::And(Register dst, Register src) {
  nr(dst, src);
}


// AND Pointer Size - dst = dst & src
void MacroAssembler::AndP(Register dst, Register src) {
  AndRR(dst, src);
}


// Non-clobbering AND 32-bit - dst = src1 & src1
void MacroAssembler::And(Register dst, Register src1, Register src2) {
  if (!dst.is(src1) && !dst.is(src2)) {
    // We prefer to generate XR/XGR, over the non clobbering XRK/XRK
    // as XR is a smaller instruction
    if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
      nrk(dst, src1, src2);
      return;
    } else {
      lr(dst, src1);
    }
  } else if (dst.is(src2)) {
    src2 = src1;
  }
  And(dst, src2);
}


// Non-clobbering AND pointer size - dst = src1 & src1
void MacroAssembler::AndP(Register dst, Register src1, Register src2) {
  if (!dst.is(src1) && !dst.is(src2)) {
    // We prefer to generate XR/XGR, over the non clobbering XRK/XRK
    // as XR is a smaller instruction
    if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
      AndP_RRR(dst, src1, src2);
      return;
    } else {
      LoadRR(dst, src1);
    }
  } else if (dst.is(src2)) {
    src2 = src1;
  }
  AndP(dst, src2);
}


// AND 32-bit (Reg - Mem)
void MacroAssembler::And(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
  if (is_uint12(opnd.offset()))
    n(dst, opnd);
  else
    ny(dst, opnd);
}


// AND Pointer Size (Reg - Mem)
void MacroAssembler::AndP(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
#if V8_TARGET_ARCH_S390X
  ng(dst, opnd);
#else
  And(dst, opnd);
#endif
}


// AND 32-bit - dst = dst & imm
void MacroAssembler::And(Register dst, const Operand& opnd) {
  nilf(dst, opnd);
}


// AND Pointer Size - dst = dst & imm
void MacroAssembler::AndP(Register dst, const Operand& opnd) {
#if V8_TARGET_ARCH_S390X
  intptr_t value = opnd.imm_;
  if (value >> 32 != -1) {
    // this may not work b/c condition code won't be set correctly
    nihf(dst, Operand(value >> 32));
  }
  nilf(dst, Operand(value & 0xFFFFFFFF));
#else
  And(dst, opnd);
#endif
}


// AND 32-bit - dst = src & imm
void MacroAssembler::And(Register dst, Register src, const Operand& opnd) {
  if (!dst.is(src))
    lr(dst, src);
  nilf(dst, opnd);
}


// AND Pointer Size - dst = src & imm
void MacroAssembler::AndP(Register dst, Register src, const Operand& opnd) {
  // Try to exploit RISBG first
  intptr_t value = opnd.imm_;
  if (CpuFeatures::IsSupported(GENERAL_INSTR_EXT)) {
    intptr_t shifted_value = value;
    int trailing_zeros = 0;

    // We start checking how many trailing zeros are left at the end.
    while ((0 != shifted_value) && (0 == (shifted_value & 1))) {
       trailing_zeros++;
       shifted_value >>= 1;
    }

    // If temp (value with right-most set of zeros shifted out) is 1 less
    // than power of 2, we have consecutive bits of 1.
    // Special case: If shift_value is zero, we cannot use RISBG, as it requires
    //               selection of at least 1 bit.
    if ((0 != shifted_value) && IsPowerOf2(shifted_value + 1)) {
      int startBit = 32 + CompilerIntrinsics::CountLeadingZeros(shifted_value) -
                     trailing_zeros;
      int endBit = 63 - trailing_zeros;
      // Start: startBit, End: endBit, Shift = 0, true = zero unselected bits.
      risbg(dst, src, Operand(startBit), Operand(endBit), Operand::Zero(),
            true);
      return;
    } else if (-1 == shifted_value) {
    // A Special case in which all top bits up to MSB are 1's.  In this case,
    // we can set startBit to be 0.
      int endBit = 63 - trailing_zeros;
      risbg(dst, src, Operand::Zero(), Operand(endBit), Operand::Zero(), true);
      return;
    }
  }

  // If we are &'ing zero, we can just whack the dst register and skip copy
  if (!dst.is(src) && (0 != value))
     LoadRR(dst, src);
  AndP(dst, opnd);
}


// OR 32-bit - dst = dst & src
void MacroAssembler::Or(Register dst, Register src) {
  or_z(dst, src);
}


// OR Pointer Size - dst = dst & src
void MacroAssembler::OrP(Register dst, Register src) {
  OrRR(dst, src);
}


// Non-clobbering OR 32-bit - dst = src1 & src1
void MacroAssembler::Or(Register dst, Register src1, Register src2) {
  if (!dst.is(src1) && !dst.is(src2)) {
    // We prefer to generate XR/XGR, over the non clobbering XRK/XRK
    // as XR is a smaller instruction
    if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
      ork(dst, src1, src2);
      return;
    } else {
      lr(dst, src1);
    }
  } else if (dst.is(src2)) {
    src2 = src1;
  }
  Or(dst, src2);
}


// Non-clobbering OR pointer size - dst = src1 & src1
void MacroAssembler::OrP(Register dst, Register src1, Register src2) {
  if (!dst.is(src1) && !dst.is(src2)) {
    // We prefer to generate XR/XGR, over the non clobbering XRK/XRK
    // as XR is a smaller instruction
    if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
      OrP_RRR(dst, src1, src2);
      return;
    } else {
      LoadRR(dst, src1);
    }
  } else if (dst.is(src2)) {
    src2 = src1;
  }
  OrP(dst, src2);
}


// OR 32-bit (Reg - Mem)
void MacroAssembler::Or(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
  if (is_uint12(opnd.offset()))
    o(dst, opnd);
  else
    oy(dst, opnd);
}


// OR Pointer Size (Reg - Mem)
void MacroAssembler::OrP(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
#if V8_TARGET_ARCH_S390X
  og(dst, opnd);
#else
  Or(dst, opnd);
#endif
}


// OR 32-bit - dst = dst & imm
void MacroAssembler::Or(Register dst, const Operand& opnd) {
  oilf(dst, opnd);
}


// OR Pointer Size - dst = dst & imm
void MacroAssembler::OrP(Register dst, const Operand& opnd) {
#if V8_TARGET_ARCH_S390X
  intptr_t value = opnd.imm_;
  if (value >> 32 != 0) {
    // this may not work b/c condition code won't be set correctly
    oihf(dst, Operand(value >> 32));
  }
  oilf(dst, Operand(value & 0xFFFFFFFF));
#else
  Or(dst, opnd);
#endif
}


// OR 32-bit - dst = src & imm
void MacroAssembler::Or(Register dst, Register src, const Operand& opnd) {
  if (!dst.is(src))
    lr(dst, src);
  oilf(dst, opnd);
}


// OR Pointer Size - dst = src & imm
void MacroAssembler::OrP(Register dst, Register src, const Operand& opnd) {
  if (!dst.is(src))
    LoadRR(dst, src);
  OrP(dst, opnd);
}


// XOR 32-bit - dst = dst & src
void MacroAssembler::Xor(Register dst, Register src) {
  xr(dst, src);
}


// XOR Pointer Size - dst = dst & src
void MacroAssembler::XorP(Register dst, Register src) {
  XorRR(dst, src);
}


// Non-clobbering XOR 32-bit - dst = src1 & src1
void MacroAssembler::Xor(Register dst, Register src1, Register src2) {
  if (!dst.is(src1) && !dst.is(src2)) {
    // We prefer to generate XR/XGR, over the non clobbering XRK/XRK
    // as XR is a smaller instruction
    if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
      xrk(dst, src1, src2);
      return;
    } else {
      lr(dst, src1);
    }
  } else if (dst.is(src2)) {
    src2 = src1;
  }
  Xor(dst, src2);
}


// Non-clobbering XOR pointer size - dst = src1 & src1
void MacroAssembler::XorP(Register dst, Register src1, Register src2) {
  if (!dst.is(src1) && !dst.is(src2)) {
    // We prefer to generate XR/XGR, over the non clobbering XRK/XRK
    // as XR is a smaller instruction
    if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
      XorP_RRR(dst, src1, src2);
      return;
    } else {
      LoadRR(dst, src1);
    }
  } else if (dst.is(src2)) {
    src2 = src1;
  }
  XorP(dst, src2);
}


// XOR 32-bit (Reg - Mem)
void MacroAssembler::Xor(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
  if (is_uint12(opnd.offset()))
    x(dst, opnd);
  else
    xy(dst, opnd);
}


// XOR Pointer Size (Reg - Mem)
void MacroAssembler::XorP(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
#if V8_TARGET_ARCH_S390X
  xg(dst, opnd);
#else
  Xor(dst, opnd);
#endif
}


// XOR 32-bit - dst = dst & imm
void MacroAssembler::Xor(Register dst, const Operand& opnd) {
  xilf(dst, opnd);
}


// XOR Pointer Size - dst = dst & imm
void MacroAssembler::XorP(Register dst, const Operand& opnd) {
#if V8_TARGET_ARCH_S390X
  intptr_t value = opnd.imm_;
  xihf(dst, Operand(value >> 32));
  xilf(dst, Operand(value & 0xFFFFFFFF));
#else
  Xor(dst, opnd);
#endif
}


// XOR 32-bit - dst = src & imm
void MacroAssembler::Xor(Register dst, Register src, const Operand& opnd) {
  if (!dst.is(src))
    lr(dst, src);
  xilf(dst, opnd);
}


// XOR Pointer Size - dst = src & imm
void MacroAssembler::XorP(Register dst, Register src, const Operand& opnd) {
  if (!dst.is(src))
    LoadRR(dst, src);
  XorP(dst, opnd);
}


void MacroAssembler::NotP(Register dst) {
#if V8_TARGET_ARCH_S390X
  xihf(dst, Operand(0xFFFFFFFF));
  xilf(dst, Operand(0xFFFFFFFF));
#else
  XorP(dst, Operand(0xFFFFFFFF));
#endif
}


// works the same as mov
void MacroAssembler::Load(Register dst, const Operand& opnd) {
  intptr_t value = opnd.immediate();
  if (is_int16(value)) {
#if V8_TARGET_ARCH_S390X
    lghi(dst, opnd);
#else
    lhi(dst, opnd);
#endif
  } else {
#if V8_TARGET_ARCH_S390X
    llilf(dst, opnd);
#else
    iilf(dst, opnd);
#endif
  }
}


void MacroAssembler::Load(Register dst, const MemOperand& opnd) {
  DCHECK(is_int20(opnd.offset()));
#if V8_TARGET_ARCH_S390X
  lgf(dst, opnd);  // 64<-32
#else
  if (is_uint12(opnd.offset())) {
    l(dst, opnd);
  } else {
    ly(dst, opnd);
  }
#endif
}


//-----------------------------------------------------------------------------
//  Compare Helpers
//-----------------------------------------------------------------------------

// Compare 32-bit Register vs Register
void MacroAssembler::Cmp32(Register src1, Register src2) {
  cr_z(src1, src2);
}


// Compare Pointer Sized Register vs Register
void MacroAssembler::CmpP(Register src1, Register src2) {
#if V8_TARGET_ARCH_S390X
  cgr(src1, src2);
#else
  Cmp32(src1, src2);
#endif
}


// Compare 32-bit Register vs Immediate
// This helper will set up proper relocation entries if required.
void MacroAssembler::Cmp32(Register dst, const Operand& opnd) {
  if (opnd.rmode_ == kRelocInfo_NONEPTR) {
    intptr_t value = opnd.immediate();
    if (is_int16(value))
      chi(dst, opnd);
    else
      cfi(dst, opnd);
  } else {
    // Need to generate relocation record here
    RecordRelocInfo(opnd.rmode_, opnd.imm_);
    cfi(dst, opnd);
  }
}


// Compare Pointer Sized  Register vs Immediate
// This helper will set up proper relocation entries if required.
void MacroAssembler::CmpP(Register dst, const Operand& opnd) {
#if V8_TARGET_ARCH_S390X
  if (opnd.rmode_ == kRelocInfo_NONEPTR) {
    cgfi(dst, opnd);
  } else {
    mov(r0, opnd);   // Need to generate 64-bit relocation
    cgr(dst, r0);
  }
#else
  Cmp32(dst, opnd);
#endif
}


// Compare 32-bit Register vs Memory
void MacroAssembler::Cmp32(Register dst, const MemOperand& opnd) {
  // make sure offset is within 20 bit range
  DCHECK(is_int20(opnd.offset()));
  if (is_uint12(opnd.offset()))
    c(dst, opnd);
  else
    cy(dst, opnd);
}


// Compare Pointer Size Register vs Memory
void MacroAssembler::CmpP(Register dst, const MemOperand& opnd) {
  // make sure offset is within 20 bit range
  DCHECK(is_int20(opnd.offset()));
#if V8_TARGET_ARCH_S390X
  cg(dst, opnd);
#else
  Cmp32(dst, opnd);
#endif
}


//-----------------------------------------------------------------------------
// Compare Logical Helpers
//-----------------------------------------------------------------------------

// Compare Logical 32-bit Register vs Register
void MacroAssembler::CmpLogical32(Register dst, Register src) {
  clr(dst, src);
}


// Compare Logical Pointer Sized Register vs Register
void MacroAssembler::CmpLogicalP(Register dst, Register src) {
#ifdef V8_TARGET_ARCH_S390X
  clgr(dst, src);
#else
  CmpLogical32(dst, src);
#endif
}


// Compare Logical 32-bit Register vs Immediate
void MacroAssembler::CmpLogical32(Register dst, const Operand& opnd) {
  clfi(dst, opnd);
}


// Compare Logical Pointer Sized Register vs Immediate
void MacroAssembler::CmpLogicalP(Register dst, const Operand& opnd) {
#if V8_TARGET_ARCH_S390X
  DCHECK(static_cast<uint32_t>(opnd.immediate() >> 32) == 0);
  clgfi(dst, opnd);
#else
  CmpLogical32(dst, opnd);
#endif
}


// Compare Logical 32-bit Register vs Memory
void MacroAssembler::CmpLogical32(Register dst, const MemOperand& opnd) {
  // make sure offset is within 20 bit range
  DCHECK(is_int20(opnd.offset()));
  if (is_uint12(opnd.offset()))
    cl(dst, opnd);
  else
    cly(dst, opnd);
}


// Compare Logical Pointer Sized Register vs Memory
void MacroAssembler::CmpLogicalP(Register dst, const MemOperand& opnd) {
  // make sure offset is within 20 bit range
  DCHECK(is_int20(opnd.offset()));
#if V8_TARGET_ARCH_S390X
  clg(dst, opnd);
#else
  CmpLogical32(dst, opnd);
#endif
}


// Compare Logical Byte (Mem - Imm)
void MacroAssembler::CmpLogicalByte(const MemOperand& mem, const Operand& imm) {
  DCHECK(is_uint8(imm.immediate()));
  if (is_uint12(mem.offset()))
    cli(mem, imm);
  else
    cliy(mem, imm);
}



void MacroAssembler::Branch(Condition c, const Operand& opnd) {
  intptr_t value = opnd.immediate();
  if (is_int16(value))
    brc(c, opnd);
  else
    brcl(c, opnd);
}


// Branch On Count.  Decrement R1, and branch if R1 != 0.
void MacroAssembler::BranchOnCount(Register r1, Label *l) {
  int32_t offset = branch_offset(l, false);
  positions_recorder()->WriteRecordedPositions();
  if (is_int16(offset)) {
#if V8_TARGET_ARCH_S390X
    brctg(r1, Operand(offset));
#else
    brct(r1, Operand(offset));
#endif
  } else {
    AddP(r1, Operand(-1));
    Branch(ne, Operand(offset));
  }
}


void MacroAssembler::LoadIntLiteral(Register dst, int value) {
  Load(dst, Operand(value));
}


void MacroAssembler::LoadSmiLiteral(Register dst, Smi *smi) {
  intptr_t value = reinterpret_cast<intptr_t>(smi);
#if V8_TARGET_ARCH_S390X
  DCHECK((value & 0xffffffff) == 0);
  // The smi value is loaded in upper 32-bits.  Lower 32-bit are zeros.
  llihf(dst, Operand(value >> 32));
#else
  llilf(dst, Operand(value));
#endif
}


void MacroAssembler::LoadDoubleLiteral(DoubleRegister result,
                                       double value,
                                       Register scratch) {
  uint64_t int_val = BitCast<uint64_t, double>(value);
  uint32_t hi_32 = int_val >> 32;
  uint32_t lo_32 = static_cast<uint32_t>(int_val);

  // Load the 64-bit value into a GPR, then transfer it to FPR via LDGR
  iihf(scratch, Operand(hi_32));
  iilf(scratch, Operand(lo_32));
  ldgr(result, scratch);
}


void MacroAssembler::CmpSmiLiteral(Register src1, Smi *smi, Register scratch) {
#if V8_TARGET_ARCH_S390X
  LoadSmiLiteral(scratch, smi);
  cgr(src1, scratch);
#else
  // CFI takes 32-bit immediate.
  cfi(src1, Operand(smi));
#endif
}


void MacroAssembler::CmpLogicalSmiLiteral(Register src1, Smi *smi,
                                          Register scratch) {
#if V8_TARGET_ARCH_S390X
  LoadSmiLiteral(scratch, smi);
  clgr(src1, scratch);
#else
  // CLFI takes 32-bit immediate
  clfi(src1, Operand(smi));
#endif
}


void MacroAssembler::AddSmiLiteral(Register dst, Register src, Smi *smi,
                                   Register scratch) {
#if V8_TARGET_ARCH_S390X
  LoadSmiLiteral(scratch, smi);
  AddP(dst, src, scratch);
#else
  AddP(dst, src, Operand(reinterpret_cast<intptr_t>(smi)));
#endif
}


void MacroAssembler::SubSmiLiteral(Register dst, Register src, Smi *smi,
                                   Register scratch) {
#if V8_TARGET_ARCH_S390X
  LoadSmiLiteral(scratch, smi);
  SubP(dst, src, scratch);
#else
  AddP(dst, src, Operand(-(reinterpret_cast<intptr_t>(smi))));
#endif
}


void MacroAssembler::AndSmiLiteral(Register dst, Register src, Smi *smi) {
  if (!dst.is(src))
    LoadRR(dst, src);
#if V8_TARGET_ARCH_S390X
  DCHECK((reinterpret_cast<intptr_t>(smi) & 0xffffffff) == 0);
  int value = static_cast<int>(reinterpret_cast<intptr_t>(smi) >> 32);
  nihf(dst, Operand(value));
#else
  nilf(dst, Operand(reinterpret_cast<int>(smi)));
#endif
}


// Load a "pointer" sized value from the memory location
void MacroAssembler::LoadP(Register dst, const MemOperand& mem,
                           Register scratch) {
  int offset = mem.offset();

  if (!scratch.is(no_reg) && !is_int20(offset)) {
    /* cannot use d-form */
    LoadIntLiteral(scratch, offset);
#if V8_TARGET_ARCH_S390X
    lg(dst, MemOperand(mem.rb(), scratch));
#else
    l(dst, MemOperand(mem.rb(), scratch));
#endif
  } else {
#if V8_TARGET_ARCH_S390X
    lg(dst, mem);
#else
    if (is_uint12(offset)) {
      l(dst, mem);
    } else {
      ly(dst, mem);
    }
#endif
  }
}


// Store a "pointer" sized value to the memory location
void MacroAssembler::StoreP(Register src, const MemOperand& mem,
                           Register scratch) {
  if (!is_int20(mem.offset())) {
    DCHECK(!scratch.is(no_reg));
    LoadIntLiteral(scratch, mem.offset());
#if V8_TARGET_ARCH_S390X
    stg(src, MemOperand(mem.rb(), scratch));
#else
    st(src, MemOperand(mem.rb(), scratch));
#endif
  } else {
#if V8_TARGET_ARCH_S390X
    stg(src, mem);
#else
    // StoreW will try to generate ST if offset fits, otherwise
    // it'll generate STY.
    StoreW(src, mem);
#endif
  }
}


// Store a "pointer" sized constant to the memory location
void MacroAssembler::StoreP(const MemOperand& mem, const Operand& opnd,
                            Register scratch) {
  // Relocations not supported
  DCHECK(opnd.rmode_ == kRelocInfo_NONEPTR);

  // Try to use MVGHI/MVHI
  if (CpuFeatures::IsSupported(GENERAL_INSTR_EXT) &&
      is_uint12(mem.offset()) &&
      mem.getIndexRegister().is(r0) &&
      is_int16(opnd.imm_)) {
#if V8_TARGET_ARCH_S390X
    mvghi(mem, opnd);
#else
    mvhi(mem, opnd);
#endif
  } else {
    LoadImmP(scratch, opnd);
    StoreP(scratch, mem);
  }
}

void MacroAssembler::LoadMultipleP(Register dst1, Register dst2,
    const MemOperand& mem) {
#if V8_TARGET_ARCH_S390X
  DCHECK(is_int20(mem.offset()));
  lmg(dst1, dst2, mem);
#else
  if (is_uint12(mem.offset())) {
    lm(dst1, dst2, mem);
  } else {
    DCHECK(is_int20(mem.offset()));
    lmy(dst1, dst2, mem);
  }
#endif
}


void MacroAssembler::StoreMultipleP(Register src1, Register src2,
    const MemOperand& mem) {
#if V8_TARGET_ARCH_S390X
  DCHECK(is_int20(mem.offset()));
  stmg(src1, src2, mem);
#else
  if (is_uint12(mem.offset())) {
    stm(src1, src2, mem);
  } else {
    DCHECK(is_int20(mem.offset()));
    stmy(src1, src2, mem);
  }
#endif
}


void MacroAssembler::LoadMultipleW(Register dst1, Register dst2,
    const MemOperand& mem) {
  if (is_uint12(mem.offset())) {
    lm(dst1, dst2, mem);
  } else {
    DCHECK(is_int20(mem.offset()));
    lmy(dst1, dst2, mem);
  }
}


void MacroAssembler::StoreMultipleW(Register src1, Register src2,
    const MemOperand& mem) {
  if (is_uint12(mem.offset())) {
    stm(src1, src2, mem);
  } else {
    DCHECK(is_int20(mem.offset()));
    stmy(src1, src2, mem);
  }
}


// Load 32-bits and sign extend if necessary.
void MacroAssembler::LoadW(Register dst, const MemOperand& mem,
                           Register scratch) {
  int offset = mem.offset();

  if (!is_int20(offset)) {
    DCHECK(!scratch.is(no_reg));
    LoadIntLiteral(scratch, offset);
#if V8_TARGET_ARCH_S390X
    lgf(dst, MemOperand(mem.rb(), scratch));
#else
    l(dst, MemOperand(mem.rb(), scratch));
#endif
  } else {
#if V8_TARGET_ARCH_S390X
    lgf(dst, mem);
#else
    if (is_uint12(offset)) {
      l(dst, mem);
    } else {
      ly(dst, mem);
    }
#endif
  }
}


// Variable length depending on whether offset fits into immediate field
// MemOperand of RX or RXY format
void MacroAssembler::LoadlW(Register dst, const MemOperand& mem,
                            Register scratch) {
  Register base = mem.rb();
  int offset = mem.offset();

#if V8_TARGET_ARCH_S390X
  if (is_int20(offset)) {
    llgf(dst, mem);
  } else if (!scratch.is(no_reg)) {
    // Materialize offset into scratch register.
    LoadIntLiteral(scratch, offset);
    llgf(dst, MemOperand(base, scratch));
  } else {
    DCHECK(false);
  }
#else
  bool use_RXform = false;
  bool use_RXYform = false;
  if (is_uint12(offset)) {
    // RX-format supports unsigned 12-bits offset.
    use_RXform = true;
  } else if (is_int20(offset)) {
    // RXY-format supports signed 20-bits offset.
    use_RXYform = true;
  } else if (!scratch.is(no_reg)) {
    // Materialize offset into scratch register.
    LoadIntLiteral(scratch, offset);
  } else {
    DCHECK(false);
  }

  if (use_RXform) {
    l(dst, mem);
  } else if (use_RXYform) {
    ly(dst, mem);
  } else {
    ly(dst, MemOperand(base, scratch));
  }
#endif
}


void MacroAssembler::LoadB(Register dst, const MemOperand& mem) {
#if V8_TARGET_ARCH_S390X
  lgb(dst, mem);
#else
  lb(dst, mem);
#endif
}


void MacroAssembler::LoadlB(Register dst, const MemOperand& mem) {
#if V8_TARGET_ARCH_S390X
  llgc(dst, mem);
#else
  llc(dst, mem);
#endif
}


// Load And Test (Reg <- Reg)
void MacroAssembler::LoadAndTest32(Register dst, Register src) {
  ltr(dst, src);
}


// Load And Test
//     (Register dst(ptr) = Register src (32 | 32->64))
// src is treated as a 32-bit signed integer, which is sign extended to
// 64-bit if necessary.
void MacroAssembler::LoadAndTestP_ExtendSrc(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
  ltgfr(dst, src);
#else
  ltr(dst, src);
#endif
}


// Load And Test Pointer Sized (Reg <- Reg)
void MacroAssembler::LoadAndTestP(Register dst, Register src) {
#if V8_TARGET_ARCH_S390X
  ltgr(dst, src);
#else
  ltr(dst, src);
#endif
}


// Load And Test 32-bit (Reg <- Mem)
void MacroAssembler::LoadAndTest32(Register dst, const MemOperand& mem) {
  lt_z(dst, mem);
}


// Load And Test Pointer Sized (Reg <- Mem)
void MacroAssembler::LoadAndTestP(Register dst, const MemOperand& mem) {
#if V8_TARGET_ARCH_S390X
  ltg(dst, mem);
#else
  lt_z(dst, mem);
#endif
}


void MacroAssembler::LoadF(DoubleRegister dst, const MemOperand& mem) {
  // for 32bit and 64bit we all use 64bit floating point regs
  if (is_uint12(mem.offset())) {
    ld(dst, mem);
  } else {
    ldy(dst, mem);
  }
}


void MacroAssembler::StoreF(DoubleRegister dst, const MemOperand& mem) {
  // for 32bit and 64bit we all use 64bit floating point regs
  if (is_uint12(mem.offset())) {
    std(dst, mem);
  } else {
    stdy(dst, mem);
  }
}


void MacroAssembler::StoreShortF(DoubleRegister dst, const MemOperand& mem) {
  // for 32bit and 64bit we all use 64bit floating point regs
  if (is_uint12(mem.offset())) {
    ste(dst, mem);
  } else {
    stey(dst, mem);
  }
}


// Variable length depending on whether offset fits into immediate field
// MemOperand of RX or RXY format
void MacroAssembler::StoreW(Register src, const MemOperand& mem,
                            Register scratch) {
  Register base = mem.rb();
  int offset = mem.offset();

  bool use_RXform = false;
  bool use_RXYform = false;

  if (is_uint12(offset)) {
    // RX-format supports unsigned 12-bits offset.
    use_RXform = true;
  } else if (is_int20(offset)) {
    // RXY-format supports signed 20-bits offset.
    use_RXYform = true;
  } else if (!scratch.is(no_reg)) {
    // Materialize offset into scratch register.
    LoadIntLiteral(scratch, offset);
  } else {
    // scratch is no_reg
    DCHECK(false);
  }

  if (use_RXform) {
    st(src, mem);
  } else if (use_RXYform) {
    sty(src, mem);
  } else {
    StoreW(src, MemOperand(base, scratch));
  }
}


// Loads 16-bits half-word value from memory and sign extends to pointer
// sized register
void MacroAssembler::LoadHalfWordP(Register dst, const MemOperand& mem,
                                  Register scratch) {
  Register base = mem.rb();
  int offset = mem.offset();

  if (!is_int20(offset)) {
    DCHECK(!scratch.is(no_reg));
    LoadIntLiteral(scratch, offset);
#if V8_TARGET_ARCH_S390X
    lgh(dst, MemOperand(base, scratch));
#else
    lh(dst, MemOperand(base, scratch));
#endif
  } else {
#if V8_TARGET_ARCH_S390X
    lgh(dst, mem);
#else
    if (is_uint12(offset)) {
      lh(dst, mem);
    } else {
      lhy(dst, mem);
    }
#endif
  }
}


// Variable length depending on whether offset fits into immediate field
// MemOperand current only supports d-form
void MacroAssembler::StoreHalfWord(Register src, const MemOperand& mem,
                                   Register scratch) {
  Register base = mem.rb();
  int offset = mem.offset();

  if (is_uint12(offset)) {
    sth(src, mem);
  } else if (is_int20(offset)) {
    sthy(src, mem);
  } else {
    DCHECK(!scratch.is(no_reg));
    LoadIntLiteral(scratch, offset);
    sth(src, MemOperand(base, scratch));
  }
}


// Variable length depending on whether offset fits into immediate field
// MemOperand current only supports d-form
void MacroAssembler::StoreByte(Register src, const MemOperand& mem,
                               Register scratch) {
  Register base = mem.rb();
  int offset = mem.offset();

  if (is_uint12(offset)) {
    stc(src, mem);
  } else if (is_int20(offset)) {
    stcy(src, mem);
  } else {
    DCHECK(!scratch.is(no_reg));
    LoadIntLiteral(scratch, offset);
    stc(src, MemOperand(base, scratch));
  }
}


// Shift left logical for 32-bit integer types.
void MacroAssembler::ShiftLeft(Register dst, Register src,
                               const Operand& val) {
  if (dst.is(src)) {
    sll(dst, val);
  } else if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    sllk(dst, src, val);
  } else {
    lr(dst, src);
    sll(dst, val);
  }
}


// Shift left logical for 32-bit integer types.
void MacroAssembler::ShiftLeft(Register dst, Register src,
                               Register val) {
  DCHECK(!dst.is(val));  // The lr/sll path clobbers val.
  if (dst.is(src)) {
    sll(dst, val);
  } else if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    sllk(dst, src, val);
  } else {
    lr(dst, src);
    sll(dst, val);
  }
}


// Shift right logical for 32-bit integer types.
void MacroAssembler::ShiftRight(Register dst, Register src,
                                const Operand& val) {
  if (dst.is(src)) {
    srl(dst, val);
  } else if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    srlk(dst, src, val);
  } else {
    lr(dst, src);
    srl(dst, val);
  }
}


// Shift right logical for 32-bit integer types.
void MacroAssembler::ShiftRight(Register dst, Register src,
                                Register val) {
  DCHECK(!dst.is(val));  // The lr/srl path clobbers val.
  if (dst.is(src)) {
    srl(dst, val);
  } else if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    srlk(dst, src, val);
  } else {
    lr(dst, src);
    srl(dst, val);
  }
}


// Shift left arithmetic for 32-bit integer types.
void MacroAssembler::ShiftLeftArith(Register dst, Register src,
                                    const Operand& val) {
  if (dst.is(src)) {
    sla(dst, val);
  } else if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    slak(dst, src, val);
  } else {
    lr(dst, src);
    sla(dst, val);
  }
}


// Shift left arithmetic for 32-bit integer types.
void MacroAssembler::ShiftLeftArith(Register dst, Register src,
                                    Register val) {
  DCHECK(!dst.is(val));  // The lr/sla path clobbers val.
  if (dst.is(src)) {
    sla(dst, val);
  } else if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    slak(dst, src, val);
  } else {
    lr(dst, src);
    sla(dst, val);
  }
}


// Shift right arithmetic for 32-bit integer types.
void MacroAssembler::ShiftRightArith(Register dst, Register src,
                                     const Operand& val) {
  if (dst.is(src)) {
    sra(dst, val);
  } else if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    srak(dst, src, val);
  } else {
    lr(dst, src);
    sra(dst, val);
  }
}


// Shift right arithmetic for 32-bit integer types.
void MacroAssembler::ShiftRightArith(Register dst, Register src,
                                     Register val) {
  DCHECK(!dst.is(val));  // The lr/sra path clobbers val.
  if (dst.is(src)) {
    sra(dst, val);
  } else if (CpuFeatures::IsSupported(DISTINCT_OPS)) {
    srak(dst, src, val);
  } else {
    lr(dst, src);
    sra(dst, val);
  }
}


// Clear right most # of bits
void MacroAssembler::ClearRightImm(Register dst, Register src,
                                  const Operand& val) {
  int numBitsToClear = val.imm_ % (kPointerSize * 8);

  // Try to use RISBG if possible
  if (CpuFeatures::IsSupported(GENERAL_INSTR_EXT)) {
    int endBit = 63 - numBitsToClear;
    risbg(dst, src, Operand::Zero(), Operand(endBit), Operand::Zero(), true);
    return;
  }

  uint64_t hexMask = ~((1L << numBitsToClear) - 1);

  // S390 AND instr clobbers source.  Make a copy if necessary
  if (!dst.is(src))
    LoadRR(dst, src);

  if (numBitsToClear <= 16) {
    nill(dst, Operand(static_cast<uint16_t>(hexMask)));
  } else if (numBitsToClear <= 32) {
    nilf(dst, Operand(static_cast<uint32_t>(hexMask)));
  } else if (numBitsToClear <= 64) {
    nilf(dst, Operand(static_cast<intptr_t>(0)));
    nihf(dst, Operand(hexMask >> 32));
  }
}


#ifdef DEBUG
bool AreAliased(Register reg1,
                Register reg2,
                Register reg3,
                Register reg4,
                Register reg5,
                Register reg6,
                Register reg7,
                Register reg8) {
  int n_of_valid_regs = reg1.is_valid() + reg2.is_valid() +
      reg3.is_valid() + reg4.is_valid() + reg5.is_valid() + reg6.is_valid() +
      reg7.is_valid() + reg8.is_valid();

  RegList regs = 0;
  if (reg1.is_valid()) regs |= reg1.bit();
  if (reg2.is_valid()) regs |= reg2.bit();
  if (reg3.is_valid()) regs |= reg3.bit();
  if (reg4.is_valid()) regs |= reg4.bit();
  if (reg5.is_valid()) regs |= reg5.bit();
  if (reg6.is_valid()) regs |= reg6.bit();
  if (reg7.is_valid()) regs |= reg7.bit();
  if (reg8.is_valid()) regs |= reg8.bit();
  int n_of_non_aliasing_regs = NumRegs(regs);

  return n_of_valid_regs != n_of_non_aliasing_regs;
}
#endif


CodePatcher::CodePatcher(byte* address,
                         int instructions,
                         FlushICache flush_cache)
    : address_(address),
      size_(instructions),
      masm_(NULL, address, size_ + Assembler::kGap),
      flush_cache_(flush_cache) {
  // Create a new macro assembler pointing to the address of the code to patch.
  // The size is adjusted with kGap on order for the assembler to generate size
  // bytes of instructions without failing with buffer size constraints.
  DCHECK(masm_.reloc_info_writer.pos() == address_ + size_ + Assembler::kGap);
}


CodePatcher::~CodePatcher() {
  // Indicate that code has changed.
  if (flush_cache_ == FLUSH) {
    CpuFeatures::FlushICache(address_, size_);
  }

  // Check that the code was patched as expected.
  DCHECK(masm_.pc_ == address_ + size_);
  DCHECK(masm_.reloc_info_writer.pos() == address_ + size_ + Assembler::kGap);
}

void MacroAssembler::TruncatingDiv(Register result,
                                   Register dividend,
                                   int32_t divisor) {
  DCHECK(!dividend.is(result));
  DCHECK(!dividend.is(r0));
  DCHECK(!result.is(r0));
  MultiplierAndShift ms(divisor);
#ifdef V8_TARGET_ARCH_S390X
  LoadRR(result, dividend);
  MulP(result, Operand(ms.multiplier()));
  ShiftRightArithP(result, result, Operand(32));

#else
  mov(r1, Operand(ms.multiplier()));
  mr_z(r0, dividend);  // r0:r1 = r1 * dividend

  LoadRR(result, r0);
#endif
  // Mul(result, dividend, r0);
  if (divisor > 0 && ms.multiplier() < 0) {
    AddP(result, dividend);
  }
  if (divisor < 0 && ms.multiplier() > 0) {
    SubP(result, dividend);
  }
  if (ms.shift() > 0)
    ShiftRightArith(result, result, Operand(ms.shift()));
  ExtractBit(r0, dividend, 31);
  AddP(result, r0);
}
void MacroAssembler::Translate(Register result,
                               const MemOperand & lookup_table,
                               Length len) {
  tr(MemOperand(result,0),lookup_table,len);     
}

} }  // namespace v8::internal
#endif  // V8_TARGET_ARCH_S390
