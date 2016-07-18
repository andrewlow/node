// Copyright 2012 the V8 project authors. All rights reserved.
//
// Copyright IBM Corp. 2012-2014. All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_S390

#include "src/bootstrapper.h"
#include "src/code-stubs.h"
#include "src/regexp-macro-assembler.h"
#include "src/stub-cache.h"

#include "src/s390/regexp-macro-assembler-s390.h"

namespace v8 {
namespace internal {


void FastNewClosureStub::InitializeInterfaceDescriptor(
    CodeStubInterfaceDescriptor* descriptor) {
  Register registers[] = { cp, r4 };
  descriptor->Initialize(
      MajorKey(), ARRAY_SIZE(registers), registers,
      Runtime::FunctionForId(Runtime::kNewClosureFromStubFailure)->entry);
}


void FastNewContextStub::InitializeInterfaceDescriptor(
    CodeStubInterfaceDescriptor* descriptor) {
  Register registers[] = { cp, r3 };
  descriptor->Initialize(MajorKey(), ARRAY_SIZE(registers), registers);
}


void ToNumberStub::InitializeInterfaceDescriptor(
    CodeStubInterfaceDescriptor* descriptor) {
  Register registers[] = { cp, r2 };
  descriptor->Initialize(MajorKey(), ARRAY_SIZE(registers), registers);
}


void NumberToStringStub::InitializeInterfaceDescriptor(
    CodeStubInterfaceDescriptor* descriptor) {
  Register registers[] = { cp, r2 };
  descriptor->Initialize(
      MajorKey(), ARRAY_SIZE(registers), registers,
      Runtime::FunctionForId(Runtime::kNumberToStringRT)->entry);
}


void FastCloneShallowArrayStub::InitializeInterfaceDescriptor(
    CodeStubInterfaceDescriptor* descriptor) {
  Register registers[] = { cp, r5, r4, r3 };
  Representation representations[] = {
    Representation::Tagged(),
    Representation::Tagged(),
    Representation::Smi(),
    Representation::Tagged() };
  descriptor->Initialize(
      MajorKey(), ARRAY_SIZE(registers), registers,
      Runtime::FunctionForId(Runtime::kCreateArrayLiteralStubBailout)->entry,
      representations);
}


void FastCloneShallowObjectStub::InitializeInterfaceDescriptor(
    CodeStubInterfaceDescriptor* descriptor) {
  Register registers[] = { cp, r5, r4, r3, r2 };
  descriptor->Initialize(
      MajorKey(), ARRAY_SIZE(registers), registers,
      Runtime::FunctionForId(Runtime::kCreateObjectLiteral)->entry);
}


void CreateAllocationSiteStub::InitializeInterfaceDescriptor(
    CodeStubInterfaceDescriptor* descriptor) {
  Register registers[] = { cp, r4, r5 };
  descriptor->Initialize(MajorKey(), ARRAY_SIZE(registers), registers);
}


void CallFunctionStub::InitializeInterfaceDescriptor(
    CodeStubInterfaceDescriptor* descriptor) {
  // r3  function    the function to call
  Register registers[] = {cp, r3};
  descriptor->Initialize(MajorKey(), ARRAY_SIZE(registers), registers);
}


void CallConstructStub::InitializeInterfaceDescriptor(
    CodeStubInterfaceDescriptor* descriptor) {
  // r2 : number of arguments
  // r3 : the function to call
  // r4 : feedback vector
  // r5 : (only if r4 is not the megamorphic symbol) slot in feedback
  //      vector (Smi)
  // TODO(turbofan): So far we don't gather type feedback and hence skip the
  // slot parameter, but ArrayConstructStub needs the vector to be undefined.
  Register registers[] = {cp, r2, r3, r4};
  descriptor->Initialize(MajorKey(), ARRAY_SIZE(registers), registers);
}


void RegExpConstructResultStub::InitializeInterfaceDescriptor(
    CodeStubInterfaceDescriptor* descriptor) {
  Register registers[] = { cp, r4, r3, r2 };
  descriptor->Initialize(
      MajorKey(), ARRAY_SIZE(registers), registers,
      Runtime::FunctionForId(Runtime::kRegExpConstructResult)->entry);
}


void TransitionElementsKindStub::InitializeInterfaceDescriptor(
    CodeStubInterfaceDescriptor* descriptor) {
  Register registers[] = { cp, r2, r3 };
  Address entry =
      Runtime::FunctionForId(Runtime::kTransitionElementsKind)->entry;
  descriptor->Initialize(MajorKey(), ARRAY_SIZE(registers), registers,
                         FUNCTION_ADDR(entry));
}


void CompareNilICStub::InitializeInterfaceDescriptor(
    CodeStubInterfaceDescriptor* descriptor) {
  Register registers[] = { cp, r2 };
  descriptor->Initialize(MajorKey(), ARRAY_SIZE(registers), registers,
                         FUNCTION_ADDR(CompareNilIC_Miss));
  descriptor->SetMissHandler(
      ExternalReference(IC_Utility(IC::kCompareNilIC_Miss), isolate()));
}


const Register InterfaceDescriptor::ContextRegister() { return cp; }


static void InitializeArrayConstructorDescriptor(
    CodeStub::Major major, CodeStubInterfaceDescriptor* descriptor,
    int constant_stack_parameter_count) {
  // register state
  // cp -- context
  // r2 -- number of arguments
  // r3 -- function
  // r4 -- allocation site with elements kind
  Address deopt_handler = Runtime::FunctionForId(
      Runtime::kArrayConstructor)->entry;

  if (constant_stack_parameter_count == 0) {
    Register registers[] = { cp, r3, r4 };
    descriptor->Initialize(major, ARRAY_SIZE(registers), registers,
                           deopt_handler, NULL, constant_stack_parameter_count,
                           JS_FUNCTION_STUB_MODE);
  } else {
    // stack param count needs (constructor pointer, and single argument)
    Register registers[] = { cp, r3, r4, r2 };
    Representation representations[] = {
        Representation::Tagged(),
        Representation::Tagged(),
        Representation::Tagged(),
        Representation::Integer32() };
    descriptor->Initialize(major, ARRAY_SIZE(registers), registers, r2,
                           deopt_handler, representations,
                           constant_stack_parameter_count,
                           JS_FUNCTION_STUB_MODE, PASS_ARGUMENTS);
  }
}


static void InitializeInternalArrayConstructorDescriptor(
    CodeStub::Major major, CodeStubInterfaceDescriptor* descriptor,
    int constant_stack_parameter_count) {
  // register state
  // cp -- context
  // r2 -- number of arguments
  // r3 -- constructor function
  Address deopt_handler = Runtime::FunctionForId(
      Runtime::kInternalArrayConstructor)->entry;

  if (constant_stack_parameter_count == 0) {
    Register registers[] = { cp, r3 };
    descriptor->Initialize(major, ARRAY_SIZE(registers), registers,
                           deopt_handler, NULL, constant_stack_parameter_count,
                           JS_FUNCTION_STUB_MODE);
  } else {
    // stack param count needs (constructor pointer, and single argument)
    Register registers[] = { cp, r3, r2 };
    Representation representations[] = {
        Representation::Tagged(),
        Representation::Tagged(),
        Representation::Integer32() };
    descriptor->Initialize(major, ARRAY_SIZE(registers), registers, r2,
                           deopt_handler, representations,
                           constant_stack_parameter_count,
                           JS_FUNCTION_STUB_MODE, PASS_ARGUMENTS);
  }
}


void ArrayNoArgumentConstructorStub::InitializeInterfaceDescriptor(
    CodeStubInterfaceDescriptor* descriptor) {
  InitializeArrayConstructorDescriptor(MajorKey(), descriptor, 0);
}


void ArraySingleArgumentConstructorStub::InitializeInterfaceDescriptor(
    CodeStubInterfaceDescriptor* descriptor) {
  InitializeArrayConstructorDescriptor(MajorKey(), descriptor, 1);
}


void ArrayNArgumentsConstructorStub::InitializeInterfaceDescriptor(
    CodeStubInterfaceDescriptor* descriptor) {
  InitializeArrayConstructorDescriptor(MajorKey(), descriptor, -1);
}


void ToBooleanStub::InitializeInterfaceDescriptor(
    CodeStubInterfaceDescriptor* descriptor) {
  Register registers[] = { cp, r2 };
  descriptor->Initialize(MajorKey(), ARRAY_SIZE(registers), registers,
                         FUNCTION_ADDR(ToBooleanIC_Miss));
  descriptor->SetMissHandler(
      ExternalReference(IC_Utility(IC::kToBooleanIC_Miss), isolate()));
}


void InternalArrayNoArgumentConstructorStub::InitializeInterfaceDescriptor(
    CodeStubInterfaceDescriptor* descriptor) {
  InitializeInternalArrayConstructorDescriptor(MajorKey(), descriptor, 0);
}


void InternalArraySingleArgumentConstructorStub::InitializeInterfaceDescriptor(
    CodeStubInterfaceDescriptor* descriptor) {
  InitializeInternalArrayConstructorDescriptor(MajorKey(), descriptor, 1);
}


void InternalArrayNArgumentsConstructorStub::InitializeInterfaceDescriptor(
    CodeStubInterfaceDescriptor* descriptor) {
  InitializeInternalArrayConstructorDescriptor(MajorKey(), descriptor, -1);
}


void BinaryOpICStub::InitializeInterfaceDescriptor(
    CodeStubInterfaceDescriptor* descriptor) {
  Register registers[] = { cp, r3, r2 };
  descriptor->Initialize(MajorKey(), ARRAY_SIZE(registers), registers,
                         FUNCTION_ADDR(BinaryOpIC_Miss));
  descriptor->SetMissHandler(
      ExternalReference(IC_Utility(IC::kBinaryOpIC_Miss), isolate()));
}


void BinaryOpWithAllocationSiteStub::InitializeInterfaceDescriptor(
    CodeStubInterfaceDescriptor* descriptor) {
  Register registers[] = { cp, r4, r3, r2 };
  descriptor->Initialize(MajorKey(), ARRAY_SIZE(registers), registers,
                         FUNCTION_ADDR(BinaryOpIC_MissWithAllocationSite));
}


void StringAddStub::InitializeInterfaceDescriptor(
    CodeStubInterfaceDescriptor* descriptor) {
  Register registers[] = { cp, r3, r2 };
  descriptor->Initialize(MajorKey(), ARRAY_SIZE(registers), registers,
                         Runtime::FunctionForId(Runtime::kStringAdd)->entry);
}


void CallDescriptors::InitializeForIsolate(Isolate* isolate) {
  {
    CallInterfaceDescriptor* descriptor =
        isolate->call_descriptor(Isolate::ArgumentAdaptorCall);
    Register registers[] = { cp,  // context
                             r3,  // JSFunction
                             r2,  // actual number of arguments
                             r4,  // expected number of arguments
    };
    Representation representations[] = {
        Representation::Tagged(),     // context
        Representation::Tagged(),     // JSFunction
        Representation::Integer32(),  // actual number of arguments
        Representation::Integer32(),  // expected number of arguments
    };
    descriptor->Initialize(ARRAY_SIZE(registers), registers, representations);
  }
  {
    CallInterfaceDescriptor* descriptor =
        isolate->call_descriptor(Isolate::KeyedCall);
    Register registers[] = { cp,  // context
                             r4,  // key
    };
    Representation representations[] = {
        Representation::Tagged(),     // context
        Representation::Tagged(),     // key
    };
    descriptor->Initialize(ARRAY_SIZE(registers), registers, representations);
  }
  {
    CallInterfaceDescriptor* descriptor =
        isolate->call_descriptor(Isolate::NamedCall);
    Register registers[] = { cp,  // context
                             r4,  // name
    };
    Representation representations[] = {
        Representation::Tagged(),     // context
        Representation::Tagged(),     // name
    };
    descriptor->Initialize(ARRAY_SIZE(registers), registers, representations);
  }
  {
    CallInterfaceDescriptor* descriptor =
        isolate->call_descriptor(Isolate::CallHandler);
    Register registers[] = { cp,  // context
                             r2,  // receiver
    };
    Representation representations[] = {
        Representation::Tagged(),  // context
        Representation::Tagged(),  // receiver
    };
    descriptor->Initialize(ARRAY_SIZE(registers), registers, representations);
  }
  {
    CallInterfaceDescriptor* descriptor =
        isolate->call_descriptor(Isolate::ApiFunctionCall);
    Register registers[] = { cp,  // context
                             r2,  // callee
                             r6,  // call_data
                             r4,  // holder
                             r3,  // api_function_address
    };
    Representation representations[] = {
        Representation::Tagged(),    // context
        Representation::Tagged(),    // callee
        Representation::Tagged(),    // call_data
        Representation::Tagged(),    // holder
        Representation::External(),  // api_function_address
    };
    descriptor->Initialize(ARRAY_SIZE(registers), registers, representations);
  }
}


#define __ ACCESS_MASM(masm)


static void EmitIdenticalObjectComparison(MacroAssembler* masm,
                                          Label* slow,
                                          Condition cond);
static void EmitSmiNonsmiComparison(MacroAssembler* masm,
                                    Register lhs,
                                    Register rhs,
                                    Label* lhs_not_nan,
                                    Label* slow,
                                    bool strict);
static void EmitStrictTwoHeapObjectCompare(MacroAssembler* masm,
                                           Register lhs,
                                           Register rhs);


void HydrogenCodeStub::GenerateLightweightMiss(MacroAssembler* masm) {
  // Update the static counter each time a new code stub is generated.
  isolate()->counters()->code_stubs()->Increment();

  CodeStubInterfaceDescriptor* descriptor = GetInterfaceDescriptor();
  int param_count = descriptor->GetEnvironmentParameterCount();
  {
    // Call the runtime system in a fresh internal frame.
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    DCHECK(param_count == 0 ||
           r2.is(descriptor->GetEnvironmentParameterRegister(
               param_count - 1)));
    // Push arguments
    for (int i = 0; i < param_count; ++i) {
      __ push(descriptor->GetEnvironmentParameterRegister(i));
    }
    ExternalReference miss = descriptor->miss_handler();
    __ CallExternalReference(miss, param_count);
  }

  __ Ret();
}


void DoubleToIStub::Generate(MacroAssembler* masm) {
  Label out_of_range, only_low, negate, done, fastpath_done;
  Register input_reg = source();
  Register result_reg = destination();
  DCHECK(is_truncating());

  int double_offset = offset();

  // Immediate values for this stub fit in instructions, so it's safe to use ip.
  Register scratch = GetRegisterThatIsNotOneOf(input_reg, result_reg);
  Register scratch_low =
      GetRegisterThatIsNotOneOf(input_reg, result_reg, scratch);
  Register scratch_high =
      GetRegisterThatIsNotOneOf(input_reg, result_reg, scratch, scratch_low);
  DoubleRegister double_scratch = kScratchDoubleReg;

  __ push(scratch);
  // Account for saved regs if input is sp.
  if (input_reg.is(sp)) double_offset += kPointerSize;

  if (!skip_fastpath()) {
    // Load double input.
    __ LoadF(double_scratch, MemOperand(input_reg, double_offset));

    // Do fast-path convert from double to int.
    __ ConvertDoubleToInt64(double_scratch, result_reg);

    // Test for overflow
    __ TestIfInt32(result_reg, r0);
    __ beq(&fastpath_done, Label::kNear);
  }

  __ Push(scratch_high, scratch_low);
  // Account for saved regs if input is sp.
  if (input_reg.is(sp)) double_offset += 2 * kPointerSize;

  __ LoadlW(scratch_high, MemOperand(input_reg, double_offset +
                                  Register::kExponentOffset));
  __ LoadlW(scratch_low, MemOperand(input_reg, double_offset +
                                 Register::kMantissaOffset));

  __ ExtractBitMask(scratch, scratch_high, HeapNumber::kExponentMask);
  // Load scratch with exponent - 1. This is faster than loading
  // with exponent because Bias + 1 = 1024 which is a *S390* immediate value.
  STATIC_ASSERT(HeapNumber::kExponentBias + 1 == 1024);
  __ SubP(scratch, Operand(HeapNumber::kExponentBias + 1));
  // If exponent is greater than or equal to 84, the 32 less significant
  // bits are 0s (2^84 = 1, 52 significant bits, 32 uncoded bits),
  // the result is 0.
  // Compare exponent with 84 (compare exponent - 1 with 83).
  __ CmpP(scratch, Operand(83));
  __ bge(&out_of_range, Label::kNear);

  // If we reach this code, 31 <= exponent <= 83.
  // So, we don't have to handle cases where 0 <= exponent <= 20 for
  // which we would need to shift right the high part of the mantissa.
  // Scratch contains exponent - 1.
  // Load scratch with 52 - exponent (load with 51 - (exponent - 1)).
  __ Load(r0, Operand(51));
  __ SubP(scratch, r0, scratch);
  __ CmpP(scratch, Operand::Zero());
  __ ble(&only_low, Label::kNear);
  // 21 <= exponent <= 51, shift scratch_low and scratch_high
  // to generate the result.
  __ ShiftRight(scratch_low, scratch_low, scratch);
  // Scratch contains: 52 - exponent.
  // We needs: exponent - 20.
  // So we use: 32 - scratch = 32 - 52 + exponent = exponent - 20.
  __ Load(r0, Operand(32));
  __ SubP(scratch, r0, scratch);
  __ ExtractBitMask(result_reg, scratch_high, HeapNumber::kMantissaMask);
  // Set the implicit 1 before the mantissa part in scratch_high.
  STATIC_ASSERT(HeapNumber::kMantissaBitsInTopWord >= 16);
  __ Load(r0, Operand(1 << ((HeapNumber::kMantissaBitsInTopWord) - 16)));
  __ ShiftLeftP(r0, r0, Operand(16));
  __ OrP(result_reg, result_reg, r0);
  __ ShiftLeft(r0, result_reg, scratch);
  __ OrP(result_reg, scratch_low, r0);
  __ b(&negate, Label::kNear);

  __ bind(&out_of_range);
  __ mov(result_reg, Operand::Zero());
  __ b(&done, Label::kNear);

  __ bind(&only_low);
  // 52 <= exponent <= 83, shift only scratch_low.
  // On entry, scratch contains: 52 - exponent.
  __ LoadComplementRR(scratch, scratch);
  __ ShiftLeft(result_reg, scratch_low, scratch);

  __ bind(&negate);
  // If input was positive, scratch_high ASR 31 equals 0 and
  // scratch_high LSR 31 equals zero.
  // New result = (result eor 0) + 0 = result.
  // If the input was negative, we have to negate the result.
  // Input_high ASR 31 equals 0xffffffff and scratch_high LSR 31 equals 1.
  // New result = (result eor 0xffffffff) + 1 = 0 - result.
  __ ShiftRightArith(r0, scratch_high, Operand(31));
#if V8_TARGET_ARCH_S390X
  __ lgfr(r0, r0);
  __ ShiftRightP(r0, r0, Operand(32));
#endif
  __ XorP(result_reg, r0);
  __ ShiftRight(r0, scratch_high, Operand(31));
  __ AddP(result_reg, r0);

  __ bind(&done);
  __ Pop(scratch_high, scratch_low);

  __ bind(&fastpath_done);
  __ pop(scratch);

  __ Ret();
}


// Handle the case where the lhs and rhs are the same object.
// Equality is almost reflexive (everything but NaN), so this is a test
// for "identity and not NaN".
static void EmitIdenticalObjectComparison(MacroAssembler* masm,
                                          Label* slow,
                                          Condition cond) {
  Label not_identical;
  Label heap_number, return_equal;
  __ CmpP(r2, r3);
  __ bne(&not_identical);

  // Test for NaN. Sadly, we can't just compare to Factory::nan_value(),
  // so we do the second best thing - test it ourselves.
  // They are both equal and they are not both Smis so both of them are not
  // Smis.  If it's not a heap number, then return equal.
  if (cond == lt || cond == gt) {
    __ CompareObjectType(r2, r6, r6, FIRST_SPEC_OBJECT_TYPE);
    __ bge(slow);
  } else {
    __ CompareObjectType(r2, r6, r6, HEAP_NUMBER_TYPE);
    __ beq(&heap_number);
    // Comparing JS objects with <=, >= is complicated.
    if (cond != eq) {
        __ CmpP(r6, Operand(FIRST_SPEC_OBJECT_TYPE));
      __ bge(slow);
      // Normally here we fall through to return_equal, but undefined is
      // special: (undefined == undefined) == true, but
      // (undefined <= undefined) == false!  See ECMAScript 11.8.5.
      if (cond == le || cond == ge) {
          __ CmpP(r6, Operand(ODDBALL_TYPE));
          __ bne(&return_equal);
          __ CompareRoot(r2, Heap::kUndefinedValueRootIndex);
          __ bne(&return_equal);
          if (cond == le) {
            // undefined <= undefined should fail.
            __ LoadImmP(r2, Operand(GREATER));
          } else  {
            // undefined >= undefined should fail.
            __ LoadImmP(r2, Operand(LESS));
        }
        __ Ret();
      }
    }
  }

  __ bind(&return_equal);
  if (cond == lt) {
    __ LoadImmP(r2, Operand(GREATER));  // Things aren't less than themselves.
  } else if (cond == gt) {
    __ LoadImmP(r2, Operand(LESS));  // Things aren't greater than themselves.
  } else {
    __ LoadImmP(r2, Operand(EQUAL));  // Things are <=, >=, ==, === themselves
  }
  __ Ret();

  // For less and greater we don't have to check for NaN since the result of
  // x < x is false regardless.  For the others here is some code to check
  // for NaN.
  if (cond != lt && cond != gt) {
    __ bind(&heap_number);
    // It is a heap number, so return non-equal if it's NaN and equal if it's
    // not NaN.

      // The representation of NaN values has all exponent bits (52..62) set,
      // and not all mantissa bits (0..51) clear.
      // Read top bits of double representation (second word of value).
      __ LoadlW(r4, FieldMemOperand(r2, HeapNumber::kExponentOffset));
      // Test that exponent bits are all set.
      STATIC_ASSERT(HeapNumber::kExponentMask == 0x7ff00000u);
      __ ExtractBitMask(r5, r4, HeapNumber::kExponentMask);
      __ CmpLogicalP(r5, Operand(0x7ff));
      __ bne(&return_equal);

      // Shift out flag and all exponent bits, retaining only mantissa.
      __ sll(r4, Operand(HeapNumber::kNonMantissaBitsInTopWord));
      // Or with all low-bits of mantissa.
      __ LoadlW(r5, FieldMemOperand(r2, HeapNumber::kMantissaOffset));
      __ OrP(r2, r5, r4);
      __ CmpP(r2, Operand::Zero());
      // For equal we already have the right value in r2:  Return zero (equal)
      // if all bits in mantissa are zero (it's an Infinity) and non-zero if
      // not (it's a NaN).  For <= and >= we need to load r0 with the failing
      // value if it's a NaN.
      if (cond != eq) {
        Label not_equal;
        __ bne(&not_equal, Label::kNear);
        // All-zero means Infinity means equal.
        __ Ret();
        __ bind(&not_equal);
        if (cond == le) {
          __ LoadImmP(r2, Operand(GREATER));  // NaN <= NaN should fail.
        } else {
          __ LoadImmP(r2, Operand(LESS));     // NaN >= NaN should fail.
      }
    }
    __ Ret();
  }
  // No fall through here.

  __ bind(&not_identical);
}


// See comment at call site.
static void EmitSmiNonsmiComparison(MacroAssembler* masm,
                                    Register lhs,
                                    Register rhs,
                                    Label* lhs_not_nan,
                                    Label* slow,
                                    bool strict) {
  DCHECK((lhs.is(r2) && rhs.is(r3)) ||
         (lhs.is(r3) && rhs.is(r2)));

  Label rhs_is_smi;
  __ JumpIfSmi(rhs, &rhs_is_smi);

  // Lhs is a Smi.  Check whether the rhs is a heap number.
  __ CompareObjectType(rhs, r5, r6, HEAP_NUMBER_TYPE);
  if (strict) {
    // If rhs is not a number and lhs is a Smi then strict equality cannot
    // succeed.  Return non-equal
    // If rhs is r2 then there is already a non zero value in it.
    Label skip;
    __ beq(&skip, Label::kNear);
    if (!rhs.is(r2)) {
      __ mov(r2, Operand(NOT_EQUAL));
    }
    __ Ret();
    __ bind(&skip);
  } else {
    // Smi compared non-strictly with a non-Smi non-heap-number.  Call
    // the runtime.
    __ bne(slow);
  }

  // Lhs is a smi, rhs is a number.
  // Convert lhs to a double in d7.
  __ SmiToDouble(d7, lhs);
  // Load the double from rhs, tagged HeapNumber r2, to d6.
  __ LoadF(d6, FieldMemOperand(rhs, HeapNumber::kValueOffset));

  // We now have both loaded as doubles but we can skip the lhs nan check
  // since it's a smi.
  __ b(lhs_not_nan);

  __ bind(&rhs_is_smi);
  // Rhs is a smi.  Check whether the non-smi lhs is a heap number.
  __ CompareObjectType(lhs, r6, r6, HEAP_NUMBER_TYPE);
  if (strict) {
    // If lhs is not a number and rhs is a smi then strict equality cannot
    // succeed.  Return non-equal.
    // If lhs is r2 then there is already a non zero value in it.
    Label skip;
    __ beq(&skip, Label::kNear);
    if (!lhs.is(r2)) {
      __ mov(r2, Operand(NOT_EQUAL));
    }
    __ Ret();
    __ bind(&skip);
  } else {
    // Smi compared non-strictly with a non-smi non-heap-number.  Call
    // the runtime.
    __ bne(slow);
  }

  // Rhs is a smi, lhs is a heap number.
  // Load the double from lhs, tagged HeapNumber r3, to d7.
  __ LoadF(d7, FieldMemOperand(lhs, HeapNumber::kValueOffset));
  // Convert rhs to a double in d6.
  __ SmiToDouble(d6, rhs);
  // Fall through to both_loaded_as_doubles.
}


// See comment at call site.
static void EmitStrictTwoHeapObjectCompare(MacroAssembler* masm,
                                           Register lhs,
                                           Register rhs) {
    DCHECK((lhs.is(r2) && rhs.is(r3)) ||
           (lhs.is(r3) && rhs.is(r2)));

    // If either operand is a JS object or an oddball value, then they are
    // not equal since their pointers are different.
    // There is no test for undetectability in strict equality.
    STATIC_ASSERT(LAST_TYPE == LAST_SPEC_OBJECT_TYPE);
    Label first_non_object;
    // Get the type of the first operand into r4 and compare it with
    // FIRST_SPEC_OBJECT_TYPE.
    __ CompareObjectType(rhs, r4, r4, FIRST_SPEC_OBJECT_TYPE);
    __ blt(&first_non_object, Label::kNear);

    // Return non-zero (r2 is not zero)
    Label return_not_equal;
    __ bind(&return_not_equal);
    __ Ret();

    __ bind(&first_non_object);
    // Check for oddballs: true, false, null, undefined.
    __ CmpP(r4, Operand(ODDBALL_TYPE));
    __ beq(&return_not_equal);

    __ CompareObjectType(lhs, r5, r5, FIRST_SPEC_OBJECT_TYPE);
    __ bge(&return_not_equal);

    // Check for oddballs: true, false, null, undefined.
    __ CmpP(r5, Operand(ODDBALL_TYPE));
    __ beq(&return_not_equal);

    // Now that we have the types we might as well check for
    // internalized-internalized.
    STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
    __ OrP(r4, r4, r5);
    __ AndP(r0, r4, Operand(kIsNotStringMask | kIsNotInternalizedMask));
    __ beq(&return_not_equal/*, cr0*/);
}


// See comment at call site.
static void EmitCheckForTwoHeapNumbers(MacroAssembler* masm,
                                       Register lhs,
                                       Register rhs,
                                       Label* both_loaded_as_doubles,
                                       Label* not_heap_numbers,
                                       Label* slow) {
  DCHECK((lhs.is(r2) && rhs.is(r3)) ||
         (lhs.is(r3) && rhs.is(r2)));

  __ CompareObjectType(rhs, r5, r4, HEAP_NUMBER_TYPE);
  __ bne(not_heap_numbers);
  __ LoadP(r4, FieldMemOperand(lhs, HeapObject::kMapOffset));
  __ CmpP(r4, r5);
  __ bne(slow);  // First was a heap number, second wasn't.  Go slow case.

  // Both are heap numbers.  Load them up then jump to the code we have
  // for that.
  __ LoadF(d6, FieldMemOperand(rhs, HeapNumber::kValueOffset));
  __ LoadF(d7, FieldMemOperand(lhs, HeapNumber::kValueOffset));

  __ b(both_loaded_as_doubles);
}


// Fast negative check for internalized-to-internalized equality.
static void EmitCheckForInternalizedStringsOrObjects(MacroAssembler* masm,
                                                     Register lhs,
                                                     Register rhs,
                                                     Label* possible_strings,
                                                     Label* not_both_strings) {
  DCHECK((lhs.is(r2) && rhs.is(r3)) ||
         (lhs.is(r3) && rhs.is(r2)));

  // r4 is object type of rhs.
  Label object_test;
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  __ mov(r0, Operand(kIsNotStringMask));
  __ AndP(r0, r4);
  __ bne(&object_test /*, cr0*/, Label::kNear);
  __ mov(r0, Operand(kIsNotInternalizedMask));
  __ AndP(r0, r4);
  __ bne(possible_strings /*, cr0*/);
  __ CompareObjectType(lhs, r5, r5, FIRST_NONSTRING_TYPE);
  __ bge(not_both_strings);
  __ mov(r0, Operand(kIsNotInternalizedMask));
  __ AndP(r0, r5);
  __ bne(possible_strings /*, cr0*/);

  // Both are internalized.  We already checked they weren't the same pointer
  // so they are not equal.
  __ LoadImmP(r2, Operand(NOT_EQUAL));
  __ Ret();

  __ bind(&object_test);
  __ CmpP(r4, Operand(FIRST_SPEC_OBJECT_TYPE));
  __ blt(not_both_strings);
  __ CompareObjectType(lhs, r4, r5, FIRST_SPEC_OBJECT_TYPE);
  __ blt(not_both_strings);
  // If both objects are undetectable, they are equal. Otherwise, they
  // are not equal, since they are different objects and an object is not
  // equal to undefined.
  __ LoadP(r5, FieldMemOperand(rhs, HeapObject::kMapOffset));
  __ LoadlB(r4, FieldMemOperand(r4, Map::kBitFieldOffset));
  __ LoadlB(r5, FieldMemOperand(r5, Map::kBitFieldOffset));
  __ AndP(r2, r5, r4);
  __ AndP(r2, Operand(1 << Map::kIsUndetectable));
  __ XorP(r2, Operand(1 << Map::kIsUndetectable));
  __ Ret();
}


static void ICCompareStub_CheckInputType(MacroAssembler* masm,
                                         Register input,
                                         Register scratch,
                                         CompareIC::State expected,
                                         Label* fail) {
  Label ok;
  if (expected == CompareIC::SMI) {
    __ JumpIfNotSmi(input, fail);
  } else if (expected == CompareIC::NUMBER) {
    __ JumpIfSmi(input, &ok);
    __ CheckMap(input, scratch, Heap::kHeapNumberMapRootIndex, fail,
                DONT_DO_SMI_CHECK);
  }
  // We could be strict about internalized/non-internalized here, but as long as
  // hydrogen doesn't care, the stub doesn't have to care either.
  __ bind(&ok);
}


// On entry r3 and r4 are the values to be compared.
// On exit r2 is 0, positive or negative to indicate the result of
// the comparison.
void ICCompareStub::GenerateGeneric(MacroAssembler* masm) {
  Register lhs = r3;
  Register rhs = r2;
  Condition cc = GetCondition();

  Label miss;
  ICCompareStub_CheckInputType(masm, lhs, r4, left_, &miss);
  ICCompareStub_CheckInputType(masm, rhs, r5, right_, &miss);

  Label slow;  // Call builtin.
  Label not_smis, both_loaded_as_doubles, lhs_not_nan;

  Label not_two_smis, smi_done;
  __ OrP(r4, r3, r2);
  __ JumpIfNotSmi(r4, &not_two_smis);
  __ SmiUntag(r3);
  __ SmiUntag(r2);
  __ SubP(r2, r3, r2);
  __ Ret();
  __ bind(&not_two_smis);

  // NOTICE! This code is only reached after a smi-fast-case check, so
  // it is certain that at least one operand isn't a smi.

  // Handle the case where the objects are identical.  Either returns the answer
  // or goes to slow.  Only falls through if the objects were not identical.
  EmitIdenticalObjectComparison(masm, &slow, cc);

  // If either is a Smi (we know that not both are), then they can only
  // be strictly equal if the other is a HeapNumber.
  STATIC_ASSERT(kSmiTag == 0);
  DCHECK_EQ(0, Smi::FromInt(0));
  __ AndP(r4, lhs, rhs);
  __ JumpIfNotSmi(r4, &not_smis);
  // One operand is a smi.  EmitSmiNonsmiComparison generates code that can:
  // 1) Return the answer.
  // 2) Go to slow.
  // 3) Fall through to both_loaded_as_doubles.
  // 4) Jump to lhs_not_nan.
  // In cases 3 and 4 we have found out we were dealing with a number-number
  // comparison.  The double values of the numbers have been loaded
  // into d7 and d6.
  EmitSmiNonsmiComparison(masm, lhs, rhs, &lhs_not_nan, &slow, strict());

  __ bind(&both_loaded_as_doubles);
  // The arguments have been converted to doubles and stored in d6 and d7
  __ bind(&lhs_not_nan);
  Label no_nan;
  __ cdbr(d7, d6);

  Label nan, equal, less_than;
  __ bunordered(&nan);
  __ beq(&equal, Label::kNear);
  __ blt(&less_than, Label::kNear);
  __ LoadImmP(r2, Operand(GREATER));
  __ Ret();
  __ bind(&equal);
  __ LoadImmP(r2, Operand(EQUAL));
  __ Ret();
  __ bind(&less_than);
  __ LoadImmP(r2, Operand(LESS));
  __ Ret();

  __ bind(&nan);
  // If one of the sides was a NaN then the v flag is set.  Load r2 with
  // whatever it takes to make the comparison fail, since comparisons with NaN
  // always fail.
  if (cc == lt || cc == le) {
    __ LoadImmP(r2, Operand(GREATER));
  } else {
    __ LoadImmP(r2, Operand(LESS));
  }
  __ Ret();

  __ bind(&not_smis);
  // At this point we know we are dealing with two different objects,
  // and neither of them is a Smi.  The objects are in rhs_ and lhs_.
  if (strict()) {
    // This returns non-equal for some object types, or falls through if it
    // was not lucky.
    EmitStrictTwoHeapObjectCompare(masm, lhs, rhs);
  }

  Label check_for_internalized_strings;
  Label flat_string_check;
  // Check for heap-number-heap-number comparison.  Can jump to slow case,
  // or load both doubles into r2, r3, r4, r5 and jump to the code that handles
  // that case.  If the inputs are not doubles then jumps to
  // check_for_internalized_strings.
  // In this case r4 will contain the type of rhs_.  Never falls through.
  EmitCheckForTwoHeapNumbers(masm,
                             lhs,
                             rhs,
                             &both_loaded_as_doubles,
                             &check_for_internalized_strings,
                             &flat_string_check);

  __ bind(&check_for_internalized_strings);
  // In the strict case the EmitStrictTwoHeapObjectCompare already took care of
  // internalized strings.
  if (cc == eq && !strict()) {
    // Returns an answer for two internalized strings or two detectable objects.
    // Otherwise jumps to string case or not both strings case.
    // Assumes that r4 is the type of rhs_ on entry.
    EmitCheckForInternalizedStringsOrObjects(
        masm, lhs, rhs, &flat_string_check, &slow);
  }

  // Check for both being sequential ASCII strings, and inline if that is the
  // case.
  __ bind(&flat_string_check);

  __ JumpIfNonSmisNotBothSequentialAsciiStrings(lhs, rhs, r4, r5, &slow);

  __ IncrementCounter(isolate()->counters()->string_compare_native(), 1, r4,
                      r5);
  if (cc == eq) {
    StringCompareStub::GenerateFlatAsciiStringEquals(masm,
                                                     lhs,
                                                     rhs,
                                                     r4,
                                                     r5);
  } else {
    StringCompareStub::GenerateCompareFlatAsciiStrings(masm,
                                                       lhs,
                                                       rhs,
                                                       r4,
                                                       r5,
                                                       r6);
  }
  // Never falls through to here.

  __ bind(&slow);

  __ Push(lhs, rhs);
  // Figure out which native to call and setup the arguments.
  Builtins::JavaScript native;
  if (cc == eq) {
    native = strict() ? Builtins::STRICT_EQUALS : Builtins::EQUALS;
  } else {
    native = Builtins::COMPARE;
    int ncr;  // NaN compare result
    if (cc == lt || cc == le) {
      ncr = GREATER;
    } else {
      DCHECK(cc == gt || cc == ge);  // remaining cases
      ncr = LESS;
    }
    __ LoadSmiLiteral(r2, Smi::FromInt(ncr));
    __ push(r2);
  }

  // Call the native; it returns -1 (less), 0 (equal), or 1 (greater)
  // tagged as a small integer.
  __ InvokeBuiltin(native, JUMP_FUNCTION);

  __ bind(&miss);
  GenerateMiss(masm);
}


void StoreBufferOverflowStub::Generate(MacroAssembler* masm) {
  // We don't allow a GC during a store buffer overflow so there is no need to
  // store the registers in any particular way, but we do have to store and
  // restore them.
  __ LoadRR(r0, r14);
  __ MultiPush(kJSCallerSaved | r0.bit());
  if (save_doubles_ == kSaveFPRegs) {
    __ SaveFPRegs(sp, 0, DoubleRegister::kNumVolatileRegisters);
  }
  const int argument_count = 1;
  const int fp_argument_count = 0;
  const Register scratch = r3;

  AllowExternalCallThatCantCauseGC scope(masm);
  __ PrepareCallCFunction(argument_count, fp_argument_count, scratch);
#ifdef V8_OS_ZOS
  __ mov(r1, Operand(ExternalReference::isolate_address(isolate())));
#else
  __ mov(r2, Operand(ExternalReference::isolate_address(isolate())));
#endif
  __ CallCFunction(
      ExternalReference::store_buffer_overflow_function(isolate()),
      argument_count);
  if (save_doubles_ == kSaveFPRegs) {
    __ RestoreFPRegs(sp, 0, DoubleRegister::kNumVolatileRegisters);
  }
  __ MultiPop(kJSCallerSaved | r0.bit());
  __ LoadRR(r14, r0);
  __ Ret();
}


void StoreRegistersStateStub::Generate(MacroAssembler* masm) {
  __ PushSafepointRegisters();
  __ b(r14);
}


void RestoreRegistersStateStub::Generate(MacroAssembler* masm) {
  __ PopSafepointRegisters();
  __ b(r14);
}


void MathPowStub::Generate(MacroAssembler* masm) {
  const Register base = r3;
  const Register exponent = r4;
  const Register heapnumbermap = r7;
  const Register heapnumber = r2;
  const DoubleRegister double_base = d1;
  const DoubleRegister double_exponent = d2;
  const DoubleRegister double_result = d3;
  const DoubleRegister double_scratch = d0;
  const Register scratch = r1;
  const Register scratch2 = r9;

  Label call_runtime, done, int_exponent;
  if (exponent_type_ == ON_STACK) {
    Label base_is_smi, unpack_exponent;
    // The exponent and base are supplied as arguments on the stack.
    // This can only happen if the stub is called from non-optimized code.
    // Load input parameters from stack to double registers.
    __ LoadP(base, MemOperand(sp, 1 * kPointerSize));
    __ LoadP(exponent, MemOperand(sp, 0 * kPointerSize));

    __ LoadRoot(heapnumbermap, Heap::kHeapNumberMapRootIndex);

    __ UntagAndJumpIfSmi(scratch, base, &base_is_smi);
    __ LoadP(scratch, FieldMemOperand(base, JSObject::kMapOffset));
    __ CmpP(scratch, heapnumbermap);
    __ bne(&call_runtime);

    __ LoadF(double_base, FieldMemOperand(base, HeapNumber::kValueOffset));
    __ b(&unpack_exponent, Label::kNear);

    __ bind(&base_is_smi);
    __ ConvertIntToDouble(scratch, double_base);
    __ bind(&unpack_exponent);

    __ UntagAndJumpIfSmi(scratch, exponent, &int_exponent);
    __ LoadP(scratch, FieldMemOperand(exponent, JSObject::kMapOffset));
    __ CmpP(scratch, heapnumbermap);
    __ bne(&call_runtime);

    __ LoadF(double_exponent,
           FieldMemOperand(exponent, HeapNumber::kValueOffset));
  } else if (exponent_type_ == TAGGED) {
    // Base is already in double_base.
    __ UntagAndJumpIfSmi(scratch, exponent, &int_exponent);

    __ LoadF(double_exponent,
           FieldMemOperand(exponent, HeapNumber::kValueOffset));
  }

  if (exponent_type_ != INTEGER) {
    // Detect integer exponents stored as double.
    __ TryDoubleToInt32Exact(scratch, double_exponent,
                             scratch2, double_scratch);
    __ beq(&int_exponent, Label::kNear);

    if (exponent_type_ == ON_STACK) {
      // Detect square root case.  Crankshaft detects constant +/-0.5 at
      // compile time and uses DoMathPowHalf instead.  We then skip this check
      // for non-constant cases of +/-0.5 as these hardly occur.
      Label not_plus_half, not_minus_inf1, not_minus_inf2;

      // Test for 0.5.
      __ LoadDoubleLiteral(double_scratch, 0.5, scratch);
      __ cdbr(double_exponent, double_scratch);
      __ bne(&not_plus_half, Label::kNear);

      // Calculates square root of base.  Check for the special case of
      // Math.pow(-Infinity, 0.5) == Infinity (ECMA spec, 15.8.2.13).
      __ LoadDoubleLiteral(double_scratch, -V8_INFINITY, scratch);
      __ cdbr(double_base, double_scratch);
      __ bne(&not_minus_inf1, Label::kNear);
      __ lcdbr(double_result, double_scratch);
      __ b(&done);
      __ bind(&not_minus_inf1);

      // Add +0 to convert -0 to +0.
      __ ldr(double_scratch, double_base);
      __ lzdr(kDoubleRegZero);
      __ adbr(double_scratch, kDoubleRegZero);
      __ sqdbr(double_result, double_scratch);
      __ b(&done);

      __ bind(&not_plus_half);
      __ LoadDoubleLiteral(double_scratch, -0.5, scratch);
      __ cdbr(double_exponent, double_scratch);
      __ bne(&call_runtime);

      // Calculates square root of base.  Check for the special case of
      // Math.pow(-Infinity, -0.5) == 0 (ECMA spec, 15.8.2.13).
      __ LoadDoubleLiteral(double_scratch, -V8_INFINITY, scratch);
      __ cdbr(double_base, double_scratch);
      __ bne(&not_minus_inf2, Label::kNear);
      __ ldr(double_result, kDoubleRegZero);
      __ b(&done);
      __ bind(&not_minus_inf2);

      // Add +0 to convert -0 to +0.
      __ ldr(double_scratch, double_base);
      __ lzdr(kDoubleRegZero);
      __ adbr(double_scratch, kDoubleRegZero);
      __ LoadDoubleLiteral(double_result, 1.0, scratch);
      __ sqdbr(double_scratch, double_scratch);
      __ ddbr(double_result, double_scratch);
      __ b(&done);
    }

    __ push(r14);
    {
      AllowExternalCallThatCantCauseGC scope(masm);
      __ PrepareCallCFunction(0, 2, scratch);
      __ MovToFloatParameters(double_base, double_exponent);
      __ CallCFunction(
          ExternalReference::power_double_double_function(isolate()),
          0, 2);
    }
    __ pop(r14);
    __ MovFromFloatResult(double_result);
    __ b(&done);
  }

  // Calculate power with integer exponent.
  __ bind(&int_exponent);

  // Get two copies of exponent in the registers scratch and exponent.
  if (exponent_type_ == INTEGER) {
    __ LoadRR(scratch, exponent);
  } else {
    // Exponent has previously been stored into scratch as untagged integer.
    __ LoadRR(exponent, scratch);
  }
  __ ldr(double_scratch, double_base);  // Back up base.
  __ LoadImmP(scratch2, Operand(1));
  __ ConvertIntToDouble(scratch2, double_result);

  // Get absolute value of exponent.
  Label positive_exponent;
  __ CmpP(scratch, Operand::Zero());
  __ bge(&positive_exponent, Label::kNear);
  __ LoadComplementRR(scratch, scratch);
  __ bind(&positive_exponent);

  Label while_true, no_carry, loop_end;
  __ bind(&while_true);
  __ mov(scratch2, Operand(1));
  __ AndP(scratch2, scratch);
  __ beq(&no_carry /*, cr0*/, Label::kNear);
  __ mdbr(double_result, double_scratch);
  __ bind(&no_carry);
  __ ShiftRightArithP(scratch, scratch, Operand(1));
  __ beq(&loop_end /*, cr0*/, Label::kNear);
  __ mdbr(double_scratch, double_scratch);
  __ b(&while_true);
  __ bind(&loop_end);

  __ CmpP(exponent, Operand::Zero());
  __ bge(&done);

  // get 1/double_result:
  __ ldr(double_scratch, double_result);
  __ LoadImmP(scratch2, Operand(1));
  __ ConvertIntToDouble(scratch2, double_result);
  __ ddbr(double_result, double_scratch);

  // Test whether result is zero.  Bail out to check for subnormal result.
  // Due to subnormals, x^-y == (1/x)^y does not hold in all cases.
  __ cdbr(double_result, kDoubleRegZero);
  __ bne(&done, Label::kNear);
  // double_exponent may not containe the exponent value if the input was a
  // smi.  We set it with exponent value before bailing out.
  __ ConvertIntToDouble(exponent, double_exponent);

  // Returning or bailing out.
  Counters* counters = isolate()->counters();
  if (exponent_type_ == ON_STACK) {
    // The arguments are still on the stack.
    __ bind(&call_runtime);
    __ TailCallRuntime(Runtime::kMathPowRT, 2, 1);

    // The stub is called from non-optimized code, which expects the result
    // as heap number in exponent.
    __ bind(&done);
    __ AllocateHeapNumber(
        heapnumber, scratch, scratch2, heapnumbermap, &call_runtime);
    __ StoreF(double_result,
            FieldMemOperand(heapnumber, HeapNumber::kValueOffset));
    DCHECK(heapnumber.is(r2));
    __ IncrementCounter(counters->math_pow(), 1, scratch, scratch2);
    __ Ret(2);
  } else {
    __ push(r14);
    {
      AllowExternalCallThatCantCauseGC scope(masm);
      __ PrepareCallCFunction(0, 2, scratch);
      __ MovToFloatParameters(double_base, double_exponent);
      __ CallCFunction(
          ExternalReference::power_double_double_function(isolate()),
          0, 2);
    }
    __ pop(r14);
    __ MovFromFloatResult(double_result);

    __ bind(&done);
    __ IncrementCounter(counters->math_pow(), 1, scratch, scratch2);
    __ Ret();
  }
}


bool CEntryStub::NeedsImmovableCode() {
  return true;
}


void CodeStub::GenerateStubsAheadOfTime(Isolate* isolate) {
  CEntryStub::GenerateAheadOfTime(isolate);
  StoreBufferOverflowStub::GenerateFixedRegStubsAheadOfTime(isolate);
  StubFailureTrampolineStub::GenerateAheadOfTime(isolate);
  ArrayConstructorStubBase::GenerateStubsAheadOfTime(isolate);
  CreateAllocationSiteStub::GenerateAheadOfTime(isolate);
  BinaryOpICStub::GenerateAheadOfTime(isolate);
  StoreRegistersStateStub::GenerateAheadOfTime(isolate);
  RestoreRegistersStateStub::GenerateAheadOfTime(isolate);
  BinaryOpICWithAllocationSiteStub::GenerateAheadOfTime(isolate);
}


void StoreRegistersStateStub::GenerateAheadOfTime(Isolate* isolate) {
  StoreRegistersStateStub stub(isolate);
  stub.GetCode();
}


void RestoreRegistersStateStub::GenerateAheadOfTime(Isolate* isolate) {
  RestoreRegistersStateStub stub(isolate);
  stub.GetCode();
}


void CodeStub::GenerateFPStubs(Isolate* isolate) {
  SaveFPRegsMode mode = kSaveFPRegs;
  CEntryStub save_doubles(isolate, 1, mode);
  StoreBufferOverflowStub stub(isolate, mode);
  // These stubs might already be in the snapshot, detect that and don't
  // regenerate, which would lead to code stub initialization state being messed
  // up.
  Code* save_doubles_code;
  if (!save_doubles.FindCodeInCache(&save_doubles_code)) {
    save_doubles_code = *save_doubles.GetCode();
  }
  Code* store_buffer_overflow_code;
  if (!stub.FindCodeInCache(&store_buffer_overflow_code)) {
      store_buffer_overflow_code = *stub.GetCode();
  }
  isolate->set_fp_stubs_generated(true);
}


void CEntryStub::GenerateAheadOfTime(Isolate* isolate) {
  CEntryStub stub(isolate, 1, kDontSaveFPRegs);
  stub.GetCode();
}


void CEntryStub::Generate(MacroAssembler* masm) {
  // Called from JavaScript; parameters are on stack as if calling JS function.
  // r2: number of arguments including receiver
  // r3: pointer to builtin function
  // fp: frame pointer  (restored after C call)
  // sp: stack pointer  (restored as callee's sp after C call)
  // cp: current context  (C callee-saved)

  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  __ LoadRR(r7, r3);

  // Compute the argv pointer.
  __ ShiftLeftP(r3, r2, Operand(kPointerSizeLog2));
  __ lay(r3, MemOperand(r3, sp, -kPointerSize));


  // Enter the exit frame that transitions from JavaScript to C++.
  FrameScope scope(masm, StackFrame::MANUAL);

  // Need at least one extra slot for return address location.
  int arg_stack_space = 1;

  // S390 LINUX ABI:
#if V8_TARGET_ARCH_S390X && !ABI_RETURNS_OBJECT_PAIRS_IN_REGS
  // Pass buffer for return value on stack if necessary
  if (result_size_ > 1) {
    DCHECK_EQ(2, result_size_);
    arg_stack_space += 2;
  }
#endif
#if V8_TARGET_ARCH_S390X

  // 64-bit linux pass Argument object by reference not value
  arg_stack_space += 2;
#endif

  __ EnterExitFrame(save_doubles_, arg_stack_space);

  // Store a copy of argc, argv in callee-saved registers for later.
#ifdef V8_OS_ZOS
  __ LoadRR(r9, r2);
  __ LoadRR(r10, r3);
  // r2, r9: number of arguments including receiver  (C callee-saved)
  // r3, r10: pointer to the first argument
  // r7: pointer to builtin function descriptor (C callee-saved)
  // r8: pointer to builtin function (C callee-saved)
#else
  __ LoadRR(r6, r2);
  __ LoadRR(r8, r3);
  // r2, r6: number of arguments including receiver  (C callee-saved)
  // r3, r8: pointer to the first argument
  // r7: pointer to builtin function  (C callee-saved)
#endif

  // Result returned in registers or stack, depending on result size and ABI.

  Register isolate_reg = r4;
#if V8_TARGET_ARCH_S390X && !ABI_RETURNS_OBJECT_PAIRS_IN_REGS
  if (result_size_ > 1) {
    // The return value is 16-byte non-scalar value.
    // Use frame storage reserved by calling function to pass return
    // buffer as implicit first argument.
    __ LoadRR(r4, r3);
    __ LoadRR(r3, r2);
    __ la(r2, MemOperand(sp, (kStackFrameExtraParamSlot + 1) * kPointerSize));
    isolate_reg = r5;
  }
#endif
  // Call C built-in.
  __ mov(isolate_reg, Operand(ExternalReference::isolate_address(isolate())));


#if V8_OS_ZOS
  // TODO(mcornac): Move params from r2-r6 to r1-r3.
  __ LoadRR(r1, r2);
  __ LoadRR(r2, r3);
  __ LoadRR(r3, r4);

  // TODO(mcornac): fn descriptor.
  // Load environment from slot 0 of fn desc.
  __ LoadP(r5, MemOperand(r7));
#if !defined(USE_SIMULATOR)
  // Load function pointer from slot 1 of fn desc.
  __ LoadP(r8, MemOperand(r7, kPointerSize));
#else
  __ LoadRR(r8, r7);
#endif  // USE_SIMULATOR
  Register target = r8;
#else
  Register target = r7;
#endif   // V8_OS_ZOS

  // To let the GC traverse the return address of the exit frames, we need to
  // know where the return address is. The CEntryStub is unmovable, so
  // we can store the address on the stack to be able to find it again and
  // we never have to restore it, because it will not change.
  // Compute the return address in lr to return to after the jump below. Pc is
  // already at '+ 8' from the current instruction but return is after three
  // instructions so add another 4 to pc to get the return address.
  { Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm);
    Label return_label;

#if V8_OS_ZOS
    Register ra = r7;
#else
    Register ra = r14;
#endif
    __ larl(ra, &return_label);  // Generate the return addr of call later.
#if V8_OS_ZOS
    // TODO(mcornac): why do I have to -2?
    __ lay(ra, MemOperand(ra, -2));
    __ StoreP(ra, MemOperand(sp, kStackFrameRASlot * kPointerSize));
#else
    __ StoreP(ra, MemOperand(sp, kStackFrameRASlot * kPointerSize));
#endif
    // zLinux ABI requires caller's frame to have sufficient space for callee
    // preserved regsiter save area.
    // __ lay(sp, MemOperand(sp, -kCalleeRegisterSaveAreaSize));
    __ positions_recorder()->WriteRecordedPositions();

#if V8_OS_ZOS
    // Load the biased stack pointer into r4 before calling native code
    // Stack Pointer Bias = Xplink Bias(2048) + SaveArea(12 ptrs +
    // Reserved(2ptrs) + Debug Area(1ptr) +
    // + Arg Area Prefix(1ptr) + Argument Area(3 ptrs).
     __ lay(r4, MemOperand(sp, -(kStackPointerBias + 19*kPointerSize)));
#endif

    __ b(target);
    __ bind(&return_label);
  }

#if V8_OS_ZOS
  // TODO(mcornac): r9 and r10 are used to store argc and argv on z/OS instead
  // of r6 and r8 since r6 is not callee saved.
  __ LoadRR(r6, r9);
  __ LoadRR(r8, r10);
  __ InitializeRootRegister();  // Rematerializing the root address in r10

  if (result_size_ == 1) {
    __ LoadRR(r2, r3);
  }
  else {
    __ LoadRR(r3, r2);
    __ LoadRR(r2, r1);
  }
#endif

  // roohack - do we need to (re)set FPU state?

#if V8_TARGET_ARCH_S390X && !ABI_RETURNS_OBJECT_PAIRS_IN_REGS
  // If return value is on the stack, pop it to registers.
  if (result_size_ > 1) {
    __ LoadP(r3, MemOperand(r2, kPointerSize));
    __ LoadP(r2, MemOperand(r2));
  }
#endif

  // Runtime functions should not return 'the hole'.  Allowing it to escape may
  // lead to crashes in the IC code later.
  if (FLAG_debug_code) {
    Label okay;
    __ CompareRoot(r2, Heap::kTheHoleValueRootIndex);
    __ bne(&okay, Label::kNear);
    __ stop("The hole escaped");
    __ bind(&okay);
  }

  // Check result for exception sentinel.
  Label exception_returned;
  __ CompareRoot(r2, Heap::kExceptionRootIndex);
  __ beq(&exception_returned, Label::kNear);

  ExternalReference pending_exception_address(
      Isolate::kPendingExceptionAddress, isolate());

  // Check that there is no pending exception, otherwise we
  // should have returned the exception sentinel.
  if (FLAG_debug_code) {
    Label okay;
    __ mov(r4, Operand(pending_exception_address));
    __ LoadP(r4, MemOperand(r4));
    __ CompareRoot(r4, Heap::kTheHoleValueRootIndex);
    // Cannot use check here as it attempts to generate call into runtime.
    __ beq(&okay, Label::kNear);
    __ stop("Unexpected pending exception");
    __ bind(&okay);
  }

  // Exit C frame and return.
  // r2:r3: result
  // sp: stack pointer
  // fp: frame pointer
  // r6: still holds argc (callee-saved).
  __ LeaveExitFrame(save_doubles_, r6, true);
  __ b(r14);

  // Handling of exception.
  __ bind(&exception_returned);

  // Retrieve the pending exception.
  __ mov(r4, Operand(pending_exception_address));
  __ LoadP(r2, MemOperand(r4));

  // Clear the pending exception.
  __ LoadRoot(r5, Heap::kTheHoleValueRootIndex);
  __ StoreP(r5, MemOperand(r4));

  // Special handling of termination exceptions which are uncatchable
  // by javascript code.
  Label throw_termination_exception;
  __ CompareRoot(r2, Heap::kTerminationExceptionRootIndex);
  __ beq(&throw_termination_exception, Label::kNear);

  // Handle normal exception.
  __ Throw(r2);

  __ bind(&throw_termination_exception);
  __ ThrowUncatchable(r2);
}


void JSEntryStub::GenerateBody(MacroAssembler* masm, bool is_construct) {
  // r2: code entry
  // r3: function
  // r4: receiver
  // r5: argc
  // r6: argv

  Label invoke, handler_entry, exit;

  // Called from C
#if ABI_USES_FUNCTION_DESCRIPTORS
  __ function_descriptor();
#endif

  ProfileEntryHookStub::MaybeCallEntryHook(masm);

#if V8_OS_ZOS
  __ LoadRR(sp, r4);
  __ lay(sp, MemOperand(sp, -12 * kPointerSize));
  __ StoreMultipleP(r4, sp, MemOperand(sp, 0));
  // Expecting paramters in r2-r6. XPLINK uses r1-r3 for the first three
  // parameters and also places them starting at r4+2112 on the biased stack.
  // Explicitly load argc and argv from stack back into r5/r6 respectively.
  __ LoadP(r5, MemOperand(r4, 2048 + (19 * kPointerSize)));
  __ LoadP(r6, MemOperand(r4, 2048 + (20 * kPointerSize)));

  __ LoadRR(r4, r3);
  __ LoadRR(r3, r2);
  __ LoadRR(r2, r1);
#endif  // V8_OS_ZOS

  // saving floating point registers
#if V8_HOST_ARCH_S390X
  // 64bit ABI requires f8 to f15 be saved
  __ lay(sp, MemOperand(sp, -8 * kDoubleSize));
  __ std(d8, MemOperand(sp));
  __ std(d9, MemOperand(sp, 1 * kDoubleSize));
  __ std(d10, MemOperand(sp, 2 * kDoubleSize));
  __ std(d11, MemOperand(sp, 3 * kDoubleSize));
  __ std(d12, MemOperand(sp, 4 * kDoubleSize));
  __ std(d13, MemOperand(sp, 5 * kDoubleSize));
  __ std(d14, MemOperand(sp, 6 * kDoubleSize));
  __ std(d15, MemOperand(sp, 7 * kDoubleSize));
#else
  // 31bit ABI requires you to store f4 and f6:
  // http://refspecs.linuxbase.org/ELF/zSeries/lzsabi0_s390.html#AEN417
  __ lay(sp, MemOperand(sp, -2 * kDoubleSize));
  __ std(d4, MemOperand(sp));
  __ std(d6, MemOperand(sp, kDoubleSize));
#endif

#if !V8_OS_ZOS
  // zLinux ABI
  //    Incoming parameters:
  //          r2: code entry
  //          r3: function
  //          r4: receiver
  //          r5: argc
  //          r6: argv
  //    Requires us to save the callee-preserved registers r6-r13
  //    General convention is to also save r14 (return addr) and
  //    sp/r15 as well in a single STM/STMG
  __ lay(sp, MemOperand(sp, -10 * kPointerSize));
  __ StoreMultipleP(r6, sp, MemOperand(sp, 0));
#endif

//  int offset_to_argv = kPointerSize * 22; // matches (22*4) above
//  __ LoadlW(r7, MemOperand(sp, offset_to_argv));

  // Push a frame with special values setup to mark it as an entry frame.
  //   Bad FP (-1)
  //   SMI Marker
  //   SMI Marker
  //   kCEntryFPAddress
  //   Frame type
  __ lay(sp, MemOperand(sp, -5 * kPointerSize));
  // Push a bad frame pointer to fail if it is used.
  __ LoadImmP(r10, Operand(-1));

  int marker = is_construct ? StackFrame::ENTRY_CONSTRUCT : StackFrame::ENTRY;
  __ LoadSmiLiteral(r9, Smi::FromInt(marker));
  __ LoadSmiLiteral(r8, Smi::FromInt(marker));
  // Save copies of the top frame descriptor on the stack.
  __ mov(r7, Operand(ExternalReference(Isolate::kCEntryFPAddress, isolate())));
  __ LoadP(r7, MemOperand(r7));
  __ StoreMultipleP(r7, r10, MemOperand(sp, kPointerSize));
  // Set up frame pointer for the frame to be pushed.
  // Need to add kPointerSize, because sp has one extra
  // frame already for the frame type being pushed later.
  __ lay(fp, MemOperand(sp, -EntryFrameConstants::kCallerFPOffset +
                            kPointerSize));


  // If this is the outermost JS call, set js_entry_sp value.
  Label non_outermost_js;
  ExternalReference js_entry_sp(Isolate::kJSEntrySPAddress, isolate());
  __ mov(r7, Operand(ExternalReference(js_entry_sp)));
  __ LoadAndTestP(r8, MemOperand(r7));
  __ bne(&non_outermost_js, Label::kNear);
  __ StoreP(fp, MemOperand(r7));
  __ LoadSmiLiteral(ip, Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME));
  Label cont;
  __ b(&cont, Label::kNear);
  __ bind(&non_outermost_js);
  __ LoadSmiLiteral(ip, Smi::FromInt(StackFrame::INNER_JSENTRY_FRAME));

  __ bind(&cont);
  __ StoreP(ip, MemOperand(sp));  // frame-type

  // Jump to a faked try block that does the invoke, with a faked catch
  // block that sets the pending exception.
  __ b(&invoke, Label::kNear);

  __ bind(&handler_entry);
  handler_offset_ = handler_entry.pos();
  // Caught exception: Store result (exception) in the pending exception
  // field in the JSEnv and return a failure sentinel.  Coming in here the
  // fp will be invalid because the PushTryHandler below sets it to 0 to
  // signal the existence of the JSEntry frame.
  __ mov(ip, Operand(ExternalReference(Isolate::kPendingExceptionAddress,
                                       isolate())));

  __ StoreP(r2, MemOperand(ip));
  __ LoadRoot(r2, Heap::kExceptionRootIndex);
  __ b(&exit, Label::kNear);

  // Invoke: Link this frame into the handler chain.  There's only one
  // handler block in this code object, so its index is 0.
  __ bind(&invoke);
  // Must preserve r0-r4, r5-r7 are available. (@TODO needs update for S390)
  __ PushTryHandler(StackHandler::JS_ENTRY, 0);
  // If an exception not caught by another handler occurs, this handler
  // returns control to the code after the b(&invoke) above, which
  // restores all kCalleeSaved registers (including cp and fp) to their
  // saved values before returning a failure to C.

  // Clear any pending exceptions.
  __ mov(ip, Operand(ExternalReference(Isolate::kPendingExceptionAddress,
                                       isolate())));
  __ mov(r7, Operand(isolate()->factory()->the_hole_value()));
  __ StoreP(r7, MemOperand(ip));

  // Invoke the function by calling through JS entry trampoline builtin.
  // Notice that we cannot store a reference to the trampoline code directly in
  // this stub, because runtime stubs are not traversed when doing GC.

  // Expected registers by Builtins::JSEntryTrampoline
  // r2: code entry
  // r3: function
  // r4: receiver
  // r5: argc
  // r6: argv
  if (is_construct) {
    ExternalReference construct_entry(Builtins::kJSConstructEntryTrampoline,
                                      isolate());
    __ mov(ip, Operand(construct_entry));
  } else {
    ExternalReference entry(Builtins::kJSEntryTrampoline, isolate());
    __ mov(ip, Operand(entry));
  }
  __ LoadP(ip, MemOperand(ip));  // deref address

  // Branch and link to JSEntryTrampoline.
  // the address points to the start of the code object, skip the header
  __ AddP(ip, Operand(Code::kHeaderSize - kHeapObjectTag));
  Label return_addr;
  // __ basr(r14, ip);
  __ larl(r14, &return_addr);
  __ b(ip);
  __ bind(&return_addr);

  // Unlink this frame from the handler chain.
  __ PopTryHandler();

  __ bind(&exit);  // r2 holds result
  // Check if the current stack frame is marked as the outermost JS frame.
  Label non_outermost_js_2;
  __ pop(r7);
  __ CmpSmiLiteral(r7, Smi::FromInt(StackFrame::OUTERMOST_JSENTRY_FRAME), r0);
  __ bne(&non_outermost_js_2, Label::kNear);
  __ mov(r8, Operand::Zero());
  __ mov(r7, Operand(ExternalReference(js_entry_sp)));
  __ StoreP(r8, MemOperand(r7));
  __ bind(&non_outermost_js_2);

  // Restore the top frame descriptors from the stack.
  __ pop(r5);
  __ mov(ip,
         Operand(ExternalReference(Isolate::kCEntryFPAddress, isolate())));
  __ StoreP(r5, MemOperand(ip));

  // Reset the stack to the callee saved registers.
  __ lay(sp, MemOperand(sp, -EntryFrameConstants::kCallerFPOffset));

  // Restore callee-saved registers and return.
#ifdef DEBUG
  if (FLAG_debug_code) {
    Label here;
    __ b(r14, &here /*, SetLK*/);
    __ bind(&here);
  }
#endif

  // Reload callee-saved preserved regs, return address reg (r14) and sp
#if !V8_OS_ZOS
  __ LoadMultipleP(r6, sp, MemOperand(sp, 0));
  __ la(sp, MemOperand(sp, 10 * kPointerSize));
#endif

  // saving floating point registers
#if V8_HOST_ARCH_S390X
  // 64bit ABI requires f8 to f15 be saved
  __ ld(d8, MemOperand(sp));
  __ ld(d9, MemOperand(sp, 1 * kDoubleSize));
  __ ld(d10, MemOperand(sp, 2 * kDoubleSize));
  __ ld(d11, MemOperand(sp, 3 * kDoubleSize));
  __ ld(d12, MemOperand(sp, 4 * kDoubleSize));
  __ ld(d13, MemOperand(sp, 5 * kDoubleSize));
  __ ld(d14, MemOperand(sp, 6 * kDoubleSize));
  __ ld(d15, MemOperand(sp, 7 * kDoubleSize));
  __ la(sp, MemOperand(sp, 8 * kDoubleSize));
#else
  // 31bit ABI requires you to store f4 and f6:
  // http://refspecs.linuxbase.org/ELF/zSeries/lzsabi0_s390.html#AEN417
  __ ld(d4, MemOperand(sp));
  __ ld(d6, MemOperand(sp, kDoubleSize));
  __ la(sp, MemOperand(sp, 2 * kDoubleSize));
#endif

#if V8_OS_ZOS
  __ LoadRR(r3, r2);
  __ LoadMultipleP(r4, sp, MemOperand(sp, 0));
  __ lay(sp, MemOperand(sp, 12 * kPointerSize));
  __ b(r7);
#else
  __ b(r14);
#endif
}


// Uses registers r2 to r6.
// Expected input (depending on whether args are in registers or on the stack):
// * object: r2 or at sp + 1 * kPointerSize.
// * function: r3 or at sp.
//
// An inlined call site may have been generated before calling this stub.
// In this case the offset to the inline site to patch is passed in r7.
// (See LCodeGen::DoInstanceOfKnownGlobal)
void InstanceofStub::Generate(MacroAssembler* masm) {
  // Call site inlining and patching implies arguments in registers.
  DCHECK(HasArgsInRegisters() || !HasCallSiteInlineCheck());

  // Fixed register usage throughout the stub:
  const Register object = r2;  // Object (lhs).
  Register map = r5;  // Map of the object.
  const Register function = r3;  // Function (rhs).
  const Register prototype = r6;  // Prototype of the function.
  const Register inline_site = r8;
  const Register scratch = r4;
  Register scratch3 = no_reg;

  // delta = mov + unaligned LoadP + cmp + bne
#if V8_TARGET_ARCH_S390X
  const int32_t kDeltaToLoadBoolResult = 28;  // IIHF + IILF + LG + CR + BRCL
#else
  const int32_t kDeltaToLoadBoolResult = 18;  // IILF + L + CR + BRCL
#endif

  Label slow, loop, is_instance, is_not_instance, not_js_object;

  if (!HasArgsInRegisters()) {
    __ LoadP(object, MemOperand(sp, 1 * kPointerSize));
    __ LoadP(function, MemOperand(sp, 0));
  }

  // Check that the left hand is a JS object and load map.
  __ JumpIfSmi(object, &not_js_object);
  __ IsObjectJSObjectType(object, map, scratch, &not_js_object);

  // If there is a call site cache don't look in the global cache, but do the
  // real lookup and update the call site cache.
  if (!HasCallSiteInlineCheck() && !ReturnTrueFalseObject()) {
    Label miss;
    __ CompareRoot(function, Heap::kInstanceofCacheFunctionRootIndex);
    __ bne(&miss, Label::kNear);
    __ CompareRoot(map, Heap::kInstanceofCacheMapRootIndex);
    __ bne(&miss, Label::kNear);
    __ LoadRoot(r2, Heap::kInstanceofCacheAnswerRootIndex);
    __ Ret(HasArgsInRegisters() ? 0 : 2);

    __ bind(&miss);
  }

  // Get the prototype of the function.
  __ TryGetFunctionPrototype(function, prototype, scratch, &slow, true);

  // Check that the function prototype is a JS object.
  __ JumpIfSmi(prototype, &slow);
  __ IsObjectJSObjectType(prototype, scratch, scratch, &slow);

  // Update the global instanceof or call site inlined cache with the current
  // map and function. The cached answer will be set when it is known below.
  if (!HasCallSiteInlineCheck()) {
    __ StoreRoot(function, Heap::kInstanceofCacheFunctionRootIndex);
    __ StoreRoot(map, Heap::kInstanceofCacheMapRootIndex);
  } else {
    DCHECK(HasArgsInRegisters());
    // Patch the (relocated) inlined map check.

    // The offset was stored in r7
    //   (See LCodeGen::DoDeferredLInstanceOfKnownGlobal).
    const Register offset = r7;
    __ CleanseP(r14);
    __ LoadRR(inline_site, r14);
    __ SubP(inline_site, inline_site, offset);
    // Get the map location in r7 and patch it.
    __ GetRelocatedValue(inline_site, offset, scratch);
    __ StoreP(map,
         FieldMemOperand(offset, Cell::kValueOffset));
  }

  // Register mapping: r5 is object map and r6 is function prototype.
  // Get prototype of object into r4.
  __ LoadP(scratch, FieldMemOperand(map, Map::kPrototypeOffset));

  // We don't need map any more. Use it as a scratch register.
  scratch3 = map;
  map = no_reg;

  // Loop through the prototype chain looking for the function prototype.
  __ LoadRoot(scratch3, Heap::kNullValueRootIndex);
  __ bind(&loop);
  __ CmpP(scratch, prototype);
  __ beq(&is_instance, Label::kNear);
  __ CmpP(scratch, scratch3);
  __ beq(&is_not_instance, Label::kNear);
  __ LoadP(scratch, FieldMemOperand(scratch, HeapObject::kMapOffset));
  __ LoadP(scratch, FieldMemOperand(scratch, Map::kPrototypeOffset));
  __ b(&loop);
  Factory* factory = isolate()->factory();

  __ bind(&is_instance);
  if (!HasCallSiteInlineCheck()) {
    __ LoadSmiLiteral(r2, Smi::FromInt(0));
    __ StoreRoot(r2, Heap::kInstanceofCacheAnswerRootIndex);
    if (ReturnTrueFalseObject()) {
      __ Move(r2, factory->true_value());
    }
  } else {
    // Patch the call site to return true.
    __ LoadRoot(r2, Heap::kTrueValueRootIndex);
    __ AddP(inline_site, Operand(kDeltaToLoadBoolResult));
    // Get the boolean result location in scratch and patch it.
    __ SetRelocatedValue(inline_site, scratch, r2);

    if (!ReturnTrueFalseObject()) {
      __ LoadSmiLiteral(r2, Smi::FromInt(0));
    }
  }
  __ Ret(HasArgsInRegisters() ? 0 : 2);

  __ bind(&is_not_instance);
  if (!HasCallSiteInlineCheck()) {
    __ LoadSmiLiteral(r2, Smi::FromInt(1));
    __ StoreRoot(r2, Heap::kInstanceofCacheAnswerRootIndex);
    if (ReturnTrueFalseObject()) {
      __ Move(r2, factory->false_value());
    }
  } else {
    // Patch the call site to return false.
    __ LoadRoot(r2, Heap::kFalseValueRootIndex);
    __ AddP(inline_site, Operand(kDeltaToLoadBoolResult));
    // Get the boolean result location in scratch and patch it.
    __ SetRelocatedValue(inline_site, scratch, r2);

    if (!ReturnTrueFalseObject()) {
      __ LoadSmiLiteral(r2, Smi::FromInt(1));
    }
  }
  __ Ret(HasArgsInRegisters() ? 0 : 2);

  Label object_not_null, object_not_null_or_smi;
  __ bind(&not_js_object);
  // Before null, smi and string value checks, check that the rhs is a function
  // as for a non-function rhs an exception needs to be thrown.
  __ JumpIfSmi(function, &slow);
  __ CompareObjectType(function, scratch3, scratch, JS_FUNCTION_TYPE);
  __ bne(&slow);

  // Null is not instance of anything.
  __ CmpP(scratch, Operand(isolate()->factory()->null_value()));
  __ bne(&object_not_null, Label::kNear);
  if (ReturnTrueFalseObject()) {
    __ Move(r2, factory->false_value());
  } else {
    __ LoadSmiLiteral(r2, Smi::FromInt(1));
  }
  __ Ret(HasArgsInRegisters() ? 0 : 2);

  __ bind(&object_not_null);
  // Smi values are not instances of anything.
  __ JumpIfNotSmi(object, &object_not_null_or_smi);
  if (ReturnTrueFalseObject()) {
    __ Move(r2, factory->false_value());
  } else {
    __ LoadSmiLiteral(r2, Smi::FromInt(1));
  }
  __ Ret(HasArgsInRegisters() ? 0 : 2);

  __ bind(&object_not_null_or_smi);
  // String values are not instances of anything.
  __ IsObjectJSStringType(object, scratch, &slow);
  if (ReturnTrueFalseObject()) {
    __ Move(r2, factory->false_value());
  } else {
    __ LoadSmiLiteral(r2, Smi::FromInt(1));
  }
  __ Ret(HasArgsInRegisters() ? 0 : 2);

  // Slow-case.  Tail call builtin.
  __ bind(&slow);
  if (!ReturnTrueFalseObject()) {
    if (HasArgsInRegisters()) {
      __ Push(r2, r3);
    }
  __ InvokeBuiltin(Builtins::INSTANCE_OF, JUMP_FUNCTION);
  } else {
    {
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      __ Push(r2, r3);
      __ InvokeBuiltin(Builtins::INSTANCE_OF, CALL_FUNCTION);
    }
    Label true_value, done;
    __ CmpP(r2, Operand::Zero());
    __ beq(&true_value, Label::kNear);

    __ LoadRoot(r2, Heap::kFalseValueRootIndex);
    __ b(&done, Label::kNear);

    __ bind(&true_value);
    __ LoadRoot(r2, Heap::kTrueValueRootIndex);

    __ bind(&done);
    __ Ret(HasArgsInRegisters() ? 0 : 2);
  }
}


void FunctionPrototypeStub::Generate(MacroAssembler* masm) {
  Label miss;
  Register receiver = LoadIC::ReceiverRegister();


  NamedLoadHandlerCompiler::GenerateLoadFunctionPrototype(masm, receiver, r5,
                                                          r6, &miss);
  __ bind(&miss);
  PropertyAccessCompiler::TailCallBuiltin(
      masm, PropertyAccessCompiler::MissBuiltin(Code::LOAD_IC));
}


Register InstanceofStub::left() { return r2; }


Register InstanceofStub::right() { return r3; }


void ArgumentsAccessStub::GenerateReadElement(MacroAssembler* masm) {
  // The displacement is the offset of the last parameter (if any)
  // relative to the frame pointer.
  const int kDisplacement =
      StandardFrameConstants::kCallerSPOffset - kPointerSize;

  // Check that the key is a smi.
  Label slow;
  __ JumpIfNotSmi(r3, &slow);

  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor;
  __ LoadP(r4, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ LoadP(r5, MemOperand(r4, StandardFrameConstants::kContextOffset));
  STATIC_ASSERT(StackFrame::ARGUMENTS_ADAPTOR < 0x3fffu);
  __ CmpSmiLiteral(r5, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR), r0);
  __ beq(&adaptor);

  // Check index against formal parameters count limit passed in
  // through register r2. Use unsigned comparison to get negative
  // check for free.
  __ CmpLogicalP(r3, r2);
  __ bge(&slow);

  // Read the argument from the stack and return it.
  __ SubP(r5, r2, r3);
  __ SmiToPtrArrayOffset(r5, r5);
  __ lay(r5, MemOperand(r5, fp));
  __ LoadP(r2, MemOperand(r5, kDisplacement));
  __ Ret();

  // Arguments adaptor case: Check index against actual arguments
  // limit found in the arguments adaptor frame. Use unsigned
  // comparison to get negative check for free.
  __ bind(&adaptor);
  __ LoadP(r2,
           MemOperand(r4, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ CmpLogicalP(r3, r2);
  __ bge(&slow);

  // Read the argument from the adaptor frame and return it.
  __ SubP(r5, r2, r3);
  __ SmiToPtrArrayOffset(r5, r5);
  __ AddP(r5, r4);
  __ LoadP(r2, MemOperand(r5, kDisplacement));
  __ Ret();

  // Slow-case: Handle non-smi or out-of-bounds access to arguments
  // by calling the runtime system.
  __ bind(&slow);
  __ push(r3);
  __ TailCallRuntime(Runtime::kGetArgumentsProperty, 1, 1);
}


void ArgumentsAccessStub::GenerateNewSloppySlow(MacroAssembler* masm) {
  // sp[0] : number of parameters
  // sp[1] : receiver displacement
  // sp[2] : function

  // Check if the calling frame is an arguments adaptor frame.
  Label runtime;
  __ LoadP(r5, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ LoadP(r4, MemOperand(r5, StandardFrameConstants::kContextOffset));
  STATIC_ASSERT(StackFrame::ARGUMENTS_ADAPTOR < 0x3fffu);
  __ CmpSmiLiteral(r4, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR), r0);
  __ bne(&runtime);

  // Patch the arguments.length and the parameters pointer in the current frame.
  __ LoadP(r4,
           MemOperand(r5, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ StoreP(r4, MemOperand(sp, 0 * kPointerSize));
  __ SmiToPtrArrayOffset(r4, r4);
  __ AddP(r5, r4);
  __ AddP(r5, Operand(StandardFrameConstants::kCallerSPOffset));
  __ StoreP(r5, MemOperand(sp, 1 * kPointerSize));

  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kNewSloppyArguments, 3, 1);
}


void ArgumentsAccessStub::GenerateNewSloppyFast(MacroAssembler* masm) {
  // Stack layout:
  //  sp[0] : number of parameters (tagged)
  //  sp[1] : address of receiver argument
  //  sp[2] : function
  // Registers used over whole function:
  //  r8 : allocated object (tagged)
  //  r13 : mapped parameter count (tagged)

  __ LoadP(r3, MemOperand(sp, 0 * kPointerSize));
  // r3 = parameter count (tagged)

  // Check if the calling frame is an arguments adaptor frame.
  Label runtime;
  Label adaptor_frame, try_allocate;
  __ LoadP(r5, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ LoadP(r4, MemOperand(r5, StandardFrameConstants::kContextOffset));
  STATIC_ASSERT(StackFrame::ARGUMENTS_ADAPTOR < 0x3fffu);
  __ CmpSmiLiteral(r4, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR), r0);
  __ beq(&adaptor_frame, Label::kNear);

  // No adaptor, parameter count = argument count.
  __ LoadRR(r4, r3);
  __ b(&try_allocate);

  // We have an adaptor frame. Patch the parameters pointer.
  __ bind(&adaptor_frame);
  __ LoadP(r4,
           MemOperand(r5, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ SmiToPtrArrayOffset(r6, r4);
  __ AddP(r5, r6);
  __ AddP(r5, Operand(StandardFrameConstants::kCallerSPOffset));
  __ StoreP(r5, MemOperand(sp, 1 * kPointerSize));

  // r3 = parameter count (tagged)
  // r4 = argument count (tagged)
  // Compute the mapped parameter count = min(r3, r4) in r3.
  Label skip;
  __ CmpP(r3, r4);
  __ blt(&skip, Label::kNear);
  __ LoadRR(r3, r4);
  __ bind(&skip);

  __ bind(&try_allocate);

  // Compute the sizes of backing store, parameter map, and arguments object.
  // 1. Parameter map, has 2 extra words containing context and backing store.
  const int kParameterMapHeaderSize =
      FixedArray::kHeaderSize + 2 * kPointerSize;
  // If there are no mapped parameters, we do not need the parameter_map.
  Label skip2, skip3;
  __ CmpSmiLiteral(r3, Smi::FromInt(0), r0);
  __ bne(&skip2, Label::kNear);
  __ LoadImmP(r1, Operand::Zero());
  __ b(&skip3, Label::kNear);
  __ bind(&skip2);
  __ SmiToPtrArrayOffset(r1, r3);
  __ AddP(r1, Operand(kParameterMapHeaderSize));
  __ bind(&skip3);

  // 2. Backing store.
  __ SmiToPtrArrayOffset(r6, r4);
  __ AddP(r1, r6);
  __ AddP(r1, Operand(FixedArray::kHeaderSize));

  // 3. Arguments object.
  __ AddP(r1, Operand(Heap::kSloppyArgumentsObjectSize));

  // Do the allocation of all three objects in one go.
  __ Allocate(r1, r2, r5, r6, &runtime, TAG_OBJECT);

  // r2 = address of new object(s) (tagged)
  // r4 = argument count (smi-tagged)
  // Get the arguments boilerplate from the current native context into r3.
  const int kNormalOffset =
      Context::SlotOffset(Context::SLOPPY_ARGUMENTS_MAP_INDEX);
  const int kAliasedOffset =
      Context::SlotOffset(Context::ALIASED_ARGUMENTS_MAP_INDEX);

  __ LoadP(r6, MemOperand(cp,
             Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  __ LoadP(r6, FieldMemOperand(r6, GlobalObject::kNativeContextOffset));
  Label skip4, skip5;
  __ CmpP(r3, Operand::Zero());
  __ bne(&skip4, Label::kNear);
  __ LoadP(r6, MemOperand(r6, kNormalOffset));
  __ b(&skip5, Label::kNear);
  __ bind(&skip4);
  __ LoadP(r6, MemOperand(r6, kAliasedOffset));
  __ bind(&skip5);

  // r2 = address of new object (tagged)
  // r3 = mapped parameter count (tagged)
  // r4 = argument count (smi-tagged)
  // r6 = address of arguments map (tagged)
  __ StoreP(r6, FieldMemOperand(r2, JSObject::kMapOffset));
  __ LoadRoot(r5, Heap::kEmptyFixedArrayRootIndex);
  __ StoreP(r5, FieldMemOperand(r2, JSObject::kPropertiesOffset));
  __ StoreP(r5, FieldMemOperand(r2, JSObject::kElementsOffset));

  // Set up the callee in-object property.
  STATIC_ASSERT(Heap::kArgumentsCalleeIndex == 1);
  __ LoadP(r5, MemOperand(sp, 2 * kPointerSize));
  __ AssertNotSmi(r5);
  const int kCalleeOffset = JSObject::kHeaderSize +
      Heap::kArgumentsCalleeIndex * kPointerSize;
  __ StoreP(r5, FieldMemOperand(r2, kCalleeOffset));

  // Use the length (smi tagged) and set that as an in-object property too.
  __ AssertSmi(r4);
  STATIC_ASSERT(Heap::kArgumentsLengthIndex == 0);
  const int kLengthOffset = JSObject::kHeaderSize +
      Heap::kArgumentsLengthIndex * kPointerSize;
  __ StoreP(r4, FieldMemOperand(r2, kLengthOffset));

  // Set up the elements pointer in the allocated arguments object.
  // If we allocated a parameter map, r6 will point there, otherwise
  // it will point to the backing store.
  __ AddP(r6, r2, Operand(Heap::kSloppyArgumentsObjectSize));
  __ StoreP(r6, FieldMemOperand(r2, JSObject::kElementsOffset));

  // r2 = address of new object (tagged)
  // r3 = mapped parameter count (tagged)
  // r4 = argument count (tagged)
  // r6 = address of parameter map or backing store (tagged)
  // Initialize parameter map. If there are no mapped arguments, we're done.
  Label skip_parameter_map, skip6;
  __ CmpSmiLiteral(r3, Smi::FromInt(0), r0);
  __ bne(&skip6, Label::kNear);
  // Move backing store address to r5, because it is
  // expected there when filling in the unmapped arguments.
  __ LoadRR(r5, r6);
  __ b(&skip_parameter_map);
  __ bind(&skip6);

  __ LoadRoot(r8, Heap::kSloppyArgumentsElementsMapRootIndex);
  __ StoreP(r8, FieldMemOperand(r6, FixedArray::kMapOffset));
  __ AddSmiLiteral(r8, r3, Smi::FromInt(2), r0);
  __ StoreP(r8, FieldMemOperand(r6, FixedArray::kLengthOffset));
  __ StoreP(cp, FieldMemOperand(r6,
                                 FixedArray::kHeaderSize + 0 * kPointerSize));
  __ SmiToPtrArrayOffset(r8, r3);
  __ AddP(r8, r6);
  __ AddP(r8, Operand(kParameterMapHeaderSize));
  __ StoreP(r8, FieldMemOperand(r6,
                                FixedArray::kHeaderSize + 1 * kPointerSize));

  // Copy the parameter slots and the holes in the arguments.
  // We need to fill in mapped_parameter_count slots. They index the context,
  // where parameters are stored in reverse order, at
  //   MIN_CONTEXT_SLOTS .. MIN_CONTEXT_SLOTS+parameter_count-1
  // The mapped parameter thus need to get indices
  //   MIN_CONTEXT_SLOTS+parameter_count-1 ..
  //       MIN_CONTEXT_SLOTS+parameter_count-mapped_parameter_count
  // We loop from right to left.
  Label parameters_loop, parameters_test;
  __ LoadRR(r8, r3);
  __ LoadP(r1, MemOperand(sp, 0 * kPointerSize));
  __ AddSmiLiteral(r1, r1,
                   Smi::FromInt(Context::MIN_CONTEXT_SLOTS), r0);
  __ SubP(r1, r1, r3);
  __ LoadRoot(r9, Heap::kTheHoleValueRootIndex);
  __ SmiToPtrArrayOffset(r5, r8);
  __ AddP(r5, r6);
  __ AddP(r5, Operand(kParameterMapHeaderSize));

  // r8 = loop variable (tagged)
  // r3 = mapping index (tagged)
  // r5 = address of backing store (tagged)
  // r6 = address of parameter map (tagged)
  // r7 = temporary scratch (a.o., for address calculation)
  // r9 = the hole value
  __ b(&parameters_test, Label::kNear);

  __ bind(&parameters_loop);
  __ SubSmiLiteral(r8, r8, Smi::FromInt(1), r0);
  __ SmiToPtrArrayOffset(r7, r8);
  __ AddP(r7, Operand(kParameterMapHeaderSize - kHeapObjectTag));
  __ StoreP(r1, MemOperand(r7, r6));
  __ SubP(r7, Operand(kParameterMapHeaderSize - FixedArray::kHeaderSize));
  __ StoreP(r9, MemOperand(r7, r5));
  __ AddSmiLiteral(r1, r1, Smi::FromInt(1), r0);
  __ bind(&parameters_test);
  __ CmpSmiLiteral(r8, Smi::FromInt(0), r0);
  __ bne(&parameters_loop);

  __ bind(&skip_parameter_map);
  // r4 = argument count (tagged)
  // r5 = address of backing store (tagged)
  // r7 = scratch
  // Copy arguments header and remaining slots (if there are any).
  __ LoadRoot(r7, Heap::kFixedArrayMapRootIndex);
  __ StoreP(r7, FieldMemOperand(r5, FixedArray::kMapOffset));
  __ StoreP(r4, FieldMemOperand(r5, FixedArray::kLengthOffset));

  Label arguments_loop, arguments_test;
  __ LoadRR(r1, r3);
  __ LoadP(r6, MemOperand(sp, 1 * kPointerSize));
  __ SmiToPtrArrayOffset(r7, r1);
  __ SubP(r6, r6, r7);
  __ b(&arguments_test, Label::kNear);

  __ bind(&arguments_loop);
  __ SubP(r6, Operand(kPointerSize));
  __ LoadP(r8, MemOperand(r6, 0));
  __ SmiToPtrArrayOffset(r7, r1);
  __ AddP(r7, r5);
  __ StoreP(r8, FieldMemOperand(r7, FixedArray::kHeaderSize));
  __ AddSmiLiteral(r1, r1, Smi::FromInt(1), r0);

  __ bind(&arguments_test);
  __ CmpP(r1, r4);
  __ blt(&arguments_loop);

  // Return and remove the on-stack parameters.
  __ la(sp, MemOperand(sp, (3 * kPointerSize)));
  __ Ret();

  // Do the runtime call to allocate the arguments object.
  // r4 = argument count (tagged)
  __ bind(&runtime);
  __ StoreP(r4, MemOperand(sp, 0 * kPointerSize));  // Patch argument count.
  __ TailCallRuntime(Runtime::kNewSloppyArguments, 3, 1);
}


void ArgumentsAccessStub::GenerateNewStrict(MacroAssembler* masm) {
  // sp[0] : number of parameters
  // sp[4] : receiver displacement
  // sp[8] : function
  // Check if the calling frame is an arguments adaptor frame.
  Label adaptor_frame, try_allocate, runtime;
  __ LoadP(r4, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ LoadP(r5, MemOperand(r4, StandardFrameConstants::kContextOffset));
  STATIC_ASSERT(StackFrame::ARGUMENTS_ADAPTOR < 0x3fffu);
  __ CmpSmiLiteral(r5, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR), r0);
  __ beq(&adaptor_frame, Label::kNear);

  // Get the length from the frame.
  __ LoadP(r3, MemOperand(sp, 0));
  __ b(&try_allocate, Label::kNear);

  // Patch the arguments.length and the parameters pointer.
  __ bind(&adaptor_frame);
  __ LoadP(r3,
           MemOperand(r4, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ StoreP(r3, MemOperand(sp, 0));
  __ SmiToPtrArrayOffset(r5, r3);
  __ AddP(r5, r4);
  __ AddP(r5, Operand(StandardFrameConstants::kCallerSPOffset));
  __ StoreP(r5, MemOperand(sp, 1 * kPointerSize));

  // Try the new space allocation. Start out with computing the size
  // of the arguments object and the elements array in words.
  Label add_arguments_object;
  __ bind(&try_allocate);
  __ CmpP(r3, Operand::Zero());
  __ beq(&add_arguments_object, Label::kNear);
  __ SmiUntag(r3);
  __ AddP(r3, Operand(FixedArray::kHeaderSize / kPointerSize));
  __ bind(&add_arguments_object);
  __ AddP(r3, Operand(Heap::kStrictArgumentsObjectSize / kPointerSize));

  // Do the allocation of both objects in one go.
  __ Allocate(r3, r2, r4, r5, &runtime,
              static_cast<AllocationFlags>(TAG_OBJECT | SIZE_IN_WORDS));

  // Get the arguments boilerplate from the current native context.
  __ LoadP(r6,
           MemOperand(cp, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  __ LoadP(r6, FieldMemOperand(r6, GlobalObject::kNativeContextOffset));
  __ LoadP(r6, MemOperand(r6, Context::SlotOffset(
         Context::STRICT_ARGUMENTS_MAP_INDEX)));

  __ StoreP(r6, FieldMemOperand(r2, JSObject::kMapOffset));
  __ LoadRoot(r5, Heap::kEmptyFixedArrayRootIndex);
  __ StoreP(r5, FieldMemOperand(r2, JSObject::kPropertiesOffset));
  __ StoreP(r5, FieldMemOperand(r2, JSObject::kElementsOffset));

  // Get the length (smi tagged) and set that as an in-object property too.
  STATIC_ASSERT(Heap::kArgumentsLengthIndex == 0);
  __ LoadP(r3, MemOperand(sp, 0 * kPointerSize));
  __ AssertSmi(r3);
  __ StoreP(r3, FieldMemOperand(r2, JSObject::kHeaderSize +
                                Heap::kArgumentsLengthIndex * kPointerSize));

  // If there are no actual arguments, we're done.
  Label done;
  __ CmpP(r3, Operand::Zero());
  __ beq(&done);

  // Get the parameters pointer from the stack.
  __ LoadP(r4, MemOperand(sp, 1 * kPointerSize));

  // Set up the elements pointer in the allocated arguments object and
  // initialize the header in the elements fixed array.
  __ AddP(r6, r2, Operand(Heap::kStrictArgumentsObjectSize));
  __ StoreP(r6, FieldMemOperand(r2, JSObject::kElementsOffset));
  __ LoadRoot(r5, Heap::kFixedArrayMapRootIndex);
  __ StoreP(r5, FieldMemOperand(r6, FixedArray::kMapOffset));
  __ StoreP(r3, FieldMemOperand(r6, FixedArray::kLengthOffset));
  // Untag the length for the loop.
  __ SmiUntag(r3);

  // Copy the fixed array slots.
  Label loop;
  // Set up r6 to point to the first array slot.
  __ AddP(r6, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ bind(&loop);
  // Pre-decrement r4 with kPointerSize on each iteration.
  // Pre-decrement in order to skip receiver.
  __ LoadP(r5, MemOperand(r4, -kPointerSize));
  __ lay(r4, MemOperand(r4, -kPointerSize));
  // Post-increment r6 with kPointerSize on each iteration.
  __ StoreP(r5, MemOperand(r6));
  __ AddP(r6, Operand(kPointerSize));
  __ SubP(r3, Operand(1));
  __ CmpP(r3, Operand::Zero());
  __ bne(&loop);

  // Return and remove the on-stack parameters.
  __ bind(&done);
  __ la(sp, MemOperand(sp, (3 * kPointerSize)));
  __ Ret();

  // Do the runtime call to allocate the arguments object.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kNewStrictArguments, 3, 1);
}


void RegExpExecStub::Generate(MacroAssembler* masm) {
  // Just jump directly to runtime if native RegExp is not selected at compile
  // time or if regexp entry in generated code is turned off runtime switch or
  // at compilation.
#ifdef V8_INTERPRETED_REGEXP
  __ TailCallRuntime(Runtime::kRegExpExecRT, 4, 1);
#else  // V8_INTERPRETED_REGEXP

  // Stack frame on entry.
  //  sp[0]: last_match_info (expected JSArray)
  //  sp[4]: previous index
  //  sp[8]: subject string
  //  sp[12]: JSRegExp object

  const int kLastMatchInfoOffset = 0 * kPointerSize;
  const int kPreviousIndexOffset = 1 * kPointerSize;
  const int kSubjectOffset = 2 * kPointerSize;
  const int kJSRegExpOffset = 3 * kPointerSize;

  Label runtime, br_over, encoding_type_UC16;

  // Allocation of registers for this function. These are in callee save
  // registers and will be preserved by the call to the native RegExp code, as
  // this code is called using the normal C calling convention. When calling
  // directly from generated code the native RegExp code will not do a GC and
  // therefore the content of these registers are safe to use after the call.
  Register subject = r6;
  Register regexp_data = r7;
  Register last_match_info_elements = r8;
  Register code = r9;

  __ CleanseP(r14);

  __ lay(sp, MemOperand(sp, -13 * kPointerSize));
  __ StoreMultipleP(r3, sp, MemOperand(sp, 0));
  __ la(fp, MemOperand(sp, 13 * kPointerSize));

  // Ensure register assigments are consistent with callee save masks
  DCHECK(subject.bit() & kCalleeSaved);
  DCHECK(regexp_data.bit() & kCalleeSaved);
  DCHECK(last_match_info_elements.bit() & kCalleeSaved);
  DCHECK(code.bit() & kCalleeSaved);

  // Ensure that a RegExp stack is allocated.
  ExternalReference address_of_regexp_stack_memory_address =
      ExternalReference::address_of_regexp_stack_memory_address(isolate());
  ExternalReference address_of_regexp_stack_memory_size =
      ExternalReference::address_of_regexp_stack_memory_size(isolate());
  __ mov(r2, Operand(address_of_regexp_stack_memory_size));
  __ LoadAndTestP(r2, MemOperand(r2));
  __ beq(&runtime);

  // Check that the first argument is a JSRegExp object.
  __ LoadP(r2, MemOperand(fp, kJSRegExpOffset));
  __ JumpIfSmi(r2, &runtime);
  __ CompareObjectType(r2, r3, r3, JS_REGEXP_TYPE);
  __ bne(&runtime);

  // Check that the RegExp has been compiled (data contains a fixed array).
  __ LoadP(regexp_data, FieldMemOperand(r2, JSRegExp::kDataOffset));
  if (FLAG_debug_code) {
    __ TestIfSmi(regexp_data);
    __ Check(ne, kUnexpectedTypeForRegExpDataFixedArrayExpected, cr0);
    __ CompareObjectType(regexp_data, r2, r2, FIXED_ARRAY_TYPE);
    __ Check(eq, kUnexpectedTypeForRegExpDataFixedArrayExpected);
  }

  // regexp_data: RegExp data (FixedArray)
  // Check the type of the RegExp. Only continue if type is JSRegExp::IRREGEXP.
  __ LoadP(r2, FieldMemOperand(regexp_data, JSRegExp::kDataTagOffset));
  // DCHECK(Smi::FromInt(JSRegExp::IRREGEXP) < (char *)0xffffu);
  __ CmpSmiLiteral(r2, Smi::FromInt(JSRegExp::IRREGEXP), r0);
  __ bne(&runtime);

  // regexp_data: RegExp data (FixedArray)
  // Check that the number of captures fit in the static offsets vector buffer.
  __ LoadP(r4,
         FieldMemOperand(regexp_data, JSRegExp::kIrregexpCaptureCountOffset));
  // Check (number_of_captures + 1) * 2 <= offsets vector size
  // Or          number_of_captures * 2 <= offsets vector size - 2
  // SmiToShortArrayOffset accomplishes the multiplication by 2 and
  // SmiUntag (which is a nop for 32-bit).
  __ SmiToShortArrayOffset(r4, r4);
  STATIC_ASSERT(Isolate::kJSRegexpStaticOffsetsVectorSize >= 2);
  __ CmpLogicalP(r4, Operand(Isolate::kJSRegexpStaticOffsetsVectorSize - 2));
  __ bgt(&runtime);

  // Reset offset for possibly sliced string.
  __ LoadImmP(r13, Operand::Zero());
  __ LoadP(subject, MemOperand(fp, kSubjectOffset));
  __ JumpIfSmi(subject, &runtime);
  __ LoadRR(r5, subject);  // Make a copy of the original subject string.
  __ LoadP(r2, FieldMemOperand(subject, HeapObject::kMapOffset));
  __ LoadlB(r2, FieldMemOperand(r2, Map::kInstanceTypeOffset));
  // subject: subject string
  // r5: subject string
  // r2: subject string instance type
  // regexp_data: RegExp data (FixedArray)
  // Handle subject string according to its encoding and representation:
  // (1) Sequential string?  If yes, go to (5).
  // (2) Anything but sequential or cons?  If yes, go to (6).
  // (3) Cons string.  If the string is flat, replace subject with first string.
  //     Otherwise bailout.
  // (4) Is subject external?  If yes, go to (7).
  // (5) Sequential string.  Load regexp code according to encoding.
  // (E) Carry on.
  /// [...]

  // Deferred code at the end of the stub:
  // (6) Not a long external string?  If yes, go to (8).
  // (7) External string.  Make it, offset-wise, look like a sequential string.
  //     Go to (5).
  // (8) Short external string or not a string?  If yes, bail out to runtime.
  // (9) Sliced string.  Replace subject with parent.  Go to (4).

  Label seq_string /* 5 */, external_string /* 7 */,
        check_underlying /* 4 */, not_seq_nor_cons /* 6 */,
        not_long_external /* 8 */;

  // (1) Sequential string?  If yes, go to (5).
  STATIC_ASSERT((kIsNotStringMask |
                  kStringRepresentationMask |
                  kShortExternalStringMask) == 0x93);
  __ mov(r3, Operand(kIsNotStringMask |
                          kStringRepresentationMask |
                          kShortExternalStringMask));
  __ AndP(r3, r2);
  STATIC_ASSERT((kStringTag | kSeqStringTag) == 0);
  __ beq(&seq_string/*, cr0*/);  // Go to (5).

  // (2) Anything but sequential or cons?  If yes, go to (6).
  STATIC_ASSERT(kConsStringTag < kExternalStringTag);
  STATIC_ASSERT(kSlicedStringTag > kExternalStringTag);
  STATIC_ASSERT(kIsNotStringMask > kExternalStringTag);
  STATIC_ASSERT(kShortExternalStringTag > kExternalStringTag);
  STATIC_ASSERT(kExternalStringTag < 0xffffu);
  __ CmpP(r3, Operand(kExternalStringTag));
  __ bge(&not_seq_nor_cons);  // Go to (6).

  // (3) Cons string.  Check that it's flat.
  // Replace subject with first string and reload instance type.
  __ LoadP(r2, FieldMemOperand(subject, ConsString::kSecondOffset));
  __ CompareRoot(r2, Heap::kempty_stringRootIndex);
  __ bne(&runtime);
  __ LoadP(subject, FieldMemOperand(subject, ConsString::kFirstOffset));

  // (4) Is subject external?  If yes, go to (7).
  __ bind(&check_underlying);
  __ LoadP(r2, FieldMemOperand(subject, HeapObject::kMapOffset));
  __ LoadlB(r2, FieldMemOperand(r2, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kSeqStringTag == 0);
  STATIC_ASSERT(kStringRepresentationMask == 3);
  __ tmll(r2, Operand(kStringRepresentationMask));
  // The underlying external string is never a short external string.
  STATIC_ASSERT(ExternalString::kMaxShortLength < ConsString::kMinLength);
  STATIC_ASSERT(ExternalString::kMaxShortLength < SlicedString::kMinLength);
  __ bne(&external_string/*, cr0*/);  // Go to (7).

  // (5) Sequential string.  Load regexp code according to encoding.
  __ bind(&seq_string);
  // subject: sequential subject string (or look-alike, external string)
  // r5: original subject string
  // Load previous index and check range before r5 is overwritten.  We have to
  // use r5 instead of subject here because subject might have been only made
  // to look like a sequential string when it actually is an external string.
  __ LoadP(r3, MemOperand(fp, kPreviousIndexOffset));
  __ JumpIfNotSmi(r3, &runtime);
  __ LoadP(r5, FieldMemOperand(r5, String::kLengthOffset));
  __ CmpLogicalP(r5, r3);
  __ ble(&runtime);
  __ SmiUntag(r3);

  STATIC_ASSERT(4 == kOneByteStringTag);
  STATIC_ASSERT(kTwoByteStringTag == 0);
  STATIC_ASSERT(kStringEncodingMask == 4);
  __ ExtractBitMask(r5, r2, kStringEncodingMask, SetRC);
  __ beq(&encoding_type_UC16 /*, cr0*/, Label::kNear);
  __ LoadP(code, FieldMemOperand(regexp_data, JSRegExp::kDataAsciiCodeOffset));
  __ b(&br_over, Label::kNear);
  __ bind(&encoding_type_UC16);
  __ LoadP(code, FieldMemOperand(regexp_data, JSRegExp::kDataUC16CodeOffset));
  __ bind(&br_over);

  // (E) Carry on.  String handling is done.
  // code: irregexp code
  // Check that the irregexp code has been generated for the actual string
  // encoding. If it has, the field contains a code object otherwise it contains
  // a smi (code flushing support).
  __ JumpIfSmi(code, &runtime);

  // r3: previous index
  // r5: encoding of subject string (1 if ASCII, 0 if two_byte);
  // code: Address of generated regexp code
  // subject: Subject string
  // regexp_data: RegExp data (FixedArray)
  // All checks done. Now push arguments for native regexp code.
  __ IncrementCounter(isolate()->counters()->regexp_entry_native(), 1, r2, r4);

  // Isolates: note we add an additional parameter here (isolate pointer).
  const int kRegExpExecuteArguments = 10;
  const int kParameterRegisters = 5;
  __ EnterExitFrame(false, kRegExpExecuteArguments - kParameterRegisters);

  // Stack pointer now points to cell where return address is to be written.
  // Arguments are before that on the stack or in registers.

  // Argument 10 (in stack parameter area): Pass current isolate address.
  __ mov(r2, Operand(ExternalReference::isolate_address(isolate())));
  __ StoreP(r2, MemOperand(sp,
       kStackFrameExtraParamSlot * kPointerSize + 4 * kPointerSize));

  // Argument 9 is a dummy that reserves the space used for
  // the return address added by the ExitFrame in native calls.
  __ mov(r2, Operand::Zero());
  __ StoreP(r2, MemOperand(sp,
        kStackFrameExtraParamSlot * kPointerSize + 3 * kPointerSize));

  // Argument 8: Indicate that this is a direct call from JavaScript.
  __ mov(r2, Operand(1));
  __ StoreP(r2, MemOperand(sp,
        kStackFrameExtraParamSlot * kPointerSize + 2 * kPointerSize));

  // Argument 7: Start (high end) of backtracking stack memory area.
  __ mov(r2, Operand(address_of_regexp_stack_memory_address));
  __ LoadP(r2, MemOperand(r2, 0));
  __ mov(r1, Operand(address_of_regexp_stack_memory_size));
  __ LoadP(r1, MemOperand(r1, 0));
  __ AddP(r2, r1);
  __ StoreP(r2, MemOperand(sp,
        kStackFrameExtraParamSlot * kPointerSize + 1 * kPointerSize));

  // Argument 6: Set the number of capture registers to zero to force
  // global egexps to behave as non-global.  This does not affect non-global
  // regexps.
  __ mov(r2, Operand::Zero());
  __ StoreP(r2, MemOperand(sp,
        kStackFrameExtraParamSlot * kPointerSize + 0 * kPointerSize));

  // Argument 1 (r2): Subject string.
  // Load the length from the original subject string from the previous stack
  // frame. Therefore we have to use fp, which points exactly to 15 pointer
  // sizes below the previous sp. (Because creating a new stack frame pushes
  // the previous fp onto the stack and moves up sp by 2 * kPointerSize and
  // 13 registers saved on the stack previously)
  __ LoadP(r2, MemOperand(fp, kSubjectOffset + 15 * kPointerSize));

  // Argument 2 (r3): Previous index.
  // Already there
  __ AddP(r1, subject, Operand(SeqString::kHeaderSize - kHeapObjectTag));

  // Argument 5 (r6): static offsets vector buffer.
  __ mov(r6,
         Operand(ExternalReference::address_of_static_offsets_vector(
         isolate())));

  // For arguments 4 (r5) and 3 (r4) get string length, calculate start of
  // string data and calculate the shift of the index (0 for ASCII and 1 for
  // two byte).
  __ XorP(r5, Operand(1));
  // If slice offset is not 0, load the length from the original sliced string.
  //
  // Argument 3, r4: Start of string data
  // Prepare start and end index of the input.
  __ ShiftLeftP(r13, r13, r5);
  __ AddP(r13, r1, r13);
  __ ShiftLeftP(r4, r3, r5);
  __ AddP(r4, r13, r4);

  // Argument 4, r5: End of string data
  __ LoadP(r1, FieldMemOperand(r2, String::kLengthOffset));
  __ SmiUntag(r1);
  __ ShiftLeftP(r0, r1, r5);
  __ AddP(r5, r13, r0);


  // Locate the code entry and call it.
  __ AddP(code, Operand(Code::kHeaderSize - kHeapObjectTag));


#if ABI_USES_FUNCTION_DESCRIPTORS && defined(USE_SIMULATOR)
  // Even Simulated AIX/S390X Linux uses a function descriptor for the
  // RegExp routine.  Extract the instruction address here since
  // DirectCEntryStub::GenerateCall will not do it for calls out to
  // what it thinks is C code compiled for the simulator/host
  // platform.
  __ LoadP(code, MemOperand(code, 0));  // Instruction address
#endif

  DirectCEntryStub stub(isolate());
  stub.GenerateCall(masm, code);

  __ LeaveExitFrame(false, no_reg, true);

  __ la(fp, MemOperand(sp, 13 * kPointerSize));

  // r2: result (int32)
  // subject: subject string -- needed to reload
  __ LoadP(subject, MemOperand(fp, kSubjectOffset));

  // regexp_data: RegExp data (callee saved)
  // last_match_info_elements: Last match info elements (callee saved)
  // Check the result.
  Label success;
  __ Cmp32(r2, Operand(1));
  // We expect exactly one result since we force the called regexp to behave
  // as non-global.
  __ beq(&success);
  Label failure;
  __ Cmp32(r2, Operand(NativeRegExpMacroAssembler::FAILURE));
  __ beq(&failure);
  __ Cmp32(r2, Operand(NativeRegExpMacroAssembler::EXCEPTION));
  // If not exception it can only be retry. Handle that in the runtime system.
  __ bne(&runtime);
  // Result must now be exception. If there is no pending exception already a
  // stack overflow (on the backtrack stack) was detected in RegExp code but
  // haven't created the exception yet. Handle that in the runtime system.
  // TODO(592): Rerunning the RegExp to get the stack overflow exception.
  __ mov(r3, Operand(isolate()->factory()->the_hole_value()));
  __ mov(r4, Operand(ExternalReference(Isolate::kPendingExceptionAddress,
                                       isolate())));
  __ LoadP(r2, MemOperand(r4, 0));
  __ CmpP(r2, r3);
  __ beq(&runtime);

  __ StoreP(r3, MemOperand(r4, 0));  // Clear pending exception.

  // Check if the exception is a termination. If so, throw as uncatchable.
  __ CompareRoot(r2, Heap::kTerminationExceptionRootIndex);

  Label termination_exception;
  __ beq(&termination_exception);

  __ Throw(r2);

  __ bind(&termination_exception);
  __ ThrowUncatchable(r2);

  __ bind(&failure);
  // For failure and exception return null.
  __ mov(r2, Operand(isolate()->factory()->null_value()));
  __ LoadMultipleP(r3, sp, MemOperand(sp, 0));
  __ la(sp, MemOperand(sp, 13 * kPointerSize));
  __ la(sp, MemOperand(sp, (4 * kPointerSize)));
  __ Ret();

  // Process the result from the native regexp code.
  __ bind(&success);
  __ LoadP(r3,
           FieldMemOperand(regexp_data, JSRegExp::kIrregexpCaptureCountOffset));
  // Calculate number of capture registers (number_of_captures + 1) * 2.
  // SmiToShortArrayOffset accomplishes the multiplication by 2 and
  // SmiUntag (which is a nop for 32-bit).
  __ SmiToShortArrayOffset(r3, r3);
  __ AddP(r3, Operand(2));

  __ LoadP(r2, MemOperand(fp, kLastMatchInfoOffset));
  __ JumpIfSmi(r2, &runtime);
  __ CompareObjectType(r2, r4, r4, JS_ARRAY_TYPE);
  __ bne(&runtime);
  // Check that the JSArray is in fast case.
  __ LoadP(last_match_info_elements,
           FieldMemOperand(r2, JSArray::kElementsOffset));
  __ LoadP(r2,
           FieldMemOperand(last_match_info_elements, HeapObject::kMapOffset));
  __ CompareRoot(r2, Heap::kFixedArrayMapRootIndex);
  __ bne(&runtime);
  // Check that the last match info has space for the capture registers and the
  // additional information.
  __ LoadP(r2,
         FieldMemOperand(last_match_info_elements, FixedArray::kLengthOffset));
  __ AddP(r4, r3, Operand(RegExpImpl::kLastMatchOverhead));
  __ SmiUntag(r0, r2);
  __ CmpP(r4, r0);
  __ bgt(&runtime);

  // r3: number of capture registers
  // subject: subject string
  // Store the capture count.
  __ SmiTag(r4, r3);
  __ StoreP(r4, FieldMemOperand(last_match_info_elements,
                                RegExpImpl::kLastCaptureCountOffset));
  // Store last subject and last input.
  __ StoreP(subject,
            FieldMemOperand(last_match_info_elements,
                            RegExpImpl::kLastSubjectOffset));
  __ LoadRR(r4, subject);
  __ RecordWriteField(last_match_info_elements,
                      RegExpImpl::kLastSubjectOffset,
                      subject,
                      r9,
                      kLRHasNotBeenSaved,
                      kDontSaveFPRegs);
  __ LoadRR(subject, r4);
  __ StoreP(subject,
            FieldMemOperand(last_match_info_elements,
                            RegExpImpl::kLastInputOffset));
  __ RecordWriteField(last_match_info_elements,
                      RegExpImpl::kLastInputOffset,
                      subject,
                      r9,
                      kLRHasNotBeenSaved,
                      kDontSaveFPRegs);

  // Get the static offsets vector filled by the native regexp code.
  ExternalReference address_of_static_offsets_vector =
      ExternalReference::address_of_static_offsets_vector(isolate());
  __ mov(r4, Operand(address_of_static_offsets_vector));

  // r3: number of capture registers
  // r4: offsets vector
  Label next_capture;
  // Capture register counter starts from number of capture registers and
  // counts down until wraping after zero.
  __ AddP(r2, last_match_info_elements,
          Operand(RegExpImpl::kFirstCaptureOffset - kHeapObjectTag -
                  kPointerSize));
  __ AddP(r4, Operand(-kIntSize));  // bias down for lwzu
  __ bind(&next_capture);
  // Read the value from the static offsets vector buffer.
  __ ly(r5, MemOperand(r4, kIntSize));
  __ lay(r4, MemOperand(r4, kIntSize));
  // Store the smi value in the last match info.
  __ SmiTag(r5);
  __ StoreP(r5, MemOperand(r2, kPointerSize));
  __ lay(r2, MemOperand(r2, kPointerSize));
  __ BranchOnCount(r3, &next_capture);

  // Return last match info.
  __ LoadP(r2, MemOperand(fp, kLastMatchInfoOffset));
  __ LoadMultipleP(r3, sp, MemOperand(sp, 0));
  __ la(sp, MemOperand(sp, 13 * kPointerSize));
  __ la(sp, MemOperand(sp, (4 * kPointerSize)));
  __ Ret();

  // Do the runtime call to execute the regexp.
  __ bind(&runtime);
  __ LoadMultipleP(r3, sp, MemOperand(sp, 0));
  __ la(sp, MemOperand(sp, 13 * kPointerSize));
  __ TailCallRuntime(Runtime::kRegExpExecRT, 4, 1);

  // Deferred code for string handling.
  // (6) Not a long external string?  If yes, go to (8).
  __ bind(&not_seq_nor_cons);
  // Compare flags are still set.
  __ bgt(&not_long_external);  // Go to (8).

  // (7) External string.  Make it, offset-wise, look like a sequential string.
  __ bind(&external_string);
  __ LoadP(r2, FieldMemOperand(subject, HeapObject::kMapOffset));
  __ LoadlB(r2, FieldMemOperand(r2, Map::kInstanceTypeOffset));
  if (FLAG_debug_code) {
    // Assert that we do not have a cons or slice (indirect strings) here.
    // Sequential strings have already been ruled out.
    STATIC_ASSERT(kIsIndirectStringMask == 1);
    __ tmll(r2, Operand(kIsIndirectStringMask));
    __ Assert(eq, kExternalStringExpectedButNotFound, cr0);
  }
  __ LoadP(subject,
         FieldMemOperand(subject, ExternalString::kResourceDataOffset));
  // Move the pointer so that offset-wise, it looks like a sequential string.
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqOneByteString::kHeaderSize);
  __ SubP(subject,
         subject,
         Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  __ b(&seq_string);  // Go to (5).

  // (8) Short external string or not a string?  If yes, bail out to runtime.
  __ bind(&not_long_external);
  STATIC_ASSERT(kNotStringTag != 0 && kShortExternalStringTag !=0);
  __ mov(r0, Operand(kIsNotStringMask | kShortExternalStringMask));
  __ AndP(r0, r3);
  __ bne(&runtime/*, cr0*/);

  // (9) Sliced string.  Replace subject with parent.  Go to (4).
  // Load offset into r13 and replace subject string with parent.
  __ LoadP(r13, FieldMemOperand(subject, SlicedString::kOffsetOffset));
  __ SmiUntag(r13);
  __ LoadP(subject, FieldMemOperand(subject, SlicedString::kParentOffset));
  __ b(&check_underlying);  // Go to (4).
#endif  // V8_INTERPRETED_REGEXP
}


static void GenerateRecordCallTarget(MacroAssembler* masm) {
  // Cache the called function in a feedback vector slot.  Cache states
  // are uninitialized, monomorphic (indicated by a JSFunction), and
  // megamorphic.
  // r2 : number of arguments to the construct function
  // r3 : the function to call
  // r4 : Feedback vector
  // r5 : slot in feedback vector (Smi)
  Label initialize, done, miss, megamorphic, not_array_function;

  DCHECK_EQ(*TypeFeedbackInfo::MegamorphicSentinel(masm->isolate()),
            masm->isolate()->heap()->megamorphic_symbol());
  DCHECK_EQ(*TypeFeedbackInfo::UninitializedSentinel(masm->isolate()),
            masm->isolate()->heap()->uninitialized_symbol());

  // Load the cache state into r6.
  __ SmiToPtrArrayOffset(r6, r5);
  __ AddP(r6, r4, r6);
  __ LoadP(r6, FieldMemOperand(r6, FixedArray::kHeaderSize));

  // A monomorphic cache hit or an already megamorphic state: invoke the
  // function without changing the state.
  __ CmpP(r6, r3);
  __ b(eq, &done);

  if (!FLAG_pretenuring_call_new) {
    // If we came here, we need to see if we are the array function.
    // If we didn't have a matching function, and we didn't find the megamorph
    // sentinel, then we have in the slot either some other function or an
    // AllocationSite. Do a map check on the object in ecx.
    __ LoadP(r7, FieldMemOperand(r6, 0));
    __ CompareRoot(r7, Heap::kAllocationSiteMapRootIndex);
    __ bne(&miss);

    // Make sure the function is the Array() function
    __ LoadGlobalFunction(Context::ARRAY_FUNCTION_INDEX, r6);
    __ CmpP(r3, r6);
    __ bne(&megamorphic);
    __ b(&done);
  }

  __ bind(&miss);

  // A monomorphic miss (i.e, here the cache is not uninitialized) goes
  // megamorphic.
  __ CompareRoot(r6, Heap::kUninitializedSymbolRootIndex);
  __ beq(&initialize);
  // MegamorphicSentinel is an immortal immovable object (undefined) so no
  // write-barrier is needed.
  __ bind(&megamorphic);
  __ SmiToPtrArrayOffset(r6, r5);
  __ AddP(r6, r4, r6);
  __ LoadRoot(ip, Heap::kMegamorphicSymbolRootIndex);
  __ StoreP(ip, FieldMemOperand(r6, FixedArray::kHeaderSize));
  __ jmp(&done);

  // An uninitialized cache is patched with the function
  __ bind(&initialize);

  if (!FLAG_pretenuring_call_new) {
    // Make sure the function is the Array() function.
    __ LoadGlobalFunction(Context::ARRAY_FUNCTION_INDEX, r6);
    __ CmpP(r3, r6);
    __ bne(&not_array_function);

    // The target function is the Array constructor,
    // Create an AllocationSite if we don't already have it, store it in the
    // slot.
    {
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);

      // Arguments register must be smi-tagged to call out.
      __ SmiTag(r2);
      __ Push(r5, r4, r3, r2);

      CreateAllocationSiteStub create_stub(masm->isolate());
      __ CallStub(&create_stub);

      __ Pop(r5, r4, r3, r2);
      __ SmiUntag(r2);
    }
    __ b(&done);

    __ bind(&not_array_function);
  }

  __ SmiToPtrArrayOffset(r6, r5);
  __ AddP(r6, r4, r6);
  __ AddP(r6, r6, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ StoreP(r3, MemOperand(r6, 0));

  __ Push(r6, r4, r3);
  __ RecordWrite(r4, r6, r3, kLRHasNotBeenSaved, kDontSaveFPRegs,
                 EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
  __ Pop(r6, r4, r3);

  __ bind(&done);
}


static void EmitContinueIfStrictOrNative(MacroAssembler* masm, Label* cont) {
  // Do not transform the receiver for strict mode functions and natives.
  __ LoadP(r5, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
  __ LoadlW(r6, FieldMemOperand(r5, SharedFunctionInfo::kCompilerHintsOffset));
  __ TestBit(r6,
#if V8_TARGET_ARCH_S390X
             SharedFunctionInfo::kStrictModeFunction,
#else
             SharedFunctionInfo::kStrictModeFunction + kSmiTagSize,
#endif
             r0);
  __ bne(cont/*, cr0*/);

  // Do not transform the receiver for native.
  __ TestBit(r6,
#if V8_TARGET_ARCH_S390X
             SharedFunctionInfo::kNative,
#else
             SharedFunctionInfo::kNative + kSmiTagSize,
#endif
             r0);
  __ bne(cont/*, cr0*/);
}


static void EmitSlowCase(MacroAssembler* masm,
                         int argc,
                         Label* non_function) {
  // Check for function proxy.
  STATIC_ASSERT(JS_FUNCTION_PROXY_TYPE < 0xffffu);
  __ CmpP(r6, Operand(JS_FUNCTION_PROXY_TYPE));
  __ bne(non_function);
  __ push(r3);  // put proxy as additional argument
  __ LoadImmP(r2, Operand(argc + 1));
  __ LoadImmP(r4, Operand::Zero());
  __ GetBuiltinFunction(r3, Builtins::CALL_FUNCTION_PROXY);
  {
    Handle<Code> adaptor =
        masm->isolate()->builtins()->ArgumentsAdaptorTrampoline();
    __ Jump(adaptor, RelocInfo::CODE_TARGET);
  }

  // CALL_NON_FUNCTION expects the non-function callee as receiver (instead
  // of the original receiver from the call site).
  __ bind(non_function);
  __ StoreP(r3, MemOperand(sp, argc * kPointerSize), r0);
  __ LoadImmP(r2, Operand(argc));  // Set up the number of arguments.
  __ LoadImmP(r4, Operand::Zero());
  __ GetBuiltinFunction(r3, Builtins::CALL_NON_FUNCTION);
  __ Jump(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
          RelocInfo::CODE_TARGET);
}


static void EmitWrapCase(MacroAssembler* masm, int argc, Label* cont) {
  // Wrap the receiver and patch it back onto the stack.
  { FrameAndConstantPoolScope frame_scope(masm, StackFrame::INTERNAL);
    __ Push(r3, r5);
    __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
    __ pop(r3);
  }
  __ StoreP(r2, MemOperand(sp, argc * kPointerSize));
  __ b(cont);
}


static void CallFunctionNoFeedback(MacroAssembler* masm,
                                   int argc, bool needs_checks,
                                   bool call_as_method) {
  // r3 : the function to call
  Label slow, non_function, wrap, cont;

  if (needs_checks) {
    // Check that the function is really a JavaScript function.
    // r3: pushed function (to be verified)
    __ JumpIfSmi(r3, &non_function);

    // Goto slow case if we do not have a function.
    __ CompareObjectType(r3, r6, r6, JS_FUNCTION_TYPE);
    __ bne(&slow);
  }

  // Fast-case: Invoke the function now.
  // r3: pushed function
  ParameterCount actual(argc);

  if (call_as_method) {
    if (needs_checks) {
      EmitContinueIfStrictOrNative(masm, &cont);
    }

    // Compute the receiver in sloppy mode.
    __ LoadP(r5, MemOperand(sp, argc * kPointerSize));

    if (needs_checks) {
      __ JumpIfSmi(r5, &wrap);
      __ CompareObjectType(r5, r6, r6, FIRST_SPEC_OBJECT_TYPE);
      __ blt(&wrap);
    } else {
      __ b(&wrap);
    }

    __ bind(&cont);
  }

  __ InvokeFunction(r3, actual, JUMP_FUNCTION, NullCallWrapper());

  if (needs_checks) {
    // Slow-case: Non-function called.
    __ bind(&slow);
    EmitSlowCase(masm, argc, &non_function);
  }

  if (call_as_method) {
    __ bind(&wrap);
    EmitWrapCase(masm, argc, &cont);
  }
}


void CallFunctionStub::Generate(MacroAssembler* masm) {
  CallFunctionNoFeedback(masm, argc_, NeedsChecks(), CallAsMethod());
}


void CallConstructStub::Generate(MacroAssembler* masm) {
  // r2 : number of arguments
  // r3 : the function to call
  // r4 : feedback vector
  // r5 : (only if r4 is not the megamorphic symbol) slot in feedback
  //      vector (Smi)
  Label slow, non_function_call;

  // Check that the function is not a smi.
  __ JumpIfSmi(r3, &non_function_call);
  // Check that the function is a JSFunction.
  __ CompareObjectType(r3, r6, r6, JS_FUNCTION_TYPE);
  __ bne(&slow, Label::kNear);

  if (RecordCallTarget()) {
    GenerateRecordCallTarget(masm);

    __ SmiToPtrArrayOffset(r7, r5);
    __ AddP(r7, r4, r7);
    if (FLAG_pretenuring_call_new) {
      // Put the AllocationSite from the feedback vector into r4.
      // By adding kPointerSize we encode that we know the AllocationSite
      // entry is at the feedback vector slot given by r5 + 1.
      __ LoadP(r4, FieldMemOperand(r7, FixedArray::kHeaderSize + kPointerSize));
    } else {
      Label feedback_register_initialized;
      // Put the AllocationSite from the feedback vector into r4, or undefined.
      __ LoadP(r4, FieldMemOperand(r7, FixedArray::kHeaderSize));
      __ LoadP(r7, FieldMemOperand(r4, AllocationSite::kMapOffset));
      __ CompareRoot(r7, Heap::kAllocationSiteMapRootIndex);
      __ beq(&feedback_register_initialized, Label::kNear);
      __ LoadRoot(r4, Heap::kUndefinedValueRootIndex);
      __ bind(&feedback_register_initialized);
    }

    __ AssertUndefinedOrAllocationSite(r4, r7);
  }

  // Jump to the function-specific construct stub.
  Register jmp_reg = r6;
  __ LoadP(jmp_reg, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(jmp_reg, FieldMemOperand(jmp_reg,
                                  SharedFunctionInfo::kConstructStubOffset));
  // __ AddP(ip, jmp_reg, Operand(Code::kHeaderSize - kHeapObjectTag));
  // __ JumpToJSEntry(ip);
  __ b(jmp_reg, Code::kHeaderSize - kHeapObjectTag);

  // r2: number of arguments
  // r3: called object
  // r6: object type
  Label do_call;
  __ bind(&slow);
  STATIC_ASSERT(JS_FUNCTION_PROXY_TYPE < 0xffffu);
  __ CmpP(r6, Operand(JS_FUNCTION_PROXY_TYPE));
  __ bne(&non_function_call);
  __ GetBuiltinFunction(r3, Builtins::CALL_FUNCTION_PROXY_AS_CONSTRUCTOR);
  __ b(&do_call);

  __ bind(&non_function_call);
  __ GetBuiltinFunction(r3, Builtins::CALL_NON_FUNCTION_AS_CONSTRUCTOR);
  __ bind(&do_call);
  // Set expected number of arguments to zero (not changing r2).
  __ LoadImmP(r4, Operand::Zero());
  __ Jump(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
          RelocInfo::CODE_TARGET);
}


static void EmitLoadTypeFeedbackVector(MacroAssembler* masm, Register vector) {
  __ LoadP(vector, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ LoadP(vector, FieldMemOperand(vector,
                                   JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(vector, FieldMemOperand(vector,
                                   SharedFunctionInfo::kFeedbackVectorOffset));
}


void CallIC_ArrayStub::Generate(MacroAssembler* masm) {
  // r3 - function
  // r5 - slot id
  Label miss;
  int argc = state_.arg_count();
  ParameterCount actual(argc);

  EmitLoadTypeFeedbackVector(masm, r4);

  __ LoadGlobalFunction(Context::ARRAY_FUNCTION_INDEX, r6);
  __ CmpP(r3, r6);
  __ bne(&miss, Label::kNear);

  __ mov(r2, Operand(arg_count()));
  __ SmiToPtrArrayOffset(r6, r5);
  __ AddP(r6, r4, r6);
  __ LoadP(r6, FieldMemOperand(r6, FixedArray::kHeaderSize));

  // Verify that r6 contains an AllocationSite
  __ LoadP(r7, FieldMemOperand(r6, HeapObject::kMapOffset));
  __ CompareRoot(r7, Heap::kAllocationSiteMapRootIndex);
  __ bne(&miss, Label::kNear);

  __ LoadRR(r4, r6);
  ArrayConstructorStub stub(masm->isolate(), arg_count());
  __ TailCallStub(&stub);

  __ bind(&miss);
  GenerateMiss(masm, IC::kCallIC_Customization_Miss);

  // The slow case, we need this no matter what to complete a call after a miss.
  CallFunctionNoFeedback(masm,
                         arg_count(),
                         true,
                         CallAsMethod());

  // Unreachable.
  __ stop("Unexpected code address");
}


void CallICStub::Generate(MacroAssembler* masm) {
  // r3 - function
  // r5 - slot id (Smi)
  Label extra_checks_or_miss, slow_start;
  Label slow, non_function, wrap, cont;
  Label have_js_function;
  int argc = state_.arg_count();
  ParameterCount actual(argc);

  EmitLoadTypeFeedbackVector(masm, r4);

  // The checks. First, does r3 match the recorded monomorphic target?
  __ SmiToPtrArrayOffset(r6, r5);
  __ AddP(r6, r4, r6);
  __ LoadP(r6, FieldMemOperand(r6, FixedArray::kHeaderSize));
  __ CmpP(r3, r6);
  __ bne(&extra_checks_or_miss, Label::kNear);

  __ bind(&have_js_function);
  if (state_.CallAsMethod()) {
    EmitContinueIfStrictOrNative(masm, &cont);
    // Compute the receiver in sloppy mode.
    __ LoadP(r5, MemOperand(sp, argc * kPointerSize));

    __ JumpIfSmi(r5, &wrap);
    __ CompareObjectType(r5, r6, r6, FIRST_SPEC_OBJECT_TYPE);
    __ blt(&wrap, Label::kNear);

    __ bind(&cont);
  }

  __ InvokeFunction(r3, actual, JUMP_FUNCTION, NullCallWrapper());

  __ bind(&slow);
  EmitSlowCase(masm, argc, &non_function);

  if (state_.CallAsMethod()) {
    __ bind(&wrap);
    EmitWrapCase(masm, argc, &cont);
  }

  __ bind(&extra_checks_or_miss);
  Label miss;

  __ CompareRoot(r6, Heap::kMegamorphicSymbolRootIndex);
  __ beq(&slow_start, Label::kNear);
  __ CompareRoot(r6, Heap::kUninitializedSymbolRootIndex);
  __ beq(&miss);

  if (!FLAG_trace_ic) {
    // We are going megamorphic. If the feedback is a JSFunction, it is fine
    // to handle it here. More complex cases are dealt with in the runtime.
    __ AssertNotSmi(r6);
    __ CompareObjectType(r6, r7, r7, JS_FUNCTION_TYPE);
    __ bne(&miss, Label::kNear);
    __ SmiToPtrArrayOffset(r6, r5);
    __ AddP(r6, r4, r6);
    __ LoadRoot(ip, Heap::kMegamorphicSymbolRootIndex);
    __ StoreP(ip, FieldMemOperand(r6, FixedArray::kHeaderSize));
    __ jmp(&slow_start, Label::kNear);
  }

  // We are here because tracing is on or we are going monomorphic.
  __ bind(&miss);
  GenerateMiss(masm, IC::kCallIC_Miss);

  // the slow case
  __ bind(&slow_start);
  // Check that the function is really a JavaScript function.
  // r3: pushed function (to be verified)
  __ JumpIfSmi(r3, &non_function);

  // Goto slow case if we do not have a function.
  __ CompareObjectType(r3, r6, r6, JS_FUNCTION_TYPE);
  __ bne(&slow);
  __ b(&have_js_function);
}


void CallICStub::GenerateMiss(MacroAssembler* masm, IC::UtilityId id) {
  // Get the receiver of the function from the stack; 1 ~ return address.
  __ LoadP(r6, MemOperand(sp, (state_.arg_count() + 1) * kPointerSize));

  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);

    // Push the receiver and the function and feedback info.
    __ Push(r6, r3, r4, r5);

    // Call the entry.
    ExternalReference miss = ExternalReference(IC_Utility(id),
                                               masm->isolate());
    __ CallExternalReference(miss, 4);

    // Move result to r3 and exit the internal frame.
    __ LoadRR(r3, r2);
  }
}


// StringCharCodeAtGenerator
void StringCharCodeAtGenerator::GenerateFast(MacroAssembler* masm) {
#ifdef V8_OS_ZOS
  bool native_ascii_encoding = false;
#else
  bool native_ascii_encoding = true;
#endif
  // If the receiver is a smi trigger the non-string case.
  __ JumpIfSmi(object_, receiver_not_string_);

  // Fetch the instance type of the receiver into result register.
  __ LoadP(result_, FieldMemOperand(object_, HeapObject::kMapOffset));
  __ LoadlB(result_, FieldMemOperand(result_, Map::kInstanceTypeOffset));
  // If the receiver is not a string trigger the non-string case.
  __ mov(r0, Operand(kIsNotStringMask));
  __ AndP(r0, result_);
  __ bne(receiver_not_string_ /*, cr0*/);

  // If the index is non-smi trigger the non-smi case.
  __ JumpIfNotSmi(index_, &index_not_smi_);
  __ bind(&got_smi_index_);

  // Check for index out of range.
  __ LoadP(ip, FieldMemOperand(object_, String::kLengthOffset));
  __ CmpLogicalP(ip, index_);
  __ ble(index_out_of_range_);

  __ SmiUntag(index_);

  StringCharLoadGenerator::Generate(masm,
                                    object_,
                                    index_,
                                    result_,
                                    &call_runtime_,
                                    !native_ascii_encoding);

  __ SmiTag(result_);
  __ bind(&exit_);
}


void StringCharCodeAtGenerator::GenerateSlow(
    MacroAssembler* masm,
    const RuntimeCallHelper& call_helper) {
  __ Abort(kUnexpectedFallthroughToCharCodeAtSlowCase);

  // Index is not a smi.
  __ bind(&index_not_smi_);
  // If index is a heap number, try converting it to an integer.
  __ CheckMap(index_,
              result_,
              Heap::kHeapNumberMapRootIndex,
              index_not_number_,
              DONT_DO_SMI_CHECK);
  call_helper.BeforeCall(masm);
  __ push(object_);
  __ push(index_);  // Consumed by runtime conversion function.
  if (index_flags_ == STRING_INDEX_IS_NUMBER) {
    __ CallRuntime(Runtime::kNumberToIntegerMapMinusZero, 1);
  } else {
    DCHECK(index_flags_ == STRING_INDEX_IS_ARRAY_INDEX);
    // NumberToSmi discards numbers that are not exact integers.
    __ CallRuntime(Runtime::kNumberToSmi, 1);
  }
  // Save the conversion result before the pop instructions below
  // have a chance to overwrite it.
  __ Move(index_, r2);
  __ pop(object_);
  // Reload the instance type.
  __ LoadP(result_, FieldMemOperand(object_, HeapObject::kMapOffset));
  __ LoadlB(result_, FieldMemOperand(result_, Map::kInstanceTypeOffset));
  call_helper.AfterCall(masm);
  // If index is still not a smi, it must be out of range.
  __ JumpIfNotSmi(index_, index_out_of_range_);
  // Otherwise, return to the fast path.
  __ b(&got_smi_index_);

  // Call runtime. We get here when the receiver is a string and the
  // index is a number, but the code of getting the actual character
  // is too complex (e.g., when the string needs to be flattened).
  __ bind(&call_runtime_);
  call_helper.BeforeCall(masm);
  __ SmiTag(index_);
  __ Push(object_, index_);
  __ CallRuntime(Runtime::kStringCharCodeAtRT, 2);
  __ Move(result_, r2);
  call_helper.AfterCall(masm);
  __ b(&exit_);

  __ Abort(kUnexpectedFallthroughFromCharCodeAtSlowCase);
}


// -------------------------------------------------------------------------
// StringCharFromCodeGenerator

  void StringCharFromCodeGenerator::GenerateFast(MacroAssembler* masm) {
  // Fast case of Heap::LookupSingleCharacterStringFromCode.
  DCHECK(IsPowerOf2(String::kMaxOneByteCharCode + 1));
  __ LoadSmiLiteral(r0, Smi::FromInt(~String::kMaxOneByteCharCode));
  __ OrP(r0, Operand(kSmiTagMask));
  __ AndP(r0, code_);
  __ CmpP(r0, Operand::Zero());
  __ bne(&slow_case_);

  // At this point code register contains smi tagged ASCII char code.
  __ LoadRR(r0, code_);
#ifdef V8_OS_ZOS
  __ mov(result_, Operand(ExternalReference::ascii_to_ebcdic_table()));
  __ lay(sp, MemOperand(sp, -kPointerSize));
  __ SmiUntag(code_);
  __ StoreByte(code_ , MemOperand(sp, 0));
  __ Translate(sp, MemOperand(result_, 0), 0);
  __ LoadlB(code_ , MemOperand(sp, 0));
  __ SmiTag(code_);
#endif
  __ LoadRoot(result_, Heap::kSingleCharacterStringCacheRootIndex);
  __ SmiToPtrArrayOffset(code_, code_);
  __ AddP(result_, code_);
  __ LoadRR(code_, r0);
  __ LoadP(result_, FieldMemOperand(result_, FixedArray::kHeaderSize));
  __ CompareRoot(result_, Heap::kUndefinedValueRootIndex);
  __ beq(&slow_case_);
  __ bind(&exit_);
}


void StringCharFromCodeGenerator::GenerateSlow(
    MacroAssembler* masm,
    const RuntimeCallHelper& call_helper) {
  __ Abort(kUnexpectedFallthroughToCharFromCodeSlowCase);

  __ bind(&slow_case_);
  call_helper.BeforeCall(masm);
#ifdef V8_OS_ZOS
  __ mov(result_, Operand(ExternalReference::ascii_to_ebcdic_table()));
  __ lay(sp, MemOperand(sp, -kPointerSize));
  __ SmiUntag(code_);
  __ StoreByte(code_ , MemOperand(sp, 0));
  __ Translate(sp, MemOperand(result_, 0), 0);
  __ LoadlB(code_ , MemOperand(sp, 0));
  __ SmiTag(code_);
#endif
  __ push(code_);
  __ CallRuntime(Runtime::kCharFromCode, 1);
  __ Move(result_, r2);
  call_helper.AfterCall(masm);
  __ b(&exit_);

  __ Abort(kUnexpectedFallthroughFromCharFromCodeSlowCase);
}


enum CopyCharactersFlags {
  COPY_ASCII = 1,
  DEST_ALWAYS_ALIGNED = 2
};


void StringHelper::GenerateCopyCharacters(MacroAssembler* masm,
                                          Register dest,
                                          Register src,
                                          Register count,
                                          Register scratch,
                                          String::Encoding encoding) {
  if (FLAG_debug_code) {
    // Check that destination is word aligned.
    __ mov(r0, Operand(kPointerAlignmentMask));
    __ AndP(r0, dest);
    __ Check(eq, kDestinationOfCopyNotAligned, cr0);
  }

  // Nothing to do for zero characters.
  Label done;
  if (encoding == String::TWO_BYTE_ENCODING) {
    // double the length
    __ AddP(count, count, count);
    __ beq(&done, Label::kNear);
  } else {
    __ CmpP(count, Operand::Zero());
    __ beq(&done, Label::kNear);
  }

  // Copy count bytes from src to dst.
  Label byte_loop;
  // TODO(joransiu): Convert into MVC loop
  __ bind(&byte_loop);
  __ LoadlB(scratch, MemOperand(src));
  __ la(src, MemOperand(src, 1));
  __ stc(scratch, MemOperand(dest));
  __ la(dest, MemOperand(dest, 1));
  __ BranchOnCount(count, &byte_loop);

  __ bind(&done);
}


void StringHelper::GenerateHashInit(MacroAssembler* masm,
                                    Register hash,
                                    Register character,
                                    Register scratch) {
  // hash = character + (character << 10);
  __ LoadRoot(hash, Heap::kHashSeedRootIndex);
  // Untag smi seed and add the character.
  __ SmiUntag(scratch, hash);
  __ AddP(hash, character, scratch);
  // hash += hash << 10;
  __ ShiftLeft(scratch, hash, Operand(10));
  __ AddP(hash, scratch);
  // hash ^= hash >> 6;
  __ ShiftRight(scratch, hash, Operand(6));
  __ XorP(hash, scratch);
}


void StringHelper::GenerateHashAddCharacter(MacroAssembler* masm,
                                            Register hash,
                                            Register character,
                                            Register scratch) {
  // hash += character;
  __ AddP(hash, character);
  // hash += hash << 10;
  __ ShiftLeft(scratch, hash, Operand(10));
  __ AddP(hash, scratch);
  // hash ^= hash >> 6;
  __ ShiftRight(scratch, hash, Operand(6));
  __ XorP(hash, scratch);
}


void StringHelper::GenerateHashGetHash(MacroAssembler* masm,
                                       Register hash,
                                       Register scratch) {
  // hash += hash << 3;
  __ ShiftLeft(scratch, hash, Operand(3));
  __ AddP(hash, scratch);
  // hash ^= hash >> 11;
  __ ShiftRight(scratch, hash, Operand(11));
  __ XorP(hash, scratch);
  // hash += hash << 15;
  __ ShiftLeft(scratch, hash, Operand(15));
  __ AddP(hash, scratch);

  __ mov(scratch, Operand(String::kHashBitMask));
  __ AndP(hash, scratch/*, SetRC*/);  // Should be okay to remove RC

  // if (hash == 0) hash = 27;
  Label done;
  __ bne(&done /*, cr0*/);
  __ LoadImmP(hash, Operand(StringHasher::kZeroHash));
  __ bind(&done);
}


void SubStringStub::Generate(MacroAssembler* masm) {
  Label runtime;

  // Stack frame on entry.
  //  lr: return address
  //  sp[0]: to
  //  sp[4]: from
  //  sp[8]: string

  // This stub is called from the native-call %_SubString(...), so
  // nothing can be assumed about the arguments. It is tested that:
  //  "string" is a sequential string,
  //  both "from" and "to" are smis, and
  //  0 <= from <= to <= string.length.
  // If any of these assumptions fail, we call the runtime system.

  const int kToOffset = 0 * kPointerSize;
  const int kFromOffset = 1 * kPointerSize;
  const int kStringOffset = 2 * kPointerSize;

  __ LoadP(r4, MemOperand(sp, kToOffset));
  __ LoadP(r5, MemOperand(sp, kFromOffset));

  // If either to or from had the smi tag bit set, then fail to generic runtime
  __ JumpIfNotSmi(r4, &runtime);
  __ JumpIfNotSmi(r5, &runtime);
  __ SmiUntag(r4);
  __ SmiUntag(r5);
  // Both r4 and r5 are untagged integers.

  // We want to bailout to runtime here if From is negative.
  __ blt(&runtime /*, cr0*/);  // From < 0.

  __ CmpLogicalP(r5, r4);
  __ bgt(&runtime);  // Fail if from > to.
  __ SubP(r4, r4, r5);

  // Make sure first argument is a string.
  __ LoadP(r2, MemOperand(sp, kStringOffset));
  __ JumpIfSmi(r2, &runtime);
  Condition is_string = masm->IsObjectStringType(r2, r3);
  __ b(NegateCondition(is_string), &runtime /*, cr0*/);

  Label single_char;
  __ CmpP(r4, Operand(1));
  __ b(eq, &single_char);

  // Short-cut for the case of trivial substring.
  Label return_r2;
  // r2: original string
  // r4: result string length
  __ LoadP(r6, FieldMemOperand(r2, String::kLengthOffset));
  __ SmiUntag(r0, r6);
  __ CmpLogicalP(r4, r0);
  // Return original string.
  __ beq(&return_r2);
  // Longer than original string's length or negative: unsafe arguments.
  __ bgt(&runtime);
  // Shorter than original string's length: an actual substring.

  // Deal with different string types: update the index if necessary
  // and put the underlying string into r7.
  // r2: original string
  // r3: instance type
  // r4: length
  // r5: from index (untagged)
  Label underlying_unpacked, sliced_string, seq_or_external_string;
  // If the string is not indirect, it can only be sequential or external.
  STATIC_ASSERT(kIsIndirectStringMask == (kSlicedStringTag & kConsStringTag));
  STATIC_ASSERT(kIsIndirectStringMask != 0);
  __ mov(r0, Operand(kIsIndirectStringMask));
  __ AndP(r0, r3);
  __ beq(&seq_or_external_string /*, cr0*/);

  __ mov(r0, Operand(kSlicedNotConsMask));
  __ AndP(r0, r3);
  __ bne(&sliced_string /*, cr0*/);
  // Cons string.  Check whether it is flat, then fetch first part.
  __ LoadP(r7, FieldMemOperand(r2, ConsString::kSecondOffset));
  __ CompareRoot(r7, Heap::kempty_stringRootIndex);
  __ bne(&runtime);
  __ LoadP(r7, FieldMemOperand(r2, ConsString::kFirstOffset));
  // Update instance type.
  __ LoadP(r3, FieldMemOperand(r7, HeapObject::kMapOffset));
  __ LoadlB(r3, FieldMemOperand(r3, Map::kInstanceTypeOffset));
  __ b(&underlying_unpacked);

  __ bind(&sliced_string);
  // Sliced string.  Fetch parent and correct start index by offset.
  __ LoadP(r7, FieldMemOperand(r2, SlicedString::kParentOffset));
  __ LoadP(r6, FieldMemOperand(r2, SlicedString::kOffsetOffset));
  __ SmiUntag(r3, r6);
  __ AddP(r5, r3);  // Add offset to index.
  // Update instance type.
  __ LoadP(r3, FieldMemOperand(r7, HeapObject::kMapOffset));
  __ LoadlB(r3, FieldMemOperand(r3, Map::kInstanceTypeOffset));
  __ b(&underlying_unpacked);

  __ bind(&seq_or_external_string);
  // Sequential or external string.  Just move string to the expected register.
  __ LoadRR(r7, r2);

  __ bind(&underlying_unpacked);

  if (FLAG_string_slices) {
    Label copy_routine;
    // r7: underlying subject string
    // r3: instance type of underlying subject string
    // r4: length
    // r5: adjusted start index (untagged)
    __ CmpP(r4, Operand(SlicedString::kMinLength));
    // Short slice.  Copy instead of slicing.
    __ blt(&copy_routine);
    // Allocate new sliced string.  At this point we do not reload the instance
    // type including the string encoding because we simply rely on the info
    // provided by the original string.  It does not matter if the original
    // string's encoding is wrong because we always have to recheck encoding of
    // the newly created string's parent anyways due to externalized strings.
    Label two_byte_slice, set_slice_header;
    STATIC_ASSERT((kStringEncodingMask & kOneByteStringTag) != 0);
    STATIC_ASSERT((kStringEncodingMask & kTwoByteStringTag) == 0);
    __ mov(r0, Operand(kStringEncodingMask));
    __ AndP(r0, r3);
    __ beq(&two_byte_slice /*, cr0*/);
    __ AllocateAsciiSlicedString(r2, r4, r8, r9, &runtime);
    __ b(&set_slice_header);
    __ bind(&two_byte_slice);
    __ AllocateTwoByteSlicedString(r2, r4, r8, r9, &runtime);
    __ bind(&set_slice_header);
    __ SmiTag(r5);
    __ StoreP(r7, FieldMemOperand(r2, SlicedString::kParentOffset));
    __ StoreP(r5, FieldMemOperand(r2, SlicedString::kOffsetOffset));
    __ b(&return_r2);

    __ bind(&copy_routine);
  }

  // r7: underlying subject string
  // r3: instance type of underlying subject string
  // r4: length
  // r5: adjusted start index (untagged)
  Label two_byte_sequential, sequential_string, allocate_result;
  STATIC_ASSERT(kExternalStringTag != 0);
  STATIC_ASSERT(kSeqStringTag == 0);
  __ mov(r0, Operand(kExternalStringTag));
  __ AndP(r0, r3);
  __ beq(&sequential_string /*, cr0*/);

  // Handle external string.
  // Rule out short external strings.
  STATIC_ASSERT(kShortExternalStringTag != 0);
  __ mov(r0, Operand(kShortExternalStringTag));
  __ AndP(r0, r3);
  __ bne(&runtime /*, cr0*/);
  __ LoadP(r7, FieldMemOperand(r7, ExternalString::kResourceDataOffset));
  // r7 already points to the first character of underlying string.
  __ b(&allocate_result);

  __ bind(&sequential_string);
  // Locate first character of underlying subject string.
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqOneByteString::kHeaderSize);
  __ AddP(r7, Operand(SeqOneByteString::kHeaderSize - kHeapObjectTag));

  __ bind(&allocate_result);
  // Sequential acii string.  Allocate the result.
  STATIC_ASSERT((kOneByteStringTag & kStringEncodingMask) != 0);
  __ mov(r0, Operand(kStringEncodingMask));
  __ AndP(r0, r3);
  __ beq(&two_byte_sequential /*, cr0*/);

  // Allocate and copy the resulting ASCII string.
  __ AllocateAsciiString(r2, r4, r6, r8, r9, &runtime);

  // Locate first character of substring to copy.
  __ AddP(r7, r5);
  // Locate first character of result.
  __ AddP(r3, r2, Operand(SeqOneByteString::kHeaderSize - kHeapObjectTag));

  // r2: result string
  // r3: first character of result string
  // r4: result string length
  // r7: first character of substring to copy
  STATIC_ASSERT((SeqOneByteString::kHeaderSize & kObjectAlignmentMask) == 0);
  StringHelper::GenerateCopyCharacters(
      masm, r3, r7, r4, r5, String::ONE_BYTE_ENCODING);
  __ b(&return_r2);

  // Allocate and copy the resulting two-byte string.
  __ bind(&two_byte_sequential);
  __ AllocateTwoByteString(r2, r4, r6, r8, r9, &runtime);

  // Locate first character of substring to copy.
  __ ShiftLeftP(r3, r5, Operand(1));
  __ AddP(r7, r3);
  // Locate first character of result.
  __ AddP(r3, r2, Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));

  // r2: result string.
  // r3: first character of result.
  // r4: result length.
  // r7: first character of substring to copy.
  STATIC_ASSERT((SeqTwoByteString::kHeaderSize & kObjectAlignmentMask) == 0);
  StringHelper::GenerateCopyCharacters(
      masm, r3, r7, r4, r5, String::TWO_BYTE_ENCODING);

  __ bind(&return_r2);
  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->sub_string_native(), 1, r5, r6);
  __ Drop(3);
  __ Ret();

  // Just jump to runtime to create the sub string.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kSubString, 3, 1);

  __ bind(&single_char);
  // r2: original string
  // r3: instance type
  // r4: length
  // r5: from index (untagged)
  __ SmiTag(r5, r5);
  StringCharAtGenerator generator(
      r2, r5, r4, r2, &runtime, &runtime, &runtime, STRING_INDEX_IS_NUMBER);
  generator.GenerateFast(masm);
  __ Drop(3);
  __ Ret();
  generator.SkipSlow(masm, &runtime);
}


void StringCompareStub::GenerateFlatAsciiStringEquals(MacroAssembler* masm,
                                                      Register left,
                                                      Register right,
                                                      Register scratch1,
                                                      Register scratch2) {
  Register length = scratch1;

  // Compare lengths.
  Label strings_not_equal, check_zero_length;
  __ LoadP(length, FieldMemOperand(left, String::kLengthOffset));
  __ LoadP(scratch2, FieldMemOperand(right, String::kLengthOffset));
  __ CmpP(length, scratch2);
  __ beq(&check_zero_length);
  __ bind(&strings_not_equal);
  __ LoadSmiLiteral(r2, Smi::FromInt(NOT_EQUAL));
  __ Ret();

  // Check if the length is zero.
  Label compare_chars;
  __ bind(&check_zero_length);
  STATIC_ASSERT(kSmiTag == 0);
  __ CmpP(length, Operand::Zero());
  __ bne(&compare_chars);
  __ LoadSmiLiteral(r2, Smi::FromInt(EQUAL));
  __ Ret();

  // Compare characters.
  __ bind(&compare_chars);
  GenerateAsciiCharsCompareLoop(masm,
                                left, right, length, scratch2,
                                &strings_not_equal);

  // Characters are equal.
  __ LoadSmiLiteral(r2, Smi::FromInt(EQUAL));
  __ Ret();
}


void StringCompareStub::GenerateCompareFlatAsciiStrings(MacroAssembler* masm,
                                                        Register left,
                                                        Register right,
                                                        Register scratch1,
                                                        Register scratch2,
                                                        Register scratch3) {
  Label skip, result_not_equal, compare_lengths;
  // Find minimum length and length difference.
  __ LoadP(scratch1, FieldMemOperand(left, String::kLengthOffset));
  __ LoadP(scratch2, FieldMemOperand(right, String::kLengthOffset));
  __ SubP(scratch3, scratch1, scratch2/*, LeaveOE, SetRC*/);
  // Removing RC looks okay here.
  Register length_delta = scratch3;
  __ ble(&skip, Label::kNear);
  __ LoadRR(scratch1, scratch2);
  __ bind(&skip);
  Register min_length = scratch1;
  STATIC_ASSERT(kSmiTag == 0);
  __ CmpP(min_length, Operand::Zero());
  __ beq(&compare_lengths);

  // Compare loop.
  GenerateAsciiCharsCompareLoop(masm,
                                left, right, min_length, scratch2,
                                &result_not_equal);

  // Compare lengths - strings up to min-length are equal.
  __ bind(&compare_lengths);
  DCHECK(Smi::FromInt(EQUAL) == static_cast<Smi*>(0));
  // Use length_delta as result if it's zero.
  __ LoadRR(r2, length_delta);
  __ CmpP(length_delta, Operand::Zero());
  __ bind(&result_not_equal);
  // Conditionally update the result based either on length_delta or
  // the last comparion performed in the loop above.
  Label less_equal, equal;
  __ ble(&less_equal);
  __ LoadSmiLiteral(r2, Smi::FromInt(GREATER));
  __ Ret();
  __ bind(&less_equal);
  __ beq(&equal);
  __ LoadSmiLiteral(r2, Smi::FromInt(LESS));
  __ bind(&equal);
  __ Ret();
}


void StringCompareStub::GenerateAsciiCharsCompareLoop(
    MacroAssembler* masm,
    Register left,
    Register right,
    Register length,
    Register scratch1,
    Label* chars_not_equal) {
  // Change index to run from -length to -1 by adding length to string
  // start. This means that loop ends when index reaches zero, which
  // doesn't need an additional compare.
  __ SmiUntag(length);
  __ AddP(scratch1, length,
          Operand(SeqOneByteString::kHeaderSize - kHeapObjectTag));
  __ AddP(left, scratch1);
  __ AddP(right, scratch1);
  __ LoadComplementRR(length, length);
  Register index = length;  // index = -length;

  // Compare loop.
  Label loop;
  __ bind(&loop);
  __ LoadlB(scratch1, MemOperand(left, index));
  __ LoadlB(r0, MemOperand(right, index));
  __ CmpP(scratch1, r0);
  __ bne(chars_not_equal);
  __ AddP(index, Operand(1));
  __ CmpP(index, Operand::Zero());
  __ bne(&loop);
}


void StringCompareStub::Generate(MacroAssembler* masm) {
  Label runtime;

  Counters* counters = isolate()->counters();

  // Stack frame on entry.
  //  sp[0]: right string
  //  sp[4]: left string
  __ LoadP(r2, MemOperand(sp));  // Load right in r2, left in r3.
  __ LoadP(r3, MemOperand(sp, kPointerSize));

  Label not_same;
  __ CmpP(r2, r3);
  __ bne(&not_same);
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ LoadSmiLiteral(r2, Smi::FromInt(EQUAL));
  __ IncrementCounter(counters->string_compare_native(), 1, r3, r4);
  __ la(sp, MemOperand(sp, (2 * kPointerSize)));
  __ Ret();

  __ bind(&not_same);

  // Check that both objects are sequential ASCII strings.
  __ JumpIfNotBothSequentialAsciiStrings(r3, r2, r4, r5, &runtime);

  // Compare flat ASCII strings natively. Remove arguments from stack first.
  __ IncrementCounter(counters->string_compare_native(), 1, r4, r5);
  __ la(sp, MemOperand(sp, (2 * kPointerSize)));
  GenerateCompareFlatAsciiStrings(masm, r3, r2, r4, r5, r6);

  // Call the runtime; it returns -1 (less), 0 (equal), or 1 (greater)
  // tagged as a small integer.
  __ bind(&runtime);
  __ TailCallRuntime(Runtime::kStringCompare, 2, 1);
}


void BinaryOpICWithAllocationSiteStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r3    : left
  //  -- r2    : right
  // r3: second string
  // -----------------------------------

  // Load r4 with the allocation site.  We stick an undefined dummy value here
  // and replace it with the real allocation site later when we instantiate this
  // stub in BinaryOpICWithAllocationSiteStub::GetCodeCopyFromTemplate().
  __ Move(r4, handle(isolate()->heap()->undefined_value()));

  // Make sure that we actually patched the allocation site.
  if (FLAG_debug_code) {
    __ TestIfSmi(r4);
    __ Assert(ne, kExpectedAllocationSite, cr0);
    __ push(r4);
    __ LoadP(r4, FieldMemOperand(r4, HeapObject::kMapOffset));
    __ CompareRoot(r4, Heap::kAllocationSiteMapRootIndex);
    __ pop(r4);
    __ Assert(eq, kExpectedAllocationSite);
  }

  // Tail call into the stub that handles binary operations with allocation
  // sites.
  BinaryOpWithAllocationSiteStub stub(isolate(), state_);
  __ TailCallStub(&stub);
}


void ICCompareStub::GenerateSmis(MacroAssembler* masm) {
  DCHECK(state_ == CompareIC::SMI);
  Label miss;
  __ OrP(r4, r3, r2);
  __ JumpIfNotSmi(r4, &miss);

  if (GetCondition() == eq) {
    // For equality we do not care about the sign of the result.
    // __ sub(r2, r2, r3, SetCC);
     __ SubP(r2, r2, r3);
  } else {
    // Untag before subtracting to avoid handling overflow.
    __ SmiUntag(r3);
    __ SmiUntag(r2);
    __ SubP(r2, r3, r2);
  }
  __ Ret();

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateNumbers(MacroAssembler* masm) {
  DCHECK(state_ == CompareIC::NUMBER);

  Label generic_stub;
  Label unordered, maybe_undefined1, maybe_undefined2;
  Label miss;
  Label equal, less_than;

  if (left_ == CompareIC::SMI) {
    __ JumpIfNotSmi(r3, &miss);
  }
  if (right_ == CompareIC::SMI) {
    __ JumpIfNotSmi(r2, &miss);
  }

  // Inlining the double comparison and falling back to the general compare
  // stub if NaN is involved.
  // Load left and right operand.
  Label done, left, left_smi, right_smi;
  __ JumpIfSmi(r2, &right_smi);
  __ CheckMap(r2, r4, Heap::kHeapNumberMapRootIndex, &maybe_undefined1,
              DONT_DO_SMI_CHECK);
  __ LoadF(d1, FieldMemOperand(r2, HeapNumber::kValueOffset));
  __ b(&left);
  __ bind(&right_smi);
  __ SmiToDouble(d1, r2);

  __ bind(&left);
  __ JumpIfSmi(r3, &left_smi);
  __ CheckMap(r3, r4, Heap::kHeapNumberMapRootIndex, &maybe_undefined2,
              DONT_DO_SMI_CHECK);
  __ LoadF(d0, FieldMemOperand(r3, HeapNumber::kValueOffset));
  __ b(&done);
  __ bind(&left_smi);
  __ SmiToDouble(d0, r3);

  __ bind(&done);

  // Compare operands
  __ cdbr(d0, d1);

  // Don't base result on status bits when a NaN is involved.
  __ bunordered(&unordered);

  // Return a result of -1, 0, or 1, based on status bits.
  __ beq(&equal);
  __ blt(&less_than);
  //  assume greater than
  __ LoadImmP(r2, Operand(GREATER));
  __ Ret();
  __ bind(&equal);
  __ LoadImmP(r2, Operand(EQUAL));
  __ Ret();
  __ bind(&less_than);
  __ LoadImmP(r2, Operand(LESS));
  __ Ret();

  __ bind(&unordered);
  __ bind(&generic_stub);
  ICCompareStub stub(isolate(), op_, CompareIC::GENERIC, CompareIC::GENERIC,
                     CompareIC::GENERIC);
  __ Jump(stub.GetCode(), RelocInfo::CODE_TARGET);

  __ bind(&maybe_undefined1);
  if (Token::IsOrderedRelationalCompareOp(op_)) {
    __ CompareRoot(r2, Heap::kUndefinedValueRootIndex);
    __ bne(&miss);
    __ JumpIfSmi(r3, &unordered);
    __ CompareObjectType(r3, r4, r4, HEAP_NUMBER_TYPE);
    __ bne(&maybe_undefined2);
    __ b(&unordered);
  }

  __ bind(&maybe_undefined2);
  if (Token::IsOrderedRelationalCompareOp(op_)) {
    __ CompareRoot(r3, Heap::kUndefinedValueRootIndex);
    __ beq(&unordered);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateInternalizedStrings(MacroAssembler* masm) {
  DCHECK(state_ == CompareIC::INTERNALIZED_STRING);
  Label miss, not_equal;

  // Registers containing left and right operands respectively.
  Register left = r3;
  Register right = r2;
  Register tmp1 = r4;
  Register tmp2 = r5;

  // Check that both operands are heap objects.
  __ JumpIfEitherSmi(left, right, &miss);

  // Check that both operands are symbols.
  __ LoadP(tmp1, FieldMemOperand(left, HeapObject::kMapOffset));
  __ LoadP(tmp2, FieldMemOperand(right, HeapObject::kMapOffset));
  __ LoadlB(tmp1, FieldMemOperand(tmp1, Map::kInstanceTypeOffset));
  __ LoadlB(tmp2, FieldMemOperand(tmp2, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kInternalizedTag == 0 && kStringTag == 0);
  __ OrP(tmp1, tmp1, tmp2);
  __ AndP(r0, tmp1, Operand(kIsNotStringMask | kIsNotInternalizedMask));
  __ bne(&miss/*, cr0*/);

  // Internalized strings are compared by identity.
  __ CmpP(left, right);
  __ bne(&not_equal);
  // Make sure r2 is non-zero. At this point input operands are
  // guaranteed to be non-zero.
  DCHECK(right.is(r2));
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ LoadSmiLiteral(r2, Smi::FromInt(EQUAL));
  __ bind(&not_equal);
  __ Ret();

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateUniqueNames(MacroAssembler* masm) {
  DCHECK(state_ == CompareIC::UNIQUE_NAME);
  DCHECK(GetCondition() == eq);
  Label miss;

  // Registers containing left and right operands respectively.
  Register left = r3;
  Register right = r2;
  Register tmp1 = r4;
  Register tmp2 = r5;

  // Check that both operands are heap objects.
  __ JumpIfEitherSmi(left, right, &miss);

  // Check that both operands are unique names. This leaves the instance
  // types loaded in tmp1 and tmp2.
  __ LoadP(tmp1, FieldMemOperand(left, HeapObject::kMapOffset));
  __ LoadP(tmp2, FieldMemOperand(right, HeapObject::kMapOffset));
  __ LoadlB(tmp1, FieldMemOperand(tmp1, Map::kInstanceTypeOffset));
  __ LoadlB(tmp2, FieldMemOperand(tmp2, Map::kInstanceTypeOffset));

  __ JumpIfNotUniqueName(tmp1, &miss);
  __ JumpIfNotUniqueName(tmp2, &miss);

  // Unique names are compared by identity.
  __ CmpP(left, right);
  __ bne(&miss);
  // Make sure r2 is non-zero. At this point input operands are
  // guaranteed to be non-zero.
  DCHECK(right.is(r2));
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ LoadSmiLiteral(r2, Smi::FromInt(EQUAL));
  __ Ret();

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateStrings(MacroAssembler* masm) {
  DCHECK(state_ == CompareIC::STRING);
  Label miss, not_identical, is_symbol;

  bool equality = Token::IsEqualityOp(op_);

  // Registers containing left and right operands respectively.
  Register left = r3;
  Register right = r2;
  Register tmp1 = r4;
  Register tmp2 = r5;
  Register tmp3 = r6;
  Register tmp4 = r7;

  // Check that both operands are heap objects.
  __ JumpIfEitherSmi(left, right, &miss);

  // Check that both operands are strings. This leaves the instance
  // types loaded in tmp1 and tmp2.
  __ LoadP(tmp1, FieldMemOperand(left, HeapObject::kMapOffset));
  __ LoadP(tmp2, FieldMemOperand(right, HeapObject::kMapOffset));
  __ LoadlB(tmp1, FieldMemOperand(tmp1, Map::kInstanceTypeOffset));
  __ LoadlB(tmp2, FieldMemOperand(tmp2, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kNotStringTag != 0);
  __ OrP(tmp3, tmp1, tmp2);
  __ AndP(r0, tmp3, Operand(kIsNotStringMask));
  __ bne(&miss /*, cr0*/);
  // TODO(JOHN): might be a problem b/c cr0 is not set

  // Fast check for identical strings.
  __ CmpP(left, right);
  STATIC_ASSERT(EQUAL == 0);
  STATIC_ASSERT(kSmiTag == 0);
  __ bne(&not_identical);
  __ LoadSmiLiteral(r2, Smi::FromInt(EQUAL));
  __ Ret();
  __ bind(&not_identical);

  // Handle not identical strings.

  // Check that both strings are internalized strings. If they are, we're done
  // because we already know they are not identical. We know they are both
  // strings.
  if (equality) {
    DCHECK(GetCondition() == eq);
    STATIC_ASSERT(kInternalizedTag == 0);
    __ OrP(tmp3, tmp1, tmp2);
    __ AndP(r0, tmp3, Operand(kIsNotInternalizedMask));
    __ bne(&is_symbol/*, cr0*/);
    // Make sure r2 is non-zero. At this point input operands are
    // guaranteed to be non-zero.
    DCHECK(right.is(r2));
    __ Ret();
    __ bind(&is_symbol);
  }

  // Check that both strings are sequential ASCII.
  Label runtime;
  __ JumpIfBothInstanceTypesAreNotSequentialAscii(
      tmp1, tmp2, tmp3, tmp4, &runtime);

  // Compare flat ASCII strings. Returns when done.
  if (equality) {
    StringCompareStub::GenerateFlatAsciiStringEquals(
        masm, left, right, tmp1, tmp2);
  } else {
    StringCompareStub::GenerateCompareFlatAsciiStrings(
        masm, left, right, tmp1, tmp2, tmp3);
  }

  // Handle more complex cases in runtime.
  __ bind(&runtime);
  __ Push(left, right);
  if (equality) {
    __ TailCallRuntime(Runtime::kStringEquals, 2, 1);
  } else {
    __ TailCallRuntime(Runtime::kStringCompare, 2, 1);
  }

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateObjects(MacroAssembler* masm) {
  DCHECK(state_ == CompareIC::OBJECT);
  Label miss;
  __ AndP(r4, r3, r2);
  __ JumpIfSmi(r4, &miss);

  __ CompareObjectType(r2, r4, r4, JS_OBJECT_TYPE);
  __ bne(&miss);
  __ CompareObjectType(r3, r4, r4, JS_OBJECT_TYPE);
  __ bne(&miss);

  DCHECK(GetCondition() == eq);
  __ SubP(r2, r2, r3);
  __ Ret();

  __ bind(&miss);
  GenerateMiss(masm);
}


void ICCompareStub::GenerateKnownObjects(MacroAssembler* masm) {
  Label miss;
  __ AndP(r4, r3, r2);
  __ JumpIfSmi(r4, &miss);
  __ LoadP(r4, FieldMemOperand(r2, HeapObject::kMapOffset));
  __ LoadP(r5, FieldMemOperand(r3, HeapObject::kMapOffset));
  __ CmpP(r4, Operand(known_map_));
  __ bne(&miss);
  __ CmpP(r5, Operand(known_map_));
  __ bne(&miss);

  __ SubP(r2, r2, r3);
  __ Ret();

  __ bind(&miss);
  GenerateMiss(masm);
}



void ICCompareStub::GenerateMiss(MacroAssembler* masm) {
  {
    // Call the runtime system in a fresh internal frame.
    ExternalReference miss =
        ExternalReference(IC_Utility(IC::kCompareIC_Miss), isolate());

    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ Push(r3, r2);
    __ Push(r3, r2);
    __ LoadSmiLiteral(r0, Smi::FromInt(op_));
    __ push(r0);
    __ CallExternalReference(miss, 3);
    // Compute the entry point of the rewritten stub.
    __ AddP(r4, r2, Operand(Code::kHeaderSize - kHeapObjectTag));
    // Restore registers.
    __ Pop(r3, r2);
  }

  __ JumpToJSEntry(r4);
}


// This stub is paired with DirectCEntryStub::GenerateCall
void DirectCEntryStub::Generate(MacroAssembler* masm) {
#ifndef V8_OS_ZOS
  __ CleanseP(r14);
#else
  __ CleanseP(r7);
  __ StoreP(r7, MemOperand(sp, kStackFrameRASlot * kPointerSize));
#endif
  // Statement positions are expected to be recorded when the target
  // address is loaded.
  __ positions_recorder()->WriteRecordedPositions();

  __ b(ip);  // Callee will return to R14 directly
}

void DirectCEntryStub::GenerateCall(MacroAssembler* masm,
                                    Register target) {
#if ABI_USES_FUNCTION_DESCRIPTORS && !defined(USE_SIMULATOR)
  // Native AIX/S390X Linux use a function descriptor.
  // __ LoadP(ToRegister(ABI_TOC_REGISTER), MemOperand(target, kPointerSize));
  // ip needs to be set for DirectCEentryStub::Generate, and also
  // for ABI_TOC_ADDRESSABILITY_VIA_IP.
  __ LoadP(ip, MemOperand(target, kPointerSize));
  __ LoadP(r5, MemOperand(target, 0));
#else
  __ Move(ip, target);
#endif
#ifdef V8_OS_ZOS
  intptr_t code =
      reinterpret_cast<intptr_t>(GetCode().location());
  __ mov(r7, Operand(code, RelocInfo::CODE_TARGET));
  __ CallC(r7);  // Call the stub.
#else
  intptr_t code =
      reinterpret_cast<intptr_t>(GetCode().location());
  __ mov(r1, Operand(code, RelocInfo::CODE_TARGET));
  __ Call(r1);  // Call the stub.
#endif
}


void NameDictionaryLookupStub::GenerateNegativeLookup(MacroAssembler* masm,
                                                        Label* miss,
                                                        Label* done,
                                                        Register receiver,
                                                        Register properties,
                                                      Handle<Name> name,
                                                        Register scratch0) {
  DCHECK(name->IsUniqueName());
  // If names of slots in range from 1 to kProbes - 1 for the hash value are
  // not equal to the name and kProbes-th slot is not used (its name is the
  // undefined value), it guarantees the hash table doesn't contain the
  // property. It's true even if some slots represent deleted properties
  // (their names are the hole value).
  for (int i = 0; i < kInlinedProbes; i++) {
    // scratch0 points to properties hash.
    // Compute the masked index: (hash + i + i * i) & mask.
    Register index = scratch0;
    // Capacity is smi 2^n.
    __ LoadP(index, FieldMemOperand(properties, kCapacityOffset));
    __ SubP(index, Operand(1));
    __ LoadSmiLiteral(ip, Smi::FromInt(name->Hash() +
                                       NameDictionary::GetProbeOffset(i)));
    __ AndP(index, ip);

    // Scale the index by multiplying by the entry size.
    DCHECK(NameDictionary::kEntrySize == 3);
    __ ShiftLeftP(ip, index, Operand(1));
    __ AddP(index, ip);  // index *= 3.

    Register entity_name = scratch0;
    // Having undefined at this place means the name is not contained.
    Register tmp = properties;
    __ SmiToPtrArrayOffset(ip, index);
    __ AddP(tmp, properties, ip);
    __ LoadP(entity_name, FieldMemOperand(tmp, kElementsStartOffset));

    DCHECK(!tmp.is(entity_name));
    __ CompareRoot(entity_name, Heap::kUndefinedValueRootIndex);
    __ beq(done);

   // Stop if found the property.
    __ CmpP(entity_name, Operand(Handle<Name>(name)));
    __ beq(miss);

    Label good;
    __ CompareRoot(entity_name, Heap::kTheHoleValueRootIndex);
    __ beq(&good);

    // Check if the entry name is not a unique name.
    __ LoadP(entity_name, FieldMemOperand(entity_name,
                                          HeapObject::kMapOffset));
    __ LoadlB(entity_name,
           FieldMemOperand(entity_name, Map::kInstanceTypeOffset));
    __ JumpIfNotUniqueName(entity_name, miss);
    __ bind(&good);

    // Restore the properties.
    __ LoadP(properties,
             FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  }

  const int spill_mask =
      (r0.bit() | r8.bit() | r7.bit() | r6.bit() | r5.bit() |
       r4.bit() | r3.bit() | r2.bit());

  __ LoadRR(r0, r14);
  __ MultiPush(spill_mask);

  __ LoadP(r2, FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  __ mov(r3, Operand(Handle<Name>(name)));
  NameDictionaryLookupStub stub(masm->isolate(), NEGATIVE_LOOKUP);
  __ CallStub(&stub);
  __ CmpP(r2, Operand::Zero());

  __ MultiPop(spill_mask);  // MultiPop does not touch condition flags
  __ LoadRR(r14, r0);

  __ beq(done);
  __ bne(miss);
}


// Probe the name dictionary in the |elements| register. Jump to the
// |done| label if a property with the given name is found. Jump to
// the |miss| label otherwise.
// If lookup was successful |scratch2| will be equal to elements + 4 * index.
void NameDictionaryLookupStub::GeneratePositiveLookup(MacroAssembler* masm,
                                                        Label* miss,
                                                        Label* done,
                                                        Register elements,
                                                        Register name,
                                                        Register scratch1,
                                                        Register scratch2) {
  DCHECK(!elements.is(scratch1));
  DCHECK(!elements.is(scratch2));
  DCHECK(!name.is(scratch1));
  DCHECK(!name.is(scratch2));

  __ AssertName(name);

  // Compute the capacity mask.
  __ LoadP(scratch1, FieldMemOperand(elements, kCapacityOffset));
  __ SmiUntag(scratch1);  // convert smi to int
  __ SubP(scratch1, Operand(1));

  // Generate an unrolled loop that performs a few probes before
  // giving up. Measurements done on Gmail indicate that 2 probes
  // cover ~93% of loads from dictionaries.
  for (int i = 0; i < kInlinedProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    __ LoadlW(scratch2, FieldMemOperand(name, String::kHashFieldOffset));
    if (i > 0) {
      // Add the probe offset (i + i * i) left shifted to avoid right shifting
      // the hash in a separate instruction. The value hash + i + i * i is right
      // shifted in the following and instruction.
      DCHECK(NameDictionary::GetProbeOffset(i) <
             1 << (32 - Name::kHashFieldOffset));
      __ AddP(scratch2, Operand(
                  NameDictionary::GetProbeOffset(i) << Name::kHashShift));
    }
    __ srl(scratch2, Operand(String::kHashShift));
    __ AndP(scratch2, scratch1);

    // Scale the index by multiplying by the element size.
    DCHECK(NameDictionary::kEntrySize == 3);
    // scratch2 = scratch2 * 3.
    __ ShiftLeftP(ip, scratch2, Operand(1));
    __ AddP(scratch2, ip);

    // Check if the key is identical to the name.
    __ ShiftLeftP(ip, scratch2, Operand(kPointerSizeLog2));
    __ AddP(scratch2, elements, ip);
    __ LoadP(ip, FieldMemOperand(scratch2, kElementsStartOffset));
    __ CmpP(name, ip);
    __ beq(done);
  }

  const int spill_mask =
      (r0.bit() | r8.bit() | r7.bit() | r6.bit() |
       r5.bit() | r4.bit() | r3.bit() | r2.bit()) &
      ~(scratch1.bit() | scratch2.bit());

  __ LoadRR(r0, r14);
  __ MultiPush(spill_mask);
  if (name.is(r2)) {
    DCHECK(!elements.is(r3));
    __ LoadRR(r3, name);
    __ LoadRR(r2, elements);
  } else {
    __ LoadRR(r2, elements);
    __ LoadRR(r3, name);
  }
  NameDictionaryLookupStub stub(masm->isolate(), POSITIVE_LOOKUP);
  __ CallStub(&stub);
  __ LoadRR(r1, r2);
  __ LoadRR(scratch2, r4);
  __ MultiPop(spill_mask);
  __ LoadRR(r14, r0);

  __ CmpP(r1, Operand::Zero());
  __ bne(done);
  __ beq(miss);
}


void NameDictionaryLookupStub::Generate(MacroAssembler* masm) {
  // This stub overrides SometimesSetsUpAFrame() to return false.  That means
  // we cannot call anything that could cause a GC from this stub.
  // Registers:
  //  result: NameDictionary to probe
  //  r3: key
  //  dictionary: NameDictionary to probe.
  //  index: will hold an index of entry if lookup is successful.
  //         might alias with result_.
  // Returns:
  //  result_ is zero if lookup failed, non zero otherwise.

  Register result = r2;
  Register dictionary = r2;
  Register key = r3;
  Register index = r4;
  Register mask = r5;
  Register hash = r6;
  Register undefined = r7;
  Register entry_key = r8;
  Register scratch = r8;

  Label in_dictionary, maybe_in_dictionary, not_in_dictionary;

  __ LoadP(mask, FieldMemOperand(dictionary, kCapacityOffset));
  __ SmiUntag(mask);
  __ SubP(mask, Operand(1));

  __ LoadlW(hash, FieldMemOperand(key, String::kHashFieldOffset));

  __ LoadRoot(undefined, Heap::kUndefinedValueRootIndex);

  for (int i = kInlinedProbes; i < kTotalProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    // Capacity is smi 2^n.
    if (i > 0) {
      // Add the probe offset (i + i * i) left shifted to avoid right shifting
      // the hash in a separate instruction. The value hash + i + i * i is right
      // shifted in the following and instruction.
      DCHECK(NameDictionary::GetProbeOffset(i) <
             1 << (32 - Name::kHashFieldOffset));
      __ AddP(index, hash, Operand(
                  NameDictionary::GetProbeOffset(i) << Name::kHashShift));
    } else {
      __ LoadRR(index, hash);
    }
    __ ShiftRight(r0, index, Operand(String::kHashShift));
    __ AndP(index, r0, mask);

    // Scale the index by multiplying by the entry size.
    DCHECK(NameDictionary::kEntrySize == 3);
    __ ShiftLeftP(scratch, index, Operand(1));
    __ AddP(index, scratch);  // index *= 3.

    __ ShiftLeftP(scratch, index, Operand(kPointerSizeLog2));
    __ AddP(index, dictionary, scratch);
    __ LoadP(entry_key, FieldMemOperand(index, kElementsStartOffset));

    // Having undefined at this place means the name is not contained.
    __ CmpP(entry_key, undefined);
    __ beq(&not_in_dictionary);

    // Stop if found the property.
    __ CmpP(entry_key, key);
    __ beq(&in_dictionary);

    if (i != kTotalProbes - 1 && mode_ == NEGATIVE_LOOKUP) {
      // Check if the entry name is not a unique name.
      __ LoadP(entry_key, FieldMemOperand(entry_key, HeapObject::kMapOffset));
      __ LoadlB(entry_key,
              FieldMemOperand(entry_key, Map::kInstanceTypeOffset));
      __ JumpIfNotUniqueName(entry_key, &maybe_in_dictionary);
    }
  }

  __ bind(&maybe_in_dictionary);
  // If we are doing negative lookup then probing failure should be
  // treated as a lookup success. For positive lookup probing failure
  // should be treated as lookup failure.
  if (mode_ == POSITIVE_LOOKUP) {
    __ LoadImmP(result, Operand::Zero());
    __ Ret();
  }

  __ bind(&in_dictionary);
  __ LoadImmP(result, Operand(1));
  __ Ret();

  __ bind(&not_in_dictionary);
  __ LoadImmP(result, Operand::Zero());
  __ Ret();
}


void StoreBufferOverflowStub::GenerateFixedRegStubsAheadOfTime(
    Isolate* isolate) {
  StoreBufferOverflowStub stub1(isolate, kDontSaveFPRegs);
  stub1.GetCode();
  // Hydrogen code stubs need stub2 at snapshot time.
  StoreBufferOverflowStub stub2(isolate, kSaveFPRegs);
  stub2.GetCode();
}


// Takes the input in 3 registers: address_ value_ and object_.  A pointer to
// the value has just been written into the object, now this stub makes sure
// we keep the GC informed.  The word in the object where the value has been
// written is in the address register.
void RecordWriteStub::Generate(MacroAssembler* masm) {
  Label skip_to_incremental_noncompacting;
  Label skip_to_incremental_compacting;

  // The first two branch instructions are generated with labels so as to
  // get the offset fixed up correctly by the bind(Label*) call.  We patch
  // it back and forth between branch condition True and False
  // when we start and stop incremental heap marking.
  // See RecordWriteStub::Patch for details.

  // Clear the bit, branch on True for NOP action initially
  __ b(CC_NOP, &skip_to_incremental_noncompacting);
  __ b(CC_NOP, &skip_to_incremental_compacting);

  if (remembered_set_action_ == EMIT_REMEMBERED_SET) {
    __ RememberedSetHelper(object_,
                           address_,
                           value_,
                           save_fp_regs_mode_,
                           MacroAssembler::kReturnAtEnd);
  }
  __ Ret();

  __ bind(&skip_to_incremental_noncompacting);
  GenerateIncremental(masm, INCREMENTAL);

  __ bind(&skip_to_incremental_compacting);
  GenerateIncremental(masm, INCREMENTAL_COMPACTION);

  // Initial mode of the stub is expected to be STORE_BUFFER_ONLY.
  // Will be checked in IncrementalMarking::ActivateGeneratedStub.
  // patching not required on S390 as the initial path is effectively NOP
}


void RecordWriteStub::GenerateIncremental(MacroAssembler* masm, Mode mode) {
  regs_.Save(masm);

  if (remembered_set_action_ == EMIT_REMEMBERED_SET) {
    Label dont_need_remembered_set;

    __ LoadP(regs_.scratch0(), MemOperand(regs_.address(), 0));
    __ JumpIfNotInNewSpace(regs_.scratch0(),  // Value.
                           regs_.scratch0(),
                           &dont_need_remembered_set);

    __ CheckPageFlag(regs_.object(),
                     regs_.scratch0(),
                     1 << MemoryChunk::SCAN_ON_SCAVENGE,
                     ne,
                     &dont_need_remembered_set);

    // First notify the incremental marker if necessary, then update the
    // remembered set.
    CheckNeedsToInformIncrementalMarker(
        masm, kUpdateRememberedSetOnNoNeedToInformIncrementalMarker, mode);
    InformIncrementalMarker(masm);
    regs_.Restore(masm);
    __ RememberedSetHelper(object_,
                           address_,
                           value_,
                           save_fp_regs_mode_,
                           MacroAssembler::kReturnAtEnd);

    __ bind(&dont_need_remembered_set);
  }

  CheckNeedsToInformIncrementalMarker(
      masm, kReturnOnNoNeedToInformIncrementalMarker, mode);
  InformIncrementalMarker(masm);
  regs_.Restore(masm);
  __ Ret();
}


void RecordWriteStub::InformIncrementalMarker(MacroAssembler* masm) {
  regs_.SaveCallerSaveRegisters(masm, save_fp_regs_mode_);
  int argument_count = 3;
  __ PrepareCallCFunction(argument_count, regs_.scratch0());
  Register address =
      r2.is(regs_.address()) ? regs_.scratch0() : regs_.address();
  DCHECK(!address.is(regs_.object()));
  DCHECK(!address.is(r2));
  __ LoadRR(address, regs_.address());
  __ LoadRR(r2, regs_.object());
  __ LoadRR(r3, address);
  __ mov(r4, Operand(ExternalReference::isolate_address(isolate())));

  AllowExternalCallThatCantCauseGC scope(masm);
#ifdef V8_OS_ZOS
  __ LoadRR(r1, r2);
  __ LoadRR(r2, r3);
  __ LoadRR(r3, r4);
#endif
  __ CallCFunction(
      ExternalReference::incremental_marking_record_write_function(isolate()),
      argument_count);
  regs_.RestoreCallerSaveRegisters(masm, save_fp_regs_mode_);
}


void RecordWriteStub::CheckNeedsToInformIncrementalMarker(
    MacroAssembler* masm,
    OnNoNeedToInformIncrementalMarker on_no_need,
    Mode mode) {
  Label on_black;
  Label need_incremental;
  Label need_incremental_pop_scratch;

  DCHECK((~Page::kPageAlignmentMask & 0xffff) == 0);
  __ AndP(regs_.scratch0(), regs_.object(), Operand(~Page::kPageAlignmentMask));
  __ LoadP(regs_.scratch1(),
         MemOperand(regs_.scratch0(),
                    MemoryChunk::kWriteBarrierCounterOffset));
  __ SubP(regs_.scratch1(), regs_.scratch1(), Operand(1));
  __ StoreP(regs_.scratch1(),
            MemOperand(regs_.scratch0(),
                       MemoryChunk::kWriteBarrierCounterOffset));
  __ CmpP(regs_.scratch1(), Operand::Zero());  // S390, we could do better here
  __ blt(&need_incremental);

  // Let's look at the color of the object:  If it is not black we don't have
  // to inform the incremental marker.
  __ JumpIfBlack(regs_.object(), regs_.scratch0(), regs_.scratch1(), &on_black);

  regs_.Restore(masm);
  if (on_no_need == kUpdateRememberedSetOnNoNeedToInformIncrementalMarker) {
    __ RememberedSetHelper(object_,
                           address_,
                           value_,
                           save_fp_regs_mode_,
                           MacroAssembler::kReturnAtEnd);
  } else {
    __ Ret();
  }

  __ bind(&on_black);

  // Get the value from the slot.
  __ LoadP(regs_.scratch0(), MemOperand(regs_.address(), 0));

  if (mode == INCREMENTAL_COMPACTION) {
    Label ensure_not_white;

    __ CheckPageFlag(regs_.scratch0(),  // Contains value.
                     regs_.scratch1(),  // Scratch.
                     MemoryChunk::kEvacuationCandidateMask,
                     eq,
                     &ensure_not_white);

    __ CheckPageFlag(regs_.object(),
                     regs_.scratch1(),  // Scratch.
                     MemoryChunk::kSkipEvacuationSlotsRecordingMask,
                     eq,
                     &need_incremental);

    __ bind(&ensure_not_white);
  }

  // We need extra registers for this, so we push the object and the address
  // register temporarily.
  __ Push(regs_.object(), regs_.address());
  __ EnsureNotWhite(regs_.scratch0(),  // The value.
                    regs_.scratch1(),  // Scratch.
                    regs_.object(),  // Scratch.
                    regs_.address(),  // Scratch.
                    &need_incremental_pop_scratch);
  __ Pop(regs_.object(), regs_.address());

  regs_.Restore(masm);
  if (on_no_need == kUpdateRememberedSetOnNoNeedToInformIncrementalMarker) {
    __ RememberedSetHelper(object_,
                           address_,
                           value_,
                           save_fp_regs_mode_,
                           MacroAssembler::kReturnAtEnd);
  } else {
    __ Ret();
  }

  __ bind(&need_incremental_pop_scratch);
  __ Pop(regs_.object(), regs_.address());

  __ bind(&need_incremental);

  // Fall through when we need to inform the incremental marker.
}


void StoreArrayLiteralElementStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2    : element value to store
  //  -- r5    : element index as smi
  //  -- sp[0] : array literal index in function as smi
  //  -- sp[4] : array literal
  // clobbers r2, r4, r6
  // -----------------------------------

  Label element_done;
  Label double_elements;
  Label smi_element;
  Label slow_elements;
  Label fast_elements;

  // Get array literal index, array literal and its map.
  __ LoadP(r6, MemOperand(sp, 0 * kPointerSize));
  __ LoadP(r3, MemOperand(sp, 1 * kPointerSize));
  __ LoadP(r4, FieldMemOperand(r3, JSObject::kMapOffset));

  __ CheckFastElements(r4, r7, &double_elements);
  // FAST_*_SMI_ELEMENTS or FAST_*_ELEMENTS
  __ JumpIfSmi(r2, &smi_element);
  __ CheckFastSmiElements(r4, r7, &fast_elements);

  // Store into the array literal requires a elements transition. Call into
  // the runtime.
  __ bind(&slow_elements);
  // call.
  __ Push(r3, r5, r2);
  __ LoadP(r7, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ LoadP(r7, FieldMemOperand(r7, JSFunction::kLiteralsOffset));
  __ Push(r7, r6);
  __ TailCallRuntime(Runtime::kStoreArrayLiteralElement, 5, 1);

  // Array literal has ElementsKind of FAST_*_ELEMENTS and value is an object.
  __ bind(&fast_elements);
  __ LoadP(r7, FieldMemOperand(r3, JSObject::kElementsOffset));
  __ SmiToPtrArrayOffset(r8, r5);
  __ AddP(r8, r7);
  __ StoreP(r2, MemOperand(r8, FixedArray::kHeaderSize - kHeapObjectTag));
  __ lay(r8, MemOperand(r8, FixedArray::kHeaderSize - kHeapObjectTag));

  // Update the write barrier for the array store.
  __ RecordWrite(r7, r8, r2, kLRHasNotBeenSaved, kDontSaveFPRegs,
                 EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
  __ Ret();

  // Array literal has ElementsKind of FAST_*_SMI_ELEMENTS or FAST_*_ELEMENTS,
  // and value is Smi.
  __ bind(&smi_element);
  __ LoadP(r7, FieldMemOperand(r3, JSObject::kElementsOffset));
  __ SmiToPtrArrayOffset(r8, r5);
  __ AddP(r8, r7);
  __ StoreP(r2, FieldMemOperand(r8, FixedArray::kHeaderSize));
  __ Ret();

  // Array literal has ElementsKind of FAST_DOUBLE_ELEMENTS.
  __ bind(&double_elements);
  __ LoadP(r7, FieldMemOperand(r3, JSObject::kElementsOffset));
  __ StoreNumberToDoubleElements(r2, r5, r7, r8, d0, &slow_elements);
  __ Ret();
}


void StubFailureTrampolineStub::Generate(MacroAssembler* masm) {
  CEntryStub ces(isolate(), 1, kSaveFPRegs);
  __ Call(ces.GetCode(), RelocInfo::CODE_TARGET);
  int parameter_count_offset =
      StubFailureTrampolineFrame::kCallerStackParameterCountFrameOffset;
  __ LoadP(r3, MemOperand(fp, parameter_count_offset));
  if (function_mode_ == JS_FUNCTION_STUB_MODE) {
    __ AddP(r3, Operand(1));
  }
  masm->LeaveFrame(StackFrame::STUB_FAILURE_TRAMPOLINE);
  __ ShiftLeftP(r3, r3, Operand(kPointerSizeLog2));
  __ la(sp, MemOperand(r3, sp));
  __ Ret();
}


void ProfileEntryHookStub::MaybeCallEntryHook(MacroAssembler* masm) {
  if (masm->isolate()->function_entry_hook() != NULL) {
    PredictableCodeSizeScope predictable(masm,
#if V8_TARGET_ARCH_S390X
                                         48);
#elif V8_HOST_ARCH_S390
                                         38);
#else
                                         34);
#endif
    ProfileEntryHookStub stub(masm->isolate());
    __ CleanseP(r14);
    __ Push(r14, ip);
    __ CallStub(&stub);
    __ Pop(r14, ip);
  }
}


void ProfileEntryHookStub::Generate(MacroAssembler* masm) {
  // The entry hook is a "push lr" instruction (LAY+ST/STG), followed by a call.
#if V8_TARGET_ARCH_S390X
  const int32_t kReturnAddressDistanceFromFunctionStart =
      Assembler::kCallTargetAddressOffset + 18;  // LAY + STG * 2
#elif V8_HOST_ARCH_S390
  const int32_t kReturnAddressDistanceFromFunctionStart =
      Assembler::kCallTargetAddressOffset + 18;  // NILH + LAY + ST * 2
#else
  const int32_t kReturnAddressDistanceFromFunctionStart =
      Assembler::kCallTargetAddressOffset + 14;  // LAY + ST * 2
#endif

  // This should contain all kJSCallerSaved registers.
  const RegList kSavedRegs =
     kJSCallerSaved |  // Caller saved registers.
     r7.bit();        // Saved stack pointer.

  // We also save r14+ip, so count here is one higher than the mask indicates.
  const int32_t kNumSavedRegs = kNumJSCallerSaved + 3;

  // Save all caller-save registers as this may be called from anywhere.
  __ CleanseP(r14);
  __ LoadRR(ip, r14);
  __ MultiPush(kSavedRegs | ip.bit());

  // Compute the function's address for the first argument.

  __ SubP(r2, ip, Operand(kReturnAddressDistanceFromFunctionStart));

  // The caller's return address is two slots above the saved temporaries.
  // Grab that for the second argument to the hook.
  __ lay(r3, MemOperand(sp, kNumSavedRegs * kPointerSize));

  // Align the stack if necessary.
  int frame_alignment = masm->ActivationFrameAlignment();
  if (frame_alignment > kPointerSize) {
    __ LoadRR(r7, sp);
    DCHECK(IsPowerOf2(frame_alignment));
    __ ClearRightImm(sp, sp, Operand(WhichPowerOf2(frame_alignment)));
  }

#if !defined(USE_SIMULATOR)
  uintptr_t entry_hook =
      reinterpret_cast<uintptr_t>(isolate()->function_entry_hook());
  __ mov(ip, Operand(entry_hook));

#if ABI_USES_FUNCTION_DESCRIPTORS
  // Function descriptor
  // __ LoadP(ToRegister(ABI_TOC_REGISTER), MemOperand(ip, kPointerSize));
  __ LoadP(ip, MemOperand(ip, kPointerSize));
  // ip already set.
#endif
#endif

  // zLinux ABI requires caller's frame to have sufficient space for callee
  // preserved regsiter save area.
  __ LoadImmP(r0, Operand::Zero());
  __ StoreP(r0, MemOperand(sp, -kCalleeRegisterSaveAreaSize -
                          kNumRequiredStackFrameSlots * kPointerSize));
  __ lay(sp, MemOperand(sp, -kCalleeRegisterSaveAreaSize -
                        kNumRequiredStackFrameSlots * kPointerSize));
#if defined(USE_SIMULATOR)
  // Under the simulator we need to indirect the entry hook through a
  // trampoline function at a known address.
  // It additionally takes an isolate as a third parameter
  __ mov(r4, Operand(ExternalReference::isolate_address(isolate())));

  ApiFunction dispatcher(FUNCTION_ADDR(EntryHookTrampoline));
  __ mov(ip, Operand(ExternalReference(&dispatcher,
                                       ExternalReference::BUILTIN_CALL,
                                       isolate())));
#endif
  __ Call(ip);

  // zLinux ABI requires caller's frame to have sufficient space for callee
  // preserved regsiter save area.
  __ la(sp, MemOperand(sp, kCalleeRegisterSaveAreaSize +
                       kNumRequiredStackFrameSlots * kPointerSize));

  // Restore the stack pointer if needed.
  if (frame_alignment > kPointerSize) {
    __ LoadRR(sp, r7);
  }

  // Also pop lr to get Ret(0).
  __ MultiPop(kSavedRegs | ip.bit());
  __ LoadRR(r14, ip);
  __ Ret();
}


template<class T>
static void CreateArrayDispatch(MacroAssembler* masm,
                                AllocationSiteOverrideMode mode) {
  if (mode == DISABLE_ALLOCATION_SITES) {
    T stub(masm->isolate(), GetInitialFastElementsKind(), mode);
    __ TailCallStub(&stub);
  } else if (mode == DONT_OVERRIDE) {
    int last_index = GetSequenceIndexFromFastElementsKind(
        TERMINAL_FAST_ELEMENTS_KIND);
    for (int i = 0; i <= last_index; ++i) {
      ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
      __ CmpP(r5, Operand(kind));
      T stub(masm->isolate(), kind);
      __ TailCallStub(&stub, eq);
    }

    // If we reached this point there is a problem.
    __ Abort(kUnexpectedElementsKindInArrayConstructor);
  } else {
    UNREACHABLE();
  }
}


static void CreateArrayDispatchOneArgument(MacroAssembler* masm,
                                           AllocationSiteOverrideMode mode) {
  // r4 - allocation site (if mode != DISABLE_ALLOCATION_SITES)
  // r5 - kind (if mode != DISABLE_ALLOCATION_SITES)
  // r2 - number of arguments
  // r3 - constructor?
  // sp[0] - last argument
  Label normal_sequence;
  if (mode == DONT_OVERRIDE) {
    DCHECK(FAST_SMI_ELEMENTS == 0);
    DCHECK(FAST_HOLEY_SMI_ELEMENTS == 1);
    DCHECK(FAST_ELEMENTS == 2);
    DCHECK(FAST_HOLEY_ELEMENTS == 3);
    DCHECK(FAST_DOUBLE_ELEMENTS == 4);
    DCHECK(FAST_HOLEY_DOUBLE_ELEMENTS == 5);

    // is the low bit set? If so, we are holey and that is good.
    __ AndP(r0, r5, Operand(1));
    __ bne(&normal_sequence/*, cr0*/);
  }

  // look at the first argument
  __ LoadP(r7, MemOperand(sp, 0));
  __ CmpP(r7, Operand::Zero());
  __ beq(&normal_sequence);

  if (mode == DISABLE_ALLOCATION_SITES) {
    ElementsKind initial = GetInitialFastElementsKind();
    ElementsKind holey_initial = GetHoleyElementsKind(initial);

    ArraySingleArgumentConstructorStub stub_holey(masm->isolate(),
                                                  holey_initial,
                                                  DISABLE_ALLOCATION_SITES);
    __ TailCallStub(&stub_holey);

    __ bind(&normal_sequence);
    ArraySingleArgumentConstructorStub stub(masm->isolate(),
                                            initial,
                                            DISABLE_ALLOCATION_SITES);
    __ TailCallStub(&stub);
  } else if (mode == DONT_OVERRIDE) {
    // We are going to create a holey array, but our kind is non-holey.
    // Fix kind and retry (only if we have an allocation site in the slot).
    __ AddP(r5, r5, Operand(1));
    if (FLAG_debug_code) {
      __ LoadP(r7, FieldMemOperand(r4, 0));
      __ CompareRoot(r7, Heap::kAllocationSiteMapRootIndex);
      __ Assert(eq, kExpectedAllocationSite);
    }

    // Save the resulting elements kind in type info. We can't just store r5
    // in the AllocationSite::transition_info field because elements kind is
    // restricted to a portion of the field...upper bits need to be left alone.
    STATIC_ASSERT(AllocationSite::ElementsKindBits::kShift == 0);
    __ LoadP(r6, FieldMemOperand(r4, AllocationSite::kTransitionInfoOffset));
    __ AddSmiLiteral(r6, r6, Smi::FromInt(kFastElementsKindPackedToHoley), r0);
    __ StoreP(r6, FieldMemOperand(r4, AllocationSite::kTransitionInfoOffset));

    __ bind(&normal_sequence);
    int last_index = GetSequenceIndexFromFastElementsKind(
        TERMINAL_FAST_ELEMENTS_KIND);
    for (int i = 0; i <= last_index; ++i) {
      ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
      __ CmpP(r5, Operand(kind));
      ArraySingleArgumentConstructorStub stub(masm->isolate(), kind);
      __ TailCallStub(&stub, eq);
    }

    // If we reached this point there is a problem.
    __ Abort(kUnexpectedElementsKindInArrayConstructor);
  } else {
    UNREACHABLE();
  }
}


template<class T>
static void ArrayConstructorStubAheadOfTimeHelper(Isolate* isolate) {
  int to_index = GetSequenceIndexFromFastElementsKind(
      TERMINAL_FAST_ELEMENTS_KIND);
  for (int i = 0; i <= to_index; ++i) {
    ElementsKind kind = GetFastElementsKindFromSequenceIndex(i);
    T stub(isolate, kind);
    stub.GetCode();
    if (AllocationSite::GetMode(kind) != DONT_TRACK_ALLOCATION_SITE) {
      T stub1(isolate, kind, DISABLE_ALLOCATION_SITES);
      stub1.GetCode();
    }
  }
}


void ArrayConstructorStubBase::GenerateStubsAheadOfTime(Isolate* isolate) {
  ArrayConstructorStubAheadOfTimeHelper<ArrayNoArgumentConstructorStub>(
      isolate);
  ArrayConstructorStubAheadOfTimeHelper<ArraySingleArgumentConstructorStub>(
      isolate);
  ArrayConstructorStubAheadOfTimeHelper<ArrayNArgumentsConstructorStub>(
      isolate);
}


void InternalArrayConstructorStubBase::GenerateStubsAheadOfTime(
    Isolate* isolate) {
  ElementsKind kinds[2] = { FAST_ELEMENTS, FAST_HOLEY_ELEMENTS };
  for (int i = 0; i < 2; i++) {
    // For internal arrays we only need a few things
    InternalArrayNoArgumentConstructorStub stubh1(isolate, kinds[i]);
    stubh1.GetCode();
    InternalArraySingleArgumentConstructorStub stubh2(isolate, kinds[i]);
    stubh2.GetCode();
    InternalArrayNArgumentsConstructorStub stubh3(isolate, kinds[i]);
    stubh3.GetCode();
  }
}


void ArrayConstructorStub::GenerateDispatchToArrayStub(
    MacroAssembler* masm,
    AllocationSiteOverrideMode mode) {
  if (argument_count_ == ANY) {
    Label not_zero_case, not_one_case;
    __ CmpP(r2, Operand::Zero());
    __ bne(&not_zero_case);
    CreateArrayDispatch<ArrayNoArgumentConstructorStub>(masm, mode);

    __ bind(&not_zero_case);
    __ CmpP(r2, Operand(1));
    __ bgt(&not_one_case);
    CreateArrayDispatchOneArgument(masm, mode);

    __ bind(&not_one_case);
    CreateArrayDispatch<ArrayNArgumentsConstructorStub>(masm, mode);
  } else if (argument_count_ == NONE) {
    CreateArrayDispatch<ArrayNoArgumentConstructorStub>(masm, mode);
  } else if (argument_count_ == ONE) {
    CreateArrayDispatchOneArgument(masm, mode);
  } else if (argument_count_ == MORE_THAN_ONE) {
    CreateArrayDispatch<ArrayNArgumentsConstructorStub>(masm, mode);
  } else {
    UNREACHABLE();
  }
}


void ArrayConstructorStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : argc (only if argument_count_ == ANY)
  //  -- r3 : constructor
  //  -- r4 : AllocationSite or undefined
  //  -- sp[0] : return address
  //  -- sp[4] : last argument
  // -----------------------------------

  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    // Initial map for the builtin Array function should be a map.
    __ LoadP(r6, FieldMemOperand(r3, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    __ TestIfSmi(r6);
    __ Assert(ne, kUnexpectedInitialMapForArrayFunction, cr0);
    __ CompareObjectType(r6, r6, r7, MAP_TYPE);
    __ Assert(eq, kUnexpectedInitialMapForArrayFunction);

    // We should either have undefined in r4 or a valid AllocationSite
    __ AssertUndefinedOrAllocationSite(r4, r6);
  }

  Label no_info;
  // Get the elements kind and case on that.
  __ CompareRoot(r4, Heap::kUndefinedValueRootIndex);
  __ beq(&no_info);

  __ LoadP(r5, FieldMemOperand(r4, AllocationSite::kTransitionInfoOffset));
  __ SmiUntag(r5);
  STATIC_ASSERT(AllocationSite::ElementsKindBits::kShift == 0);
  __ AndP(r5, Operand(AllocationSite::ElementsKindBits::kMask));
  GenerateDispatchToArrayStub(masm, DONT_OVERRIDE);

  __ bind(&no_info);
  GenerateDispatchToArrayStub(masm, DISABLE_ALLOCATION_SITES);
}


void InternalArrayConstructorStub::GenerateCase(
    MacroAssembler* masm, ElementsKind kind) {
  __ CmpLogicalP(r2, Operand(1));

  InternalArrayNoArgumentConstructorStub stub0(isolate(), kind);
  __ TailCallStub(&stub0, lt);

  InternalArrayNArgumentsConstructorStub stubN(isolate(), kind);
  __ TailCallStub(&stubN, gt);

  if (IsFastPackedElementsKind(kind)) {
    // We might need to create a holey array
    // look at the first argument
    __ LoadP(r5, MemOperand(sp, 0));
    __ CmpP(r5, Operand::Zero());

    InternalArraySingleArgumentConstructorStub
        stub1_holey(isolate(), GetHoleyElementsKind(kind));
    __ TailCallStub(&stub1_holey, ne);
  }

  InternalArraySingleArgumentConstructorStub stub1(isolate(), kind);
  __ TailCallStub(&stub1);
}


void InternalArrayConstructorStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : argc
  //  -- r3 : constructor
  //  -- sp[0] : return address
  //  -- sp[4] : last argument
  // -----------------------------------

  if (FLAG_debug_code) {
    // The array construct code is only set for the global and natives
    // builtin Array functions which always have maps.

    // Initial map for the builtin Array function should be a map.
    __ LoadP(r5, FieldMemOperand(r3, JSFunction::kPrototypeOrInitialMapOffset));
    // Will both indicate a NULL and a Smi.
    __ TestIfSmi(r5);
    __ Assert(ne, kUnexpectedInitialMapForArrayFunction, cr0);
    __ CompareObjectType(r5, r5, r6, MAP_TYPE);
    __ Assert(eq, kUnexpectedInitialMapForArrayFunction);
  }

  // Figure out the right elements kind
  __ LoadP(r5, FieldMemOperand(r3, JSFunction::kPrototypeOrInitialMapOffset));
  // Load the map's "bit field 2" into |result|.
  __ LoadlB(r5, FieldMemOperand(r5, Map::kBitField2Offset));
  // Retrieve elements_kind from bit field 2.
  __ DecodeField<Map::ElementsKindBits>(r5);

  if (FLAG_debug_code) {
    Label done;
    __ CmpP(r5, Operand(FAST_ELEMENTS));
    __ beq(&done);
    __ CmpP(r5, Operand(FAST_HOLEY_ELEMENTS));
    __ Assert(eq,
              kInvalidElementsKindForInternalArrayOrInternalPackedArray);
    __ bind(&done);
  }

  Label fast_elements_case;
  __ CmpP(r5, Operand(FAST_ELEMENTS));
  __ beq(&fast_elements_case);
  GenerateCase(masm, FAST_HOLEY_ELEMENTS);

  __ bind(&fast_elements_case);
  GenerateCase(masm, FAST_ELEMENTS);
}


void CallApiFunctionStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2                  : callee
  //  -- r6                  : call_data
  //  -- r4                  : holder
  //  -- r3                  : api_function_address
  //  -- cp                  : context
  //  --
  //  -- sp[0]               : last argument
  //  -- ...
  //  -- sp[(argc - 1)* 4]   : first argument
  //  -- sp[argc * 4]        : receiver
  // -----------------------------------

  Register callee = r2;
  Register call_data = r6;
  Register holder = r4;
  Register api_function_address = r3;
  Register context = cp;

  int argc = ArgumentBits::decode(bit_field_);
  bool is_store = IsStoreBits::decode(bit_field_);
  bool call_data_undefined = CallDataUndefinedBits::decode(bit_field_);

  typedef FunctionCallbackArguments FCA;

  STATIC_ASSERT(FCA::kContextSaveIndex == 6);
  STATIC_ASSERT(FCA::kCalleeIndex == 5);
  STATIC_ASSERT(FCA::kDataIndex == 4);
  STATIC_ASSERT(FCA::kReturnValueOffset == 3);
  STATIC_ASSERT(FCA::kReturnValueDefaultValueIndex == 2);
  STATIC_ASSERT(FCA::kIsolateIndex == 1);
  STATIC_ASSERT(FCA::kHolderIndex == 0);
  STATIC_ASSERT(FCA::kArgsLength == 7);

  // context save
  __ push(context);
  // load context from callee
  __ LoadP(context, FieldMemOperand(callee, JSFunction::kContextOffset));

  // callee
  __ push(callee);

  // call data
  __ push(call_data);

  Register scratch = call_data;
  if (!call_data_undefined) {
    __ LoadRoot(scratch, Heap::kUndefinedValueRootIndex);
  }
  // return value
  __ push(scratch);
  // return value default
  __ push(scratch);
  // isolate
  __ mov(scratch,
         Operand(ExternalReference::isolate_address(isolate())));
  __ push(scratch);
  // holder
  __ push(holder);

  // Prepare arguments.
  __ LoadRR(scratch, sp);

  // Allocate the v8::Arguments structure in the arguments' space since
  // it's not controlled by GC.
  // S390 LINUX ABI:
  //
  // Create 5 extra slots on stack:
  //    [0] space for DirectCEntryStub's LR save
  //    [1-4] FunctionCallbackInfo
  const int kApiStackSpace = 5;

  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(false, kApiStackSpace);

  DCHECK(!api_function_address.is(r2) && !scratch.is(r2));
  // r2 = FunctionCallbackInfo&
  // Arguments is after the return address.
  __ AddP(r2, sp, Operand((kStackFrameExtraParamSlot + 1) * kPointerSize));
  // FunctionCallbackInfo::implicit_args_
  __ StoreP(scratch, MemOperand(r2, 0 * kPointerSize));
  // FunctionCallbackInfo::values_
  __ AddP(ip, scratch, Operand((FCA::kArgsLength - 1 + argc) * kPointerSize));
  __ StoreP(ip, MemOperand(r2, 1 * kPointerSize));
  // FunctionCallbackInfo::length_ = argc
  __ LoadImmP(ip, Operand(argc));
  __ StoreW(ip, MemOperand(r2, 2 * kPointerSize));
  // FunctionCallbackInfo::is_construct_call = 0
  __ LoadImmP(ip, Operand::Zero());
  __ StoreW(ip, MemOperand(r2, 2 * kPointerSize + kIntSize));

  const int kStackUnwindSpace = argc + FCA::kArgsLength + 1;
  ExternalReference thunk_ref =
      ExternalReference::invoke_function_callback(isolate());

  AllowExternalCallThatCantCauseGC scope(masm);
  MemOperand context_restore_operand(
      fp, (2 + FCA::kContextSaveIndex) * kPointerSize);
  // Stores return the first js argument
  int return_value_offset = 0;
  if (is_store) {
    return_value_offset = 2 + FCA::kArgsLength;
  } else {
    return_value_offset = 2 + FCA::kReturnValueOffset;
  }
  MemOperand return_value_operand(fp, return_value_offset * kPointerSize);

  __ CallApiFunctionAndReturn(api_function_address,
                              thunk_ref,
                              kStackUnwindSpace,
                              return_value_operand,
                              &context_restore_operand);
}


void CallApiGetterStub::Generate(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- sp[0]                  : name
  //  -- sp[4 - kArgsLength*4]  : PropertyCallbackArguments object
  //  -- ...
  //  -- r4                     : api_function_address
  // -----------------------------------

  Register api_function_address = r4;
  __ LoadRR(r2, sp);  // r0 = Handle<Name>

  __ AddP(r3, r2, Operand(1 * kPointerSize));  // r3 = PCA

  // If ABI passes Handles (pointer-sized struct) in a register:
  //
  // Create 2 extra slots on stack:
  //    [0] space for DirectCEntryStub's LR save
  //    [1] AccessorInfo&
  //
  // Otherwise:
  //
  // Create 3 extra slots on stack:
  //    [0] space for DirectCEntryStub's LR save
  //    [1] copy of Handle (first arg)
  //    [2] AccessorInfo&
#if ABI_PASSES_HANDLES_IN_REGS
  const int kAccessorInfoSlot = kStackFrameExtraParamSlot + 1;
  const int kApiStackSpace = 2;
#else
  const int kArg0Slot = kStackFrameExtraParamSlot + 1;
  const int kAccessorInfoSlot = kArg0Slot + 1;
  const int kApiStackSpace = 3;
#endif

  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(false, kApiStackSpace);

#if !ABI_PASSES_HANDLES_IN_REGS
  // pass 1st arg by reference
  __ StoreP(r2,
            MemOperand(sp, kArg0Slot * kPointerSize));
  __ AddP(r2, sp, Operand(kArg0Slot * kPointerSize));
#endif

  // Create PropertyAccessorInfo instance on the stack above the exit frame with
  // r3 (internal::Object** args_) as the data.
  __ StoreP(r3, MemOperand(sp, kAccessorInfoSlot * kPointerSize));
  // r3 = AccessorInfo&
  __ AddP(r3, sp, Operand(kAccessorInfoSlot * kPointerSize));

  const int kStackUnwindSpace = PropertyCallbackArguments::kArgsLength + 1;

  ExternalReference thunk_ref =
      ExternalReference::invoke_accessor_getter_callback(isolate());
  __ CallApiFunctionAndReturn(api_function_address,
                              thunk_ref,
                              kStackUnwindSpace,
                              MemOperand(fp, 6 * kPointerSize),
                              NULL);
}


#undef __
} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_S390
