// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/code-generator.h"

#include "src/compiler/code-generator-impl.h"
#include "src/compiler/gap-resolver.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties-inl.h"
#include "src/s390/macro-assembler-s390.h"
#include "src/scopes.h"

namespace v8 {
namespace internal {
namespace compiler {

#define __ masm()->


#define kScratchReg r13


// Adds S390-specific methods to convert InstructionOperands.
class S390OperandConverter : public InstructionOperandConverter {
 public:
  S390OperandConverter(CodeGenerator* gen, Instruction* instr)
      : InstructionOperandConverter(gen, instr) {}


  bool CompareLogical() const {
    switch (instr_->flags_condition()) {
      case kUnsignedLessThan:
      case kUnsignedGreaterThanOrEqual:
      case kUnsignedLessThanOrEqual:
      case kUnsignedGreaterThan:
        return true;
      default:
        return false;
    }
    UNREACHABLE();
    return false;
  }

  Operand InputImmediate(int index) {
    Constant constant = ToConstant(instr_->InputAt(index));
    switch (constant.type()) {
      case Constant::kInt32:
        return Operand(constant.ToInt32());
      case Constant::kFloat64:
        return Operand(
            isolate()->factory()->NewNumber(constant.ToFloat64(), TENURED));
      case Constant::kInt64:
#if V8_TARGET_ARCH_S390X
        return Operand(constant.ToInt64());
#endif
      case Constant::kExternalReference:
      case Constant::kHeapObject:
        break;
    }
    UNREACHABLE();
    return Operand::Zero();
  }

  MemOperand MemoryOperand(int* first_index, AddressingMode* mode) {
    const int index = *first_index;
    *mode = AddressingModeField::decode(instr_->opcode());
    switch (*mode) {
      case kMode_None:
        break;
      case kMode_MRI:
        *first_index += 2;
        return MemOperand(InputRegister(index + 0), InputInt32(index + 1));
      case kMode_MRR:
        *first_index += 2;
        return MemOperand(InputRegister(index + 0), InputRegister(index + 1));
    }
    UNREACHABLE();
    return MemOperand(r0);
  }

  MemOperand MemoryOperand(AddressingMode* mode) {
    int index = 0;
    return MemoryOperand(&index, mode);
  }

  MemOperand ToMemOperand(InstructionOperand* op) const {
    DCHECK(op != NULL);
    DCHECK(!op->IsRegister());
    DCHECK(!op->IsDoubleRegister());
    DCHECK(op->IsStackSlot() || op->IsDoubleStackSlot());
    // The linkage computes where all spill slots are located.
    FrameOffset offset = linkage()->GetFrameOffset(op->index(), frame(), 0);
    return MemOperand(offset.from_stack_pointer() ? sp : fp, offset.offset());
  }
};


static inline bool HasRegisterInput(Instruction* instr, int index) {
  return instr->InputAt(index)->IsRegister();
}


#define ASSEMBLE_FLOAT_UNOP(asm_instr) \
  do { \
  __ asm_instr(i.OutputDoubleRegister(), i.InputDoubleRegister(0)); \
  } while (0)


#define ASSEMBLE_FLOAT_BINOP(asm_instr) \
  do { \
  __ asm_instr(i.OutputDoubleRegister(), i.InputDoubleRegister(0), \
  i.InputDoubleRegister(1)); \
  } while (0)


#define ASSEMBLE_BINOP(asm_instr_reg, asm_instr_imm) \
  do { \
    if (HasRegisterInput(instr, 1)) { \
      __ asm_instr_reg(i.OutputRegister(), i.InputRegister(0), \
                       i.InputRegister(1)); \
    } else { \
      __ asm_instr_imm(i.OutputRegister(), i.InputRegister(0), \
                       i.InputImmediate(1)); \
    } \
  } while (0)


#define ASSEMBLE_BINOP_INT(asm_instr_reg, asm_instr_imm) \
   { \
  if (HasRegisterInput(instr, 1)) { \
  __ asm_instr_reg(i.OutputRegister(), i.InputRegister(0), \
  i.InputRegister(1)); \
  } else { \
  __ asm_instr_imm(i.OutputRegister(), i.InputRegister(0), \
  i.InputImmediate(1).immediate()); \
  } \
  } while (0)


#if V8_TARGET_ARCH_S390X
#define ASSEMBLE_ADD_WITH_OVERFLOW() \
  do { \
    ASSEMBLE_BINOP(AddP, AddP); \
    __ TestIfInt32(i.OutputRegister(), r0); \
  } while (0)
#else
#define ASSEMBLE_ADD_WITH_OVERFLOW()\
  do { \
    if (HasRegisterInput(instr, 1)) { \
    __ AddAndCheckForOverflow(i.OutputRegister(), i.InputRegister(0), \
                              i.InputRegister(1), kScratchReg, r0); \
    } else { \
    __ AddAndCheckForOverflow(i.OutputRegister(), i.InputRegister(0), \
       i.InputImmediate(1).immediate(), kScratchReg, r0); \
    }\
  } while (0)
#endif


#if V8_TARGET_ARCH_S390X
#define ASSEMBLE_SUB_WITH_OVERFLOW() \
  do { \
  ASSEMBLE_BINOP(SubP, SubP); \
  __ TestIfInt32(i.OutputRegister(), r0); \
  } while (0)
#else
#define ASSEMBLE_SUB_WITH_OVERFLOW()\
  do { \
  if (HasRegisterInput(instr, 1)) { \
  __ SubAndCheckForOverflow(i.OutputRegister(), i.InputRegister(0), \
  i.InputRegister(1), kScratchReg, r0); \
  } else { \
  __ AddAndCheckForOverflow(i.OutputRegister(), i.InputRegister(0), \
  -i.InputImmediate(1).immediate(), kScratchReg, r0); \
  } \
  } while (0)
#endif


#define ASSEMBLE_COMPARE(cmp_instr, cmpl_instr) \
  do { \
  if (HasRegisterInput(instr, 1)) { \
  if (i.CompareLogical()) { \
  __ cmpl_instr(i.InputRegister(0), i.InputRegister(1)); \
  } else { \
  __ cmp_instr(i.InputRegister(0), i.InputRegister(1)); \
  } \
  } else { \
  if (i.CompareLogical()) { \
  __ cmpl_instr##i(i.InputRegister(0), i.InputImmediate(1)); \
  } else { \
  __ cmp_instr##i(i.InputRegister(0), i.InputImmediate(1)); \
  } \
  } \
  } while (0)


#define ASSEMBLE_FLOAT_COMPARE(cmp_instr) \
  do { \
  __ cmp_instr(i.InputDoubleRegister(0), i.InputDoubleRegister(1)); \
  } while (0)


#define ASSEMBLE_MODULO(load_instr) \
  do { \
  // Divide instruction dr will implicity use register pair
  // r0 & r1 below.
  DCHECK(!left_reg.is(r1)); \
  DCHECK(!right_reg.is(r1)); \
  DCHECK(!result_reg.is(r1)); \
  __ load_instr(r0, i.InputRegister(0)); \
  __ srda(r0, Operand(32));
  __ dr(r0, i.InputRegister(1)); \  // R0:R1 = R1 / divisor - R0 remainder
  __ ltr(i.OutputRegister(), r0);    // Copy remainder to output reg
  } while (0)


#define ASSEMBLE_FLOAT_MODULO() \
  do { \
  FrameScope scope(masm(), StackFrame::MANUAL); \
  __ PrepareCallCFunction(0, 2, kScratchReg); \
  __ MovToFloatParameters(i.InputDoubleRegister(0), \
  i.InputDoubleRegister(1)); \
  __ CallCFunction(ExternalReference::mod_two_doubles_operation(isolate()), \
  0, 2); \
  __ MovFromFloatResult(i.OutputDoubleRegister()); \
  } while (0)


#define ASSEMBLE_STORE_WRITE_BARRIER() \
  do { \
  Register object = i.InputRegister(0); \
  Register index = i.InputRegister(1); \
  Register value = i.InputRegister(2); \
  __ AddP(index, object); \
  __ StoreP(value, MemOperand(index)); \
  SaveFPRegsMode mode = \
  frame()->DidAllocateDoubleRegisters() ? kSaveFPRegs : kDontSaveFPRegs; \
  LinkRegisterStatus lr_status = kLRHasNotBeenSaved; \
  __ RecordWrite(object, index, value, lr_status, mode); \
  } while (0)


// Assembles an instruction after register allocation, producing machine code.
void CodeGenerator::AssembleArchInstruction(Instruction* instr) {
  S390OperandConverter i(this, instr);

  switch (ArchOpcodeField::decode(instr->opcode())) {
    case kArchJmp:
      __ b(code_->GetLabel(i.InputBlock(0)));
      break;
    case kArchNop:
      // don't emit code for nops.
      break;
    case kArchRet:
      AssembleReturn();
      break;
    case kArchDeoptimize: {
      int deoptimization_id = MiscField::decode(instr->opcode());
      BuildTranslation(instr, deoptimization_id);

      Address deopt_entry = Deoptimizer::GetDeoptimizationEntry(
          isolate(), deoptimization_id, Deoptimizer::LAZY);
      __ Call(deopt_entry, RelocInfo::RUNTIME_ENTRY);
      break;
    }
    case kS390_And32:
      ASSEMBLE_BINOP(And, And);
      break;
    case kS390_And64:
      ASSEMBLE_BINOP(AndP, AndP);
      break;
    case kS390_Or32:
      ASSEMBLE_BINOP(Or, Or);
      break;
    case kS390_Or64:
      ASSEMBLE_BINOP(OrP, OrP);
      break;
    case kS390_Xor32:
       ASSEMBLE_BINOP(Xor, Xor);
      break;
    case kS390_Xor64:
      ASSEMBLE_BINOP(XorP, XorP);
      break;
    case kS390_Shl32:
      ASSEMBLE_BINOP(ShiftLeft, ShiftLeft);

      break;
#if V8_TARGET_ARCH_S390X
    case kS390_Shl64:
      ASSEMBLE_BINOP(sllg, sllg);
      break;
#endif
    case kS390_Shr32:
      ASSEMBLE_BINOP(ShiftRight, ShiftRight);
      break;
#if V8_TARGET_ARCH_S390X
    case kS390_Shr64:
      ASSEMBLE_BINOP(srlg, srlg);
      break;
#endif
    case kS390_Sar32:
      ASSEMBLE_BINOP(ShiftRightArith, ShiftRightArith);
      break;
#if V8_TARGET_ARCH_S390X
    case kS390_Sar64:
      ASSEMBLE_BINOP(srag, srag);
      break;
#endif
    case kS390_Not32:
    case kS390_Not64:
      __ LoadRR(i.OutputRegister(), i.InputRegister(0));
      __ NotP(i.OutputRegister());
      break;
    case kS390_Add32:
    case kS390_Add64:
      ASSEMBLE_BINOP(AddP, AddP);
      break;
    case kS390_AddWithOverflow32:
      ASSEMBLE_ADD_WITH_OVERFLOW();
      break;
    case kS390_AddFloat64:
    // Ensure we don't clobber right/InputReg(1)
    if (i.OutputDoubleRegister().is(i.InputDoubleRegister(1))) {
        ASSEMBLE_FLOAT_UNOP(adbr);
    } else {
        if (!i.OutputDoubleRegister().is(i.InputDoubleRegister(0)))
          __ ldr(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      __ adbr(i.OutputDoubleRegister(), i.InputDoubleRegister(1));
    }
      break;
    case kS390_Sub32:
      ASSEMBLE_BINOP(Sub32, Sub32);
      break;
    case kS390_Sub64:
      ASSEMBLE_BINOP(SubP, SubP);
      break;
    case kS390_SubWithOverflow32:
      ASSEMBLE_SUB_WITH_OVERFLOW();
      break;
    }
    case kS390_SubFloat64:
    // InputDoubleReg(1) = i.InputDoubleRegister(0) - i.InputDoubleRegister(1)
    if (i.OutputDoubleRegister().is(i.InputDoubleRegister(1))) {
        __ ldr(kScratchDoubleReg, i.InputDoubleRegister(1));
        __ ldr(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
        __ sdbr(i.OutputDoubleRegister(), kScratchDoubleReg);
      } else {
        if (!i.OutputDoubleRegister().is(i.InputDoubleRegister(0)))
          __ ldr(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
          __ sdbr(i.OutputDoubleRegister(), i.InputDoubleRegister(1));
      }
      break;
    case kS390_Mul32:
    case kS390_Mul64:
      __ Mul(i.OutputRegister(), i.InputRegister(0), i.InputRegister(1));
      break;
    case kS390_MulFloat64:
      // Ensure we don't clobber right
      if (i.OutputDoubleRegister().is(i.InputDoubleRegister(1))) {
        ASSEMBLE_FLOAT_UNOP(mdbr);
      } else {
        if (!i.OutputDoubleRegister().is(i.InputDoubleRegister(0)))
          __ ldr(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
        __ mdbr(i.OutputDoubleRegister(), i.InputDoubleRegister(1));
      }
      break;
#if V8_TARGET_ARCH_S390X
    case kS390_Div64:
    case kS390_DivU64:
#endif
    case kS390_Div32:
    case kS390_DivU32:
      __ LoadRR(r0, i.InputRegister(0)); \
      __ srda(r0, Operand(32));
      __ dr(r0, i.InputRegister(1)); \  // R0:R1 = R1 / divisor - R0 remainder
      __ ltr(i.OutputRegister(), r0);    // Copy remainder to output reg
      break;

    case kS390_DivFloat64:
      // InputDoubleRegister(1)=InputDoubleRegister(0)/InputDoubleRegister(1)
      if (i.OutputDoubleRegister().is(i.InputDoubleRegister(1))) {
      __ ldr(kScratchDoubleReg, i.InputDoubleRegister(1));
      __ ldr(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      __ ddbr(i.OutputDoubleRegister(), kScratchDoubleReg);
      } else {
      if (!i.OutputDoubleRegister().is(i.InputDoubleRegister(0)))
      __ ldr(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      __ ddbr(i.OutputDoubleRegister(), i.InputDoubleRegister(1));
}
      break;
    case kS390_Mod32:
    case kS390_ModU32:
      ASSEMBLE_MODULO(lr);
      break;
#if V8_TARGET_ARCH_S390x
    case kS390_Mod64:
    case kS390_ModU64:
      ASSEMBLE_MODULO(lgr);
      break;
#endif
    case kS390_ModFloat64: {
      // TODO(bmeurer): We should really get rid of this special instruction,
      // and generate a CallAddress instruction instead.
      ASSEMBLE_FLOAT_MODULO();
      break;
    case kS390_Neg32:
      __ lcr(i.OutputRegister(), i.InputRegister(0));
      break;
    case kS390_Neg64:
      __ lcgr(i.OutputRegister(), i.InputRegister(0));
      break;
    case kS390_NegFloat64:
      __ lcdbr(i.OutputDoubleRegister(), i.InputDoubleRegister(0));
      break;
    case kS390_Cmp32:
      ASSEMBLE_COMPARE(Cmp32, CmpLogical32);
      break;
#if V8_TARGET_ARCH_S390X
    case kS390_Cmp64:
      ASSEMBLE_COMPARE(CmpP, CmpLogicalP);
      break;
#endif
    case kS390_CmpFloat64:
      __ cdbr(i.InputDoubleRegister(0), i.InputDoubleRegister(1));
      break;
    case kS390_Tst32:
      if (HasRegisterInput(instr, 1)) {
        __ AndP(r0, i.InputRegister(0), i.InputRegister(1));
      } else {
        __ AndP(r0, i.InputRegister(0), i.InputImmediate(1));
      }
#if V8_TARGET_ARCH_S390X
      __ lgfr(r0, r0);
#endif
      break;
#if V8_TARGET_ARCH_S390X
    case kS390_Tst64:
      if (HasRegisterInput(instr, 1)) {
        __ AndP(r0, i.InputRegister(0), i.InputRegister(1));
      } else {
        __ AndP(r0, i.InputRegister(0), i.InputImmediate(1));
      }
      break;
#endif
    case kS390_CallCodeObject: {
      if (HasRegisterInput(instr, 0)) {
        Register reg = i.InputRegister(0);
        int entry = Code::kHeaderSize - kHeapObjectTag;
        __ LoadP(reg, MemOperand(reg, entry));
        __ Call(reg);
        RecordSafepoint(instr->pointer_map(), Safepoint::kSimple, 0,
                        Safepoint::kNoLazyDeopt);
      } else {
        Handle<Code> code = Handle<Code>::cast(i.InputHeapObject(0));
        __ Call(code, RelocInfo::CODE_TARGET);
        RecordSafepoint(instr->pointer_map(), Safepoint::kSimple, 0,
                        Safepoint::kNoLazyDeopt);
      }
      bool lazy_deopt = (MiscField::decode(instr->opcode()) == 1);
      if (lazy_deopt) {
        RecordLazyDeoptimizationEntry(instr);
      }
      break;
    }
    case kS390_CallJSFunction: {
      Register func = i.InputRegister(0);

      // TODO(jarin) The load of the context should be separated from the call.
      __ LoadP(cp, FieldMemOperand(func, JSFunction::kContextOffset));
      __ LoadP(ip, FieldMemOperand(func, JSFunction::kCodeEntryOffset));
      __ Call(ip);

      RecordSafepoint(instr->pointer_map(), Safepoint::kSimple, 0,
                      Safepoint::kNoLazyDeopt);
      RecordLazyDeoptimizationEntry(instr);
      break;
    }
    case kS390_CallAddress: {
      DirectCEntryStub stub(isolate());
      stub.GenerateCall(masm(), i.InputRegister(0));
      break;
    }
    case kS390_Push:
      __ Push(i.InputRegister(0));
      break;
    case kS390_Drop: {
      int count = MiscField::decode(instr->opcode());
      __ Drop(count);
      break;
    }
#if V8_TARGET_ARCH_S390X
    case kS390_Int32ToInt64:
      __ lgfr(i.OutputRegister(), i.InputRegister(0));
      break;
    case kS390_Int64ToInt32:
      __ Move(i.OutputRegister(), i.InputRegister(0));
      break;
#endif
    case kS390_Int32ToFloat64:
      __ ConvertIntToDouble(i.InputRegister(0),
                            i.OutputDoubleRegister());
      break;
    case kS390_Uint32ToFloat64:
      __ ConvertUnsignedIntToDouble(i.InputRegister(0),
                                    i.OutputDoubleRegister());
      break;
    case kS390_Float64ToInt32:
    case kS390_Float64ToUint32:
      __ ConvertDoubleToInt64(i.InputDoubleRegister(0),
#if !V8_TARGET_ARCH_S390X
                              kScratchReg,
#endif
                              i.OutputRegister(), kScratchDoubleReg);
      break;
    case kS390_LoadWord8:
      __ LoadlB(i.OutputRegister(), i.MemoryOperand());
      break;
    case kS390_LoadWord16:
      __ LoadLogicalHalfWordP(i.OutputRegister(), i.MemoryOperand());
      break;
    case kS390_LoadWord32:
      __ LoadW(i.OutputRegister(), i.MemoryOperand());
      break;
#if V8_TARGET_ARCH_S390X
    case kS390_LoadWord64:
      __ lg(i.OutputRegister(), i.MemoryOperand());
      break;
#endif
    case kS390_LoadFloat64:
      __ ld(i.OutputRegister(), i.MemoryOperand());
      break;
    case kS390_StoreWord8:
      __ StoreByte(i.OutputRegister(), i.MemoryOperand());
      break;
    case kS390_StoreWord16:
      __ StoreHalfWord(i.OutputRegister(), i.MemoryOperand());
      break;
    case kS390_StoreWord32:
      __ StoreW(i.OutputRegister(), i.MemoryOperand());
      break;
#if V8_TARGET_ARCH_S390X
    case kS390_StoreWord64:
      __ StoreP(i.OutputRegister(), i.MemoryOperand());
      break;
#endif
    case kS390_StoreFloat64:
      __ std(i.OutputRegister(), i.MemoryOperand());
      break;
    case kS390_StoreWriteBarrier:
      ASSEMBLE_FLOAT_COMPARE(cdbr);
      break;
    default:
      UNREACHABLE();
      break;
  }
}


// Assembles branches after an instruction.
void CodeGenerator::AssembleArchBranch(Instruction* instr,
                                       FlagsCondition condition) {
  S390OperandConverter i(this, instr);
  Label done;
  Condition cond = kNoCondition;
#if DEBUG
  ArchOpcode op = ArchOpcodeField::decode(instr->opcode());
#endif

  // Emit a branch. The true and false targets are always the last two inputs
  // to the instruction.
  BasicBlock* tblock = i.InputBlock(instr->InputCount() - 2);
  BasicBlock* fblock = i.InputBlock(instr->InputCount() - 1);
  bool fallthru = IsNextInAssemblyOrder(fblock);
  Label* tlabel = code()->GetLabel(tblock);
  Label* flabel = fallthru ? &done : code()->GetLabel(fblock);

  switch (condition) {
    case kUnorderedEqual:
    case kUnorderedLessThan:
    case kUnorderedLessThanOrEqual:
      __ bunordered(flabel);
      break;
    case kUnorderedNotEqual:
    case kUnorderedGreaterThanOrEqual:
    case kUnorderedGreaterThan:
      __ bunordered(tlabel);
      break;
    case kOverflow:
      // Currently used only for add/sub overflow
      DCHECK(op == kS390_AddWithOverflow32 || op == kS390_SubWithOverflow32);
#if V8_TARGET_ARCH_S390X
      condition = kNotEqual;
#else
      condition = kSignedLessThan;
#endif
      break;
    case kNotOverflow:
      // Currently used only for add/sub overflow
      DCHECK(op == kS390_AddWithOverflow32 || op == kS390_SubWithOverflow32);
#if V8_TARGET_ARCH_S390X
      condition = kEqual;
#else
      condition = kSignedGreaterThanOrEqual;
#endif
      break;
    default:
      break;
  }

  switch (condition) {
    case kUnorderedEqual:
    case kEqual:
      cond = eq;
      break;
    case kUnorderedNotEqual:
    case kNotEqual:
      cond = ne;
      break;
    case kUnorderedLessThan:
    case kSignedLessThan:
    case kUnsignedLessThan:
      cond = lt;
      break;
    case kUnorderedGreaterThanOrEqual:
    case kSignedGreaterThanOrEqual:
    case kUnsignedGreaterThanOrEqual:
      cond = ge;
      break;
    case kUnorderedLessThanOrEqual:
    case kSignedLessThanOrEqual:
    case kUnsignedLessThanOrEqual:
      cond = le;
      break;
    case kUnorderedGreaterThan:
    case kSignedGreaterThan:
    case kUnsignedGreaterThan:
      cond = gt;
      break;
    case kOverflow:
    case kNotOverflow:
      UNREACHABLE();
      break;
  }
  __ b(cond, tlabel);
  if (!fallthru) __ b(flabel);  // no fallthru to flabel.
  __ bind(&done);
}


// Assembles boolean materializations after an instruction.
void CodeGenerator::AssembleArchBoolean(Instruction* instr,
                                        FlagsCondition condition) {
  S390OperandConverter i(this, instr);
  Label done;
#if DEBUG
  ArchOpcode op = ArchOpcodeField::decode(instr->opcode());
#endif

  // Materialize a full 32-bit 1 or 0 value. The result register is always the
  // last output of the instruction.
  DCHECK_NE(0, instr->OutputCount());
  Register reg = i.OutputRegister(instr->OutputCount() - 1);

  switch (condition) {
    case kOverflow:
      // Currently used only for add/sub overflow
      DCHECK(op == kS390_AddWithOverflow32 || op == kS390_SubWithOverflow32);
#if V8_TARGET_ARCH_S390X
      condition = kNotEqual;
#else
      condition = kSignedLessThan;
#endif
      break;
    case kNotOverflow:
      // Currently used only for add/sub overflow
      DCHECK(op == kS390_AddWithOverflow32 || op == kS390_SubWithOverflow32);
#if V8_TARGET_ARCH_S390X
      condition = kEqual;
#else
      condition = kSignedGreaterThanOrEqual;
#endif
      break;
    default:
      break;
  }

  switch (condition) {
    case kUnorderedEqual:
    case kEqual:
      __ mov(reg, Operand::Zero());
      __ mov(kScratchReg, Operand(1));
      if (condition == kUnorderedEqual) __ bunordered(&done);
      Label not_equal;
      __ bne(&not_equal);
      __ LoadRR(reg, kScratchReg);
      __ bind(&not_equal);
      break;
    case kUnorderedNotEqual:
    case kNotEqual:
      __ mov(reg, Operand(1));
      if (condition == kUnorderedNotEqual) __ bunordered(&done);
      Label not_equal;
      __ bne(&not_equal);
      __ LoadRR(reg, r0);
      __ bind(&not_equal);
      break;
    case kUnorderedLessThan:
    case kSignedLessThan:
    case kUnsignedLessThan:
      __ mov(reg, Operand::Zero());
      __ mov(kScratchReg, Operand(1));
      if (condition == kUnorderedLessThan) __ bunordered(&done);
      Label greater_eq;
      __ bge(&greater_eq);
      __ LoadRR(reg, kScratchReg);
      __ bind(&greater_eq);
      break;
    case kUnorderedGreaterThanOrEqual:
    case kSignedGreaterThanOrEqual:
    case kUnsignedGreaterThanOrEqual:
      __ mov(reg, Operand(1));
      if (condition == kUnorderedGreaterThanOrEqual) __ bunordered(&done);
      Label greater_eq;
      __ bge(&greater_eq);
      __ LoadRR(reg, r0);
      __ bind(&greater_eq);
      break;
    case kUnorderedLessThanOrEqual:
      __ mov(reg, Operand::Zero());
      __ mov(kScratchReg, Operand(1));
      __ bunordered(&done);
      Label less_eq;
      __ LoadRR(reg, kScratchReg);
      __ ble(&less_eq);
      __ LoadRR(reg, r0);
      __ bind(&less_eq);
      break;
    case kSignedLessThanOrEqual:
    case kUnsignedLessThanOrEqual:
      __ mov(reg, Operand(1));
      Label less_eq;
      __ ble(&less_eq);
      __ LoadRR(reg, r0);
      __ bind(&less_eq);
      break;
    case kUnorderedGreaterThan:
      __ mov(reg, Operand(1));
      __ mov(kScratchReg, Operand::Zero());
      __ bunordered(&done);
      Label greater_than;
      __ bgt(&greater_than);
      __ LoadRR(reg, kScratchReg);
      __ bind(&greater_than);
      break;
    case kSignedGreaterThan:
    case kUnsignedGreaterThan:
      __ mov(reg, Operand::Zero());
      __ mov(kScratchReg, Operand(1));
      Label greater;
      __ ble(&less_eq);
      __ LoadRR(reg, kScratchReg);
      __ bind(&less_eq);
      break;
    case kOverflow:
    case kNotOverflow:
      UNREACHABLE();
      break;
  }
  __ bind(&done);
}


void CodeGenerator::AssemblePrologue() {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();
  if (descriptor->kind() == CallDescriptor::kCallAddress) {
#if ABI_USES_FUNCTION_DESCRIPTORS
    __function_descriptor();
#endif
    int register_save_area_size = 0;
    RegList saves = descriptor->CalleeSavedRegisters();
    __ Push(r14, fp);
    __ LoadRR(fp, sp);
    if (saves != 0 || register_save_area_size) {
      // Save callee-saved registers.
      for (int i = Register::kNumRegisters - 1; i >= 0; i--) {
        if (!((1 << i) & saves)) continue;
        register_save_area_size += kPointerSize;
      }
      frame()->SetRegisterSaveAreaSize(register_save_area_size);
      __ MultiPush(saves);
    }
  } else if (descriptor->IsJSFunctionCall()) {
    CompilationInfo* info = linkage()->info();
    __ Prologue(info->IsCodePreAgingActive());
    frame()->SetRegisterSaveAreaSize(
        StandardFrameConstants::kFixedFrameSizeFromFp);

    // Sloppy mode functions and builtins need to replace the receiver with the
    // global proxy when called as functions (without an explicit receiver
    // object).
    // TODO(mstarzinger/verwaest): Should this be moved back into the CallIC?
    if (info->strict_mode() == SLOPPY && !info->is_native()) {
      Label ok;
      // +2 for return address and saved frame pointer.
      int receiver_slot = info->scope()->num_parameters() + 2;
      __ LoadP(r4, MemOperand(fp, receiver_slot * kPointerSize));
      __ CompareRoot(r4, Heap::kUndefinedValueRootIndex);
      __ bne(&ok);
      __ LoadP(r4, GlobalObjectOperand());
      __ LoadP(r4, FieldMemOperand(r4, GlobalObject::kGlobalProxyOffset));
      __ StoreP(r4, MemOperand(fp, receiver_slot * kPointerSize));
      __ bind(&ok);
    }

  } else {
    __ StubPrologue();
    frame()->SetRegisterSaveAreaSize(
        StandardFrameConstants::kFixedFrameSizeFromFp);
  }
  int stack_slots = frame()->GetSpillSlotCount();
  if (stack_slots > 0) {
    __ lay(sp, MemOperand(sp, -stack_slots * kPointerSize));
  }
}


void CodeGenerator::AssembleReturn() {
  CallDescriptor* descriptor = linkage()->GetIncomingDescriptor();
  if (descriptor->kind() == CallDescriptor::kCallAddress) {
    if (frame()->GetRegisterSaveAreaSize() > 0) {
      // Remove this frame's spill slots first.
      int stack_slots = frame()->GetSpillSlotCount();
      if (stack_slots > 0) {
        __ lay(sp, MemOperand(sp, stack_slots * kPointerSize));
      }
      // Restore registers.
      RegList saves = descriptor->CalleeSavedRegisters();
      if (saves != 0) {
        __ MultiPop(saves);
      }
    }
    __ LeaveFrame(StackFrame::MANUAL);
    __ Ret();
  } else {
    int pop_count = descriptor->IsJSFunctionCall()
                        ? static_cast<int>(descriptor->ParameterCount())
                        : 0;
    __ LeaveFrame(StackFrame::MANUAL, pop_count * kPointerSize);
    __ Ret();
  }
}


void CodeGenerator::AssembleMove(InstructionOperand* source,
                                 InstructionOperand* destination) {
  S390OperandConverter g(this, NULL);
  // Dispatch on the source and destination operand kinds.  Not all
  // combinations are possible.
  if (source->IsRegister()) {
    DCHECK(destination->IsRegister() || destination->IsStackSlot());
    Register src = g.ToRegister(source);
    if (destination->IsRegister()) {
      __ Move(g.ToRegister(destination), src);
    } else {
      __ StoreP(src, g.ToMemOperand(destination));
    }
  } else if (source->IsStackSlot()) {
    DCHECK(destination->IsRegister() || destination->IsStackSlot());
    MemOperand src = g.ToMemOperand(source);
    if (destination->IsRegister()) {
      __ LoadP(g.ToRegister(destination), src);
    } else {
      Register temp = kScratchReg;
      __ LoadP(temp, src, r0);
      __ StoreP(temp, g.ToMemOperand(destination));
    }
  } else if (source->IsConstant()) {
    if (destination->IsRegister() || destination->IsStackSlot()) {
      Register dst =
          destination->IsRegister() ? g.ToRegister(destination) : kScratchReg;
      Constant src = g.ToConstant(source);
      switch (src.type()) {
        case Constant::kInt32:
          __ mov(dst, Operand(src.ToInt32()));
          break;
        case Constant::kInt64:
          __ mov(dst, Operand(src.ToInt64()));
          break;
        case Constant::kFloat64:
          __ Move(dst,
                  isolate()->factory()->NewNumber(src.ToFloat64(), TENURED));
          break;
        case Constant::kExternalReference:
          __ mov(dst, Operand(src.ToExternalReference()));
          break;
        case Constant::kHeapObject:
          __ Move(dst, src.ToHeapObject());
          break;
      }
      if (destination->IsStackSlot()) {
        __ StoreP(dst, g.ToMemOperand(destination), r0);
      }
    } else if (destination->IsDoubleRegister()) {
      DoubleRegister result = g.ToDoubleRegister(destination);
      __ LoadDoubleLiteral(result, g.ToDouble(source), kScratchReg);
    } else {
      DCHECK(destination->IsDoubleStackSlot());
      DoubleRegister temp = kScratchDoubleReg;
      __ LoadDoubleLiteral(temp, g.ToDouble(source), kScratchReg);
      __ StoreF(temp, g.ToMemOperand(destination));
    }
  } else if (source->IsDoubleRegister()) {
    DoubleRegister src = g.ToDoubleRegister(source);
    if (destination->IsDoubleRegister()) {
      DoubleRegister dst = g.ToDoubleRegister(destination);
      __ Move(dst, src);
    } else {
      DCHECK(destination->IsDoubleStackSlot());
      __ StoreF(src, g.ToMemOperand(destination));
    }
  } else if (source->IsDoubleStackSlot()) {
    DCHECK(destination->IsDoubleRegister() || destination->IsDoubleStackSlot());
    MemOperand src = g.ToMemOperand(source);
    if (destination->IsDoubleRegister()) {
      __ LoadF(g.ToDoubleRegister(destination), src);
    } else {
      DoubleRegister temp = kScratchDoubleReg;
      __ LoadF(temp, src);
      __ StoreF(temp, g.ToMemOperand(destination));
    }
  } else {
    UNREACHABLE();
  }
}


void CodeGenerator::AssembleSwap(InstructionOperand* source,
                                 InstructionOperand* destination) {
  S390OperandConverter g(this, NULL);
  // Dispatch on the source and destination operand kinds.  Not all
  // combinations are possible.
  if (source->IsRegister()) {
    // Register-register.
    Register temp = kScratchReg;
    Register src = g.ToRegister(source);
    if (destination->IsRegister()) {
      Register dst = g.ToRegister(destination);
      __ LoadRR(temp, src);
      __ LoadRR(src, dst);
      __ LoadRR(dst, temp);
    } else {
      DCHECK(destination->IsStackSlot());
      MemOperand dst = g.ToMemOperand(destination);
      __ LoadRR(temp, src);
      __ LoadP(src, dst);
      __ StoreP(temp, dst);
    }
#if V8_TARGET_ARCH_S390X
  } else if (source->IsStackSlot() || source->IsDoubleStackSlot()) {
#else
  } else if (source->IsStackSlot()) {
#endif
    DCHECK(destination->IsStackSlot());
    Register temp_0 = kScratchReg;
    Register temp_1 = r0;
    MemOperand src = g.ToMemOperand(source);
    MemOperand dst = g.ToMemOperand(destination);
    __ LoadP(temp_0, src);
    __ LoadP(temp_1, dst);
    __ StoreP(temp_0, dst);
    __ StoreP(temp_1, src);
  } else if (source->IsDoubleRegister()) {
    DoubleRegister temp = kScratchDoubleReg;
    DoubleRegister src = g.ToDoubleRegister(source);
    if (destination->IsDoubleRegister()) {
      DoubleRegister dst = g.ToDoubleRegister(destination);
      __ ldr(temp, src);
      __ ldr(src, dst);
      __ ldr(src, temp);
    } else {
      DCHECK(destination->IsDoubleStackSlot());
      MemOperand dst = g.ToMemOperand(destination);
      __ ldr(temp, src);
      __ LoadF(src, dst);
      __ StoreF(temp, dst);
    }
#if !V8_TARGET_ARCH_S390X
  } else if (source->IsDoubleStackSlot()) {
    DCHECK(destination->IsDoubleStackSlot());
    DoubleRegister temp_0 = kScratchDoubleReg;
    DoubleRegister temp_1 = d0;
    MemOperand src = g.ToMemOperand(source);
    MemOperand dst = g.ToMemOperand(destination);
    __ LoadF(temp_0, src);
    __ LoadF(temp_1, dst);
    __ StoreF(temp_0, dst);
    __ StoreF(temp_1, src);
#endif
  } else {
    // No other combinations are possible.
    UNREACHABLE();
  }
}


void CodeGenerator::AddNopForSmiCodeInlining() {
  // On 32-bit ARM we do not insert nops for inlined Smi code.
  UNREACHABLE();
}

#ifdef DEBUG

// Checks whether the code between start_pc and end_pc is a no-op.
bool CodeGenerator::IsNopForSmiCodeInlining(Handle<Code> code, int start_pc,
                                            int end_pc) {
  return false;
}

#endif  // DEBUG

#undef __
}
}
}  // namespace v8::internal::compiler
