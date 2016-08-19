// Copyright 2012 the V8 project authors. All rights reserved.
//
// Copyright IBM Corp. 2012-2014. All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_S390

#include "src/codegen.h"
#include "src/debug.h"
#include "src/deoptimizer.h"
#include "src/full-codegen.h"
#include "src/runtime.h"
#include "src/stub-cache.h"

namespace v8 {
namespace internal {


#define __ ACCESS_MASM(masm)

void Builtins::Generate_Adaptor(MacroAssembler* masm,
                                CFunctionId id,
                                BuiltinExtraArguments extra_args) {
  // ----------- S t a t e -------------
  //  -- r2                 : number of arguments excluding receiver
  //  -- r3                 : called function (only guaranteed when
  //                          extra_args requires it)
  //  -- cp                 : context
  //  -- sp[0]              : last argument
  //  -- ...
  //  -- sp[4 * (argc - 1)] : first argument (argc == r0)
  //  -- sp[4 * argc]       : receiver
  // -----------------------------------

  // Insert extra arguments.
  int num_extra_args = 0;
  if (extra_args == NEEDS_CALLED_FUNCTION) {
    num_extra_args = 1;
    __ push(r3);
  } else {
    DCHECK(extra_args == NO_EXTRA_ARGUMENTS);
  }

  // JumpToExternalReference expects r0 to contain the number of arguments
  // including the receiver and the extra arguments.
  __ AddP(r2, Operand(num_extra_args + 1));
  __ JumpToExternalReference(ExternalReference(id, masm->isolate()));
}


// Load the built-in InternalArray function from the current context.
static void GenerateLoadInternalArrayFunction(MacroAssembler* masm,
                                              Register result) {
  // Load the native context.

  __ LoadP(result,
           MemOperand(cp, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  __ LoadP(result,
           FieldMemOperand(result, GlobalObject::kNativeContextOffset));
  // Load the InternalArray function from the native context.
  __ LoadP(result,
           MemOperand(result,
                      Context::SlotOffset(
                        Context::INTERNAL_ARRAY_FUNCTION_INDEX)));
}


// Load the built-in Array function from the current context.
static void GenerateLoadArrayFunction(MacroAssembler* masm, Register result) {
  // Load the native context.

  __ LoadP(result,
           MemOperand(cp, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  __ LoadP(result,
           FieldMemOperand(result, GlobalObject::kNativeContextOffset));
  // Load the Array function from the native context.
  __ LoadP(result,
           MemOperand(result,
                      Context::SlotOffset(Context::ARRAY_FUNCTION_INDEX)));
}


void Builtins::Generate_InternalArrayCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2     : number of arguments
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_array_code, one_or_more_arguments, two_or_more_arguments;

  // Get the InternalArray function.
  GenerateLoadInternalArrayFunction(masm, r3);

  if (FLAG_debug_code) {
    // Initial map for the builtin InternalArray functions should be maps.
    __ LoadP(r4,
        FieldMemOperand(r3, JSFunction::kPrototypeOrInitialMapOffset));
    __ TestIfSmi(r4);
    __ Assert(ne, kUnexpectedInitialMapForInternalArrayFunction, cr0);
    __ CompareObjectType(r4, r5, r6, MAP_TYPE);
    __ Assert(eq, kUnexpectedInitialMapForInternalArrayFunction);
  }

  // Run the native code for the InternalArray function called as a normal
  // function.
  // tail call a stub
  InternalArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}


void Builtins::Generate_ArrayCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2     : number of arguments
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------
  Label generic_array_code, one_or_more_arguments, two_or_more_arguments;

  // Get the Array function.
  GenerateLoadArrayFunction(masm, r3);

  if (FLAG_debug_code) {
    // Initial map for the builtin Array functions should be maps.
    __ LoadP(r4,
        FieldMemOperand(r3, JSFunction::kPrototypeOrInitialMapOffset));
    __ TestIfSmi(r4);
    __ Assert(ne, kUnexpectedInitialMapForArrayFunction, cr0);
    __ CompareObjectType(r4, r5, r6, MAP_TYPE);
    __ Assert(eq, kUnexpectedInitialMapForArrayFunction);
  }

  // Run the native code for the Array function called as a normal function.
  // tail call a stub
  __ LoadRoot(r4, Heap::kUndefinedValueRootIndex);
  ArrayConstructorStub stub(masm->isolate());
  __ TailCallStub(&stub);
}


void Builtins::Generate_StringConstructCode(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2                     : number of arguments
  //  -- r3                     : constructor function
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero based)
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->string_ctor_calls(), 1, r4, r5);

  Register function = r3;
  if (FLAG_debug_code) {
    __ LoadGlobalFunction(Context::STRING_FUNCTION_INDEX, r4);
    __ CmpP(function, r4);
    __ Assert(eq, kUnexpectedStringFunction);
  }

  // Load the first arguments in r2 and get rid of the rest.
  Label no_arguments;
  __ CmpP(r2, Operand::Zero());
  __ beq(&no_arguments);
  // First args = sp[(argc - 1) * 4].
  __ SubP(r2, Operand(1));
  __ ShiftLeftP(r2, r2, Operand(kPointerSizeLog2));
  __ la(sp, MemOperand(sp, r2));
  __ LoadP(r2, MemOperand(sp));
  // sp now point to args[0], drop args[0] + receiver.
  __ Drop(2);

  Register argument = r4;
  Label not_cached, argument_is_string;
  __ LookupNumberStringCache(r2,        // Input.
                             argument,  // Result.
                             r5,        // Scratch.
                             r6,        // Scratch.
                             r7,        // Scratch.
                             &not_cached);
  __ IncrementCounter(counters->string_ctor_cached_number(), 1, r5, r6);
  __ bind(&argument_is_string);

  // ----------- S t a t e -------------
  //  -- r4     : argument converted to string
  //  -- r3     : constructor function
  //  -- lr     : return address
  // -----------------------------------

  Label gc_required;
  __ Allocate(JSValue::kSize,
              r2,  // Result.
              r5,  // Scratch.
              r6,  // Scratch.
              &gc_required,
              TAG_OBJECT);

  // Initialising the String Object.
  Register map = r5;
  __ LoadGlobalFunctionInitialMap(function, map, r6);
  if (FLAG_debug_code) {
    __ LoadlB(r6, FieldMemOperand(map, Map::kInstanceSizeOffset));
    __ CmpP(r6, Operand(JSValue::kSize >> kPointerSizeLog2));
    __ Assert(eq, kUnexpectedStringWrapperInstanceSize);
    __ LoadlB(r6, FieldMemOperand(map, Map::kUnusedPropertyFieldsOffset));
    __ CmpP(r6, Operand::Zero());
    __ Assert(eq, kUnexpectedUnusedPropertiesOfStringWrapper);
  }
  __ StoreP(map, FieldMemOperand(r2, HeapObject::kMapOffset));

  __ LoadRoot(r5, Heap::kEmptyFixedArrayRootIndex);
  __ StoreP(r5, FieldMemOperand(r2, JSObject::kPropertiesOffset));
  __ StoreP(r5, FieldMemOperand(r2, JSObject::kElementsOffset));

  __ StoreP(argument, FieldMemOperand(r2, JSValue::kValueOffset));

  // Ensure the object is fully initialized.
  STATIC_ASSERT(JSValue::kSize == 4 * kPointerSize);

  __ Ret();

  // The argument was not found in the number to string cache. Check
  // if it's a string already before calling the conversion builtin.
  Label convert_argument;
  __ bind(&not_cached);
  __ JumpIfSmi(r2, &convert_argument);

  // Is it a String?
  __ LoadP(r4, FieldMemOperand(r2, HeapObject::kMapOffset));
  __ LoadlB(r5, FieldMemOperand(r4, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kNotStringTag != 0);
  __ mov(r0, Operand(kIsNotStringMask));
  __ AndP(r0, r5);
  __ bne(&convert_argument, Label::kNear);
  __ LoadRR(argument, r2);
  __ IncrementCounter(counters->string_ctor_conversions(), 1, r5, r6);
  __ b(&argument_is_string);

  // Invoke the conversion builtin and put the result into r4.
  __ bind(&convert_argument);
  __ push(function);  // Preserve the function.
  __ IncrementCounter(counters->string_ctor_conversions(), 1, r5, r6);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ push(r2);
    __ InvokeBuiltin(Builtins::TO_STRING, CALL_FUNCTION);
  }
  __ pop(function);
  __ LoadRR(argument, r2);
  __ b(&argument_is_string);

  // Load the empty string into r4, remove the receiver from the
  // stack, and jump back to the case where the argument is a string.
  __ bind(&no_arguments);
  __ LoadRoot(argument, Heap::kempty_stringRootIndex);
  __ Drop(1);
  __ b(&argument_is_string);

  // At this point the argument is already a string. Call runtime to
  // create a string wrapper.
  __ bind(&gc_required);
  __ IncrementCounter(counters->string_ctor_gc_required(), 1, r5, r6);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ push(argument);
    __ CallRuntime(Runtime::kNewStringWrapper, 1);
  }
  __ Ret();
}


static void CallRuntimePassFunction(
    MacroAssembler* masm, Runtime::FunctionId function_id) {
  FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
  // Push a copy of the function onto the stack.
  // Push function as parameter to the runtime call.
  __ Push(r3, r3);

  __ CallRuntime(function_id, 1);
  // Restore reciever.
  __ Pop(r3);
}


static void GenerateTailCallToSharedCode(MacroAssembler* masm) {
  __ LoadP(ip, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
  __ LoadP(ip, FieldMemOperand(ip, SharedFunctionInfo::kCodeOffset));
  __ AddP(ip, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(ip);
}


static void GenerateTailCallToReturnedCode(MacroAssembler* masm) {
  __ AddP(ip, r2, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ JumpToJSEntry(ip);
}


void Builtins::Generate_InOptimizationQueue(MacroAssembler* masm) {
  // Checking whether the queued function is ready for install is optional,
  // since we come across interrupts and stack checks elsewhere.  However,
  // not checking may delay installing ready functions, and always checking
  // would be quite expensive.  A good compromise is to first check against
  // stack limit as a cue for an interrupt signal.
  Label ok;
  __ CmpLogicalP(sp, RootMemOperand(Heap::kStackLimitRootIndex));
  __ bge(&ok, Label::kNear);

  CallRuntimePassFunction(masm, Runtime::kTryInstallOptimizedCode);
  GenerateTailCallToReturnedCode(masm);

  __ bind(&ok);
  GenerateTailCallToSharedCode(masm);
}


static void Generate_JSConstructStubHelper(MacroAssembler* masm,
                                           bool is_api_function,
                                           bool create_memento) {
  // ----------- S t a t e -------------
  //  -- r2     : number of arguments
  //  -- r3     : constructor function
  //  -- r4     : allocation site or undefined
  //  -- lr     : return address
  //  -- sp[...]: constructor arguments
  // -----------------------------------

  // Should never create mementos for api functions.
  DCHECK(!is_api_function || !create_memento);

  Isolate* isolate = masm->isolate();

  // Enter a construct frame.
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::CONSTRUCT);

    if (create_memento) {
      __ AssertUndefinedOrAllocationSite(r4, r5);
      __ push(r4);
    }

    // Preserve the two incoming parameters on the stack.
    __ SmiTag(r2);
    __ push(r2);  // Smi-tagged arguments count.
    __ push(r3);  // Constructor function.

    // Try to allocate the object without transitioning into C code. If any of
    // the preconditions is not met, the code bails out to the runtime call.
    Label rt_call, allocated;
    if (FLAG_inline_new) {
      Label undo_allocation;
      ExternalReference debug_step_in_fp =
          ExternalReference::debug_step_in_fp_address(isolate);
      __ mov(r4, Operand(debug_step_in_fp));
      __ LoadP(r4, MemOperand(r4));
      __ CmpP(r4, Operand::Zero());
      __ bne(&rt_call);

      // Load the initial map and verify that it is in fact a map.
      // r3: constructor function
      __ LoadP(r4, FieldMemOperand(r3,
                                   JSFunction::kPrototypeOrInitialMapOffset));
      __ JumpIfSmi(r4, &rt_call);
      __ CompareObjectType(r4, r5, r6, MAP_TYPE);
      __ bne(&rt_call);

      // Check that the constructor is not constructing a JSFunction (see
      // comments in Runtime_NewObject in runtime.cc). In which case the
      // initial map's instance type would be JS_FUNCTION_TYPE.
      // r3: constructor function
      // r4: initial map
      __ CompareInstanceType(r4, r5, JS_FUNCTION_TYPE);
      __ beq(&rt_call);

      if (!is_api_function) {
        Label allocate;
        MemOperand bit_field3 = FieldMemOperand(r4, Map::kBitField3Offset);
        // Check if slack tracking is enabled.
        __ LoadlW(r6, bit_field3);
        __ DecodeField<Map::ConstructionCount>(r5, r6);
        STATIC_ASSERT(JSFunction::kNoSlackTracking == 0);
        __ CmpP(r5, Operand::Zero());  // JSFunction::kNoSlackTracking
        __ beq(&allocate);
        // Decrease generous allocation count.
        __ AddP(r6, Operand(-(1 << Map::ConstructionCount::kShift)));
        __ StoreW(r6, bit_field3);
        __ CmpP(r5, Operand(JSFunction::kFinishSlackTracking));
        __ bne(&allocate);

        __ push(r3);

        __ Push(r4, r3);  // r3 = constructor
        __ CallRuntime(Runtime::kFinalizeInstanceSize, 1);

        __ Pop(r3, r4);

        __ bind(&allocate);
      }

      // Now allocate the JSObject on the heap.
      // r3: constructor function
      // r4: initial map
      __ LoadlB(r5, FieldMemOperand(r4, Map::kInstanceSizeOffset));
      if (create_memento) {
        __ AddP(r5, Operand(AllocationMemento::kSize / kPointerSize));
      }

      __ Allocate(r5, r6, r7, r8, &rt_call, SIZE_IN_WORDS);

      // Allocated the JSObject, now initialize the fields. Map is set to
      // initial map and properties and elements are set to empty fixed array.
      // r3: constructor function
      // r4: initial map
      // r5: object size (not including memento if create_memento)
      // r6: JSObject (not tagged)
      __ LoadRoot(r8, Heap::kEmptyFixedArrayRootIndex);
      __ StoreP(r4, MemOperand(r6, JSObject::kMapOffset));
      __ StoreP(r8, MemOperand(r6, JSObject::kPropertiesOffset));
      __ StoreP(r8, MemOperand(r6, JSObject::kElementsOffset));
      __ la(r7, MemOperand(r6, 3 * kPointerSize));

      __ ShiftLeftP(r8, r5, Operand(kPointerSizeLog2));
      __ AddP(r8, r6);  // End of object.

      // Fill all the in-object properties with the appropriate filler.
      // r3: constructor function
      // r4: initial map
      // r5: object size (in words, including memento if create_memento)
      // r6: JSObject (not tagged)
      // r7: First in-object property of JSObject (not tagged)
      // r8: End of object
      DCHECK_EQ(3 * kPointerSize, JSObject::kHeaderSize);
      __ LoadRoot(r9, Heap::kUndefinedValueRootIndex);

      if (!is_api_function) {
        Label no_inobject_slack_tracking;

        // Check if slack tracking is enabled.
        STATIC_ASSERT(JSFunction::kNoSlackTracking == 0);
        __ LoadlW(ip, FieldMemOperand(r4, Map::kBitField3Offset));
        __ DecodeField<Map::ConstructionCount>(ip);
        __ CmpP(ip, Operand::Zero());  // JSFunction::kNoSlackTracking
        __ beq(&no_inobject_slack_tracking);

        // Allocate object with a slack.
        __ LoadlB(r2, FieldMemOperand(r4,
                                      Map::kPreAllocatedPropertyFieldsOffset));
        if (FLAG_debug_code) {
          __ ShiftLeftP(r0, r2, Operand(kPointerSizeLog2));
          __ AddP(r0, r7);
          // r0: offset of first field after pre-allocated fields
          __ CmpP(r0, r8);
          __ Assert(le, kUnexpectedNumberOfPreAllocatedPropertyFields);
        }
        { Label done;
          __ CmpP(r2, Operand::Zero());
          __ beq(&done);
          __ InitializeNFieldsWithFiller(r7, r2, r9);
          __ bind(&done);
        }
        // To allow for truncation.
        __ LoadRoot(r9, Heap::kOnePointerFillerMapRootIndex);
        // Fill the remaining fields with one pointer filler map.
        __ bind(&no_inobject_slack_tracking);
      }

      if (create_memento) {
        __ SubP(r2, r8, Operand(AllocationMemento::kSize));
        __ InitializeFieldsWithFiller(r7, r2, r9);

        // Fill in memento fields.
        // r7: points to the allocated but uninitialized memento.
        __ LoadRoot(r9, Heap::kAllocationMementoMapRootIndex);
        __ StoreP(r9, MemOperand(r7, AllocationMemento::kMapOffset));
        // Load the AllocationSite
        __ LoadP(r9, MemOperand(sp, 2 * kPointerSize));
        __ StoreP(r9,
                  MemOperand(r7, AllocationMemento::kAllocationSiteOffset));
        __ AddP(r7, Operand(AllocationMemento::kAllocationSiteOffset +
                                kPointerSize));
      } else {
        __ InitializeFieldsWithFiller(r7, r8, r9);
      }

      // Add the object tag to make the JSObject real, so that we can continue
      // and jump into the continuation code at any time from now on. Any
      // failures need to undo the allocation, so that the heap is in a
      // consistent state and verifiable.
      __ AddP(r6, Operand(kHeapObjectTag));

      // Check if a non-empty properties array is needed. Continue with
      // allocated object if not fall through to runtime call if it is.
      // r3: constructor function
      // r6: JSObject
      // r7: start of next object (not tagged)
      __ LoadlB(r5, FieldMemOperand(r4, Map::kUnusedPropertyFieldsOffset));
      // The field instance sizes contains both pre-allocated property fields
      // and in-object properties.
      __ LoadlB(r0, FieldMemOperand(r4,
                                    Map::kPreAllocatedPropertyFieldsOffset));
      __ AddP(r5, r0);
      __ LoadlB(r0, FieldMemOperand(r4, Map::kInObjectPropertiesOffset));
      __ SubP(r5, r5, r0);

      // Done if no extra properties are to be allocated.
      __ beq(&allocated);
      __ Assert(ge, kPropertyAllocationCountFailed, cr0);

      // Scale the number of elements by pointer size and add the header for
      // FixedArrays to the start of the next object calculation from above.
      // r3: constructor
      // r5: number of elements in properties array
      // r6: JSObject
      // r7: start of next object
      __ AddP(r2, r5, Operand(FixedArray::kHeaderSize / kPointerSize));
      __ Allocate(
          r2,
          r7,
          r8,
          r4,
          &undo_allocation,
          static_cast<AllocationFlags>(RESULT_CONTAINS_TOP | SIZE_IN_WORDS));

      // Initialize the FixedArray.
      // r3: constructor
      // r5: number of elements in properties array
      // r6: JSObject
      // r7: FixedArray (not tagged)
      __ LoadRoot(r8, Heap::kFixedArrayMapRootIndex);
      __ LoadRR(r4, r7);
      DCHECK_EQ(0 * kPointerSize, JSObject::kMapOffset);
      __ StoreP(r8, MemOperand(r4));
      DCHECK_EQ(1 * kPointerSize, FixedArray::kLengthOffset);
      __ SmiTag(r2, r5);
      __ StoreP(r2, MemOperand(r4, kPointerSize));
      __ AddP(r4, Operand(2 * kPointerSize));

      // Initialize the fields to undefined.
      // r3: constructor function
      // r4: First element of FixedArray (not tagged)
      // r5: number of elements in properties array
      // r6: JSObject
      // r7: FixedArray (not tagged)
      DCHECK_EQ(2 * kPointerSize, FixedArray::kHeaderSize);
      { Label done;
        __ CmpP(r5, Operand::Zero());
        __ beq(&done);
        if (!is_api_function || create_memento) {
          __ LoadRoot(r9, Heap::kUndefinedValueRootIndex);
        } else if (FLAG_debug_code) {
          __ CompareRoot(r9, Heap::kUndefinedValueRootIndex);
          __ Assert(eq, kUndefinedValueNotLoaded);
        }
        __ InitializeNFieldsWithFiller(r4, r5, r9);
        __ bind(&done);
      }

      // Store the initialized FixedArray into the properties field of
      // the JSObject
      // r3: constructor function
      // r6: JSObject
      // r7: FixedArray (not tagged)
      __ AddP(r7, Operand(kHeapObjectTag));  // Add the heap tag.
      __ StoreP(r7, FieldMemOperand(r6, JSObject::kPropertiesOffset));

      // Continue with JSObject being successfully allocated
      // r3: constructor function
      // r6: JSObject
      __ b(&allocated);

      // Undo the setting of the new top so that the heap is verifiable. For
      // example, the map's unused properties potentially do not match the
      // allocated objects unused properties.
      // r6: JSObject (previous new top)
      __ bind(&undo_allocation);
      __ UndoAllocationInNewSpace(r6, r7);
    }

    // Allocate the new receiver object using the runtime call.
    // r3: constructor function
    __ bind(&rt_call);
    if (create_memento) {
      // Get the cell or allocation site.
      __ LoadP(r4, MemOperand(sp, 2 * kPointerSize));
      __ push(r4);
    }

    __ push(r3);  // argument for Runtime_NewObject
    if (create_memento) {
      __ CallRuntime(Runtime::kNewObjectWithAllocationSite, 2);
    } else {
      __ CallRuntime(Runtime::kNewObject, 1);
    }
    __ LoadRR(r6, r2);

    // If we ended up using the runtime, and we want a memento, then the
    // runtime call made it for us, and we shouldn't do create count
    // increment.
    Label count_incremented;
    if (create_memento) {
      __ b(&count_incremented, Label::kNear);
    }

    // Receiver for constructor call allocated.
    // r6: JSObject
    __ bind(&allocated);

    if (create_memento) {
      __ LoadP(r4, MemOperand(sp, kPointerSize * 2));
      __ CompareRoot(r4, Heap::kUndefinedValueRootIndex);
      __ beq(&count_incremented);
      // r4 is an AllocationSite. We are creating a memento from it, so we
      // need to increment the memento create count.
      __ LoadP(r5, FieldMemOperand(r4,
                                AllocationSite::kPretenureCreateCountOffset));
      __ AddSmiLiteral(r5, r5, Smi::FromInt(1), r0);
      __ StoreP(r5, FieldMemOperand(r4,
                                AllocationSite::kPretenureCreateCountOffset),
                r0);
      __ bind(&count_incremented);
    }

    __ Push(r6, r6);

    // Reload the number of arguments and the constructor from the stack.
    // sp[0]: receiver
    // sp[1]: receiver
    // sp[2]: constructor function
    // sp[3]: number of arguments (smi-tagged)
    __ LoadP(r3, MemOperand(sp, 2 * kPointerSize));
    __ LoadP(r5, MemOperand(sp, 3 * kPointerSize));

    // Set up pointer to last argument.
    __ la(r4, MemOperand(fp, StandardFrameConstants::kCallerSPOffset));

    // Set up number of arguments for function call below
    __ SmiUntag(r2, r5);

    // Copy arguments and receiver to the expression stack.
    // r2: number of arguments
    // r3: constructor function
    // r4: address of last argument (caller sp)
    // r5: number of arguments (smi-tagged)
    // sp[0]: receiver
    // sp[1]: receiver
    // sp[2]: constructor function
    // sp[3]: number of arguments (smi-tagged)
    Label loop, no_args;
    __ CmpP(r2, Operand::Zero());
    __ beq(&no_args, Label::kNear);
    __ ShiftLeftP(ip, r2, Operand(kPointerSizeLog2));
    __ bind(&loop);
    __ SubP(ip, Operand(kPointerSize));
    __ LoadP(r0, MemOperand(r4, ip));
    __ push(r0);
    __ BranchOnCount(r2, &loop);
    __ SmiUntag(r2, r5);
    __ bind(&no_args);

    // Call the function.
    // r2: number of arguments
    // r3: constructor function
    if (is_api_function) {
      __ LoadP(cp, FieldMemOperand(r3, JSFunction::kContextOffset));
      Handle<Code> code =
          masm->isolate()->builtins()->HandleApiCallConstruct();
      __ Call(code, RelocInfo::CODE_TARGET);
    } else {
      ParameterCount actual(r2);
      __ InvokeFunction(r3, actual, CALL_FUNCTION, NullCallWrapper());
    }

    // Store offset of return address for deoptimizer.
    if (!is_api_function) {
      masm->isolate()->heap()->SetConstructStubDeoptPCOffset(masm->pc_offset());
    }

    // Restore context from the frame.
    // r2: result
    // sp[0]: receiver
    // sp[1]: constructor function
    // sp[2]: number of arguments (smi-tagged)
    __ LoadP(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));

    // If the result is an object (in the ECMA sense), we should get rid
    // of the receiver and use the result; see ECMA-262 section 13.2.2-7
    // on page 74.
    Label use_receiver, exit;

    // If the result is a smi, it is *not* an object in the ECMA sense.
    // r2: result
    // sp[0]: receiver (newly allocated object)
    // sp[1]: constructor function
    // sp[2]: number of arguments (smi-tagged)
    __ JumpIfSmi(r2, &use_receiver);

    // If the type of the result (stored in its map) is less than
    // FIRST_SPEC_OBJECT_TYPE, it is not an object in the ECMA sense.
    __ CompareObjectType(r2, r3, r5, FIRST_SPEC_OBJECT_TYPE);
    __ bge(&exit, Label::kNear);

    // Throw away the result of the constructor invocation and use the
    // on-stack receiver as the result.
    __ bind(&use_receiver);
    __ LoadP(r2, MemOperand(sp));

    // Remove receiver from the stack, remove caller arguments, and
    // return.
    __ bind(&exit);
    // r2: result
    // sp[0]: receiver (newly allocated object)
    // sp[1]: constructor function
    // sp[2]: number of arguments (smi-tagged)
    __ LoadP(r3, MemOperand(sp, 2 * kPointerSize));

    // Leave construct frame.
  }

  __ SmiToPtrArrayOffset(r3, r3);
  __ la(sp, MemOperand(sp, r3, kPointerSize));
  __ IncrementCounter(isolate->counters()->constructed_objects(),
                      1, r3, r4);
  __ Ret();
}


void Builtins::Generate_JSConstructStubGeneric(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, false, FLAG_pretenuring_call_new);
}


void Builtins::Generate_JSConstructStubApi(MacroAssembler* masm) {
  Generate_JSConstructStubHelper(masm, true, false);
}


static void Generate_JSEntryTrampolineHelper(MacroAssembler* masm,
                                             bool is_construct) {
  // Called from Generate_JS_Entry
  // r2: code entry
  // r3: function
  // r4: receiver
  // r5: argc
  // r6: argv
  // r0,r7-r9, cp may be clobbered
  ProfileEntryHookStub::MaybeCallEntryHook(masm);

  // Clear the context before we push it when entering the internal frame.
  __ LoadImmP(cp, Operand::Zero());

  // Enter an internal frame.
  {
    // FrameScope ends up calling MacroAssembler::EnterFrame here
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Set up the context from the function argument.
    __ LoadP(cp, FieldMemOperand(r3, JSFunction::kContextOffset));

    __ InitializeRootRegister();

    // Push the function and the receiver onto the stack.
    __ lay(sp, MemOperand(sp, -2 * kPointerSize));
    __ StoreP(r3, MemOperand(sp, 1 * kPointerSize));
    __ StoreP(r4, MemOperand(sp, 0 * kPointerSize));

    // Copy arguments to the stack in a loop from argv to sp.
    // The arguments are actually placed in reverse order on sp
    // compared to argv (i.e. arg1 is highest memory in sp).
    // r3: function
    // r5: argc
    // r6: argv, i.e. points to first arg
    // r7: scratch reg to hold scaled argc
    // r8: scratch reg to hold arg handle
    // r9: scratch reg to hold index into argv
    Label argLoop, argExit;
    intptr_t zero = 0;
    __ ShiftLeftP(r7, r5, Operand(kPointerSizeLog2));
    __ SubRR(sp, r7);    // Buy the stack frame to fit args
    __ LoadImmP(r9, Operand(zero));  // Initialize argv index
    __ bind(&argLoop);
    __ CmpPH(r7, Operand(zero));
    __ beq(&argExit, Label::kNear);
    __ lay(r7, MemOperand(r7, -kPointerSize));
    __ LoadP(r8, MemOperand(r9, r6));  // read next parameter
    __ la(r9, MemOperand(r9, kPointerSize));  // r9++;
    __ LoadP(r0, MemOperand(r8));  // dereference handle
    __ StoreP(r0, MemOperand(r7, sp));  // push parameter
    __ b(&argLoop);
    __ bind(&argExit);

    // Initialize all JavaScript callee-saved registers, since they will be seen
    // by the garbage collector as part of handlers.
    __ LoadRoot(r6, Heap::kUndefinedValueRootIndex);
    __ LoadRR(r7, r6);
    __ LoadRR(r8, r6);
    __ LoadRR(r9, r6);

    // Invoke the code and pass argc as r2.
    __ LoadRR(r2, r5);
    if (is_construct) {
      // No type feedback cell is available
      __ LoadRoot(r4, Heap::kUndefinedValueRootIndex);
      CallConstructStub stub(masm->isolate(), NO_CALL_CONSTRUCTOR_FLAGS);
      __ CallStub(&stub);
    } else {
      ParameterCount actual(r2);
      __ InvokeFunction(r3, actual, CALL_FUNCTION, NullCallWrapper());
    }
    // Exit the JS frame and remove the parameters (except function), and
    // return.
  }
  __ b(r14);

  // r2: result
}


void Builtins::Generate_JSEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, false);
}


void Builtins::Generate_JSConstructEntryTrampoline(MacroAssembler* masm) {
  Generate_JSEntryTrampolineHelper(masm, true);
}


void Builtins::Generate_CompileUnoptimized(MacroAssembler* masm) {
  CallRuntimePassFunction(masm, Runtime::kCompileUnoptimized);
  GenerateTailCallToReturnedCode(masm);
}


static void CallCompileOptimized(MacroAssembler* masm, bool concurrent) {
  FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    // Push a copy of the function onto the stack.
    __ lay(sp, MemOperand(sp, -2 * kPointerSize));
    __ StoreP(r3, MemOperand(sp, 1 * kPointerSize));
    // Push function as parameter to the runtime call.
    __ StoreP(r3, MemOperand(sp, 0 * kPointerSize));
  // Whether to compile in a background thread.
  __ Push(masm->isolate()->factory()->ToBoolean(concurrent));
  __ CallRuntime(Runtime::kCompileOptimized, 2);

  // Restore receiver.
    __ LoadP(r3, MemOperand(sp, 0));
    __ la(sp, MemOperand(sp, kPointerSize));
}


void Builtins::Generate_CompileOptimized(MacroAssembler* masm) {
  CallCompileOptimized(masm, false);
  GenerateTailCallToReturnedCode(masm);
}


void Builtins::Generate_CompileOptimizedConcurrent(MacroAssembler* masm) {
  CallCompileOptimized(masm, true);
  GenerateTailCallToReturnedCode(masm);
}


static void GenerateMakeCodeYoungAgainCommon(MacroAssembler* masm) {
  // For now, we are relying on the fact that make_code_young doesn't do any
  // garbage collection which allows us to save/restore the registers without
  // worrying about which of them contain pointers. We also don't build an
  // internal frame to make the code faster, since we shouldn't have to do stack
  // crawls in MakeCodeYoung. This seems a bit fragile.

  // Point r2 at the start of the PlatformCodeAge sequence.
  __ LoadRR(r2, ip);
  // The following registers must be saved and restored when calling through to
  // the runtime:
  //   r2 - contains return address (beginning of patch sequence)
  //   r3 - isolate
  //   ir - return address
  FrameScope scope(masm, StackFrame::MANUAL);
  __ CleanseP(r14);
  __ LoadRR(r0, r14);
#ifdef V8_OS_ZOS
  __ MultiPush(r0.bit() | r1.bit() | r2.bit() | r3.bit() | fp.bit());
#else
  __ MultiPush(r0.bit() | r2.bit() | r3.bit() | fp.bit());
#endif
  __ PrepareCallCFunction(2, 0, r4);
  __ mov(r3, Operand(ExternalReference::isolate_address(masm->isolate())));
#ifdef V8_OS_ZOS
  __ LoadRR(r1, r2);
  __ LoadRR(r2, r3);
#endif  
  __ CallCFunction(
      ExternalReference::get_make_code_young_function(masm->isolate()), 2);
#ifdef V8_OS_ZOS
  __ MultiPop(r0.bit() | r1.bit() | r2.bit() | r3.bit() | fp.bit());
#else
  __ MultiPop(r0.bit() | r2.bit() | r3.bit() | fp.bit());
#endif
  __ LoadRR(r14, r0);
  __ LoadRR(ip, r2);
  __ Jump(ip);
}

#define DEFINE_CODE_AGE_BUILTIN_GENERATOR(C)                 \
void Builtins::Generate_Make##C##CodeYoungAgainEvenMarking(  \
    MacroAssembler* masm) {                                  \
  GenerateMakeCodeYoungAgainCommon(masm);                    \
}                                                            \
void Builtins::Generate_Make##C##CodeYoungAgainOddMarking(   \
    MacroAssembler* masm) {                                  \
  GenerateMakeCodeYoungAgainCommon(masm);                    \
}
CODE_AGE_LIST(DEFINE_CODE_AGE_BUILTIN_GENERATOR)
#undef DEFINE_CODE_AGE_BUILTIN_GENERATOR


void Builtins::Generate_MarkCodeAsExecutedOnce(MacroAssembler* masm) {
  // For now, we are relying on the fact that make_code_young doesn't do any
  // garbage collection which allows us to save/restore the registers without
  // worrying about which of them contain pointers. We also don't build an
  // internal frame to make the code faster, since we shouldn't have to do stack
  // crawls in MakeCodeYoung. This seems a bit fragile.

  // Point r2 at the start of the PlatformCodeAge sequence.
  __ LoadRR(r2, ip);

  // The following registers must be saved and restored when calling through to
  // the runtime:
  //   r2 - contains return address (beginning of patch sequence)
  //   r3 - isolate
  //   ir - return address
  FrameScope scope(masm, StackFrame::MANUAL);
  __ CleanseP(r14);
  __ LoadRR(r0, r14);
#ifdef V8_OS_ZOS
  __ MultiPush(r0.bit() | r1.bit() | r2.bit() | r3.bit() | fp.bit());
#else
  __ MultiPush(r0.bit() | r2.bit() | r3.bit() | fp.bit());
#endif
  
  __ PrepareCallCFunction(2, 0, r4);
  __ mov(r3, Operand(ExternalReference::isolate_address(masm->isolate())));
#ifdef V8_OS_ZOS
  __ LoadRR(r1, r2);
  __ LoadRR(r2, r3);
#endif
  __ CallCFunction(ExternalReference::get_mark_code_as_executed_function(
        masm->isolate()), 2);
#ifdef V8_OS_ZOS
  __ MultiPop(r0.bit() | r1.bit() | r2.bit() | r3.bit() | fp.bit());
#else
  __ MultiPop(r0.bit() | r2.bit() | r3.bit() | fp.bit());
#endif
  __ LoadRR(r14, r0);
  __ LoadRR(ip, r2);

  // Perform prologue operations usually performed by the young code stub.
  __ PushFixedFrame(r3);
  __ la(fp, MemOperand(sp, StandardFrameConstants::kFixedFrameSizeFromFp));

  // Jump to point after the code-age stub.
  __ AddP(r2, ip, Operand(kNoCodeAgeSequenceLength));
  __ Jump(r2);
}


void Builtins::Generate_MarkCodeAsExecutedTwice(MacroAssembler* masm) {
  GenerateMakeCodeYoungAgainCommon(masm);
}


static void Generate_NotifyStubFailureHelper(MacroAssembler* masm,
                                             SaveFPRegsMode save_doubles) {
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);

    // Preserve registers across notification, this is important for compiled
    // stubs that tail call the runtime on deopts passing their parameters in
    // registers.
    __ MultiPush(kJSCallerSaved | kCalleeSaved);
    // Pass the function and deoptimization type to the runtime system.
    __ CallRuntime(Runtime::kNotifyStubFailure, 0, save_doubles);
    __ MultiPop(kJSCallerSaved | kCalleeSaved);
  }

  __ la(sp, MemOperand(sp, kPointerSize));  // Ignore state
  __ Ret();  // Jump to miss handler
}


void Builtins::Generate_NotifyStubFailure(MacroAssembler* masm) {
  Generate_NotifyStubFailureHelper(masm, kDontSaveFPRegs);
}


void Builtins::Generate_NotifyStubFailureSaveDoubles(MacroAssembler* masm) {
  Generate_NotifyStubFailureHelper(masm, kSaveFPRegs);
}


static void Generate_NotifyDeoptimizedHelper(MacroAssembler* masm,
                                             Deoptimizer::BailoutType type) {
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    // Pass the function and deoptimization type to the runtime system.
    __ LoadSmiLiteral(r2, Smi::FromInt(static_cast<int>(type)));
    __ push(r2);
    __ CallRuntime(Runtime::kNotifyDeoptimized, 1);
  }

  // Get the full codegen state from the stack and untag it -> r8.
  __ LoadP(r8, MemOperand(sp, 0 * kPointerSize));
  __ SmiUntag(r8);
  // Switch on the state.
  Label with_tos_register, unknown_state;
  __ CmpP(r8, Operand(FullCodeGenerator::NO_REGISTERS));
  __ bne(&with_tos_register);
  __ la(sp, MemOperand(sp, 1 * kPointerSize));  // Remove state.
  __ Ret();

  __ bind(&with_tos_register);
  __ LoadP(r2, MemOperand(sp, 1 * kPointerSize));
  __ CmpP(r8, Operand(FullCodeGenerator::TOS_REG));
  __ bne(&unknown_state);
  __ la(sp, MemOperand(sp, 2 * kPointerSize));  // Remove state.
  __ Ret();

  __ bind(&unknown_state);
  __ stop("no cases left");
}


void Builtins::Generate_NotifyDeoptimized(MacroAssembler* masm) {
  Generate_NotifyDeoptimizedHelper(masm, Deoptimizer::EAGER);
}


void Builtins::Generate_NotifySoftDeoptimized(MacroAssembler* masm) {
  Generate_NotifyDeoptimizedHelper(masm, Deoptimizer::SOFT);
}


void Builtins::Generate_NotifyLazyDeoptimized(MacroAssembler* masm) {
  Generate_NotifyDeoptimizedHelper(masm, Deoptimizer::LAZY);
}


void Builtins::Generate_OnStackReplacement(MacroAssembler* masm) {
  // Lookup the function in the JavaScript frame.
  __ LoadP(r2, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    // Pass function as argument.
    __ push(r2);
    __ CallRuntime(Runtime::kCompileForOnStackReplacement, 1);
  }

  // If the code object is null, just return to the unoptimized code.
  Label skip;
  __ CmpSmiLiteral(r2, Smi::FromInt(0), r0);
  __ bne(&skip);
  __ Ret();

  __ bind(&skip);

  // Load deoptimization data from the code object.
  // <deopt_data> = <code>[#deoptimization_data_offset]
  __ LoadP(r3, FieldMemOperand(r2, Code::kDeoptimizationDataOffset));

    // Load the OSR entrypoint offset from the deoptimization data.
    // <osr_offset> = <deopt_data>[#header_size + #osr_pc_offset]
    __ LoadP(r3, FieldMemOperand(r3, FixedArray::OffsetOfElementAt(
                                 DeoptimizationInputData::kOsrPcOffsetIndex)));
    __ SmiUntag(r3);

    // Compute the target address = code_obj + header_size + osr_offset
    // <entry_addr> = <code_obj> + #header_size + <osr_offset>
    __ AddP(r2, r3);
    __ AddP(r0, r2, Operand(Code::kHeaderSize - kHeapObjectTag));
    __ LoadRR(r14, r0);

    // And "return" to the OSR entry point of the function.
    __ Ret();
}


void Builtins::Generate_OsrAfterStackCheck(MacroAssembler* masm) {
  // We check the stack limit as indicator that recompilation might be done.
  Label ok;
  __ CmpLogicalP(sp, RootMemOperand(Heap::kStackLimitRootIndex));
  __ bge(&ok, Label::kNear);
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
    __ CallRuntime(Runtime::kStackGuard, 0);
  }
  __ Jump(masm->isolate()->builtins()->OnStackReplacement(),
          RelocInfo::CODE_TARGET);

  __ bind(&ok);
  __ Ret();
}


void Builtins::Generate_FunctionCall(MacroAssembler* masm) {
  // 1. Make sure we have at least one argument.
  // r2: actual number of arguments
  { Label done;
    __ CmpP(r2, Operand::Zero());
    __ bne(&done, Label::kNear);
    __ LoadRoot(r4, Heap::kUndefinedValueRootIndex);
    __ push(r4);
    __ AddP(r2, Operand(1));
    __ bind(&done);
  }

  // 2. Get the function to call (passed as receiver) from the stack, check
  //    if it is a function.
  // r2: actual number of arguments
  Label slow, non_function;
  __ ShiftLeftP(r3, r2, Operand(kPointerSizeLog2));
  __ lay(r3, MemOperand(r3, sp));
  __ LoadP(r3, MemOperand(r3));
  __ JumpIfSmi(r3, &non_function);
  __ CompareObjectType(r3, r4, r4, JS_FUNCTION_TYPE);
  __ bne(&slow);

  // 3a. Patch the first argument if necessary when calling a function.
  // r2: actual number of arguments
  // r3: function
  Label shift_arguments;
  __ LoadImmP(r6, Operand::Zero());  // indicate regular
                                                   // JS_FUNCTION
  { Label convert_to_object, use_global_proxy, patch_receiver;
    // Change context eagerly in case we need the global receiver.
    __ LoadP(cp, FieldMemOperand(r3, JSFunction::kContextOffset));

    // Do not transform the receiver for strict mode functions.
    __ LoadP(r4,
             FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
    __ LoadlW(r5,
              FieldMemOperand(r4, SharedFunctionInfo::kCompilerHintsOffset));
    __ TestBit(r5,
#if V8_TARGET_ARCH_S390X
               SharedFunctionInfo::kStrictModeFunction,
#else
               SharedFunctionInfo::kStrictModeFunction + kSmiTagSize,
#endif
               r0);
    __ bne(&shift_arguments /*, cr0*/);

    // Do not transform the receiver for native (Compilerhints already in r5).
    __ TestBit(r5,
#if V8_TARGET_ARCH_S390X
               SharedFunctionInfo::kNative,
#else
               SharedFunctionInfo::kNative + kSmiTagSize,
#endif
               r0);
    __ bne(&shift_arguments /*, cr0*/);

    // Compute the receiver in sloppy mode.
    __ ShiftLeftP(ip, r2, Operand(kPointerSizeLog2));
    __ lay(r4, MemOperand(sp, ip));
    __ LoadP(r4, MemOperand(r4, -kPointerSize));
    // r2: actual number of arguments
    // r3: function
    // r4: first argument
    __ JumpIfSmi(r4, &convert_to_object);

    __ CompareRoot(r4, Heap::kUndefinedValueRootIndex);
    __ beq(&use_global_proxy, Label::kNear);
    __ CompareRoot(r4, Heap::kNullValueRootIndex);
    __ beq(&use_global_proxy, Label::kNear);

    STATIC_ASSERT(LAST_SPEC_OBJECT_TYPE == LAST_TYPE);
    __ CompareObjectType(r4, r5, r5, FIRST_SPEC_OBJECT_TYPE);
    __ bge(&shift_arguments);

    __ bind(&convert_to_object);

    {
      // Enter an internal frame in order to preserve argument count.
      FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);
      __ SmiTag(r2);
      __ Push(r2, r4);
      __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
      __ LoadRR(r4, r2);

      __ pop(r2);
      __ SmiUntag(r2);

      // Exit the internal frame.
    }

    // Restore the function to r3, and the flag to r6.
    __ ShiftLeftP(r6, r2, Operand(kPointerSizeLog2));
    __ lay(r6, MemOperand(r6, sp));
    __ LoadP(r3, MemOperand(r6));
    __ LoadImmP(r6, Operand::Zero());
    __ b(&patch_receiver, Label::kNear);

    __ bind(&use_global_proxy);
    __ LoadP(r4, ContextOperand(cp, Context::GLOBAL_OBJECT_INDEX));
    __ LoadP(r4, FieldMemOperand(r4, GlobalObject::kGlobalProxyOffset));

    __ bind(&patch_receiver);
    __ ShiftLeftP(ip, r2, Operand(kPointerSizeLog2));
    __ lay(r5, MemOperand(sp, ip));
    __ StoreP(r4, MemOperand(r5, -kPointerSize));

    __ b(&shift_arguments);
  }

  // 3b. Check for function proxy.
  __ bind(&slow);
  __ LoadImmP(r6, Operand(1, kRelocInfo_NONEPTR));  // indicate function proxy
  __ CmpP(r4, Operand(JS_FUNCTION_PROXY_TYPE));
  __ beq(&shift_arguments, Label::kNear);
  __ bind(&non_function);
  __ LoadImmP(r6, Operand(2, kRelocInfo_NONEPTR));  // indicate non-function

  // 3c. Patch the first argument when calling a non-function.  The
  //     CALL_NON_FUNCTION builtin expects the non-function callee as
  //     receiver, so overwrite the first argument which will ultimately
  //     become the receiver.
  // r2: actual number of arguments
  // r3: function
  // r6: call type (0: JS function, 1: function proxy, 2: non-function)
  __ ShiftLeftP(ip, r2, Operand(kPointerSizeLog2));
  __ lay(r4, MemOperand(sp, ip));
  __ StoreP(r3, MemOperand(r4, -kPointerSize));

  // 4. Shift arguments and return address one slot down on the stack
  //    (overwriting the original receiver).  Adjust argument count to make
  //    the original first argument the new receiver.
  // r2: actual number of arguments
  // r3: function
  // r6: call type (0: JS function, 1: function proxy, 2: non-function)
  __ bind(&shift_arguments);
  { Label loop;
    // Calculate the copy start address (destination). Copy end address is sp.
    __ ShiftLeftP(ip, r2, Operand(kPointerSizeLog2));
    __ lay(r4, MemOperand(sp, ip));

    __ bind(&loop);
    __ LoadP(ip, MemOperand(r4, -kPointerSize));
    __ StoreP(ip, MemOperand(r4));
    __ SubP(r4, Operand(kPointerSize));
    __ CmpP(r4, sp);
    __ bne(&loop);
    // Adjust the actual number of arguments and remove the top element
    // (which is a copy of the last argument).
    __ SubP(r2, Operand(1));
    __ pop();
  }

  // 5a. Call non-function via tail call to CALL_NON_FUNCTION builtin,
  //     or a function proxy via CALL_FUNCTION_PROXY.
  // r2: actual number of arguments
  // r3: function
  // r6: call type (0: JS function, 1: function proxy, 2: non-function)
  { Label function, non_proxy;
    __ CmpP(r6, Operand::Zero());
    __ beq(&function);
    // Expected number of arguments is 0 for CALL_NON_FUNCTION.
    __ LoadImmP(r4, Operand::Zero());
    __ CmpP(r6, Operand(1));
    __ bne(&non_proxy);

    __ push(r3);  // re-add proxy object as additional argument
    __ AddP(r2, Operand(1));
    __ GetBuiltinFunction(r3, Builtins::CALL_FUNCTION_PROXY);
    __ Jump(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
            RelocInfo::CODE_TARGET);

    __ bind(&non_proxy);
    __ GetBuiltinFunction(r3, Builtins::CALL_NON_FUNCTION);
    __ Jump(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
            RelocInfo::CODE_TARGET);
    __ bind(&function);
  }

  // 5b. Get the code to call from the function and check that the number of
  //     expected arguments matches what we're providing.  If so, jump
  //     (tail-call) to the code in register edx without checking arguments.
  // r2: actual number of arguments
  // r3: function
  __ LoadP(r5, FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));
  __ LoadW(r4, FieldMemOperand(r5,
           SharedFunctionInfo::kFormalParameterCountOffset));
#if !V8_TARGET_ARCH_S390X
  __ SmiUntag(r4);
#endif
  __ CmpP(r4, r2);  // Check formal and actual parameter counts.
  __ Jump(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
          RelocInfo::CODE_TARGET,
          ne);

  __ LoadP(ip, FieldMemOperand(r3, JSFunction::kCodeEntryOffset));
  ParameterCount expected(0);
  __ InvokeCode(ip, expected, expected, JUMP_FUNCTION, NullCallWrapper());
}


void Builtins::Generate_FunctionApply(MacroAssembler* masm) {
  const int kIndexOffset    =
      StandardFrameConstants::kExpressionsOffset - (2 * kPointerSize);
  const int kLimitOffset    =
      StandardFrameConstants::kExpressionsOffset - (1 * kPointerSize);
  const int kArgsOffset     = 2 * kPointerSize;
  const int kRecvOffset     = 3 * kPointerSize;
  const int kFunctionOffset = 4 * kPointerSize;

  {
    FrameAndConstantPoolScope frame_scope(masm, StackFrame::INTERNAL);

    __ LoadP(r2, MemOperand(fp, kFunctionOffset));  // get the function
    __ push(r2);
    __ LoadP(r2, MemOperand(fp, kArgsOffset));  // get the args array
    __ push(r2);
    __ InvokeBuiltin(Builtins::APPLY_PREPARE, CALL_FUNCTION);

    // Check the stack for overflow. We are not trying to catch
    // interruptions (e.g. debug break and preemption) here, so the "real stack
    // limit" is checked.
    Label okay;
    __ LoadRoot(r4, Heap::kRealStackLimitRootIndex);
    // Make r4 the space we have left. The stack might already be overflowed
    // here which will cause r4 to become negative.
    __ SubP(r4, sp, r4);
    // Check if the arguments will overflow the stack.
    __ SmiToPtrArrayOffset(r0, r2);
    __ CmpP(r4, r0);
    __ bgt(&okay);  // Signed comparison.

    // Out of stack space.
    __ LoadP(r3, MemOperand(fp, kFunctionOffset));
    __ Push(r3, r2);
    __ InvokeBuiltin(Builtins::STACK_OVERFLOW, CALL_FUNCTION);
    // End of stack check.

    // Push current limit and index.
    __ bind(&okay);
    __ LoadImmP(r3, Operand::Zero());
    __ Push(r2, r3);  // limit and initial index.

    // Get the receiver.
    __ LoadP(r2, MemOperand(fp, kRecvOffset));

    // Check that the function is a JS function (otherwise it must be a proxy).
    Label push_receiver;
    __ LoadP(r3, MemOperand(fp, kFunctionOffset));
    __ CompareObjectType(r3, r4, r4, JS_FUNCTION_TYPE);
    __ bne(&push_receiver);

    // Change context eagerly to get the right global object if necessary.
    __ LoadP(cp, FieldMemOperand(r3, JSFunction::kContextOffset));
    // Load the shared function info while the function is still in r3.
    __ LoadP(r4,
             FieldMemOperand(r3, JSFunction::kSharedFunctionInfoOffset));

    // Compute the receiver.
    // Do not transform the receiver for strict mode functions.
    Label call_to_object, use_global_proxy;
    __ LoadlW(r4,
              FieldMemOperand(r4, SharedFunctionInfo::kCompilerHintsOffset));
    __ TestBit(r4,
#if V8_TARGET_ARCH_S390X
               SharedFunctionInfo::kStrictModeFunction,
#else
               SharedFunctionInfo::kStrictModeFunction + kSmiTagSize,
#endif
               r0);
    __ bne(&push_receiver /*, cr0*/);

    // Do not transform the receiver for strict mode functions.
    __ TestBit(r4,
#if V8_TARGET_ARCH_S390X
               SharedFunctionInfo::kNative,
#else
               SharedFunctionInfo::kNative + kSmiTagSize,
#endif
               r0);
    __ bne(&push_receiver /*, cr0*/);

    // Compute the receiver in sloppy mode.
    __ JumpIfSmi(r2, &call_to_object);
    __ CompareRoot(r2, Heap::kNullValueRootIndex);
    __ beq(&use_global_proxy);
    __ LoadRoot(r3, Heap::kUndefinedValueRootIndex);
    __ CmpP(r2, r3);
    __ beq(&use_global_proxy);

    // Check if the receiver is already a JavaScript object.
    // r2: receiver
    STATIC_ASSERT(LAST_SPEC_OBJECT_TYPE == LAST_TYPE);
    __ CompareObjectType(r2, r3, r3, FIRST_SPEC_OBJECT_TYPE);
    __ bge(&push_receiver);

    // Convert the receiver to a regular object.
    // r2: receiver
    __ bind(&call_to_object);
    __ push(r2);
    __ InvokeBuiltin(Builtins::TO_OBJECT, CALL_FUNCTION);
    __ b(&push_receiver);

    __ bind(&use_global_proxy);
    __ LoadP(r2, ContextOperand(cp, Context::GLOBAL_OBJECT_INDEX));
    __ LoadP(r2, FieldMemOperand(r2, GlobalObject::kGlobalProxyOffset));

    // Push the receiver.
    // r2: receiver
    __ bind(&push_receiver);
    __ push(r2);

    // Copy all arguments from the array to the stack.
    Label entry, loop;
    __ LoadP(r2, MemOperand(fp, kIndexOffset));
    __ b(&entry, Label::kNear);

    // Load the current argument from the arguments array and push it to the
    // stack.
    // r2: current argument index
    __ bind(&loop);
    __ LoadP(r3, MemOperand(fp, kArgsOffset));
    __ Push(r3, r2);

    // Call the runtime to access the property in the arguments array.
    __ CallRuntime(Runtime::kGetProperty, 2);
    __ push(r2);

    // Use inline caching to access the arguments.
    __ LoadP(r2, MemOperand(fp, kIndexOffset));
    __ AddSmiLiteral(r2, r2, Smi::FromInt(1), r0);
    __ StoreP(r2, MemOperand(fp, kIndexOffset));

    // Test if the copy loop has finished copying all the elements from the
    // arguments object.
    __ bind(&entry);
    __ LoadP(r3, MemOperand(fp, kLimitOffset));
    __ CmpP(r2, r3);
    __ bne(&loop);

    // Call the function.
    Label call_proxy;
    ParameterCount actual(r2);
    __ SmiUntag(r2);
    __ LoadP(r3, MemOperand(fp, kFunctionOffset));
    __ CompareObjectType(r3, r4, r4, JS_FUNCTION_TYPE);
    __ bne(&call_proxy);
    __ InvokeFunction(r3, actual, CALL_FUNCTION, NullCallWrapper());

    frame_scope.GenerateLeaveFrame();
    __ la(sp, MemOperand(sp, 3 * kPointerSize));
    __ Ret();

    // Call the function proxy.
    __ bind(&call_proxy);
    __ push(r3);  // add function proxy as last argument
    __ AddP(r2, Operand(1));
    __ LoadImmP(r4, Operand::Zero());
    __ GetBuiltinFunction(r3, Builtins::CALL_FUNCTION_PROXY);
    __ Call(masm->isolate()->builtins()->ArgumentsAdaptorTrampoline(),
            RelocInfo::CODE_TARGET);

    // Tear down the internal frame and remove function, receiver and args.
  }
  __ la(sp, MemOperand(sp, 3 * kPointerSize));
  __ Ret();
}


static void ArgumentAdaptorStackCheck(MacroAssembler* masm,
                                      Label* stack_overflow) {
  // ----------- S t a t e -------------
  //  -- r2 : actual number of arguments
  //  -- r3 : function (passed through to callee)
  //  -- r4 : expected number of arguments
  // -----------------------------------
  // Check the stack for overflow. We are not trying to catch
  // interruptions (e.g. debug break and preemption) here, so the "real stack
  // limit" is checked.
  __ LoadRoot(r7, Heap::kRealStackLimitRootIndex);
  // Make r7 the space we have left. The stack might already be overflowed
  // here which will cause r7 to become negative.
  __ SubP(r7, sp, r7);
  // Check if the arguments will overflow the stack.
  __ ShiftLeftP(r0, r4, Operand(kPointerSizeLog2));
  __ CmpP(r7, r0);
  __ ble(stack_overflow);  // Signed comparison.
}


static void EnterArgumentsAdaptorFrame(MacroAssembler* masm) {
  __ SmiTag(r2);
  __ LoadSmiLiteral(r6, Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR));
  // Stack updated as such:
  //    old SP --->
  //                 R14 Return Addr
  //                 Old FP                     <--- New FP
  //                 Argument Adapter SMI
  //                 Function
  //                 ArgC as SMI                <--- New SP
  __ lay(sp, MemOperand(sp, -5 * kPointerSize));

  // Cleanse the top nibble of 31-bit pointers.
  __ CleanseP(r14);
  __ StoreP(r14, MemOperand(sp, 4 * kPointerSize));
  __ StoreP(fp, MemOperand(sp, 3 * kPointerSize));
  __ StoreP(r6, MemOperand(sp, 2 * kPointerSize));
  __ StoreP(r3, MemOperand(sp, 1 * kPointerSize));
  __ StoreP(r2, MemOperand(sp, 0 * kPointerSize));
  __ la(fp, MemOperand(sp,
        StandardFrameConstants::kFixedFrameSizeFromFp + kPointerSize));
}


static void LeaveArgumentsAdaptorFrame(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : result being passed through
  // -----------------------------------
  // Get the number of arguments passed (as a smi), tear down the frame and
  // then tear down the parameters.
  __ LoadP(r3, MemOperand(fp, -(StandardFrameConstants::kFixedFrameSizeFromFp +
                                kPointerSize)));
  int stack_adjustment = kPointerSize;  // adjust for receiver
  __ LeaveFrame(StackFrame::ARGUMENTS_ADAPTOR, stack_adjustment);
  __ SmiToPtrArrayOffset(r3, r3);
  __ lay(sp, MemOperand(sp, r3));
}


void Builtins::Generate_ArgumentsAdaptorTrampoline(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2 : actual number of arguments
  //  -- r3 : function (passed through to callee)
  //  -- r4 : expected number of arguments
  // -----------------------------------

  Label stack_overflow;
  ArgumentAdaptorStackCheck(masm, &stack_overflow);
  Label invoke, dont_adapt_arguments;

  Label enough, too_few;
  __ LoadP(ip, FieldMemOperand(r3, JSFunction::kCodeEntryOffset));
  __ CmpP(r2, r4);
  __ blt(&too_few);
  __ CmpP(r4, Operand(SharedFunctionInfo::kDontAdaptArgumentsSentinel));
  __ beq(&dont_adapt_arguments);

  {  // Enough parameters: actual >= expected
    __ bind(&enough);
    EnterArgumentsAdaptorFrame(masm);

    // Calculate copy start address into r2 and copy end address into r4.
    // r2: actual number of arguments as a smi
    // r3: function
    // r4: expected number of arguments
    // ip: code entry to call
    __ SmiToPtrArrayOffset(r2, r2);
    __ AddP(r2, fp);
    // adjust for return address and receiver
    __ AddP(r2, Operand(2 * kPointerSize));
    __ ShiftLeftP(r4, r4, Operand(kPointerSizeLog2));
    __ SubP(r4, r2, r4);

    // Copy the arguments (including the receiver) to the new stack frame.
    // r2: copy start address
    // r3: function
    // r4: copy end address
    // ip: code entry to call

    Label copy;
    __ bind(&copy);
    __ LoadP(r0, MemOperand(r2, 0));
    __ push(r0);
    __ CmpP(r2, r4);  // Compare before moving to next argument.
    __ lay(r2, MemOperand(r2, -kPointerSize));
    __ bne(&copy);

    __ b(&invoke);
  }

  {  // Too few parameters: Actual < expected
    __ bind(&too_few);
    EnterArgumentsAdaptorFrame(masm);

    // Calculate copy start address into r0 and copy end address is fp.
    // r2: actual number of arguments as a smi
    // r3: function
    // r4: expected number of arguments
    // ip: code entry to call
    __ SmiToPtrArrayOffset(r2, r2);
    __ lay(r2, MemOperand(r2, fp));

    // Copy the arguments (including the receiver) to the new stack frame.
    // r2: copy start address
    // r3: function
    // r4: expected number of arguments
    // ip: code entry to call
    Label copy;
    __ bind(&copy);
    // Adjust load for return address and receiver.
    __ LoadP(r0, MemOperand(r2, 2 * kPointerSize));
    __ push(r0);
    __ CmpP(r2, fp);  // Compare before moving to next argument.
    __ lay(r2, MemOperand(r2, -kPointerSize));
    __ bne(&copy);

    // Fill the remaining expected arguments with undefined.
    // r3: function
    // r4: expected number of argumentus
    // ip: code entry to call
    __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
    __ ShiftLeftP(r4, r4, Operand(kPointerSizeLog2));
    __ SubP(r4, fp, r4);
    // Adjust for frame.
    __ SubP(r4, Operand(StandardFrameConstants::kFixedFrameSizeFromFp +
                            2 * kPointerSize));

    Label fill;
    __ bind(&fill);
    __ push(r0);
    __ CmpP(sp, r4);
    __ bne(&fill);
  }

  // Call the entry point.
  __ bind(&invoke);
  __ CallJSEntry(ip);

  // Store offset of return address for deoptimizer.
  masm->isolate()->heap()->SetArgumentsAdaptorDeoptPCOffset(masm->pc_offset());

  // Exit frame and return.
  LeaveArgumentsAdaptorFrame(masm);
  __ Ret();


  // -------------------------------------------
  // Dont adapt arguments.
  // -------------------------------------------
  __ bind(&dont_adapt_arguments);
  __ JumpToJSEntry(ip);

  __ bind(&stack_overflow);
  {
    FrameScope frame(masm, StackFrame::MANUAL);
    EnterArgumentsAdaptorFrame(masm);
    __ InvokeBuiltin(Builtins::STACK_OVERFLOW, CALL_FUNCTION);
    __ bkpt(0);
  }
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_S390
