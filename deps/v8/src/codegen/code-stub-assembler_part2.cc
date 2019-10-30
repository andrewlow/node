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

template TNode<MaybeObject>
CodeStubAssembler::LoadArrayElement<TransitionArray>(TNode<TransitionArray>,
                                                     int, Node*, int,
                                                     ParameterMode,
                                                     LoadSensitivity);

template TNode<MaybeObject>
CodeStubAssembler::LoadArrayElement<DescriptorArray>(TNode<DescriptorArray>,
                                                     int, Node*, int,
                                                     ParameterMode,
                                                     LoadSensitivity);

template <typename Array, typename T>
TNode<T> CodeStubAssembler::LoadArrayElement(TNode<Array> array,
                                             int array_header_size,
                                             Node* index_node,
                                             int additional_offset,
                                             ParameterMode parameter_mode,
                                             LoadSensitivity needs_poisoning) {
  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(
                       ParameterToIntPtr(index_node, parameter_mode),
                       IntPtrConstant(0)));
  DCHECK(IsAligned(additional_offset, kTaggedSize));
  int32_t header_size = array_header_size + additional_offset - kHeapObjectTag;
  TNode<IntPtrT> offset = ElementOffsetFromIndex(index_node, HOLEY_ELEMENTS,
                                                 parameter_mode, header_size);
  CSA_ASSERT(this, IsOffsetInBounds(offset, LoadArrayLength(array),
                                    array_header_size));
  static constexpr MachineType machine_type = MachineTypeOf<T>::value;
  // TODO(gsps): Remove the Load case once LoadFromObject supports poisoning
  if (needs_poisoning == LoadSensitivity::kSafe) {
    return UncheckedCast<T>(LoadFromObject(machine_type, array, offset));
  } else {
    return UncheckedCast<T>(Load(machine_type, array, offset, needs_poisoning));
  }
}

TNode<Object> CodeStubAssembler::LoadFixedArrayElement(
    TNode<FixedArray> object, Node* index_node, int additional_offset,
    ParameterMode parameter_mode, LoadSensitivity needs_poisoning,
    CheckBounds check_bounds) {
  CSA_ASSERT(this, IsFixedArraySubclass(object));
  CSA_ASSERT(this, IsNotWeakFixedArraySubclass(object));
  if (NeedsBoundsCheck(check_bounds)) {
    FixedArrayBoundsCheck(object, index_node, additional_offset,
                          parameter_mode);
  }
  TNode<MaybeObject> element =
      LoadArrayElement(object, FixedArray::kHeaderSize, index_node,
                       additional_offset, parameter_mode, needs_poisoning);
  return CAST(element);
}

TNode<Object> CodeStubAssembler::LoadPropertyArrayElement(
    TNode<PropertyArray> object, SloppyTNode<IntPtrT> index) {
  int additional_offset = 0;
  ParameterMode parameter_mode = INTPTR_PARAMETERS;
  LoadSensitivity needs_poisoning = LoadSensitivity::kSafe;
  return CAST(LoadArrayElement(object, PropertyArray::kHeaderSize, index,
                               additional_offset, parameter_mode,
                               needs_poisoning));
}

template <typename Array>
TNode<Int32T> CodeStubAssembler::LoadAndUntagToWord32ArrayElement(
    TNode<Array> object, int array_header_size, Node* index_node,
    int additional_offset, ParameterMode parameter_mode) {
  CSA_SLOW_ASSERT(this, MatchesParameterMode(index_node, parameter_mode));
  DCHECK(IsAligned(additional_offset, kTaggedSize));
  int endian_correction = 0;
#if V8_TARGET_LITTLE_ENDIAN
  if (SmiValuesAre32Bits()) endian_correction = 4;
#endif
  int32_t header_size = array_header_size + additional_offset - kHeapObjectTag +
                        endian_correction;
  Node* offset = ElementOffsetFromIndex(index_node, HOLEY_ELEMENTS,
                                        parameter_mode, header_size);
  CSA_ASSERT(this, IsOffsetInBounds(offset, LoadArrayLength(object),
                                    array_header_size + endian_correction));
  if (SmiValuesAre32Bits()) {
    return UncheckedCast<Int32T>(Load(MachineType::Int32(), object, offset));
  } else {
    return SmiToInt32(Load(MachineType::AnyTagged(), object, offset));
  }
}

TNode<Int32T> CodeStubAssembler::LoadAndUntagToWord32FixedArrayElement(
    TNode<FixedArray> object, Node* index_node, int additional_offset,
    ParameterMode parameter_mode) {
  CSA_SLOW_ASSERT(this, IsFixedArraySubclass(object));
  return LoadAndUntagToWord32ArrayElement(object, FixedArray::kHeaderSize,
                                          index_node, additional_offset,
                                          parameter_mode);
}

TNode<MaybeObject> CodeStubAssembler::LoadWeakFixedArrayElement(
    TNode<WeakFixedArray> object, Node* index, int additional_offset,
    ParameterMode parameter_mode, LoadSensitivity needs_poisoning) {
  return LoadArrayElement(object, WeakFixedArray::kHeaderSize, index,
                          additional_offset, parameter_mode, needs_poisoning);
}

TNode<Float64T> CodeStubAssembler::LoadFixedDoubleArrayElement(
    SloppyTNode<FixedDoubleArray> object, Node* index_node,
    MachineType machine_type, int additional_offset,
    ParameterMode parameter_mode, Label* if_hole) {
  CSA_ASSERT(this, IsFixedDoubleArray(object));
  DCHECK(IsAligned(additional_offset, kTaggedSize));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(index_node, parameter_mode));
  int32_t header_size =
      FixedDoubleArray::kHeaderSize + additional_offset - kHeapObjectTag;
  TNode<IntPtrT> offset = ElementOffsetFromIndex(
      index_node, HOLEY_DOUBLE_ELEMENTS, parameter_mode, header_size);
  CSA_ASSERT(this, IsOffsetInBounds(
                       offset, LoadAndUntagFixedArrayBaseLength(object),
                       FixedDoubleArray::kHeaderSize, HOLEY_DOUBLE_ELEMENTS));
  return LoadDoubleWithHoleCheck(object, offset, if_hole, machine_type);
}

TNode<Object> CodeStubAssembler::LoadFixedArrayBaseElementAsTagged(
    TNode<FixedArrayBase> elements, TNode<IntPtrT> index,
    TNode<Int32T> elements_kind, Label* if_accessor, Label* if_hole) {
  TVARIABLE(Object, var_result);
  Label done(this), if_packed(this), if_holey(this), if_packed_double(this),
      if_holey_double(this), if_dictionary(this, Label::kDeferred);

  int32_t kinds[] = {// Handled by if_packed.
                     PACKED_SMI_ELEMENTS, PACKED_ELEMENTS,
                     PACKED_SEALED_ELEMENTS, PACKED_FROZEN_ELEMENTS,
                     // Handled by if_holey.
                     HOLEY_SMI_ELEMENTS, HOLEY_ELEMENTS, HOLEY_SEALED_ELEMENTS,
                     HOLEY_FROZEN_ELEMENTS,
                     // Handled by if_packed_double.
                     PACKED_DOUBLE_ELEMENTS,
                     // Handled by if_holey_double.
                     HOLEY_DOUBLE_ELEMENTS};
  Label* labels[] = {// PACKED_{SMI,}_ELEMENTS
                     &if_packed, &if_packed, &if_packed, &if_packed,
                     // HOLEY_{SMI,}_ELEMENTS
                     &if_holey, &if_holey, &if_holey, &if_holey,
                     // PACKED_DOUBLE_ELEMENTS
                     &if_packed_double,
                     // HOLEY_DOUBLE_ELEMENTS
                     &if_holey_double};
  Switch(elements_kind, &if_dictionary, kinds, labels, arraysize(kinds));

  BIND(&if_packed);
  {
    var_result = LoadFixedArrayElement(CAST(elements), index, 0);
    Goto(&done);
  }

  BIND(&if_holey);
  {
    var_result = LoadFixedArrayElement(CAST(elements), index);
    Branch(WordEqual(var_result.value(), TheHoleConstant()), if_hole, &done);
  }

  BIND(&if_packed_double);
  {
    var_result = AllocateHeapNumberWithValue(LoadFixedDoubleArrayElement(
        CAST(elements), index, MachineType::Float64()));
    Goto(&done);
  }

  BIND(&if_holey_double);
  {
    var_result = AllocateHeapNumberWithValue(LoadFixedDoubleArrayElement(
        CAST(elements), index, MachineType::Float64(), 0, INTPTR_PARAMETERS,
        if_hole));
    Goto(&done);
  }

  BIND(&if_dictionary);
  {
    CSA_ASSERT(this, IsDictionaryElementsKind(elements_kind));
    var_result = BasicLoadNumberDictionaryElement(CAST(elements), index,
                                                  if_accessor, if_hole);
    Goto(&done);
  }

  BIND(&done);
  return var_result.value();
}

TNode<Float64T> CodeStubAssembler::LoadDoubleWithHoleCheck(
    SloppyTNode<Object> base, SloppyTNode<IntPtrT> offset, Label* if_hole,
    MachineType machine_type) {
  if (if_hole) {
    // TODO(ishell): Compare only the upper part for the hole once the
    // compiler is able to fold addition of already complex |offset| with
    // |kIeeeDoubleExponentWordOffset| into one addressing mode.
    if (Is64()) {
      Node* element = Load(MachineType::Uint64(), base, offset);
      GotoIf(Word64Equal(element, Int64Constant(kHoleNanInt64)), if_hole);
    } else {
      Node* element_upper = Load(
          MachineType::Uint32(), base,
          IntPtrAdd(offset, IntPtrConstant(kIeeeDoubleExponentWordOffset)));
      GotoIf(Word32Equal(element_upper, Int32Constant(kHoleNanUpper32)),
             if_hole);
    }
  }
  if (machine_type.IsNone()) {
    // This means the actual value is not needed.
    return TNode<Float64T>();
  }
  return UncheckedCast<Float64T>(Load(machine_type, base, offset));
}

void CodeStubAssembler::ThrowTypeError(Node* context, MessageTemplate message,
                                       Node* arg0, Node* arg1, Node* arg2) {
  Node* template_index = SmiConstant(static_cast<int>(message));
  if (arg0 == nullptr) {
    CallRuntime(Runtime::kThrowTypeError, context, template_index);
  } else if (arg1 == nullptr) {
    CallRuntime(Runtime::kThrowTypeError, context, template_index, arg0);
  } else if (arg2 == nullptr) {
    CallRuntime(Runtime::kThrowTypeError, context, template_index, arg0, arg1);
  } else {
    CallRuntime(Runtime::kThrowTypeError, context, template_index, arg0, arg1,
                arg2);
  }
  Unreachable();
}

TNode<BoolT> CodeStubAssembler::InstanceTypeEqual(
    SloppyTNode<Int32T> instance_type, int type) {
  return Word32Equal(instance_type, Int32Constant(type));
}

TNode<BoolT> CodeStubAssembler::IsDictionaryMap(SloppyTNode<Map> map) {
  CSA_SLOW_ASSERT(this, IsMap(map));
  return IsSetWord32<Map::IsDictionaryMapBit>(LoadMapBitField3(map));
}

TNode<BoolT> CodeStubAssembler::IsExtensibleMap(SloppyTNode<Map> map) {
  CSA_ASSERT(this, IsMap(map));
  return IsSetWord32<Map::IsExtensibleBit>(LoadMapBitField3(map));
}

TNode<BoolT> CodeStubAssembler::IsFrozenOrSealedElementsKindMap(
    SloppyTNode<Map> map) {
  CSA_ASSERT(this, IsMap(map));
  return IsElementsKindInRange(LoadMapElementsKind(map), PACKED_SEALED_ELEMENTS,
                               HOLEY_FROZEN_ELEMENTS);
}

TNode<BoolT> CodeStubAssembler::IsExtensibleNonPrototypeMap(TNode<Map> map) {
  int kMask = Map::IsExtensibleBit::kMask | Map::IsPrototypeMapBit::kMask;
  int kExpected = Map::IsExtensibleBit::kMask;
  return Word32Equal(Word32And(LoadMapBitField3(map), Int32Constant(kMask)),
                     Int32Constant(kExpected));
}

TNode<BoolT> CodeStubAssembler::IsCallableMap(SloppyTNode<Map> map) {
  CSA_ASSERT(this, IsMap(map));
  return IsSetWord32<Map::IsCallableBit>(LoadMapBitField(map));
}

TNode<BoolT> CodeStubAssembler::IsDebugInfo(TNode<HeapObject> object) {
  return HasInstanceType(object, DEBUG_INFO_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsDeprecatedMap(SloppyTNode<Map> map) {
  CSA_ASSERT(this, IsMap(map));
  return IsSetWord32<Map::IsDeprecatedBit>(LoadMapBitField3(map));
}

TNode<BoolT> CodeStubAssembler::IsUndetectableMap(SloppyTNode<Map> map) {
  CSA_ASSERT(this, IsMap(map));
  return IsSetWord32<Map::IsUndetectableBit>(LoadMapBitField(map));
}

TNode<BoolT> CodeStubAssembler::IsNoElementsProtectorCellInvalid() {
  Node* invalid = SmiConstant(Isolate::kProtectorInvalid);
  Node* cell = LoadRoot(RootIndex::kNoElementsProtector);
  Node* cell_value = LoadObjectField(cell, PropertyCell::kValueOffset);
  return WordEqual(cell_value, invalid);
}

TNode<BoolT> CodeStubAssembler::IsArrayIteratorProtectorCellInvalid() {
  Node* invalid = SmiConstant(Isolate::kProtectorInvalid);
  Node* cell = LoadRoot(RootIndex::kArrayIteratorProtector);
  Node* cell_value = LoadObjectField(cell, PropertyCell::kValueOffset);
  return WordEqual(cell_value, invalid);
}

TNode<BoolT> CodeStubAssembler::IsPromiseResolveProtectorCellInvalid() {
  Node* invalid = SmiConstant(Isolate::kProtectorInvalid);
  Node* cell = LoadRoot(RootIndex::kPromiseResolveProtector);
  Node* cell_value = LoadObjectField(cell, Cell::kValueOffset);
  return WordEqual(cell_value, invalid);
}

TNode<BoolT> CodeStubAssembler::IsPromiseThenProtectorCellInvalid() {
  Node* invalid = SmiConstant(Isolate::kProtectorInvalid);
  Node* cell = LoadRoot(RootIndex::kPromiseThenProtector);
  Node* cell_value = LoadObjectField(cell, PropertyCell::kValueOffset);
  return WordEqual(cell_value, invalid);
}

TNode<BoolT> CodeStubAssembler::IsArraySpeciesProtectorCellInvalid() {
  Node* invalid = SmiConstant(Isolate::kProtectorInvalid);
  Node* cell = LoadRoot(RootIndex::kArraySpeciesProtector);
  Node* cell_value = LoadObjectField(cell, PropertyCell::kValueOffset);
  return WordEqual(cell_value, invalid);
}

TNode<BoolT> CodeStubAssembler::IsTypedArraySpeciesProtectorCellInvalid() {
  Node* invalid = SmiConstant(Isolate::kProtectorInvalid);
  Node* cell = LoadRoot(RootIndex::kTypedArraySpeciesProtector);
  Node* cell_value = LoadObjectField(cell, PropertyCell::kValueOffset);
  return WordEqual(cell_value, invalid);
}

TNode<BoolT> CodeStubAssembler::IsRegExpSpeciesProtectorCellInvalid(
    TNode<Context> native_context) {
  CSA_ASSERT(this, IsNativeContext(native_context));
  TNode<PropertyCell> cell = CAST(LoadContextElement(
      native_context, Context::REGEXP_SPECIES_PROTECTOR_INDEX));
  TNode<Object> cell_value = LoadObjectField(cell, PropertyCell::kValueOffset);
  TNode<Smi> invalid = SmiConstant(Isolate::kProtectorInvalid);
  return WordEqual(cell_value, invalid);
}

TNode<BoolT> CodeStubAssembler::IsPromiseSpeciesProtectorCellInvalid() {
  Node* invalid = SmiConstant(Isolate::kProtectorInvalid);
  Node* cell = LoadRoot(RootIndex::kPromiseSpeciesProtector);
  Node* cell_value = LoadObjectField(cell, PropertyCell::kValueOffset);
  return WordEqual(cell_value, invalid);
}

TNode<BoolT> CodeStubAssembler::IsPrototypeInitialArrayPrototype(
    SloppyTNode<Context> context, SloppyTNode<Map> map) {
  Node* const native_context = LoadNativeContext(context);
  Node* const initial_array_prototype = LoadContextElement(
      native_context, Context::INITIAL_ARRAY_PROTOTYPE_INDEX);
  Node* proto = LoadMapPrototype(map);
  return WordEqual(proto, initial_array_prototype);
}

TNode<BoolT> CodeStubAssembler::IsPrototypeTypedArrayPrototype(
    SloppyTNode<Context> context, SloppyTNode<Map> map) {
  TNode<Context> const native_context = LoadNativeContext(context);
  TNode<Object> const typed_array_prototype =
      LoadContextElement(native_context, Context::TYPED_ARRAY_PROTOTYPE_INDEX);
  TNode<HeapObject> proto = LoadMapPrototype(map);
  TNode<HeapObject> proto_of_proto = Select<HeapObject>(
      IsJSObject(proto), [=] { return LoadMapPrototype(LoadMap(proto)); },
      [=] { return NullConstant(); });
  return WordEqual(proto_of_proto, typed_array_prototype);
}

TNode<BoolT> CodeStubAssembler::IsFastAliasedArgumentsMap(
    TNode<Context> context, TNode<Map> map) {
  TNode<Context> const native_context = LoadNativeContext(context);
  TNode<Object> const arguments_map = LoadContextElement(
      native_context, Context::FAST_ALIASED_ARGUMENTS_MAP_INDEX);
  return WordEqual(arguments_map, map);
}

TNode<BoolT> CodeStubAssembler::IsSlowAliasedArgumentsMap(
    TNode<Context> context, TNode<Map> map) {
  TNode<Context> const native_context = LoadNativeContext(context);
  TNode<Object> const arguments_map = LoadContextElement(
      native_context, Context::SLOW_ALIASED_ARGUMENTS_MAP_INDEX);
  return WordEqual(arguments_map, map);
}

TNode<BoolT> CodeStubAssembler::IsSloppyArgumentsMap(TNode<Context> context,
                                                     TNode<Map> map) {
  TNode<Context> const native_context = LoadNativeContext(context);
  TNode<Object> const arguments_map =
      LoadContextElement(native_context, Context::SLOPPY_ARGUMENTS_MAP_INDEX);
  return WordEqual(arguments_map, map);
}

TNode<BoolT> CodeStubAssembler::IsStrictArgumentsMap(TNode<Context> context,
                                                     TNode<Map> map) {
  TNode<Context> const native_context = LoadNativeContext(context);
  TNode<Object> const arguments_map =
      LoadContextElement(native_context, Context::STRICT_ARGUMENTS_MAP_INDEX);
  return WordEqual(arguments_map, map);
}

TNode<BoolT> CodeStubAssembler::TaggedIsCallable(TNode<Object> object) {
  return Select<BoolT>(
      TaggedIsSmi(object), [=] { return Int32FalseConstant(); },
      [=] {
        return IsCallableMap(LoadMap(UncheckedCast<HeapObject>(object)));
      });
}

TNode<BoolT> CodeStubAssembler::IsCallable(SloppyTNode<HeapObject> object) {
  return IsCallableMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsCell(SloppyTNode<HeapObject> object) {
  return WordEqual(LoadMap(object), LoadRoot(RootIndex::kCellMap));
}

TNode<BoolT> CodeStubAssembler::IsCode(SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, CODE_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsConstructorMap(SloppyTNode<Map> map) {
  CSA_ASSERT(this, IsMap(map));
  return IsSetWord32<Map::IsConstructorBit>(LoadMapBitField(map));
}

TNode<BoolT> CodeStubAssembler::IsConstructor(SloppyTNode<HeapObject> object) {
  return IsConstructorMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsFunctionWithPrototypeSlotMap(
    SloppyTNode<Map> map) {
  CSA_ASSERT(this, IsMap(map));
  return IsSetWord32<Map::HasPrototypeSlotBit>(LoadMapBitField(map));
}

TNode<BoolT> CodeStubAssembler::IsSpecialReceiverInstanceType(
    TNode<Int32T> instance_type) {
  STATIC_ASSERT(JS_GLOBAL_OBJECT_TYPE <= LAST_SPECIAL_RECEIVER_TYPE);
  return Int32LessThanOrEqual(instance_type,
                              Int32Constant(LAST_SPECIAL_RECEIVER_TYPE));
}

TNode<BoolT> CodeStubAssembler::IsCustomElementsReceiverInstanceType(
    TNode<Int32T> instance_type) {
  return Int32LessThanOrEqual(instance_type,
                              Int32Constant(LAST_CUSTOM_ELEMENTS_RECEIVER));
}

TNode<BoolT> CodeStubAssembler::IsStringInstanceType(
    SloppyTNode<Int32T> instance_type) {
  STATIC_ASSERT(INTERNALIZED_STRING_TYPE == FIRST_TYPE);
  return Int32LessThan(instance_type, Int32Constant(FIRST_NONSTRING_TYPE));
}

TNode<BoolT> CodeStubAssembler::IsOneByteStringInstanceType(
    SloppyTNode<Int32T> instance_type) {
  CSA_ASSERT(this, IsStringInstanceType(instance_type));
  return Word32Equal(
      Word32And(instance_type, Int32Constant(kStringEncodingMask)),
      Int32Constant(kOneByteStringTag));
}

TNode<BoolT> CodeStubAssembler::IsSequentialStringInstanceType(
    SloppyTNode<Int32T> instance_type) {
  CSA_ASSERT(this, IsStringInstanceType(instance_type));
  return Word32Equal(
      Word32And(instance_type, Int32Constant(kStringRepresentationMask)),
      Int32Constant(kSeqStringTag));
}

TNode<BoolT> CodeStubAssembler::IsConsStringInstanceType(
    SloppyTNode<Int32T> instance_type) {
  CSA_ASSERT(this, IsStringInstanceType(instance_type));
  return Word32Equal(
      Word32And(instance_type, Int32Constant(kStringRepresentationMask)),
      Int32Constant(kConsStringTag));
}

TNode<BoolT> CodeStubAssembler::IsIndirectStringInstanceType(
    SloppyTNode<Int32T> instance_type) {
  CSA_ASSERT(this, IsStringInstanceType(instance_type));
  STATIC_ASSERT(kIsIndirectStringMask == 0x1);
  STATIC_ASSERT(kIsIndirectStringTag == 0x1);
  return UncheckedCast<BoolT>(
      Word32And(instance_type, Int32Constant(kIsIndirectStringMask)));
}

TNode<BoolT> CodeStubAssembler::IsExternalStringInstanceType(
    SloppyTNode<Int32T> instance_type) {
  CSA_ASSERT(this, IsStringInstanceType(instance_type));
  return Word32Equal(
      Word32And(instance_type, Int32Constant(kStringRepresentationMask)),
      Int32Constant(kExternalStringTag));
}

TNode<BoolT> CodeStubAssembler::IsUncachedExternalStringInstanceType(
    SloppyTNode<Int32T> instance_type) {
  CSA_ASSERT(this, IsStringInstanceType(instance_type));
  STATIC_ASSERT(kUncachedExternalStringTag != 0);
  return IsSetWord32(instance_type, kUncachedExternalStringMask);
}

TNode<BoolT> CodeStubAssembler::IsJSReceiverInstanceType(
    SloppyTNode<Int32T> instance_type) {
  STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  return Int32GreaterThanOrEqual(instance_type,
                                 Int32Constant(FIRST_JS_RECEIVER_TYPE));
}

TNode<BoolT> CodeStubAssembler::IsJSReceiverMap(SloppyTNode<Map> map) {
  return IsJSReceiverInstanceType(LoadMapInstanceType(map));
}

TNode<BoolT> CodeStubAssembler::IsJSReceiver(SloppyTNode<HeapObject> object) {
  return IsJSReceiverMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsNullOrJSReceiver(
    SloppyTNode<HeapObject> object) {
  return UncheckedCast<BoolT>(Word32Or(IsJSReceiver(object), IsNull(object)));
}

TNode<BoolT> CodeStubAssembler::IsNullOrUndefined(SloppyTNode<Object> value) {
  return UncheckedCast<BoolT>(Word32Or(IsUndefined(value), IsNull(value)));
}

TNode<BoolT> CodeStubAssembler::IsJSGlobalProxyInstanceType(
    SloppyTNode<Int32T> instance_type) {
  return InstanceTypeEqual(instance_type, JS_GLOBAL_PROXY_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsJSGlobalProxyMap(SloppyTNode<Map> map) {
  return IsJSGlobalProxyInstanceType(LoadMapInstanceType(map));
}

TNode<BoolT> CodeStubAssembler::IsJSGlobalProxy(
    SloppyTNode<HeapObject> object) {
  return IsJSGlobalProxyMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsJSObjectInstanceType(
    SloppyTNode<Int32T> instance_type) {
  STATIC_ASSERT(LAST_JS_OBJECT_TYPE == LAST_TYPE);
  return Int32GreaterThanOrEqual(instance_type,
                                 Int32Constant(FIRST_JS_OBJECT_TYPE));
}

TNode<BoolT> CodeStubAssembler::IsJSObjectMap(SloppyTNode<Map> map) {
  CSA_ASSERT(this, IsMap(map));
  return IsJSObjectInstanceType(LoadMapInstanceType(map));
}

TNode<BoolT> CodeStubAssembler::IsJSObject(SloppyTNode<HeapObject> object) {
  return IsJSObjectMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsJSPromiseMap(SloppyTNode<Map> map) {
  CSA_ASSERT(this, IsMap(map));
  return InstanceTypeEqual(LoadMapInstanceType(map), JS_PROMISE_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsJSPromise(SloppyTNode<HeapObject> object) {
  return IsJSPromiseMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsJSProxy(SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, JS_PROXY_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsJSStringIterator(
    SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, JS_STRING_ITERATOR_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsMap(SloppyTNode<HeapObject> map) {
  return IsMetaMap(LoadMap(map));
}

TNode<BoolT> CodeStubAssembler::IsJSPrimitiveWrapperInstanceType(
    SloppyTNode<Int32T> instance_type) {
  return InstanceTypeEqual(instance_type, JS_PRIMITIVE_WRAPPER_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsJSPrimitiveWrapper(
    SloppyTNode<HeapObject> object) {
  return IsJSPrimitiveWrapperMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsJSPrimitiveWrapperMap(SloppyTNode<Map> map) {
  return IsJSPrimitiveWrapperInstanceType(LoadMapInstanceType(map));
}

TNode<BoolT> CodeStubAssembler::IsJSArrayInstanceType(
    SloppyTNode<Int32T> instance_type) {
  return InstanceTypeEqual(instance_type, JS_ARRAY_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsJSArray(SloppyTNode<HeapObject> object) {
  return IsJSArrayMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsJSArrayMap(SloppyTNode<Map> map) {
  return IsJSArrayInstanceType(LoadMapInstanceType(map));
}

TNode<BoolT> CodeStubAssembler::IsJSArrayIterator(
    SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, JS_ARRAY_ITERATOR_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsJSAsyncGeneratorObject(
    SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, JS_ASYNC_GENERATOR_OBJECT_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsContext(SloppyTNode<HeapObject> object) {
  Node* instance_type = LoadInstanceType(object);
  return UncheckedCast<BoolT>(Word32And(
      Int32GreaterThanOrEqual(instance_type, Int32Constant(FIRST_CONTEXT_TYPE)),
      Int32LessThanOrEqual(instance_type, Int32Constant(LAST_CONTEXT_TYPE))));
}

TNode<BoolT> CodeStubAssembler::IsFixedArray(SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, FIXED_ARRAY_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsFixedArraySubclass(
    SloppyTNode<HeapObject> object) {
  Node* instance_type = LoadInstanceType(object);
  return UncheckedCast<BoolT>(
      Word32And(Int32GreaterThanOrEqual(instance_type,
                                        Int32Constant(FIRST_FIXED_ARRAY_TYPE)),
                Int32LessThanOrEqual(instance_type,
                                     Int32Constant(LAST_FIXED_ARRAY_TYPE))));
}

TNode<BoolT> CodeStubAssembler::IsNotWeakFixedArraySubclass(
    SloppyTNode<HeapObject> object) {
  Node* instance_type = LoadInstanceType(object);
  return UncheckedCast<BoolT>(Word32Or(
      Int32LessThan(instance_type, Int32Constant(FIRST_WEAK_FIXED_ARRAY_TYPE)),
      Int32GreaterThan(instance_type,
                       Int32Constant(LAST_WEAK_FIXED_ARRAY_TYPE))));
}

TNode<BoolT> CodeStubAssembler::IsPromiseCapability(
    SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, PROMISE_CAPABILITY_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsPropertyArray(
    SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, PROPERTY_ARRAY_TYPE);
}

// This complicated check is due to elements oddities. If a smi array is empty
// after Array.p.shift, it is replaced by the empty array constant. If it is
// later filled with a double element, we try to grow it but pass in a double
// elements kind. Usually this would cause a size mismatch (since the source
// fixed array has HOLEY_ELEMENTS and destination has
// HOLEY_DOUBLE_ELEMENTS), but we don't have to worry about it when the
// source array is empty.
// TODO(jgruber): It might we worth creating an empty_double_array constant to
// simplify this case.
TNode<BoolT> CodeStubAssembler::IsFixedArrayWithKindOrEmpty(
    SloppyTNode<HeapObject> object, ElementsKind kind) {
  Label out(this);
  TVARIABLE(BoolT, var_result, Int32TrueConstant());

  GotoIf(IsFixedArrayWithKind(object, kind), &out);

  TNode<Smi> const length = LoadFixedArrayBaseLength(CAST(object));
  GotoIf(SmiEqual(length, SmiConstant(0)), &out);

  var_result = Int32FalseConstant();
  Goto(&out);

  BIND(&out);
  return var_result.value();
}

TNode<BoolT> CodeStubAssembler::IsFixedArrayWithKind(
    SloppyTNode<HeapObject> object, ElementsKind kind) {
  if (IsDoubleElementsKind(kind)) {
    return IsFixedDoubleArray(object);
  } else {
    DCHECK(IsSmiOrObjectElementsKind(kind) || IsSealedElementsKind(kind));
    return IsFixedArraySubclass(object);
  }
}

TNode<BoolT> CodeStubAssembler::IsBoolean(SloppyTNode<HeapObject> object) {
  return IsBooleanMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsPropertyCell(SloppyTNode<HeapObject> object) {
  return IsPropertyCellMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsAccessorInfo(SloppyTNode<HeapObject> object) {
  return IsAccessorInfoMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsAccessorPair(SloppyTNode<HeapObject> object) {
  return IsAccessorPairMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsAllocationSite(
    SloppyTNode<HeapObject> object) {
  return IsAllocationSiteInstanceType(LoadInstanceType(object));
}

TNode<BoolT> CodeStubAssembler::IsAnyHeapNumber(
    SloppyTNode<HeapObject> object) {
  return UncheckedCast<BoolT>(
      Word32Or(IsMutableHeapNumber(object), IsHeapNumber(object)));
}

TNode<BoolT> CodeStubAssembler::IsHeapNumber(SloppyTNode<HeapObject> object) {
  return IsHeapNumberMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsHeapNumberInstanceType(
    SloppyTNode<Int32T> instance_type) {
  return InstanceTypeEqual(instance_type, HEAP_NUMBER_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsOddball(SloppyTNode<HeapObject> object) {
  return IsOddballInstanceType(LoadInstanceType(object));
}

TNode<BoolT> CodeStubAssembler::IsOddballInstanceType(
    SloppyTNode<Int32T> instance_type) {
  return InstanceTypeEqual(instance_type, ODDBALL_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsMutableHeapNumber(
    SloppyTNode<HeapObject> object) {
  return IsMutableHeapNumberMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsFeedbackCell(SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, FEEDBACK_CELL_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsFeedbackVector(
    SloppyTNode<HeapObject> object) {
  return IsFeedbackVectorMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsName(SloppyTNode<HeapObject> object) {
  return IsNameInstanceType(LoadInstanceType(object));
}

TNode<BoolT> CodeStubAssembler::IsNameInstanceType(
    SloppyTNode<Int32T> instance_type) {
  return Int32LessThanOrEqual(instance_type, Int32Constant(LAST_NAME_TYPE));
}

TNode<BoolT> CodeStubAssembler::IsString(SloppyTNode<HeapObject> object) {
  return IsStringInstanceType(LoadInstanceType(object));
}

TNode<BoolT> CodeStubAssembler::IsSymbolInstanceType(
    SloppyTNode<Int32T> instance_type) {
  return InstanceTypeEqual(instance_type, SYMBOL_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsSymbol(SloppyTNode<HeapObject> object) {
  return IsSymbolMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsInternalizedStringInstanceType(
    TNode<Int32T> instance_type) {
  STATIC_ASSERT(kNotInternalizedTag != 0);
  return Word32Equal(
      Word32And(instance_type,
                Int32Constant(kIsNotStringMask | kIsNotInternalizedMask)),
      Int32Constant(kStringTag | kInternalizedTag));
}

TNode<BoolT> CodeStubAssembler::IsUniqueName(TNode<HeapObject> object) {
  TNode<Int32T> instance_type = LoadInstanceType(object);
  return Select<BoolT>(
      IsInternalizedStringInstanceType(instance_type),
      [=] { return Int32TrueConstant(); },
      [=] { return IsSymbolInstanceType(instance_type); });
}

TNode<BoolT> CodeStubAssembler::IsUniqueNameNoIndex(TNode<HeapObject> object) {
  TNode<Int32T> instance_type = LoadInstanceType(object);
  return Select<BoolT>(
      IsInternalizedStringInstanceType(instance_type),
      [=] {
        return IsSetWord32(LoadNameHashField(CAST(object)),
                           Name::kIsNotArrayIndexMask);
      },
      [=] { return IsSymbolInstanceType(instance_type); });
}

TNode<BoolT> CodeStubAssembler::IsBigIntInstanceType(
    SloppyTNode<Int32T> instance_type) {
  return InstanceTypeEqual(instance_type, BIGINT_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsBigInt(SloppyTNode<HeapObject> object) {
  return IsBigIntInstanceType(LoadInstanceType(object));
}

TNode<BoolT> CodeStubAssembler::IsPrimitiveInstanceType(
    SloppyTNode<Int32T> instance_type) {
  return Int32LessThanOrEqual(instance_type,
                              Int32Constant(LAST_PRIMITIVE_TYPE));
}

TNode<BoolT> CodeStubAssembler::IsPrivateSymbol(
    SloppyTNode<HeapObject> object) {
  return Select<BoolT>(
      IsSymbol(object),
      [=] {
        TNode<Symbol> symbol = CAST(object);
        TNode<Uint32T> flags =
            LoadObjectField<Uint32T>(symbol, Symbol::kFlagsOffset);
        return IsSetWord32<Symbol::IsPrivateBit>(flags);
      },
      [=] { return Int32FalseConstant(); });
}

TNode<BoolT> CodeStubAssembler::IsPrivateName(SloppyTNode<Symbol> symbol) {
  TNode<Uint32T> flags = LoadObjectField<Uint32T>(symbol, Symbol::kFlagsOffset);
  return IsSetWord32<Symbol::IsPrivateNameBit>(flags);
}

TNode<BoolT> CodeStubAssembler::IsNativeContext(
    SloppyTNode<HeapObject> object) {
  return WordEqual(LoadMap(object), LoadRoot(RootIndex::kNativeContextMap));
}

TNode<BoolT> CodeStubAssembler::IsFixedDoubleArray(
    SloppyTNode<HeapObject> object) {
  return WordEqual(LoadMap(object), FixedDoubleArrayMapConstant());
}

TNode<BoolT> CodeStubAssembler::IsHashTable(SloppyTNode<HeapObject> object) {
  Node* instance_type = LoadInstanceType(object);
  return UncheckedCast<BoolT>(
      Word32And(Int32GreaterThanOrEqual(instance_type,
                                        Int32Constant(FIRST_HASH_TABLE_TYPE)),
                Int32LessThanOrEqual(instance_type,
                                     Int32Constant(LAST_HASH_TABLE_TYPE))));
}

TNode<BoolT> CodeStubAssembler::IsEphemeronHashTable(
    SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, EPHEMERON_HASH_TABLE_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsNameDictionary(
    SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, NAME_DICTIONARY_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsGlobalDictionary(
    SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, GLOBAL_DICTIONARY_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsNumberDictionary(
    SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, NUMBER_DICTIONARY_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsJSGeneratorObject(
    SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, JS_GENERATOR_OBJECT_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsJSFunctionInstanceType(
    SloppyTNode<Int32T> instance_type) {
  return InstanceTypeEqual(instance_type, JS_FUNCTION_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsAllocationSiteInstanceType(
    SloppyTNode<Int32T> instance_type) {
  return InstanceTypeEqual(instance_type, ALLOCATION_SITE_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsJSFunction(SloppyTNode<HeapObject> object) {
  return IsJSFunctionMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsJSFunctionMap(SloppyTNode<Map> map) {
  return IsJSFunctionInstanceType(LoadMapInstanceType(map));
}

TNode<BoolT> CodeStubAssembler::IsJSTypedArrayInstanceType(
    SloppyTNode<Int32T> instance_type) {
  return InstanceTypeEqual(instance_type, JS_TYPED_ARRAY_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsJSTypedArrayMap(SloppyTNode<Map> map) {
  return IsJSTypedArrayInstanceType(LoadMapInstanceType(map));
}

TNode<BoolT> CodeStubAssembler::IsJSTypedArray(SloppyTNode<HeapObject> object) {
  return IsJSTypedArrayMap(LoadMap(object));
}

TNode<BoolT> CodeStubAssembler::IsJSArrayBuffer(
    SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, JS_ARRAY_BUFFER_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsJSDataView(TNode<HeapObject> object) {
  return HasInstanceType(object, JS_DATA_VIEW_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsJSRegExp(SloppyTNode<HeapObject> object) {
  return HasInstanceType(object, JS_REGEXP_TYPE);
}

TNode<BoolT> CodeStubAssembler::IsNumber(SloppyTNode<Object> object) {
  return Select<BoolT>(
      TaggedIsSmi(object), [=] { return Int32TrueConstant(); },
      [=] { return IsHeapNumber(CAST(object)); });
}

TNode<BoolT> CodeStubAssembler::IsNumeric(SloppyTNode<Object> object) {
  return Select<BoolT>(
      TaggedIsSmi(object), [=] { return Int32TrueConstant(); },
      [=] {
        return UncheckedCast<BoolT>(
            Word32Or(IsHeapNumber(CAST(object)), IsBigInt(CAST(object))));
      });
}

TNode<BoolT> CodeStubAssembler::IsNumberNormalized(SloppyTNode<Number> number) {
  TVARIABLE(BoolT, var_result, Int32TrueConstant());
  Label out(this);

  GotoIf(TaggedIsSmi(number), &out);

  TNode<Float64T> value = LoadHeapNumberValue(CAST(number));
  TNode<Float64T> smi_min =
      Float64Constant(static_cast<double>(Smi::kMinValue));
  TNode<Float64T> smi_max =
      Float64Constant(static_cast<double>(Smi::kMaxValue));

  GotoIf(Float64LessThan(value, smi_min), &out);
  GotoIf(Float64GreaterThan(value, smi_max), &out);
  GotoIfNot(Float64Equal(value, value), &out);  // NaN.

  var_result = Int32FalseConstant();
  Goto(&out);

  BIND(&out);
  return var_result.value();
}

TNode<BoolT> CodeStubAssembler::IsNumberPositive(SloppyTNode<Number> number) {
  return Select<BoolT>(
      TaggedIsSmi(number), [=] { return TaggedIsPositiveSmi(number); },
      [=] { return IsHeapNumberPositive(CAST(number)); });
}

// TODO(cbruni): Use TNode<HeapNumber> instead of custom name.
TNode<BoolT> CodeStubAssembler::IsHeapNumberPositive(TNode<HeapNumber> number) {
  TNode<Float64T> value = LoadHeapNumberValue(number);
  TNode<Float64T> float_zero = Float64Constant(0.);
  return Float64GreaterThanOrEqual(value, float_zero);
}

TNode<BoolT> CodeStubAssembler::IsNumberNonNegativeSafeInteger(
    TNode<Number> number) {
  return Select<BoolT>(
      // TODO(cbruni): Introduce TaggedIsNonNegateSmi to avoid confusion.
      TaggedIsSmi(number), [=] { return TaggedIsPositiveSmi(number); },
      [=] {
        TNode<HeapNumber> heap_number = CAST(number);
        return Select<BoolT>(
            IsInteger(heap_number),
            [=] { return IsHeapNumberPositive(heap_number); },
            [=] { return Int32FalseConstant(); });
      });
}

TNode<BoolT> CodeStubAssembler::IsSafeInteger(TNode<Object> number) {
  return Select<BoolT>(
      TaggedIsSmi(number), [=] { return Int32TrueConstant(); },
      [=] {
        return Select<BoolT>(
            IsHeapNumber(CAST(number)),
            [=] { return IsSafeInteger(UncheckedCast<HeapNumber>(number)); },
            [=] { return Int32FalseConstant(); });
      });
}

TNode<BoolT> CodeStubAssembler::IsSafeInteger(TNode<HeapNumber> number) {
  // Load the actual value of {number}.
  TNode<Float64T> number_value = LoadHeapNumberValue(number);
  // Truncate the value of {number} to an integer (or an infinity).
  TNode<Float64T> integer = Float64Trunc(number_value);

  return Select<BoolT>(
      // Check if {number}s value matches the integer (ruling out the
      // infinities).
      Float64Equal(Float64Sub(number_value, integer), Float64Constant(0.0)),
      [=] {
        // Check if the {integer} value is in safe integer range.
        return Float64LessThanOrEqual(Float64Abs(integer),
                                      Float64Constant(kMaxSafeInteger));
      },
      [=] { return Int32FalseConstant(); });
}

TNode<BoolT> CodeStubAssembler::IsInteger(TNode<Object> number) {
  return Select<BoolT>(
      TaggedIsSmi(number), [=] { return Int32TrueConstant(); },
      [=] {
        return Select<BoolT>(
            IsHeapNumber(CAST(number)),
            [=] { return IsInteger(UncheckedCast<HeapNumber>(number)); },
            [=] { return Int32FalseConstant(); });
      });
}

TNode<BoolT> CodeStubAssembler::IsInteger(TNode<HeapNumber> number) {
  TNode<Float64T> number_value = LoadHeapNumberValue(number);
  // Truncate the value of {number} to an integer (or an infinity).
  TNode<Float64T> integer = Float64Trunc(number_value);
  // Check if {number}s value matches the integer (ruling out the infinities).
  return Float64Equal(Float64Sub(number_value, integer), Float64Constant(0.0));
}

TNode<BoolT> CodeStubAssembler::IsHeapNumberUint32(TNode<HeapNumber> number) {
  // Check that the HeapNumber is a valid uint32
  return Select<BoolT>(
      IsHeapNumberPositive(number),
      [=] {
        TNode<Float64T> value = LoadHeapNumberValue(number);
        TNode<Uint32T> int_value = TruncateFloat64ToWord32(value);
        return Float64Equal(value, ChangeUint32ToFloat64(int_value));
      },
      [=] { return Int32FalseConstant(); });
}

TNode<BoolT> CodeStubAssembler::IsNumberArrayIndex(TNode<Number> number) {
  return Select<BoolT>(
      TaggedIsSmi(number), [=] { return TaggedIsPositiveSmi(number); },
      [=] { return IsHeapNumberUint32(CAST(number)); });
}

Node* CodeStubAssembler::FixedArraySizeDoesntFitInNewSpace(Node* element_count,
                                                           int base_size,
                                                           ParameterMode mode) {
  int max_newspace_elements =
      (kMaxRegularHeapObjectSize - base_size) / kTaggedSize;
  return IntPtrOrSmiGreaterThan(
      element_count, IntPtrOrSmiConstant(max_newspace_elements, mode), mode);
}

TNode<Int32T> CodeStubAssembler::StringCharCodeAt(SloppyTNode<String> string,
                                                  SloppyTNode<IntPtrT> index) {
  CSA_ASSERT(this, IsString(string));

  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(index, IntPtrConstant(0)));
  CSA_ASSERT(this, IntPtrLessThan(index, LoadStringLengthAsWord(string)));

  TVARIABLE(Int32T, var_result);

  Label return_result(this), if_runtime(this, Label::kDeferred),
      if_stringistwobyte(this), if_stringisonebyte(this);

  ToDirectStringAssembler to_direct(state(), string);
  to_direct.TryToDirect(&if_runtime);
  Node* const offset = IntPtrAdd(index, to_direct.offset());
  Node* const instance_type = to_direct.instance_type();

  Node* const string_data = to_direct.PointerToData(&if_runtime);

  // Check if the {string} is a TwoByteSeqString or a OneByteSeqString.
  Branch(IsOneByteStringInstanceType(instance_type), &if_stringisonebyte,
         &if_stringistwobyte);

  BIND(&if_stringisonebyte);
  {
    var_result =
        UncheckedCast<Int32T>(Load(MachineType::Uint8(), string_data, offset));
    Goto(&return_result);
  }

  BIND(&if_stringistwobyte);
  {
    var_result =
        UncheckedCast<Int32T>(Load(MachineType::Uint16(), string_data,
                                   WordShl(offset, IntPtrConstant(1))));
    Goto(&return_result);
  }

  BIND(&if_runtime);
  {
    Node* result = CallRuntime(Runtime::kStringCharCodeAt, NoContextConstant(),
                               string, SmiTag(index));
    var_result = SmiToInt32(result);
    Goto(&return_result);
  }

  BIND(&return_result);
  return var_result.value();
}

TNode<String> CodeStubAssembler::StringFromSingleCharCode(TNode<Int32T> code) {
  VARIABLE(var_result, MachineRepresentation::kTagged);

  // Check if the {code} is a one-byte char code.
  Label if_codeisonebyte(this), if_codeistwobyte(this, Label::kDeferred),
      if_done(this);
  Branch(Int32LessThanOrEqual(code, Int32Constant(String::kMaxOneByteCharCode)),
         &if_codeisonebyte, &if_codeistwobyte);
  BIND(&if_codeisonebyte);
  {
    // Load the isolate wide single character string cache.
    TNode<FixedArray> cache =
        CAST(LoadRoot(RootIndex::kSingleCharacterStringCache));
    TNode<IntPtrT> code_index = Signed(ChangeUint32ToWord(code));

    // Check if we have an entry for the {code} in the single character string
    // cache already.
    Label if_entryisundefined(this, Label::kDeferred),
        if_entryisnotundefined(this);
    Node* entry = UnsafeLoadFixedArrayElement(cache, code_index);
    Branch(IsUndefined(entry), &if_entryisundefined, &if_entryisnotundefined);

    BIND(&if_entryisundefined);
    {
      // Allocate a new SeqOneByteString for {code} and store it in the {cache}.
      TNode<String> result = AllocateSeqOneByteString(1);
      StoreNoWriteBarrier(
          MachineRepresentation::kWord8, result,
          IntPtrConstant(SeqOneByteString::kHeaderSize - kHeapObjectTag), code);
      StoreFixedArrayElement(cache, code_index, result);
      var_result.Bind(result);
      Goto(&if_done);
    }

    BIND(&if_entryisnotundefined);
    {
      // Return the entry from the {cache}.
      var_result.Bind(entry);
      Goto(&if_done);
    }
  }

  BIND(&if_codeistwobyte);
  {
    // Allocate a new SeqTwoByteString for {code}.
    Node* result = AllocateSeqTwoByteString(1);
    StoreNoWriteBarrier(
        MachineRepresentation::kWord16, result,
        IntPtrConstant(SeqTwoByteString::kHeaderSize - kHeapObjectTag), code);
    var_result.Bind(result);
    Goto(&if_done);
  }

  BIND(&if_done);
  CSA_ASSERT(this, IsString(var_result.value()));
  return CAST(var_result.value());
}

// A wrapper around CopyStringCharacters which determines the correct string
// encoding, allocates a corresponding sequential string, and then copies the
// given character range using CopyStringCharacters.
// |from_string| must be a sequential string.
// 0 <= |from_index| <= |from_index| + |character_count| < from_string.length.
TNode<String> CodeStubAssembler::AllocAndCopyStringCharacters(
    Node* from, Node* from_instance_type, TNode<IntPtrT> from_index,
    TNode<IntPtrT> character_count) {
  Label end(this), one_byte_sequential(this), two_byte_sequential(this);
  TVARIABLE(String, var_result);

  Branch(IsOneByteStringInstanceType(from_instance_type), &one_byte_sequential,
         &two_byte_sequential);

  // The subject string is a sequential one-byte string.
  BIND(&one_byte_sequential);
  {
    TNode<String> result = AllocateSeqOneByteString(
        NoContextConstant(), Unsigned(TruncateIntPtrToInt32(character_count)));
    CopyStringCharacters(from, result, from_index, IntPtrConstant(0),
                         character_count, String::ONE_BYTE_ENCODING,
                         String::ONE_BYTE_ENCODING);
    var_result = result;
    Goto(&end);
  }

  // The subject string is a sequential two-byte string.
  BIND(&two_byte_sequential);
  {
    TNode<String> result = AllocateSeqTwoByteString(
        NoContextConstant(), Unsigned(TruncateIntPtrToInt32(character_count)));
    CopyStringCharacters(from, result, from_index, IntPtrConstant(0),
                         character_count, String::TWO_BYTE_ENCODING,
                         String::TWO_BYTE_ENCODING);
    var_result = result;
    Goto(&end);
  }

  BIND(&end);
  return var_result.value();
}

TNode<String> CodeStubAssembler::SubString(TNode<String> string,
                                           TNode<IntPtrT> from,
                                           TNode<IntPtrT> to) {
  TVARIABLE(String, var_result);
  ToDirectStringAssembler to_direct(state(), string);
  Label end(this), runtime(this);

  TNode<IntPtrT> const substr_length = IntPtrSub(to, from);
  TNode<IntPtrT> const string_length = LoadStringLengthAsWord(string);

  // Begin dispatching based on substring length.

  Label original_string_or_invalid_length(this);
  GotoIf(UintPtrGreaterThanOrEqual(substr_length, string_length),
         &original_string_or_invalid_length);

  // A real substring (substr_length < string_length).
  Label empty(this);
  GotoIf(IntPtrEqual(substr_length, IntPtrConstant(0)), &empty);

  Label single_char(this);
  GotoIf(IntPtrEqual(substr_length, IntPtrConstant(1)), &single_char);

  // Deal with different string types: update the index if necessary
  // and extract the underlying string.

  TNode<String> direct_string = to_direct.TryToDirect(&runtime);
  TNode<IntPtrT> offset = IntPtrAdd(from, to_direct.offset());
  Node* const instance_type = to_direct.instance_type();

  // The subject string can only be external or sequential string of either
  // encoding at this point.
  Label external_string(this);
  {
    if (FLAG_string_slices) {
      Label next(this);

      // Short slice.  Copy instead of slicing.
      GotoIf(IntPtrLessThan(substr_length,
                            IntPtrConstant(SlicedString::kMinLength)),
             &next);

      // Allocate new sliced string.

      Counters* counters = isolate()->counters();
      IncrementCounter(counters->sub_string_native(), 1);

      Label one_byte_slice(this), two_byte_slice(this);
      Branch(IsOneByteStringInstanceType(to_direct.instance_type()),
             &one_byte_slice, &two_byte_slice);

      BIND(&one_byte_slice);
      {
        var_result = AllocateSlicedOneByteString(
            Unsigned(TruncateIntPtrToInt32(substr_length)), direct_string,
            SmiTag(offset));
        Goto(&end);
      }

      BIND(&two_byte_slice);
      {
        var_result = AllocateSlicedTwoByteString(
            Unsigned(TruncateIntPtrToInt32(substr_length)), direct_string,
            SmiTag(offset));
        Goto(&end);
      }

      BIND(&next);
    }

    // The subject string can only be external or sequential string of either
    // encoding at this point.
    GotoIf(to_direct.is_external(), &external_string);

    var_result = AllocAndCopyStringCharacters(direct_string, instance_type,
                                              offset, substr_length);

    Counters* counters = isolate()->counters();
    IncrementCounter(counters->sub_string_native(), 1);

    Goto(&end);
  }

  // Handle external string.
  BIND(&external_string);
  {
    Node* const fake_sequential_string = to_direct.PointerToString(&runtime);

    var_result = AllocAndCopyStringCharacters(
        fake_sequential_string, instance_type, offset, substr_length);

    Counters* counters = isolate()->counters();
    IncrementCounter(counters->sub_string_native(), 1);

    Goto(&end);
  }

  BIND(&empty);
  {
    var_result = EmptyStringConstant();
    Goto(&end);
  }

  // Substrings of length 1 are generated through CharCodeAt and FromCharCode.
  BIND(&single_char);
  {
    TNode<Int32T> char_code = StringCharCodeAt(string, from);
    var_result = StringFromSingleCharCode(char_code);
    Goto(&end);
  }

  BIND(&original_string_or_invalid_length);
  {
    CSA_ASSERT(this, IntPtrEqual(substr_length, string_length));

    // Equal length - check if {from, to} == {0, str.length}.
    GotoIf(UintPtrGreaterThan(from, IntPtrConstant(0)), &runtime);

    // Return the original string (substr_length == string_length).

    Counters* counters = isolate()->counters();
    IncrementCounter(counters->sub_string_native(), 1);

    var_result = string;
    Goto(&end);
  }

  // Fall back to a runtime call.
  BIND(&runtime);
  {
    var_result =
        CAST(CallRuntime(Runtime::kStringSubstring, NoContextConstant(), string,
                         SmiTag(from), SmiTag(to)));
    Goto(&end);
  }

  BIND(&end);
  return var_result.value();
}

ToDirectStringAssembler::ToDirectStringAssembler(
    compiler::CodeAssemblerState* state, Node* string, Flags flags)
    : CodeStubAssembler(state),
      var_string_(this, MachineRepresentation::kTagged, string),
      var_instance_type_(this, MachineRepresentation::kWord32),
      var_offset_(this, MachineType::PointerRepresentation()),
      var_is_external_(this, MachineRepresentation::kWord32),
      flags_(flags) {
  CSA_ASSERT(this, TaggedIsNotSmi(string));
  CSA_ASSERT(this, IsString(string));

  var_string_.Bind(string);
  var_offset_.Bind(IntPtrConstant(0));
  var_instance_type_.Bind(LoadInstanceType(string));
  var_is_external_.Bind(Int32Constant(0));
}

TNode<String> ToDirectStringAssembler::TryToDirect(Label* if_bailout) {
  VariableList vars({&var_string_, &var_offset_, &var_instance_type_}, zone());
  Label dispatch(this, vars);
  Label if_iscons(this);
  Label if_isexternal(this);
  Label if_issliced(this);
  Label if_isthin(this);
  Label out(this);

  Branch(IsSequentialStringInstanceType(var_instance_type_.value()), &out,
         &dispatch);

  // Dispatch based on string representation.
  BIND(&dispatch);
  {
    int32_t values[] = {
        kSeqStringTag,    kConsStringTag, kExternalStringTag,
        kSlicedStringTag, kThinStringTag,
    };
    Label* labels[] = {
        &out, &if_iscons, &if_isexternal, &if_issliced, &if_isthin,
    };
    STATIC_ASSERT(arraysize(values) == arraysize(labels));

    Node* const representation = Word32And(
        var_instance_type_.value(), Int32Constant(kStringRepresentationMask));
    Switch(representation, if_bailout, values, labels, arraysize(values));
  }

  // Cons string.  Check whether it is flat, then fetch first part.
  // Flat cons strings have an empty second part.
  BIND(&if_iscons);
  {
    Node* const string = var_string_.value();
    GotoIfNot(IsEmptyString(LoadObjectField(string, ConsString::kSecondOffset)),
              if_bailout);

    Node* const lhs = LoadObjectField(string, ConsString::kFirstOffset);
    var_string_.Bind(lhs);
    var_instance_type_.Bind(LoadInstanceType(lhs));

    Goto(&dispatch);
  }

  // Sliced string. Fetch parent and correct start index by offset.
  BIND(&if_issliced);
  {
    if (!FLAG_string_slices || (flags_ & kDontUnpackSlicedStrings)) {
      Goto(if_bailout);
    } else {
      Node* const string = var_string_.value();
      Node* const sliced_offset =
          LoadAndUntagObjectField(string, SlicedString::kOffsetOffset);
      var_offset_.Bind(IntPtrAdd(var_offset_.value(), sliced_offset));

      Node* const parent = LoadObjectField(string, SlicedString::kParentOffset);
      var_string_.Bind(parent);
      var_instance_type_.Bind(LoadInstanceType(parent));

      Goto(&dispatch);
    }
  }

  // Thin string. Fetch the actual string.
  BIND(&if_isthin);
  {
    Node* const string = var_string_.value();
    Node* const actual_string =
        LoadObjectField(string, ThinString::kActualOffset);
    Node* const actual_instance_type = LoadInstanceType(actual_string);

    var_string_.Bind(actual_string);
    var_instance_type_.Bind(actual_instance_type);

    Goto(&dispatch);
  }

  // External string.
  BIND(&if_isexternal);
  var_is_external_.Bind(Int32Constant(1));
  Goto(&out);

  BIND(&out);
  return CAST(var_string_.value());
}

TNode<RawPtrT> ToDirectStringAssembler::TryToSequential(
    StringPointerKind ptr_kind, Label* if_bailout) {
  CHECK(ptr_kind == PTR_TO_DATA || ptr_kind == PTR_TO_STRING);

  TVARIABLE(RawPtrT, var_result);
  Label out(this), if_issequential(this), if_isexternal(this, Label::kDeferred);
  Branch(is_external(), &if_isexternal, &if_issequential);

  BIND(&if_issequential);
  {
    STATIC_ASSERT(SeqOneByteString::kHeaderSize ==
                  SeqTwoByteString::kHeaderSize);
    TNode<IntPtrT> result = BitcastTaggedToWord(var_string_.value());
    if (ptr_kind == PTR_TO_DATA) {
      result = IntPtrAdd(result, IntPtrConstant(SeqOneByteString::kHeaderSize -
                                                kHeapObjectTag));
    }
    var_result = ReinterpretCast<RawPtrT>(result);
    Goto(&out);
  }

  BIND(&if_isexternal);
  {
    GotoIf(IsUncachedExternalStringInstanceType(var_instance_type_.value()),
           if_bailout);

    TNode<String> string = CAST(var_string_.value());
    TNode<IntPtrT> result =
        LoadObjectField<IntPtrT>(string, ExternalString::kResourceDataOffset);
    if (ptr_kind == PTR_TO_STRING) {
      result = IntPtrSub(result, IntPtrConstant(SeqOneByteString::kHeaderSize -
                                                kHeapObjectTag));
    }
    var_result = ReinterpretCast<RawPtrT>(result);
    Goto(&out);
  }

  BIND(&out);
  return var_result.value();
}

void CodeStubAssembler::BranchIfCanDerefIndirectString(Node* string,
                                                       Node* instance_type,
                                                       Label* can_deref,
                                                       Label* cannot_deref) {
  CSA_ASSERT(this, IsString(string));
  Node* representation =
      Word32And(instance_type, Int32Constant(kStringRepresentationMask));
  GotoIf(Word32Equal(representation, Int32Constant(kThinStringTag)), can_deref);
  GotoIf(Word32NotEqual(representation, Int32Constant(kConsStringTag)),
         cannot_deref);
  // Cons string.
  Node* rhs = LoadObjectField(string, ConsString::kSecondOffset);
  GotoIf(IsEmptyString(rhs), can_deref);
  Goto(cannot_deref);
}

Node* CodeStubAssembler::DerefIndirectString(TNode<String> string,
                                             TNode<Int32T> instance_type,
                                             Label* cannot_deref) {
  Label deref(this);
  BranchIfCanDerefIndirectString(string, instance_type, &deref, cannot_deref);
  BIND(&deref);
  STATIC_ASSERT(static_cast<int>(ThinString::kActualOffset) ==
                static_cast<int>(ConsString::kFirstOffset));
  return LoadObjectField(string, ThinString::kActualOffset);
}

void CodeStubAssembler::DerefIndirectString(Variable* var_string,
                                            Node* instance_type) {
#ifdef DEBUG
  Label can_deref(this), cannot_deref(this);
  BranchIfCanDerefIndirectString(var_string->value(), instance_type, &can_deref,
                                 &cannot_deref);
  BIND(&cannot_deref);
  DebugBreak();  // Should be able to dereference string.
  Goto(&can_deref);
  BIND(&can_deref);
#endif  // DEBUG

  STATIC_ASSERT(static_cast<int>(ThinString::kActualOffset) ==
                static_cast<int>(ConsString::kFirstOffset));
  var_string->Bind(
      LoadObjectField(var_string->value(), ThinString::kActualOffset));
}

void CodeStubAssembler::MaybeDerefIndirectString(Variable* var_string,
                                                 Node* instance_type,
                                                 Label* did_deref,
                                                 Label* cannot_deref) {
  Label deref(this);
  BranchIfCanDerefIndirectString(var_string->value(), instance_type, &deref,
                                 cannot_deref);

  BIND(&deref);
  {
    DerefIndirectString(var_string, instance_type);
    Goto(did_deref);
  }
}

void CodeStubAssembler::MaybeDerefIndirectStrings(Variable* var_left,
                                                  Node* left_instance_type,
                                                  Variable* var_right,
                                                  Node* right_instance_type,
                                                  Label* did_something) {
  Label did_nothing_left(this), did_something_left(this),
      didnt_do_anything(this);
  MaybeDerefIndirectString(var_left, left_instance_type, &did_something_left,
                           &did_nothing_left);

  BIND(&did_something_left);
  {
    MaybeDerefIndirectString(var_right, right_instance_type, did_something,
                             did_something);
  }

  BIND(&did_nothing_left);
  {
    MaybeDerefIndirectString(var_right, right_instance_type, did_something,
                             &didnt_do_anything);
  }

  BIND(&didnt_do_anything);
  // Fall through if neither string was an indirect string.
}

TNode<String> CodeStubAssembler::StringAdd(Node* context, TNode<String> left,
                                           TNode<String> right) {
  TVARIABLE(String, result);
  Label check_right(this), runtime(this, Label::kDeferred), cons(this),
      done(this, &result), done_native(this, &result);
  Counters* counters = isolate()->counters();

  TNode<Uint32T> left_length = LoadStringLengthAsWord32(left);
  GotoIfNot(Word32Equal(left_length, Uint32Constant(0)), &check_right);
  result = right;
  Goto(&done_native);

  BIND(&check_right);
  TNode<Uint32T> right_length = LoadStringLengthAsWord32(right);
  GotoIfNot(Word32Equal(right_length, Uint32Constant(0)), &cons);
  result = left;
  Goto(&done_native);

  BIND(&cons);
  {
    TNode<Uint32T> new_length = Uint32Add(left_length, right_length);

    // If new length is greater than String::kMaxLength, goto runtime to
    // throw. Note: we also need to invalidate the string length protector, so
    // can't just throw here directly.
    GotoIf(Uint32GreaterThan(new_length, Uint32Constant(String::kMaxLength)),
           &runtime);

    TVARIABLE(String, var_left, left);
    TVARIABLE(String, var_right, right);
    Variable* input_vars[2] = {&var_left, &var_right};
    Label non_cons(this, 2, input_vars);
    Label slow(this, Label::kDeferred);
    GotoIf(Uint32LessThan(new_length, Uint32Constant(ConsString::kMinLength)),
           &non_cons);

    result =
        AllocateConsString(new_length, var_left.value(), var_right.value());
    Goto(&done_native);

    BIND(&non_cons);

    Comment("Full string concatenate");
    Node* left_instance_type = LoadInstanceType(var_left.value());
    Node* right_instance_type = LoadInstanceType(var_right.value());
    // Compute intersection and difference of instance types.

    Node* ored_instance_types =
        Word32Or(left_instance_type, right_instance_type);
    Node* xored_instance_types =
        Word32Xor(left_instance_type, right_instance_type);

    // Check if both strings have the same encoding and both are sequential.
    GotoIf(IsSetWord32(xored_instance_types, kStringEncodingMask), &runtime);
    GotoIf(IsSetWord32(ored_instance_types, kStringRepresentationMask), &slow);

    TNode<IntPtrT> word_left_length = Signed(ChangeUint32ToWord(left_length));
    TNode<IntPtrT> word_right_length = Signed(ChangeUint32ToWord(right_length));

    Label two_byte(this);
    GotoIf(Word32Equal(Word32And(ored_instance_types,
                                 Int32Constant(kStringEncodingMask)),
                       Int32Constant(kTwoByteStringTag)),
           &two_byte);
    // One-byte sequential string case
    result = AllocateSeqOneByteString(context, new_length);
    CopyStringCharacters(var_left.value(), result.value(), IntPtrConstant(0),
                         IntPtrConstant(0), word_left_length,
                         String::ONE_BYTE_ENCODING, String::ONE_BYTE_ENCODING);
    CopyStringCharacters(var_right.value(), result.value(), IntPtrConstant(0),
                         word_left_length, word_right_length,
                         String::ONE_BYTE_ENCODING, String::ONE_BYTE_ENCODING);
    Goto(&done_native);

    BIND(&two_byte);
    {
      // Two-byte sequential string case
      result = AllocateSeqTwoByteString(context, new_length);
      CopyStringCharacters(var_left.value(), result.value(), IntPtrConstant(0),
                           IntPtrConstant(0), word_left_length,
                           String::TWO_BYTE_ENCODING,
                           String::TWO_BYTE_ENCODING);
      CopyStringCharacters(var_right.value(), result.value(), IntPtrConstant(0),
                           word_left_length, word_right_length,
                           String::TWO_BYTE_ENCODING,
                           String::TWO_BYTE_ENCODING);
      Goto(&done_native);
    }

    BIND(&slow);
    {
      // Try to unwrap indirect strings, restart the above attempt on success.
      MaybeDerefIndirectStrings(&var_left, left_instance_type, &var_right,
                                right_instance_type, &non_cons);
      Goto(&runtime);
    }
  }
  BIND(&runtime);
  {
    result = CAST(CallRuntime(Runtime::kStringAdd, context, left, right));
    Goto(&done);
  }

  BIND(&done_native);
  {
    IncrementCounter(counters->string_add_native(), 1);
    Goto(&done);
  }

  BIND(&done);
  return result.value();
}

TNode<String> CodeStubAssembler::StringFromSingleUTF16EncodedCodePoint(
    TNode<Int32T> codepoint) {
  VARIABLE(var_result, MachineRepresentation::kTagged, EmptyStringConstant());

  Label if_isword16(this), if_isword32(this), return_result(this);

  Branch(Uint32LessThan(codepoint, Int32Constant(0x10000)), &if_isword16,
         &if_isword32);

  BIND(&if_isword16);
  {
    var_result.Bind(StringFromSingleCharCode(codepoint));
    Goto(&return_result);
  }

  BIND(&if_isword32);
  {
    Node* value = AllocateSeqTwoByteString(2);
    StoreNoWriteBarrier(
        MachineRepresentation::kWord32, value,
        IntPtrConstant(SeqTwoByteString::kHeaderSize - kHeapObjectTag),
        codepoint);
    var_result.Bind(value);
    Goto(&return_result);
  }

  BIND(&return_result);
  return CAST(var_result.value());
}

TNode<Number> CodeStubAssembler::StringToNumber(TNode<String> input) {
  Label runtime(this, Label::kDeferred);
  Label end(this);

  TVARIABLE(Number, var_result);

  // Check if string has a cached array index.
  TNode<Uint32T> hash = LoadNameHashField(input);
  GotoIf(IsSetWord32(hash, Name::kDoesNotContainCachedArrayIndexMask),
         &runtime);

  var_result =
      SmiTag(Signed(DecodeWordFromWord32<String::ArrayIndexValueBits>(hash)));
  Goto(&end);

  BIND(&runtime);
  {
    var_result =
        CAST(CallRuntime(Runtime::kStringToNumber, NoContextConstant(), input));
    Goto(&end);
  }

  BIND(&end);
  return var_result.value();
}

TNode<String> CodeStubAssembler::NumberToString(TNode<Number> input) {
  TVARIABLE(String, result);
  TVARIABLE(Smi, smi_input);
  Label runtime(this, Label::kDeferred), if_smi(this), if_heap_number(this),
      done(this, &result);

  // Load the number string cache.
  Node* number_string_cache = LoadRoot(RootIndex::kNumberStringCache);

  // Make the hash mask from the length of the number string cache. It
  // contains two elements (number and string) for each cache entry.
  // TODO(ishell): cleanup mask handling.
  Node* mask =
      BitcastTaggedSignedToWord(LoadFixedArrayBaseLength(number_string_cache));
  TNode<IntPtrT> one = IntPtrConstant(1);
  mask = IntPtrSub(mask, one);

  GotoIfNot(TaggedIsSmi(input), &if_heap_number);
  smi_input = CAST(input);
  Goto(&if_smi);

  BIND(&if_heap_number);
  {
    TNode<HeapNumber> heap_number_input = CAST(input);
    // Try normalizing the HeapNumber.
    TryHeapNumberToSmi(heap_number_input, smi_input, &if_smi);

    // Make a hash from the two 32-bit values of the double.
    TNode<Int32T> low =
        LoadObjectField<Int32T>(heap_number_input, HeapNumber::kValueOffset);
    TNode<Int32T> high = LoadObjectField<Int32T>(
        heap_number_input, HeapNumber::kValueOffset + kIntSize);
    TNode<Word32T> hash = Word32Xor(low, high);
    TNode<WordT> word_hash = WordShl(ChangeInt32ToIntPtr(hash), one);
    TNode<WordT> index =
        WordAnd(word_hash, WordSar(mask, SmiShiftBitsConstant()));

    // Cache entry's key must be a heap number
    Node* number_key =
        UnsafeLoadFixedArrayElement(CAST(number_string_cache), index);
    GotoIf(TaggedIsSmi(number_key), &runtime);
    GotoIfNot(IsHeapNumber(number_key), &runtime);

    // Cache entry's key must match the heap number value we're looking for.
    Node* low_compare = LoadObjectField(number_key, HeapNumber::kValueOffset,
                                        MachineType::Int32());
    Node* high_compare = LoadObjectField(
        number_key, HeapNumber::kValueOffset + kIntSize, MachineType::Int32());
    GotoIfNot(Word32Equal(low, low_compare), &runtime);
    GotoIfNot(Word32Equal(high, high_compare), &runtime);

    // Heap number match, return value from cache entry.
    result = CAST(UnsafeLoadFixedArrayElement(CAST(number_string_cache), index,
                                              kTaggedSize));
    Goto(&done);
  }

  BIND(&if_smi);
  {
    // Load the smi key, make sure it matches the smi we're looking for.
    Node* smi_index = BitcastWordToTagged(WordAnd(
        WordShl(BitcastTaggedSignedToWord(smi_input.value()), one), mask));
    Node* smi_key = UnsafeLoadFixedArrayElement(CAST(number_string_cache),
                                                smi_index, 0, SMI_PARAMETERS);
    GotoIf(WordNotEqual(smi_key, smi_input.value()), &runtime);

    // Smi match, return value from cache entry.
    result = CAST(UnsafeLoadFixedArrayElement(
        CAST(number_string_cache), smi_index, kTaggedSize, SMI_PARAMETERS));
    Goto(&done);
  }

  BIND(&runtime);
  {
    // No cache entry, go to the runtime.
    result =
        CAST(CallRuntime(Runtime::kNumberToString, NoContextConstant(), input));
    Goto(&done);
  }
  BIND(&done);
  return result.value();
}

Node* CodeStubAssembler::NonNumberToNumberOrNumeric(
    Node* context, Node* input, Object::Conversion mode,
    BigIntHandling bigint_handling) {
  CSA_ASSERT(this, Word32BinaryNot(TaggedIsSmi(input)));
  CSA_ASSERT(this, Word32BinaryNot(IsHeapNumber(input)));

  // We might need to loop once here due to ToPrimitive conversions.
  VARIABLE(var_input, MachineRepresentation::kTagged, input);
  VARIABLE(var_result, MachineRepresentation::kTagged);
  Label loop(this, &var_input);
  Label end(this);
  Goto(&loop);
  BIND(&loop);
  {
    // Load the current {input} value (known to be a HeapObject).
    Node* input = var_input.value();

    // Dispatch on the {input} instance type.
    Node* input_instance_type = LoadInstanceType(input);
    Label if_inputisstring(this), if_inputisoddball(this),
        if_inputisbigint(this), if_inputisreceiver(this, Label::kDeferred),
        if_inputisother(this, Label::kDeferred);
    GotoIf(IsStringInstanceType(input_instance_type), &if_inputisstring);
    GotoIf(IsBigIntInstanceType(input_instance_type), &if_inputisbigint);
    GotoIf(InstanceTypeEqual(input_instance_type, ODDBALL_TYPE),
           &if_inputisoddball);
    Branch(IsJSReceiverInstanceType(input_instance_type), &if_inputisreceiver,
           &if_inputisother);

    BIND(&if_inputisstring);
    {
      // The {input} is a String, use the fast stub to convert it to a Number.
      TNode<String> string_input = CAST(input);
      var_result.Bind(StringToNumber(string_input));
      Goto(&end);
    }

    BIND(&if_inputisbigint);
    if (mode == Object::Conversion::kToNumeric) {
      var_result.Bind(input);
      Goto(&end);
    } else {
      DCHECK_EQ(mode, Object::Conversion::kToNumber);
      if (bigint_handling == BigIntHandling::kThrow) {
        Goto(&if_inputisother);
      } else {
        DCHECK_EQ(bigint_handling, BigIntHandling::kConvertToNumber);
        var_result.Bind(CallRuntime(Runtime::kBigIntToNumber, context, input));
        Goto(&end);
      }
    }

    BIND(&if_inputisoddball);
    {
      // The {input} is an Oddball, we just need to load the Number value of it.
      var_result.Bind(LoadObjectField(input, Oddball::kToNumberOffset));
      Goto(&end);
    }

    BIND(&if_inputisreceiver);
    {
      // The {input} is a JSReceiver, we need to convert it to a Primitive first
      // using the ToPrimitive type conversion, preferably yielding a Number.
      Callable callable = CodeFactory::NonPrimitiveToPrimitive(
          isolate(), ToPrimitiveHint::kNumber);
      Node* result = CallStub(callable, context, input);

      // Check if the {result} is already a Number/Numeric.
      Label if_done(this), if_notdone(this);
      Branch(mode == Object::Conversion::kToNumber ? IsNumber(result)
                                                   : IsNumeric(result),
             &if_done, &if_notdone);

      BIND(&if_done);
      {
        // The ToPrimitive conversion already gave us a Number/Numeric, so we're
        // done.
        var_result.Bind(result);
        Goto(&end);
      }

      BIND(&if_notdone);
      {
        // We now have a Primitive {result}, but it's not yet a Number/Numeric.
        var_input.Bind(result);
        Goto(&loop);
      }
    }

    BIND(&if_inputisother);
    {
      // The {input} is something else (e.g. Symbol), let the runtime figure
      // out the correct exception.
      // Note: We cannot tail call to the runtime here, as js-to-wasm
      // trampolines also use this code currently, and they declare all
      // outgoing parameters as untagged, while we would push a tagged
      // object here.
      auto function_id = mode == Object::Conversion::kToNumber
                             ? Runtime::kToNumber
                             : Runtime::kToNumeric;
      var_result.Bind(CallRuntime(function_id, context, input));
      Goto(&end);
    }
  }

  BIND(&end);
  if (mode == Object::Conversion::kToNumeric) {
    CSA_ASSERT(this, IsNumeric(var_result.value()));
  } else {
    DCHECK_EQ(mode, Object::Conversion::kToNumber);
    CSA_ASSERT(this, IsNumber(var_result.value()));
  }
  return var_result.value();
}

TNode<Number> CodeStubAssembler::NonNumberToNumber(
    SloppyTNode<Context> context, SloppyTNode<HeapObject> input,
    BigIntHandling bigint_handling) {
  return CAST(NonNumberToNumberOrNumeric(
      context, input, Object::Conversion::kToNumber, bigint_handling));
}

TNode<Numeric> CodeStubAssembler::NonNumberToNumeric(
    SloppyTNode<Context> context, SloppyTNode<HeapObject> input) {
  Node* result = NonNumberToNumberOrNumeric(context, input,
                                            Object::Conversion::kToNumeric);
  CSA_SLOW_ASSERT(this, IsNumeric(result));
  return UncheckedCast<Numeric>(result);
}

TNode<Number> CodeStubAssembler::ToNumber_Inline(SloppyTNode<Context> context,
                                                 SloppyTNode<Object> input) {
  TVARIABLE(Number, var_result);
  Label end(this), not_smi(this, Label::kDeferred);

  GotoIfNot(TaggedIsSmi(input), &not_smi);
  var_result = CAST(input);
  Goto(&end);

  BIND(&not_smi);
  {
    var_result = Select<Number>(
        IsHeapNumber(CAST(input)), [=] { return CAST(input); },
        [=] {
          return CAST(
              CallBuiltin(Builtins::kNonNumberToNumber, context, input));
        });
    Goto(&end);
  }

  BIND(&end);
  return var_result.value();
}

TNode<Number> CodeStubAssembler::ToNumber(SloppyTNode<Context> context,
                                          SloppyTNode<Object> input,
                                          BigIntHandling bigint_handling) {
  TVARIABLE(Number, var_result);
  Label end(this);

  Label not_smi(this, Label::kDeferred);
  GotoIfNot(TaggedIsSmi(input), &not_smi);
  TNode<Smi> input_smi = CAST(input);
  var_result = input_smi;
  Goto(&end);

  BIND(&not_smi);
  {
    Label not_heap_number(this, Label::kDeferred);
    TNode<HeapObject> input_ho = CAST(input);
    GotoIfNot(IsHeapNumber(input_ho), &not_heap_number);

    TNode<HeapNumber> input_hn = CAST(input_ho);
    var_result = input_hn;
    Goto(&end);

    BIND(&not_heap_number);
    {
      var_result = NonNumberToNumber(context, input_ho, bigint_handling);
      Goto(&end);
    }
  }

  BIND(&end);
  return var_result.value();
}

TNode<BigInt> CodeStubAssembler::ToBigInt(SloppyTNode<Context> context,
                                          SloppyTNode<Object> input) {
  TVARIABLE(BigInt, var_result);
  Label if_bigint(this), done(this), if_throw(this);

  GotoIf(TaggedIsSmi(input), &if_throw);
  GotoIf(IsBigInt(CAST(input)), &if_bigint);
  var_result = CAST(CallRuntime(Runtime::kToBigInt, context, input));
  Goto(&done);

  BIND(&if_bigint);
  var_result = CAST(input);
  Goto(&done);

  BIND(&if_throw);
  ThrowTypeError(context, MessageTemplate::kBigIntFromObject, input);

  BIND(&done);
  return var_result.value();
}

void CodeStubAssembler::TaggedToNumeric(Node* context, Node* value, Label* done,
                                        Variable* var_numeric) {
  TaggedToNumeric(context, value, done, var_numeric, nullptr);
}

void CodeStubAssembler::TaggedToNumericWithFeedback(Node* context, Node* value,
                                                    Label* done,
                                                    Variable* var_numeric,
                                                    Variable* var_feedback) {
  DCHECK_NOT_NULL(var_feedback);
  TaggedToNumeric(context, value, done, var_numeric, var_feedback);
}

void CodeStubAssembler::TaggedToNumeric(Node* context, Node* value, Label* done,
                                        Variable* var_numeric,
                                        Variable* var_feedback) {
  var_numeric->Bind(value);
  Label if_smi(this), if_heapnumber(this), if_bigint(this), if_oddball(this);
  GotoIf(TaggedIsSmi(value), &if_smi);
  Node* map = LoadMap(value);
  GotoIf(IsHeapNumberMap(map), &if_heapnumber);
  Node* instance_type = LoadMapInstanceType(map);
  GotoIf(IsBigIntInstanceType(instance_type), &if_bigint);

  // {value} is not a Numeric yet.
  GotoIf(Word32Equal(instance_type, Int32Constant(ODDBALL_TYPE)), &if_oddball);
  var_numeric->Bind(CallBuiltin(Builtins::kNonNumberToNumeric, context, value));
  OverwriteFeedback(var_feedback, BinaryOperationFeedback::kAny);
  Goto(done);

  BIND(&if_smi);
  OverwriteFeedback(var_feedback, BinaryOperationFeedback::kSignedSmall);
  Goto(done);

  BIND(&if_heapnumber);
  OverwriteFeedback(var_feedback, BinaryOperationFeedback::kNumber);
  Goto(done);

  BIND(&if_bigint);
  OverwriteFeedback(var_feedback, BinaryOperationFeedback::kBigInt);
  Goto(done);

  BIND(&if_oddball);
  OverwriteFeedback(var_feedback, BinaryOperationFeedback::kNumberOrOddball);
  var_numeric->Bind(LoadObjectField(value, Oddball::kToNumberOffset));
  Goto(done);
}

// ES#sec-touint32
TNode<Number> CodeStubAssembler::ToUint32(SloppyTNode<Context> context,
                                          SloppyTNode<Object> input) {
  Node* const float_zero = Float64Constant(0.0);
  Node* const float_two_32 = Float64Constant(static_cast<double>(1ULL << 32));

  Label out(this);

  VARIABLE(var_result, MachineRepresentation::kTagged, input);

  // Early exit for positive smis.
  {
    // TODO(jgruber): This branch and the recheck below can be removed once we
    // have a ToNumber with multiple exits.
    Label next(this, Label::kDeferred);
    Branch(TaggedIsPositiveSmi(input), &out, &next);
    BIND(&next);
  }

  Node* const number = ToNumber(context, input);
  var_result.Bind(number);

  // Perhaps we have a positive smi now.
  {
    Label next(this, Label::kDeferred);
    Branch(TaggedIsPositiveSmi(number), &out, &next);
    BIND(&next);
  }

  Label if_isnegativesmi(this), if_isheapnumber(this);
  Branch(TaggedIsSmi(number), &if_isnegativesmi, &if_isheapnumber);

  BIND(&if_isnegativesmi);
  {
    Node* const uint32_value = SmiToInt32(number);
    Node* float64_value = ChangeUint32ToFloat64(uint32_value);
    var_result.Bind(AllocateHeapNumberWithValue(float64_value));
    Goto(&out);
  }

  BIND(&if_isheapnumber);
  {
    Label return_zero(this);
    Node* const value = LoadHeapNumberValue(number);

    {
      // +-0.
      Label next(this);
      Branch(Float64Equal(value, float_zero), &return_zero, &next);
      BIND(&next);
    }

    {
      // NaN.
      Label next(this);
      Branch(Float64Equal(value, value), &next, &return_zero);
      BIND(&next);
    }

    {
      // +Infinity.
      Label next(this);
      Node* const positive_infinity =
          Float64Constant(std::numeric_limits<double>::infinity());
      Branch(Float64Equal(value, positive_infinity), &return_zero, &next);
      BIND(&next);
    }

    {
      // -Infinity.
      Label next(this);
      Node* const negative_infinity =
          Float64Constant(-1.0 * std::numeric_limits<double>::infinity());
      Branch(Float64Equal(value, negative_infinity), &return_zero, &next);
      BIND(&next);
    }

    // * Let int be the mathematical value that is the same sign as number and
    //   whose magnitude is floor(abs(number)).
    // * Let int32bit be int modulo 2^32.
    // * Return int32bit.
    {
      Node* x = Float64Trunc(value);
      x = Float64Mod(x, float_two_32);
      x = Float64Add(x, float_two_32);
      x = Float64Mod(x, float_two_32);

      Node* const result = ChangeFloat64ToTagged(x);
      var_result.Bind(result);
      Goto(&out);
    }

    BIND(&return_zero);
    {
      var_result.Bind(SmiConstant(0));
      Goto(&out);
    }
  }

  BIND(&out);
  return CAST(var_result.value());
}

TNode<String> CodeStubAssembler::ToString_Inline(SloppyTNode<Context> context,
                                                 SloppyTNode<Object> input) {
  VARIABLE(var_result, MachineRepresentation::kTagged, input);
  Label stub_call(this, Label::kDeferred), out(this);

  GotoIf(TaggedIsSmi(input), &stub_call);
  Branch(IsString(CAST(input)), &out, &stub_call);

  BIND(&stub_call);
  var_result.Bind(CallBuiltin(Builtins::kToString, context, input));
  Goto(&out);

  BIND(&out);
  return CAST(var_result.value());
}

Node* CodeStubAssembler::JSReceiverToPrimitive(Node* context, Node* input) {
  Label if_isreceiver(this, Label::kDeferred), if_isnotreceiver(this);
  VARIABLE(result, MachineRepresentation::kTagged);
  Label done(this, &result);

  BranchIfJSReceiver(input, &if_isreceiver, &if_isnotreceiver);

  BIND(&if_isreceiver);
  {
    // Convert {input} to a primitive first passing Number hint.
    Callable callable = CodeFactory::NonPrimitiveToPrimitive(isolate());
    result.Bind(CallStub(callable, context, input));
    Goto(&done);
  }

  BIND(&if_isnotreceiver);
  {
    result.Bind(input);
    Goto(&done);
  }

  BIND(&done);
  return result.value();
}

TNode<JSReceiver> CodeStubAssembler::ToObject(SloppyTNode<Context> context,
                                              SloppyTNode<Object> input) {
  return CAST(CallBuiltin(Builtins::kToObject, context, input));
}

TNode<JSReceiver> CodeStubAssembler::ToObject_Inline(TNode<Context> context,
                                                     TNode<Object> input) {
  TVARIABLE(JSReceiver, result);
  Label if_isreceiver(this), if_isnotreceiver(this, Label::kDeferred);
  Label done(this);

  BranchIfJSReceiver(input, &if_isreceiver, &if_isnotreceiver);

  BIND(&if_isreceiver);
  {
    result = CAST(input);
    Goto(&done);
  }

  BIND(&if_isnotreceiver);
  {
    result = ToObject(context, input);
    Goto(&done);
  }

  BIND(&done);
  return result.value();
}

TNode<Smi> CodeStubAssembler::ToSmiIndex(TNode<Context> context,
                                         TNode<Object> input,
                                         Label* range_error) {
  TVARIABLE(Smi, result);
  Label check_undefined(this), return_zero(this), defined(this),
      negative_check(this), done(this);

  GotoIfNot(TaggedIsSmi(input), &check_undefined);
  result = CAST(input);
  Goto(&negative_check);

  BIND(&check_undefined);
  Branch(IsUndefined(input), &return_zero, &defined);

  BIND(&defined);
  TNode<Number> integer_input =
      CAST(CallBuiltin(Builtins::kToInteger_TruncateMinusZero, context, input));
  GotoIfNot(TaggedIsSmi(integer_input), range_error);
  result = CAST(integer_input);
  Goto(&negative_check);

  BIND(&negative_check);
  Branch(SmiLessThan(result.value(), SmiConstant(0)), range_error, &done);

  BIND(&return_zero);
  result = SmiConstant(0);
  Goto(&done);

  BIND(&done);
  return result.value();
}

TNode<Smi> CodeStubAssembler::ToSmiLength(TNode<Context> context,
                                          TNode<Object> input,
                                          Label* range_error) {
  TVARIABLE(Smi, result);
  Label to_integer(this), negative_check(this),
      heap_number_negative_check(this), return_zero(this), done(this);

  GotoIfNot(TaggedIsSmi(input), &to_integer);
  result = CAST(input);
  Goto(&negative_check);

  BIND(&to_integer);
  {
    TNode<Number> integer_input = CAST(
        CallBuiltin(Builtins::kToInteger_TruncateMinusZero, context, input));
    GotoIfNot(TaggedIsSmi(integer_input), &heap_number_negative_check);
    result = CAST(integer_input);
    Goto(&negative_check);

    // integer_input can still be a negative HeapNumber here.
    BIND(&heap_number_negative_check);
    TNode<HeapNumber> heap_number_input = CAST(integer_input);
    Branch(IsTrue(CallBuiltin(Builtins::kLessThan, context, heap_number_input,
                              SmiConstant(0))),
           &return_zero, range_error);
  }

  BIND(&negative_check);
  Branch(SmiLessThan(result.value(), SmiConstant(0)), &return_zero, &done);

  BIND(&return_zero);
  result = SmiConstant(0);
  Goto(&done);

  BIND(&done);
  return result.value();
}

TNode<Number> CodeStubAssembler::ToLength_Inline(SloppyTNode<Context> context,
                                                 SloppyTNode<Object> input) {
  TNode<Smi> smi_zero = SmiConstant(0);
  return Select<Number>(
      TaggedIsSmi(input), [=] { return SmiMax(CAST(input), smi_zero); },
      [=] { return CAST(CallBuiltin(Builtins::kToLength, context, input)); });
}

TNode<Number> CodeStubAssembler::ToInteger_Inline(
    SloppyTNode<Context> context, SloppyTNode<Object> input,
    ToIntegerTruncationMode mode) {
  Builtins::Name builtin = (mode == kNoTruncation)
                               ? Builtins::kToInteger
                               : Builtins::kToInteger_TruncateMinusZero;
  return Select<Number>(
      TaggedIsSmi(input), [=] { return CAST(input); },
      [=] { return CAST(CallBuiltin(builtin, context, input)); });
}

TNode<Number> CodeStubAssembler::ToInteger(SloppyTNode<Context> context,
                                           SloppyTNode<Object> input,
                                           ToIntegerTruncationMode mode) {
  // We might need to loop once for ToNumber conversion.
  TVARIABLE(Object, var_arg, input);
  Label loop(this, &var_arg), out(this);
  Goto(&loop);
  BIND(&loop);
  {
    // Shared entry points.
    Label return_zero(this, Label::kDeferred);

    // Load the current {arg} value.
    TNode<Object> arg = var_arg.value();

    // Check if {arg} is a Smi.
    GotoIf(TaggedIsSmi(arg), &out);

    // Check if {arg} is a HeapNumber.
    Label if_argisheapnumber(this),
        if_argisnotheapnumber(this, Label::kDeferred);
    Branch(IsHeapNumber(CAST(arg)), &if_argisheapnumber,
           &if_argisnotheapnumber);

    BIND(&if_argisheapnumber);
    {
      TNode<HeapNumber> arg_hn = CAST(arg);
      // Load the floating-point value of {arg}.
      Node* arg_value = LoadHeapNumberValue(arg_hn);

      // Check if {arg} is NaN.
      GotoIfNot(Float64Equal(arg_value, arg_value), &return_zero);

      // Truncate {arg} towards zero.
      TNode<Float64T> value = Float64Trunc(arg_value);

      if (mode == kTruncateMinusZero) {
        // Truncate -0.0 to 0.
        GotoIf(Float64Equal(value, Float64Constant(0.0)), &return_zero);
      }

      var_arg = ChangeFloat64ToTagged(value);
      Goto(&out);
    }

    BIND(&if_argisnotheapnumber);
    {
      // Need to convert {arg} to a Number first.
      var_arg = UncheckedCast<Object>(
          CallBuiltin(Builtins::kNonNumberToNumber, context, arg));
      Goto(&loop);
    }

    BIND(&return_zero);
    var_arg = SmiConstant(0);
    Goto(&out);
  }

  BIND(&out);
  if (mode == kTruncateMinusZero) {
    CSA_ASSERT(this, IsNumberNormalized(CAST(var_arg.value())));
  }
  return CAST(var_arg.value());
}

TNode<Uint32T> CodeStubAssembler::DecodeWord32(SloppyTNode<Word32T> word32,
                                               uint32_t shift, uint32_t mask) {
  return UncheckedCast<Uint32T>(Word32Shr(
      Word32And(word32, Int32Constant(mask)), static_cast<int>(shift)));
}

TNode<UintPtrT> CodeStubAssembler::DecodeWord(SloppyTNode<WordT> word,
                                              uint32_t shift, uint32_t mask) {
  return Unsigned(
      WordShr(WordAnd(word, IntPtrConstant(mask)), static_cast<int>(shift)));
}

TNode<WordT> CodeStubAssembler::UpdateWord(TNode<WordT> word,
                                           TNode<WordT> value, uint32_t shift,
                                           uint32_t mask) {
  TNode<WordT> encoded_value = WordShl(value, static_cast<int>(shift));
  TNode<IntPtrT> inverted_mask = IntPtrConstant(~static_cast<intptr_t>(mask));
  // Ensure the {value} fits fully in the mask.
  CSA_ASSERT(this, WordEqual(WordAnd(encoded_value, inverted_mask),
                             IntPtrConstant(0)));
  return WordOr(WordAnd(word, inverted_mask), encoded_value);
}

void CodeStubAssembler::SetCounter(StatsCounter* counter, int value) {
  if (FLAG_native_code_counters && counter->Enabled()) {
    Node* counter_address =
        ExternalConstant(ExternalReference::Create(counter));
    StoreNoWriteBarrier(MachineRepresentation::kWord32, counter_address,
                        Int32Constant(value));
  }
}

void CodeStubAssembler::IncrementCounter(StatsCounter* counter, int delta) {
  DCHECK_GT(delta, 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Node* counter_address =
        ExternalConstant(ExternalReference::Create(counter));
    // This operation has to be exactly 32-bit wide in case the external
    // reference table redirects the counter to a uint32_t dummy_stats_counter_
    // field.
    Node* value = Load(MachineType::Int32(), counter_address);
    value = Int32Add(value, Int32Constant(delta));
    StoreNoWriteBarrier(MachineRepresentation::kWord32, counter_address, value);
  }
}

void CodeStubAssembler::DecrementCounter(StatsCounter* counter, int delta) {
  DCHECK_GT(delta, 0);
  if (FLAG_native_code_counters && counter->Enabled()) {
    Node* counter_address =
        ExternalConstant(ExternalReference::Create(counter));
    // This operation has to be exactly 32-bit wide in case the external
    // reference table redirects the counter to a uint32_t dummy_stats_counter_
    // field.
    Node* value = Load(MachineType::Int32(), counter_address);
    value = Int32Sub(value, Int32Constant(delta));
    StoreNoWriteBarrier(MachineRepresentation::kWord32, counter_address, value);
  }
}

void CodeStubAssembler::Increment(Variable* variable, int value,
                                  ParameterMode mode) {
  DCHECK_IMPLIES(mode == INTPTR_PARAMETERS,
                 variable->rep() == MachineType::PointerRepresentation());
  DCHECK_IMPLIES(mode == SMI_PARAMETERS, CanBeTaggedSigned(variable->rep()));
  variable->Bind(IntPtrOrSmiAdd(variable->value(),
                                IntPtrOrSmiConstant(value, mode), mode));
}

void CodeStubAssembler::Use(Label* label) {
  GotoIf(Word32Equal(Int32Constant(0), Int32Constant(1)), label);
}

void CodeStubAssembler::TryToName(Node* key, Label* if_keyisindex,
                                  Variable* var_index, Label* if_keyisunique,
                                  Variable* var_unique, Label* if_bailout,
                                  Label* if_notinternalized) {
  DCHECK_EQ(MachineType::PointerRepresentation(), var_index->rep());
  DCHECK_EQ(MachineRepresentation::kTagged, var_unique->rep());
  Comment("TryToName");

  Label if_hascachedindex(this), if_keyisnotindex(this), if_thinstring(this),
      if_keyisother(this, Label::kDeferred);
  // Handle Smi and HeapNumber keys.
  var_index->Bind(TryToIntptr(key, &if_keyisnotindex));
  Goto(if_keyisindex);

  BIND(&if_keyisnotindex);
  Node* key_map = LoadMap(key);
  var_unique->Bind(key);
  // Symbols are unique.
  GotoIf(IsSymbolMap(key_map), if_keyisunique);
  Node* key_instance_type = LoadMapInstanceType(key_map);
  // Miss if |key| is not a String.
  STATIC_ASSERT(FIRST_NAME_TYPE == FIRST_TYPE);
  GotoIfNot(IsStringInstanceType(key_instance_type), &if_keyisother);

  // |key| is a String. Check if it has a cached array index.
  Node* hash = LoadNameHashField(key);
  GotoIf(IsClearWord32(hash, Name::kDoesNotContainCachedArrayIndexMask),
         &if_hascachedindex);
  // No cached array index. If the string knows that it contains an index,
  // then it must be an uncacheable index. Handle this case in the runtime.
  GotoIf(IsClearWord32(hash, Name::kIsNotArrayIndexMask), if_bailout);
  // Check if we have a ThinString.
  GotoIf(InstanceTypeEqual(key_instance_type, THIN_STRING_TYPE),
         &if_thinstring);
  GotoIf(InstanceTypeEqual(key_instance_type, THIN_ONE_BYTE_STRING_TYPE),
         &if_thinstring);
  // Finally, check if |key| is internalized.
  STATIC_ASSERT(kNotInternalizedTag != 0);
  GotoIf(IsSetWord32(key_instance_type, kIsNotInternalizedMask),
         if_notinternalized != nullptr ? if_notinternalized : if_bailout);
  Goto(if_keyisunique);

  BIND(&if_thinstring);
  var_unique->Bind(LoadObjectField(key, ThinString::kActualOffset));
  Goto(if_keyisunique);

  BIND(&if_hascachedindex);
  var_index->Bind(DecodeWordFromWord32<Name::ArrayIndexValueBits>(hash));
  Goto(if_keyisindex);

  BIND(&if_keyisother);
  GotoIfNot(InstanceTypeEqual(key_instance_type, ODDBALL_TYPE), if_bailout);
  var_unique->Bind(LoadObjectField(key, Oddball::kToStringOffset));
  Goto(if_keyisunique);
}

void CodeStubAssembler::TryInternalizeString(
    Node* string, Label* if_index, Variable* var_index, Label* if_internalized,
    Variable* var_internalized, Label* if_not_internalized, Label* if_bailout) {
  DCHECK(var_index->rep() == MachineType::PointerRepresentation());
  DCHECK_EQ(var_internalized->rep(), MachineRepresentation::kTagged);
  CSA_SLOW_ASSERT(this, IsString(string));
  Node* function =
      ExternalConstant(ExternalReference::try_internalize_string_function());
  Node* const isolate_ptr =
      ExternalConstant(ExternalReference::isolate_address(isolate()));
  Node* result =
      CallCFunction(function, MachineType::AnyTagged(),
                    std::make_pair(MachineType::Pointer(), isolate_ptr),
                    std::make_pair(MachineType::AnyTagged(), string));
  Label internalized(this);
  GotoIf(TaggedIsNotSmi(result), &internalized);
  Node* word_result = SmiUntag(result);
  GotoIf(WordEqual(word_result, IntPtrConstant(ResultSentinel::kNotFound)),
         if_not_internalized);
  GotoIf(WordEqual(word_result, IntPtrConstant(ResultSentinel::kUnsupported)),
         if_bailout);
  var_index->Bind(word_result);
  Goto(if_index);

  BIND(&internalized);
  var_internalized->Bind(result);
  Goto(if_internalized);
}

template <typename Dictionary>
TNode<IntPtrT> CodeStubAssembler::EntryToIndex(TNode<IntPtrT> entry,
                                               int field_index) {
  TNode<IntPtrT> entry_index =
      IntPtrMul(entry, IntPtrConstant(Dictionary::kEntrySize));
  return IntPtrAdd(entry_index, IntPtrConstant(Dictionary::kElementsStartIndex +
                                               field_index));
}

template <typename T>
TNode<T> CodeStubAssembler::LoadDescriptorArrayElement(
    TNode<DescriptorArray> object, TNode<IntPtrT> index,
    int additional_offset) {
  return LoadArrayElement<DescriptorArray, T>(
      object, DescriptorArray::kHeaderSize, index, additional_offset);
}

TNode<Name> CodeStubAssembler::LoadKeyByKeyIndex(
    TNode<DescriptorArray> container, TNode<IntPtrT> key_index) {
  return CAST(LoadDescriptorArrayElement<HeapObject>(container, key_index, 0));
}

TNode<Uint32T> CodeStubAssembler::LoadDetailsByKeyIndex(
    TNode<DescriptorArray> container, TNode<IntPtrT> key_index) {
  const int kKeyToDetailsOffset =
      DescriptorArray::kEntryDetailsOffset - DescriptorArray::kEntryKeyOffset;
  return Unsigned(LoadAndUntagToWord32ArrayElement(
      container, DescriptorArray::kHeaderSize, key_index, kKeyToDetailsOffset));
}

TNode<Object> CodeStubAssembler::LoadValueByKeyIndex(
    TNode<DescriptorArray> container, TNode<IntPtrT> key_index) {
  const int kKeyToValueOffset =
      DescriptorArray::kEntryValueOffset - DescriptorArray::kEntryKeyOffset;
  return LoadDescriptorArrayElement<Object>(container, key_index,
                                            kKeyToValueOffset);
}

TNode<MaybeObject> CodeStubAssembler::LoadFieldTypeByKeyIndex(
    TNode<DescriptorArray> container, TNode<IntPtrT> key_index) {
  const int kKeyToValueOffset =
      DescriptorArray::kEntryValueOffset - DescriptorArray::kEntryKeyOffset;
  return LoadDescriptorArrayElement<MaybeObject>(container, key_index,
                                                 kKeyToValueOffset);
}

TNode<IntPtrT> CodeStubAssembler::DescriptorEntryToIndex(
    TNode<IntPtrT> descriptor_entry) {
  return IntPtrMul(descriptor_entry,
                   IntPtrConstant(DescriptorArray::kEntrySize));
}

TNode<Name> CodeStubAssembler::LoadKeyByDescriptorEntry(
    TNode<DescriptorArray> container, TNode<IntPtrT> descriptor_entry) {
  return CAST(LoadDescriptorArrayElement<HeapObject>(
      container, DescriptorEntryToIndex(descriptor_entry),
      DescriptorArray::ToKeyIndex(0) * kTaggedSize));
}

TNode<Name> CodeStubAssembler::LoadKeyByDescriptorEntry(
    TNode<DescriptorArray> container, int descriptor_entry) {
  return CAST(LoadDescriptorArrayElement<HeapObject>(
      container, IntPtrConstant(0),
      DescriptorArray::ToKeyIndex(descriptor_entry) * kTaggedSize));
}

TNode<Uint32T> CodeStubAssembler::LoadDetailsByDescriptorEntry(
    TNode<DescriptorArray> container, TNode<IntPtrT> descriptor_entry) {
  return Unsigned(LoadAndUntagToWord32ArrayElement(
      container, DescriptorArray::kHeaderSize,
      DescriptorEntryToIndex(descriptor_entry),
      DescriptorArray::ToDetailsIndex(0) * kTaggedSize));
}

TNode<Uint32T> CodeStubAssembler::LoadDetailsByDescriptorEntry(
    TNode<DescriptorArray> container, int descriptor_entry) {
  return Unsigned(LoadAndUntagToWord32ArrayElement(
      container, DescriptorArray::kHeaderSize, IntPtrConstant(0),
      DescriptorArray::ToDetailsIndex(descriptor_entry) * kTaggedSize));
}

TNode<Object> CodeStubAssembler::LoadValueByDescriptorEntry(
    TNode<DescriptorArray> container, int descriptor_entry) {
  return LoadDescriptorArrayElement<Object>(
      container, IntPtrConstant(0),
      DescriptorArray::ToValueIndex(descriptor_entry) * kTaggedSize);
}

TNode<MaybeObject> CodeStubAssembler::LoadFieldTypeByDescriptorEntry(
    TNode<DescriptorArray> container, TNode<IntPtrT> descriptor_entry) {
  return LoadDescriptorArrayElement<MaybeObject>(
      container, DescriptorEntryToIndex(descriptor_entry),
      DescriptorArray::ToValueIndex(0) * kTaggedSize);
}

template TNode<IntPtrT> CodeStubAssembler::EntryToIndex<NameDictionary>(
    TNode<IntPtrT>, int);
template TNode<IntPtrT> CodeStubAssembler::EntryToIndex<GlobalDictionary>(
    TNode<IntPtrT>, int);
template TNode<IntPtrT> CodeStubAssembler::EntryToIndex<NumberDictionary>(
    TNode<IntPtrT>, int);

// This must be kept in sync with HashTableBase::ComputeCapacity().
TNode<IntPtrT> CodeStubAssembler::HashTableComputeCapacity(
    TNode<IntPtrT> at_least_space_for) {
  TNode<IntPtrT> capacity = IntPtrRoundUpToPowerOfTwo32(
      IntPtrAdd(at_least_space_for, WordShr(at_least_space_for, 1)));
  return IntPtrMax(capacity, IntPtrConstant(HashTableBase::kMinCapacity));
}

TNode<IntPtrT> CodeStubAssembler::IntPtrMax(SloppyTNode<IntPtrT> left,
                                            SloppyTNode<IntPtrT> right) {
  intptr_t left_constant;
  intptr_t right_constant;
  if (ToIntPtrConstant(left, left_constant) &&
      ToIntPtrConstant(right, right_constant)) {
    return IntPtrConstant(std::max(left_constant, right_constant));
  }
  return SelectConstant<IntPtrT>(IntPtrGreaterThanOrEqual(left, right), left,
                                 right);
}

TNode<IntPtrT> CodeStubAssembler::IntPtrMin(SloppyTNode<IntPtrT> left,
                                            SloppyTNode<IntPtrT> right) {
  intptr_t left_constant;
  intptr_t right_constant;
  if (ToIntPtrConstant(left, left_constant) &&
      ToIntPtrConstant(right, right_constant)) {
    return IntPtrConstant(std::min(left_constant, right_constant));
  }
  return SelectConstant<IntPtrT>(IntPtrLessThanOrEqual(left, right), left,
                                 right);
}

template <>
TNode<HeapObject> CodeStubAssembler::LoadName<NameDictionary>(
    TNode<HeapObject> key) {
  CSA_ASSERT(this, Word32Or(IsTheHole(key), IsName(key)));
  return key;
}

template <>
TNode<HeapObject> CodeStubAssembler::LoadName<GlobalDictionary>(
    TNode<HeapObject> key) {
  TNode<PropertyCell> property_cell = CAST(key);
  return CAST(LoadObjectField(property_cell, PropertyCell::kNameOffset));
}

template <typename Dictionary>
void CodeStubAssembler::NameDictionaryLookup(
    TNode<Dictionary> dictionary, TNode<Name> unique_name, Label* if_found,
    TVariable<IntPtrT>* var_name_index, Label* if_not_found, LookupMode mode) {
  static_assert(std::is_same<Dictionary, NameDictionary>::value ||
                    std::is_same<Dictionary, GlobalDictionary>::value,
                "Unexpected NameDictionary");
  DCHECK_EQ(MachineType::PointerRepresentation(), var_name_index->rep());
  DCHECK_IMPLIES(mode == kFindInsertionIndex, if_found == nullptr);
  Comment("NameDictionaryLookup");
  CSA_ASSERT(this, IsUniqueName(unique_name));

  TNode<IntPtrT> capacity = SmiUntag(GetCapacity<Dictionary>(dictionary));
  TNode<WordT> mask = IntPtrSub(capacity, IntPtrConstant(1));
  TNode<WordT> hash = ChangeUint32ToWord(LoadNameHash(unique_name));

  // See Dictionary::FirstProbe().
  TNode<IntPtrT> count = IntPtrConstant(0);
  TNode<IntPtrT> entry = Signed(WordAnd(hash, mask));
  Node* undefined = UndefinedConstant();

  // Appease the variable merging algorithm for "Goto(&loop)" below.
  *var_name_index = IntPtrConstant(0);

  TVARIABLE(IntPtrT, var_count, count);
  TVARIABLE(IntPtrT, var_entry, entry);
  Variable* loop_vars[] = {&var_count, &var_entry, var_name_index};
  Label loop(this, arraysize(loop_vars), loop_vars);
  Goto(&loop);
  BIND(&loop);
  {
    TNode<IntPtrT> entry = var_entry.value();

    TNode<IntPtrT> index = EntryToIndex<Dictionary>(entry);
    *var_name_index = index;

    TNode<HeapObject> current =
        CAST(UnsafeLoadFixedArrayElement(dictionary, index));
    GotoIf(WordEqual(current, undefined), if_not_found);
    if (mode == kFindExisting) {
      current = LoadName<Dictionary>(current);
      GotoIf(WordEqual(current, unique_name), if_found);
    } else {
      DCHECK_EQ(kFindInsertionIndex, mode);
      GotoIf(WordEqual(current, TheHoleConstant()), if_not_found);
    }

    // See Dictionary::NextProbe().
    Increment(&var_count);
    entry = Signed(WordAnd(IntPtrAdd(entry, var_count.value()), mask));

    var_entry = entry;
    Goto(&loop);
  }
}

// Instantiate template methods to workaround GCC compilation issue.
template V8_EXPORT_PRIVATE void
CodeStubAssembler::NameDictionaryLookup<NameDictionary>(TNode<NameDictionary>,
                                                        TNode<Name>, Label*,
                                                        TVariable<IntPtrT>*,
                                                        Label*, LookupMode);
template V8_EXPORT_PRIVATE void CodeStubAssembler::NameDictionaryLookup<
    GlobalDictionary>(TNode<GlobalDictionary>, TNode<Name>, Label*,
                      TVariable<IntPtrT>*, Label*, LookupMode);

Node* CodeStubAssembler::ComputeUnseededHash(Node* key) {
  // See v8::internal::ComputeUnseededHash()
  Node* hash = TruncateIntPtrToInt32(key);
  hash = Int32Add(Word32Xor(hash, Int32Constant(0xFFFFFFFF)),
                  Word32Shl(hash, Int32Constant(15)));
  hash = Word32Xor(hash, Word32Shr(hash, Int32Constant(12)));
  hash = Int32Add(hash, Word32Shl(hash, Int32Constant(2)));
  hash = Word32Xor(hash, Word32Shr(hash, Int32Constant(4)));
  hash = Int32Mul(hash, Int32Constant(2057));
  hash = Word32Xor(hash, Word32Shr(hash, Int32Constant(16)));
  return Word32And(hash, Int32Constant(0x3FFFFFFF));
}

Node* CodeStubAssembler::ComputeSeededHash(Node* key) {
  Node* const function_addr =
      ExternalConstant(ExternalReference::compute_integer_hash());
  Node* const isolate_ptr =
      ExternalConstant(ExternalReference::isolate_address(isolate()));

  MachineType type_ptr = MachineType::Pointer();
  MachineType type_uint32 = MachineType::Uint32();

  Node* const result = CallCFunction(
      function_addr, type_uint32, std::make_pair(type_ptr, isolate_ptr),
      std::make_pair(type_uint32, TruncateIntPtrToInt32(key)));
  return result;
}

void CodeStubAssembler::NumberDictionaryLookup(
    TNode<NumberDictionary> dictionary, TNode<IntPtrT> intptr_index,
    Label* if_found, TVariable<IntPtrT>* var_entry, Label* if_not_found) {
  CSA_ASSERT(this, IsNumberDictionary(dictionary));
  DCHECK_EQ(MachineType::PointerRepresentation(), var_entry->rep());
  Comment("NumberDictionaryLookup");

  TNode<IntPtrT> capacity = SmiUntag(GetCapacity<NumberDictionary>(dictionary));
  TNode<WordT> mask = IntPtrSub(capacity, IntPtrConstant(1));

  TNode<WordT> hash = ChangeUint32ToWord(ComputeSeededHash(intptr_index));
  Node* key_as_float64 = RoundIntPtrToFloat64(intptr_index);

  // See Dictionary::FirstProbe().
  TNode<IntPtrT> count = IntPtrConstant(0);
  TNode<IntPtrT> entry = Signed(WordAnd(hash, mask));

  Node* undefined = UndefinedConstant();
  Node* the_hole = TheHoleConstant();

  TVARIABLE(IntPtrT, var_count, count);
  Variable* loop_vars[] = {&var_count, var_entry};
  Label loop(this, 2, loop_vars);
  *var_entry = entry;
  Goto(&loop);
  BIND(&loop);
  {
    TNode<IntPtrT> entry = var_entry->value();

    TNode<IntPtrT> index = EntryToIndex<NumberDictionary>(entry);
    Node* current = UnsafeLoadFixedArrayElement(dictionary, index);
    GotoIf(WordEqual(current, undefined), if_not_found);
    Label next_probe(this);
    {
      Label if_currentissmi(this), if_currentisnotsmi(this);
      Branch(TaggedIsSmi(current), &if_currentissmi, &if_currentisnotsmi);
      BIND(&if_currentissmi);
      {
        Node* current_value = SmiUntag(current);
        Branch(WordEqual(current_value, intptr_index), if_found, &next_probe);
      }
      BIND(&if_currentisnotsmi);
      {
        GotoIf(WordEqual(current, the_hole), &next_probe);
        // Current must be the Number.
        Node* current_value = LoadHeapNumberValue(current);
        Branch(Float64Equal(current_value, key_as_float64), if_found,
               &next_probe);
      }
    }

    BIND(&next_probe);
    // See Dictionary::NextProbe().
    Increment(&var_count);
    entry = Signed(WordAnd(IntPtrAdd(entry, var_count.value()), mask));

    *var_entry = entry;
    Goto(&loop);
  }
}

TNode<Object> CodeStubAssembler::BasicLoadNumberDictionaryElement(
    TNode<NumberDictionary> dictionary, TNode<IntPtrT> intptr_index,
    Label* not_data, Label* if_hole) {
  TVARIABLE(IntPtrT, var_entry);
  Label if_found(this);
  NumberDictionaryLookup(dictionary, intptr_index, &if_found, &var_entry,
                         if_hole);
  BIND(&if_found);

  // Check that the value is a data property.
  TNode<IntPtrT> index = EntryToIndex<NumberDictionary>(var_entry.value());
  TNode<Uint32T> details =
      LoadDetailsByKeyIndex<NumberDictionary>(dictionary, index);
  TNode<Uint32T> kind = DecodeWord32<PropertyDetails::KindField>(details);
  // TODO(jkummerow): Support accessors without missing?
  GotoIfNot(Word32Equal(kind, Int32Constant(kData)), not_data);
  // Finally, load the value.
  return LoadValueByKeyIndex<NumberDictionary>(dictionary, index);
}

void CodeStubAssembler::BasicStoreNumberDictionaryElement(
    TNode<NumberDictionary> dictionary, TNode<IntPtrT> intptr_index,
    TNode<Object> value, Label* not_data, Label* if_hole, Label* read_only) {
  TVARIABLE(IntPtrT, var_entry);
  Label if_found(this);
  NumberDictionaryLookup(dictionary, intptr_index, &if_found, &var_entry,
                         if_hole);
  BIND(&if_found);

  // Check that the value is a data property.
  TNode<IntPtrT> index = EntryToIndex<NumberDictionary>(var_entry.value());
  TNode<Uint32T> details =
      LoadDetailsByKeyIndex<NumberDictionary>(dictionary, index);
  TNode<Uint32T> kind = DecodeWord32<PropertyDetails::KindField>(details);
  // TODO(jkummerow): Support accessors without missing?
  GotoIfNot(Word32Equal(kind, Int32Constant(kData)), not_data);

  // Check that the property is writeable.
  GotoIf(IsSetWord32(details, PropertyDetails::kAttributesReadOnlyMask),
         read_only);

  // Finally, store the value.
  StoreValueByKeyIndex<NumberDictionary>(dictionary, index, value);
}

template <class Dictionary>
void CodeStubAssembler::FindInsertionEntry(TNode<Dictionary> dictionary,
                                           TNode<Name> key,
                                           TVariable<IntPtrT>* var_key_index) {
  UNREACHABLE();
}

template <>
void CodeStubAssembler::FindInsertionEntry<NameDictionary>(
    TNode<NameDictionary> dictionary, TNode<Name> key,
    TVariable<IntPtrT>* var_key_index) {
  Label done(this);
  NameDictionaryLookup<NameDictionary>(dictionary, key, nullptr, var_key_index,
                                       &done, kFindInsertionIndex);
  BIND(&done);
}

template <class Dictionary>
void CodeStubAssembler::InsertEntry(TNode<Dictionary> dictionary,
                                    TNode<Name> key, TNode<Object> value,
                                    TNode<IntPtrT> index,
                                    TNode<Smi> enum_index) {
  UNREACHABLE();  // Use specializations instead.
}

template <>
void CodeStubAssembler::InsertEntry<NameDictionary>(
    TNode<NameDictionary> dictionary, TNode<Name> name, TNode<Object> value,
    TNode<IntPtrT> index, TNode<Smi> enum_index) {
  // Store name and value.
  StoreFixedArrayElement(dictionary, index, name);
  StoreValueByKeyIndex<NameDictionary>(dictionary, index, value);

  // Prepare details of the new property.
  PropertyDetails d(kData, NONE, PropertyCellType::kNoCell);
  enum_index =
      SmiShl(enum_index, PropertyDetails::DictionaryStorageField::kShift);
  // We OR over the actual index below, so we expect the initial value to be 0.
  DCHECK_EQ(0, d.dictionary_index());
  TVARIABLE(Smi, var_details, SmiOr(SmiConstant(d.AsSmi()), enum_index));

  // Private names must be marked non-enumerable.
  Label not_private(this, &var_details);
  GotoIfNot(IsPrivateSymbol(name), &not_private);
  TNode<Smi> dont_enum =
      SmiShl(SmiConstant(DONT_ENUM), PropertyDetails::AttributesField::kShift);
  var_details = SmiOr(var_details.value(), dont_enum);
  Goto(&not_private);
  BIND(&not_private);

  // Finally, store the details.
  StoreDetailsByKeyIndex<NameDictionary>(dictionary, index,
                                         var_details.value());
}

template <>
void CodeStubAssembler::InsertEntry<GlobalDictionary>(
    TNode<GlobalDictionary> dictionary, TNode<Name> key, TNode<Object> value,
    TNode<IntPtrT> index, TNode<Smi> enum_index) {
  UNIMPLEMENTED();
}

template <class Dictionary>
void CodeStubAssembler::Add(TNode<Dictionary> dictionary, TNode<Name> key,
                            TNode<Object> value, Label* bailout) {
  CSA_ASSERT(this, Word32BinaryNot(IsEmptyPropertyDictionary(dictionary)));
  TNode<Smi> capacity = GetCapacity<Dictionary>(dictionary);
  TNode<Smi> nof = GetNumberOfElements<Dictionary>(dictionary);
  TNode<Smi> new_nof = SmiAdd(nof, SmiConstant(1));
  // Require 33% to still be free after adding additional_elements.
  // Computing "x + (x >> 1)" on a Smi x does not return a valid Smi!
  // But that's OK here because it's only used for a comparison.
  TNode<Smi> required_capacity_pseudo_smi = SmiAdd(new_nof, SmiShr(new_nof, 1));
  GotoIf(SmiBelow(capacity, required_capacity_pseudo_smi), bailout);
  // Require rehashing if more than 50% of free elements are deleted elements.
  TNode<Smi> deleted = GetNumberOfDeletedElements<Dictionary>(dictionary);
  CSA_ASSERT(this, SmiAbove(capacity, new_nof));
  TNode<Smi> half_of_free_elements = SmiShr(SmiSub(capacity, new_nof), 1);
  GotoIf(SmiAbove(deleted, half_of_free_elements), bailout);

  TNode<Smi> enum_index = GetNextEnumerationIndex<Dictionary>(dictionary);
  TNode<Smi> new_enum_index = SmiAdd(enum_index, SmiConstant(1));
  TNode<Smi> max_enum_index =
      SmiConstant(PropertyDetails::DictionaryStorageField::kMax);
  GotoIf(SmiAbove(new_enum_index, max_enum_index), bailout);

  // No more bailouts after this point.
  // Operations from here on can have side effects.

  SetNextEnumerationIndex<Dictionary>(dictionary, new_enum_index);
  SetNumberOfElements<Dictionary>(dictionary, new_nof);

  TVARIABLE(IntPtrT, var_key_index);
  FindInsertionEntry<Dictionary>(dictionary, key, &var_key_index);
  InsertEntry<Dictionary>(dictionary, key, value, var_key_index.value(),
                          enum_index);
}

template void CodeStubAssembler::Add<NameDictionary>(TNode<NameDictionary>,
                                                     TNode<Name>, TNode<Object>,
                                                     Label*);

template <typename Array>
void CodeStubAssembler::LookupLinear(TNode<Name> unique_name,
                                     TNode<Array> array,
                                     TNode<Uint32T> number_of_valid_entries,
                                     Label* if_found,
                                     TVariable<IntPtrT>* var_name_index,
                                     Label* if_not_found) {
  static_assert(std::is_base_of<FixedArray, Array>::value ||
                    std::is_base_of<WeakFixedArray, Array>::value ||
                    std::is_base_of<DescriptorArray, Array>::value,
                "T must be a descendant of FixedArray or a WeakFixedArray");
  Comment("LookupLinear");
  CSA_ASSERT(this, IsUniqueName(unique_name));
  TNode<IntPtrT> first_inclusive = IntPtrConstant(Array::ToKeyIndex(0));
  TNode<IntPtrT> factor = IntPtrConstant(Array::kEntrySize);
  TNode<IntPtrT> last_exclusive = IntPtrAdd(
      first_inclusive,
      IntPtrMul(ChangeInt32ToIntPtr(number_of_valid_entries), factor));

  BuildFastLoop(
      last_exclusive, first_inclusive,
      [=](SloppyTNode<IntPtrT> name_index) {
        TNode<MaybeObject> element =
            LoadArrayElement(array, Array::kHeaderSize, name_index);
        TNode<Name> candidate_name = CAST(element);
        *var_name_index = name_index;
        GotoIf(WordEqual(candidate_name, unique_name), if_found);
      },
      -Array::kEntrySize, INTPTR_PARAMETERS, IndexAdvanceMode::kPre);
  Goto(if_not_found);
}

template <>
TNode<Uint32T> CodeStubAssembler::NumberOfEntries<DescriptorArray>(
    TNode<DescriptorArray> descriptors) {
  return Unsigned(LoadNumberOfDescriptors(descriptors));
}

template <>
TNode<Uint32T> CodeStubAssembler::NumberOfEntries<TransitionArray>(
    TNode<TransitionArray> transitions) {
  TNode<IntPtrT> length = LoadAndUntagWeakFixedArrayLength(transitions);
  return Select<Uint32T>(
      UintPtrLessThan(length, IntPtrConstant(TransitionArray::kFirstIndex)),
      [=] { return Unsigned(Int32Constant(0)); },
      [=] {
        return Unsigned(LoadAndUntagToWord32ArrayElement(
            transitions, WeakFixedArray::kHeaderSize,
            IntPtrConstant(TransitionArray::kTransitionLengthIndex)));
      });
}

template <typename Array>
TNode<IntPtrT> CodeStubAssembler::EntryIndexToIndex(
    TNode<Uint32T> entry_index) {
  TNode<Int32T> entry_size = Int32Constant(Array::kEntrySize);
  TNode<Word32T> index = Int32Mul(entry_index, entry_size);
  return ChangeInt32ToIntPtr(index);
}

template <typename Array>
TNode<IntPtrT> CodeStubAssembler::ToKeyIndex(TNode<Uint32T> entry_index) {
  return IntPtrAdd(IntPtrConstant(Array::ToKeyIndex(0)),
                   EntryIndexToIndex<Array>(entry_index));
}

template TNode<IntPtrT> CodeStubAssembler::ToKeyIndex<DescriptorArray>(
    TNode<Uint32T>);
template TNode<IntPtrT> CodeStubAssembler::ToKeyIndex<TransitionArray>(
    TNode<Uint32T>);

template <>
TNode<Uint32T> CodeStubAssembler::GetSortedKeyIndex<DescriptorArray>(
    TNode<DescriptorArray> descriptors, TNode<Uint32T> descriptor_number) {
  TNode<Uint32T> details =
      DescriptorArrayGetDetails(descriptors, descriptor_number);
  return DecodeWord32<PropertyDetails::DescriptorPointer>(details);
}

template <>
TNode<Uint32T> CodeStubAssembler::GetSortedKeyIndex<TransitionArray>(
    TNode<TransitionArray> transitions, TNode<Uint32T> transition_number) {
  return transition_number;
}

template <typename Array>
TNode<Name> CodeStubAssembler::GetKey(TNode<Array> array,
                                      TNode<Uint32T> entry_index) {
  static_assert(std::is_base_of<TransitionArray, Array>::value ||
                    std::is_base_of<DescriptorArray, Array>::value,
                "T must be a descendant of DescriptorArray or TransitionArray");
  const int key_offset = Array::ToKeyIndex(0) * kTaggedSize;
  TNode<MaybeObject> element =
      LoadArrayElement(array, Array::kHeaderSize,
                       EntryIndexToIndex<Array>(entry_index), key_offset);
  return CAST(element);
}

template TNode<Name> CodeStubAssembler::GetKey<DescriptorArray>(
    TNode<DescriptorArray>, TNode<Uint32T>);
template TNode<Name> CodeStubAssembler::GetKey<TransitionArray>(
    TNode<TransitionArray>, TNode<Uint32T>);

TNode<Uint32T> CodeStubAssembler::DescriptorArrayGetDetails(
    TNode<DescriptorArray> descriptors, TNode<Uint32T> descriptor_number) {
  const int details_offset = DescriptorArray::ToDetailsIndex(0) * kTaggedSize;
  return Unsigned(LoadAndUntagToWord32ArrayElement(
      descriptors, DescriptorArray::kHeaderSize,
      EntryIndexToIndex<DescriptorArray>(descriptor_number), details_offset));
}

template <typename Array>
void CodeStubAssembler::LookupBinary(TNode<Name> unique_name,
                                     TNode<Array> array,
                                     TNode<Uint32T> number_of_valid_entries,
                                     Label* if_found,
                                     TVariable<IntPtrT>* var_name_index,
                                     Label* if_not_found) {
  Comment("LookupBinary");
  TVARIABLE(Uint32T, var_low, Unsigned(Int32Constant(0)));
  TNode<Uint32T> limit =
      Unsigned(Int32Sub(NumberOfEntries<Array>(array), Int32Constant(1)));
  TVARIABLE(Uint32T, var_high, limit);
  TNode<Uint32T> hash = LoadNameHashField(unique_name);
  CSA_ASSERT(this, Word32NotEqual(hash, Int32Constant(0)));

  // Assume non-empty array.
  CSA_ASSERT(this, Uint32LessThanOrEqual(var_low.value(), var_high.value()));

  Label binary_loop(this, {&var_high, &var_low});
  Goto(&binary_loop);
  BIND(&binary_loop);
  {
    // mid = low + (high - low) / 2 (to avoid overflow in "(low + high) / 2").
    TNode<Uint32T> mid = Unsigned(
        Int32Add(var_low.value(),
                 Word32Shr(Int32Sub(var_high.value(), var_low.value()), 1)));
    // mid_name = array->GetSortedKey(mid).
    TNode<Uint32T> sorted_key_index = GetSortedKeyIndex<Array>(array, mid);
    TNode<Name> mid_name = GetKey<Array>(array, sorted_key_index);

    TNode<Uint32T> mid_hash = LoadNameHashField(mid_name);

    Label mid_greater(this), mid_less(this), merge(this);
    Branch(Uint32GreaterThanOrEqual(mid_hash, hash), &mid_greater, &mid_less);
    BIND(&mid_greater);
    {
      var_high = mid;
      Goto(&merge);
    }
    BIND(&mid_less);
    {
      var_low = Unsigned(Int32Add(mid, Int32Constant(1)));
      Goto(&merge);
    }
    BIND(&merge);
    GotoIf(Word32NotEqual(var_low.value(), var_high.value()), &binary_loop);
  }

  Label scan_loop(this, &var_low);
  Goto(&scan_loop);
  BIND(&scan_loop);
  {
    GotoIf(Int32GreaterThan(var_low.value(), limit), if_not_found);

    TNode<Uint32T> sort_index =
        GetSortedKeyIndex<Array>(array, var_low.value());
    TNode<Name> current_name = GetKey<Array>(array, sort_index);
    TNode<Uint32T> current_hash = LoadNameHashField(current_name);
    GotoIf(Word32NotEqual(current_hash, hash), if_not_found);
    Label next(this);
    GotoIf(WordNotEqual(current_name, unique_name), &next);
    GotoIf(Uint32GreaterThanOrEqual(sort_index, number_of_valid_entries),
           if_not_found);
    *var_name_index = ToKeyIndex<Array>(sort_index);
    Goto(if_found);

    BIND(&next);
    var_low = Unsigned(Int32Add(var_low.value(), Int32Constant(1)));
    Goto(&scan_loop);
  }
}

void CodeStubAssembler::ForEachEnumerableOwnProperty(
    TNode<Context> context, TNode<Map> map, TNode<JSObject> object,
    ForEachEnumerationMode mode, const ForEachKeyValueFunction& body,
    Label* bailout) {
  TNode<Int32T> type = LoadMapInstanceType(map);
  TNode<Uint32T> bit_field3 = EnsureOnlyHasSimpleProperties(map, type, bailout);

  TNode<DescriptorArray> descriptors = LoadMapDescriptors(map);
  TNode<Uint32T> nof_descriptors =
      DecodeWord32<Map::NumberOfOwnDescriptorsBits>(bit_field3);

  TVARIABLE(BoolT, var_stable, Int32TrueConstant());

  TVARIABLE(BoolT, var_has_symbol, Int32FalseConstant());
  // false - iterate only string properties, true - iterate only symbol
  // properties
  TVARIABLE(BoolT, var_is_symbol_processing_loop, Int32FalseConstant());
  TVARIABLE(IntPtrT, var_start_key_index,
            ToKeyIndex<DescriptorArray>(Unsigned(Int32Constant(0))));
  // Note: var_end_key_index is exclusive for the loop
  TVARIABLE(IntPtrT, var_end_key_index,
            ToKeyIndex<DescriptorArray>(nof_descriptors));
  VariableList list(
      {&var_stable, &var_has_symbol, &var_is_symbol_processing_loop,
       &var_start_key_index, &var_end_key_index},
      zone());
  Label descriptor_array_loop(
      this, {&var_stable, &var_has_symbol, &var_is_symbol_processing_loop,
             &var_start_key_index, &var_end_key_index});

  Goto(&descriptor_array_loop);
  BIND(&descriptor_array_loop);

  BuildFastLoop(
      list, var_start_key_index.value(), var_end_key_index.value(),
      [=, &var_stable, &var_has_symbol, &var_is_symbol_processing_loop,
       &var_start_key_index, &var_end_key_index](Node* index) {
        TNode<IntPtrT> descriptor_key_index =
            TNode<IntPtrT>::UncheckedCast(index);
        TNode<Name> next_key =
            LoadKeyByKeyIndex(descriptors, descriptor_key_index);

        TVARIABLE(Object, var_value, SmiConstant(0));
        Label callback(this), next_iteration(this);

        if (mode == kEnumerationOrder) {
          // |next_key| is either a string or a symbol
          // Skip strings or symbols depending on
          // |var_is_symbol_processing_loop|.
          Label if_string(this), if_symbol(this), if_name_ok(this);
          Branch(IsSymbol(next_key), &if_symbol, &if_string);
          BIND(&if_symbol);
          {
            // Process symbol property when |var_is_symbol_processing_loop| is
            // true.
            GotoIf(var_is_symbol_processing_loop.value(), &if_name_ok);
            // First iteration need to calculate smaller range for processing
            // symbols
            Label if_first_symbol(this);
            // var_end_key_index is still inclusive at this point.
            var_end_key_index = descriptor_key_index;
            Branch(var_has_symbol.value(), &next_iteration, &if_first_symbol);
            BIND(&if_first_symbol);
            {
              var_start_key_index = descriptor_key_index;
              var_has_symbol = Int32TrueConstant();
              Goto(&next_iteration);
            }
          }
          BIND(&if_string);
          {
            CSA_ASSERT(this, IsString(next_key));
            // Process string property when |var_is_symbol_processing_loop| is
            // false.
            Branch(var_is_symbol_processing_loop.value(), &next_iteration,
                   &if_name_ok);
          }
          BIND(&if_name_ok);
        }
        {
          TVARIABLE(Map, var_map);
          TVARIABLE(HeapObject, var_meta_storage);
          TVARIABLE(IntPtrT, var_entry);
          TVARIABLE(Uint32T, var_details);
          Label if_found(this);

          Label if_found_fast(this), if_found_dict(this);

          Label if_stable(this), if_not_stable(this);
          Branch(var_stable.value(), &if_stable, &if_not_stable);
          BIND(&if_stable);
          {
            // Directly decode from the descriptor array if |object| did not
            // change shape.
            var_map = map;
            var_meta_storage = descriptors;
            var_entry = Signed(descriptor_key_index);
            Goto(&if_found_fast);
          }
          BIND(&if_not_stable);
          {
            // If the map did change, do a slower lookup. We are still
            // guaranteed that the object has a simple shape, and that the key
            // is a name.
            var_map = LoadMap(object);
            TryLookupPropertyInSimpleObject(
                object, var_map.value(), next_key, &if_found_fast,
                &if_found_dict, &var_meta_storage, &var_entry, &next_iteration);
          }

          BIND(&if_found_fast);
          {
            TNode<DescriptorArray> descriptors = CAST(var_meta_storage.value());
            TNode<IntPtrT> name_index = var_entry.value();

            // Skip non-enumerable properties.
            var_details = LoadDetailsByKeyIndex(descriptors, name_index);
            GotoIf(IsSetWord32(var_details.value(),
                               PropertyDetails::kAttributesDontEnumMask),
                   &next_iteration);

            LoadPropertyFromFastObject(object, var_map.value(), descriptors,
                                       name_index, var_details.value(),
                                       &var_value);
            Goto(&if_found);
          }
          BIND(&if_found_dict);
          {
            TNode<NameDictionary> dictionary = CAST(var_meta_storage.value());
            TNode<IntPtrT> entry = var_entry.value();

            TNode<Uint32T> details =
                LoadDetailsByKeyIndex<NameDictionary>(dictionary, entry);
            // Skip non-enumerable properties.
            GotoIf(
                IsSetWord32(details, PropertyDetails::kAttributesDontEnumMask),
                &next_iteration);

            var_details = details;
            var_value = LoadValueByKeyIndex<NameDictionary>(dictionary, entry);
            Goto(&if_found);
          }

          // Here we have details and value which could be an accessor.
          BIND(&if_found);
          {
            Label slow_load(this, Label::kDeferred);

            var_value = CallGetterIfAccessor(var_value.value(),
                                             var_details.value(), context,
                                             object, &slow_load, kCallJSGetter);
            Goto(&callback);

            BIND(&slow_load);
            var_value =
                CallRuntime(Runtime::kGetProperty, context, object, next_key);
            Goto(&callback);

            BIND(&callback);
            body(next_key, var_value.value());

            // Check if |object| is still stable, i.e. we can proceed using
            // property details from preloaded |descriptors|.
            var_stable = Select<BoolT>(
                var_stable.value(),
                [=] { return WordEqual(LoadMap(object), map); },
                [=] { return Int32FalseConstant(); });

            Goto(&next_iteration);
          }
        }
        BIND(&next_iteration);
      },
      DescriptorArray::kEntrySize, INTPTR_PARAMETERS, IndexAdvanceMode::kPost);

  if (mode == kEnumerationOrder) {
    Label done(this);
    GotoIf(var_is_symbol_processing_loop.value(), &done);
    GotoIfNot(var_has_symbol.value(), &done);
    // All string properties are processed, now process symbol properties.
    var_is_symbol_processing_loop = Int32TrueConstant();
    // Add DescriptorArray::kEntrySize to make the var_end_key_index exclusive
    // as BuildFastLoop() expects.
    Increment(&var_end_key_index, DescriptorArray::kEntrySize,
              INTPTR_PARAMETERS);
    Goto(&descriptor_array_loop);

    BIND(&done);
  }
}

void CodeStubAssembler::DescriptorLookup(
    SloppyTNode<Name> unique_name, SloppyTNode<DescriptorArray> descriptors,
    SloppyTNode<Uint32T> bitfield3, Label* if_found,
    TVariable<IntPtrT>* var_name_index, Label* if_not_found) {
  Comment("DescriptorArrayLookup");
  TNode<Uint32T> nof = DecodeWord32<Map::NumberOfOwnDescriptorsBits>(bitfield3);
  Lookup<DescriptorArray>(unique_name, descriptors, nof, if_found,
                          var_name_index, if_not_found);
}

void CodeStubAssembler::TransitionLookup(
    SloppyTNode<Name> unique_name, SloppyTNode<TransitionArray> transitions,
    Label* if_found, TVariable<IntPtrT>* var_name_index, Label* if_not_found) {
  Comment("TransitionArrayLookup");
  TNode<Uint32T> number_of_valid_transitions =
      NumberOfEntries<TransitionArray>(transitions);
  Lookup<TransitionArray>(unique_name, transitions, number_of_valid_transitions,
                          if_found, var_name_index, if_not_found);
}

template <typename Array>
void CodeStubAssembler::Lookup(TNode<Name> unique_name, TNode<Array> array,
                               TNode<Uint32T> number_of_valid_entries,
                               Label* if_found,
                               TVariable<IntPtrT>* var_name_index,
                               Label* if_not_found) {
  Comment("ArrayLookup");
  if (!number_of_valid_entries) {
    number_of_valid_entries = NumberOfEntries(array);
  }
  GotoIf(Word32Equal(number_of_valid_entries, Int32Constant(0)), if_not_found);
  Label linear_search(this), binary_search(this);
  const int kMaxElementsForLinearSearch = 32;
  Branch(Uint32LessThanOrEqual(number_of_valid_entries,
                               Int32Constant(kMaxElementsForLinearSearch)),
         &linear_search, &binary_search);
  BIND(&linear_search);
  {
    LookupLinear<Array>(unique_name, array, number_of_valid_entries, if_found,
                        var_name_index, if_not_found);
  }
  BIND(&binary_search);
  {
    LookupBinary<Array>(unique_name, array, number_of_valid_entries, if_found,
                        var_name_index, if_not_found);
  }
}

TNode<BoolT> CodeStubAssembler::IsSimpleObjectMap(TNode<Map> map) {
  uint32_t mask =
      Map::HasNamedInterceptorBit::kMask | Map::IsAccessCheckNeededBit::kMask;
  // !IsSpecialReceiverType && !IsNamedInterceptor && !IsAccessCheckNeeded
  return Select<BoolT>(
      IsSpecialReceiverInstanceType(LoadMapInstanceType(map)),
      [=] { return Int32FalseConstant(); },
      [=] { return IsClearWord32(LoadMapBitField(map), mask); });
}

void CodeStubAssembler::TryLookupPropertyInSimpleObject(
    TNode<JSObject> object, TNode<Map> map, TNode<Name> unique_name,
    Label* if_found_fast, Label* if_found_dict,
    TVariable<HeapObject>* var_meta_storage, TVariable<IntPtrT>* var_name_index,
    Label* if_not_found) {
  CSA_ASSERT(this, IsSimpleObjectMap(map));
  CSA_ASSERT(this, IsUniqueNameNoIndex(unique_name));

  TNode<Uint32T> bit_field3 = LoadMapBitField3(map);
  Label if_isfastmap(this), if_isslowmap(this);
  Branch(IsSetWord32<Map::IsDictionaryMapBit>(bit_field3), &if_isslowmap,
         &if_isfastmap);
  BIND(&if_isfastmap);
  {
    TNode<DescriptorArray> descriptors = LoadMapDescriptors(map);
    *var_meta_storage = descriptors;

    DescriptorLookup(unique_name, descriptors, bit_field3, if_found_fast,
                     var_name_index, if_not_found);
  }
  BIND(&if_isslowmap);
  {
    TNode<NameDictionary> dictionary = CAST(LoadSlowProperties(object));
    *var_meta_storage = dictionary;

    NameDictionaryLookup<NameDictionary>(dictionary, unique_name, if_found_dict,
                                         var_name_index, if_not_found);
  }
}

void CodeStubAssembler::TryLookupProperty(
    SloppyTNode<JSObject> object, SloppyTNode<Map> map,
    SloppyTNode<Int32T> instance_type, SloppyTNode<Name> unique_name,
    Label* if_found_fast, Label* if_found_dict, Label* if_found_global,
    TVariable<HeapObject>* var_meta_storage, TVariable<IntPtrT>* var_name_index,
    Label* if_not_found, Label* if_bailout) {
  Label if_objectisspecial(this);
  GotoIf(IsSpecialReceiverInstanceType(instance_type), &if_objectisspecial);

  TryLookupPropertyInSimpleObject(object, map, unique_name, if_found_fast,
                                  if_found_dict, var_meta_storage,
                                  var_name_index, if_not_found);

  BIND(&if_objectisspecial);
  {
    // Handle global object here and bailout for other special objects.
    GotoIfNot(InstanceTypeEqual(instance_type, JS_GLOBAL_OBJECT_TYPE),
              if_bailout);

    // Handle interceptors and access checks in runtime.
    TNode<Int32T> bit_field = LoadMapBitField(map);
    int mask =
        Map::HasNamedInterceptorBit::kMask | Map::IsAccessCheckNeededBit::kMask;
    GotoIf(IsSetWord32(bit_field, mask), if_bailout);

    TNode<GlobalDictionary> dictionary = CAST(LoadSlowProperties(object));
    *var_meta_storage = dictionary;

    NameDictionaryLookup<GlobalDictionary>(
        dictionary, unique_name, if_found_global, var_name_index, if_not_found);
  }
}

void CodeStubAssembler::TryHasOwnProperty(Node* object, Node* map,
                                          Node* instance_type,
                                          Node* unique_name, Label* if_found,
                                          Label* if_not_found,
                                          Label* if_bailout) {
  Comment("TryHasOwnProperty");
  CSA_ASSERT(this, IsUniqueNameNoIndex(CAST(unique_name)));
  TVARIABLE(HeapObject, var_meta_storage);
  TVARIABLE(IntPtrT, var_name_index);

  Label if_found_global(this);
  TryLookupProperty(object, map, instance_type, unique_name, if_found, if_found,
                    &if_found_global, &var_meta_storage, &var_name_index,
                    if_not_found, if_bailout);

  BIND(&if_found_global);
  {
    VARIABLE(var_value, MachineRepresentation::kTagged);
    VARIABLE(var_details, MachineRepresentation::kWord32);
    // Check if the property cell is not deleted.
    LoadPropertyFromGlobalDictionary(var_meta_storage.value(),
                                     var_name_index.value(), &var_value,
                                     &var_details, if_not_found);
    Goto(if_found);
  }
}

Node* CodeStubAssembler::GetMethod(Node* context, Node* object,
                                   Handle<Name> name,
                                   Label* if_null_or_undefined) {
  Node* method = GetProperty(context, object, name);

  GotoIf(IsUndefined(method), if_null_or_undefined);
  GotoIf(IsNull(method), if_null_or_undefined);

  return method;
}

TNode<Object> CodeStubAssembler::GetIteratorMethod(
    TNode<Context> context, TNode<HeapObject> heap_obj,
    Label* if_iteratorundefined) {
  return CAST(GetMethod(context, heap_obj,
                        isolate()->factory()->iterator_symbol(),
                        if_iteratorundefined));
}

void CodeStubAssembler::LoadPropertyFromFastObject(
    Node* object, Node* map, TNode<DescriptorArray> descriptors,
    Node* name_index, Variable* var_details, Variable* var_value) {
  DCHECK_EQ(MachineRepresentation::kWord32, var_details->rep());
  DCHECK_EQ(MachineRepresentation::kTagged, var_value->rep());

  Node* details =
      LoadDetailsByKeyIndex(descriptors, UncheckedCast<IntPtrT>(name_index));
  var_details->Bind(details);

  LoadPropertyFromFastObject(object, map, descriptors, name_index, details,
                             var_value);
}

void CodeStubAssembler::LoadPropertyFromFastObject(
    Node* object, Node* map, TNode<DescriptorArray> descriptors,
    Node* name_index, Node* details, Variable* var_value) {
  Comment("[ LoadPropertyFromFastObject");

  Node* location = DecodeWord32<PropertyDetails::LocationField>(details);

  Label if_in_field(this), if_in_descriptor(this), done(this);
  Branch(Word32Equal(location, Int32Constant(kField)), &if_in_field,
         &if_in_descriptor);
  BIND(&if_in_field);
  {
    Node* field_index =
        DecodeWordFromWord32<PropertyDetails::FieldIndexField>(details);
    Node* representation =
        DecodeWord32<PropertyDetails::RepresentationField>(details);

    field_index =
        IntPtrAdd(field_index, LoadMapInobjectPropertiesStartInWords(map));
    Node* instance_size_in_words = LoadMapInstanceSizeInWords(map);

    Label if_inobject(this), if_backing_store(this);
    VARIABLE(var_double_value, MachineRepresentation::kFloat64);
    Label rebox_double(this, &var_double_value);
    Branch(UintPtrLessThan(field_index, instance_size_in_words), &if_inobject,
           &if_backing_store);
    BIND(&if_inobject);
    {
      Comment("if_inobject");
      Node* field_offset = TimesTaggedSize(field_index);

      Label if_double(this), if_tagged(this);
      Branch(Word32NotEqual(representation,
                            Int32Constant(Representation::kDouble)),
             &if_tagged, &if_double);
      BIND(&if_tagged);
      {
        var_value->Bind(LoadObjectField(object, field_offset));
        Goto(&done);
      }
      BIND(&if_double);
      {
        if (FLAG_unbox_double_fields) {
          var_double_value.Bind(
              LoadObjectField(object, field_offset, MachineType::Float64()));
        } else {
          Node* mutable_heap_number = LoadObjectField(object, field_offset);
          var_double_value.Bind(LoadHeapNumberValue(mutable_heap_number));
        }
        Goto(&rebox_double);
      }
    }
    BIND(&if_backing_store);
    {
      Comment("if_backing_store");
      TNode<HeapObject> properties = LoadFastProperties(object);
      field_index = IntPtrSub(field_index, instance_size_in_words);
      Node* value = LoadPropertyArrayElement(CAST(properties), field_index);

      Label if_double(this), if_tagged(this);
      Branch(Word32NotEqual(representation,
                            Int32Constant(Representation::kDouble)),
             &if_tagged, &if_double);
      BIND(&if_tagged);
      {
        var_value->Bind(value);
        Goto(&done);
      }
      BIND(&if_double);
      {
        var_double_value.Bind(LoadHeapNumberValue(value));
        Goto(&rebox_double);
      }
    }
    BIND(&rebox_double);
    {
      Comment("rebox_double");
      Node* heap_number = AllocateHeapNumberWithValue(var_double_value.value());
      var_value->Bind(heap_number);
      Goto(&done);
    }
  }
  BIND(&if_in_descriptor);
  {
    var_value->Bind(
        LoadValueByKeyIndex(descriptors, UncheckedCast<IntPtrT>(name_index)));
    Goto(&done);
  }
  BIND(&done);

  Comment("] LoadPropertyFromFastObject");
}

void CodeStubAssembler::LoadPropertyFromNameDictionary(Node* dictionary,
                                                       Node* name_index,
                                                       Variable* var_details,
                                                       Variable* var_value) {
  Comment("LoadPropertyFromNameDictionary");
  CSA_ASSERT(this, IsNameDictionary(dictionary));

  var_details->Bind(
      LoadDetailsByKeyIndex<NameDictionary>(dictionary, name_index));
  var_value->Bind(LoadValueByKeyIndex<NameDictionary>(dictionary, name_index));

  Comment("] LoadPropertyFromNameDictionary");
}

void CodeStubAssembler::LoadPropertyFromGlobalDictionary(Node* dictionary,
                                                         Node* name_index,
                                                         Variable* var_details,
                                                         Variable* var_value,
                                                         Label* if_deleted) {
  Comment("[ LoadPropertyFromGlobalDictionary");
  CSA_ASSERT(this, IsGlobalDictionary(dictionary));

  Node* property_cell = LoadFixedArrayElement(CAST(dictionary), name_index);
  CSA_ASSERT(this, IsPropertyCell(property_cell));

  Node* value = LoadObjectField(property_cell, PropertyCell::kValueOffset);
  GotoIf(WordEqual(value, TheHoleConstant()), if_deleted);

  var_value->Bind(value);

  Node* details = LoadAndUntagToWord32ObjectField(
      property_cell, PropertyCell::kPropertyDetailsRawOffset);
  var_details->Bind(details);

  Comment("] LoadPropertyFromGlobalDictionary");
}

// |value| is the property backing store's contents, which is either a value
// or an accessor pair, as specified by |details|.
// Returns either the original value, or the result of the getter call.
TNode<Object> CodeStubAssembler::CallGetterIfAccessor(
    Node* value, Node* details, Node* context, Node* receiver,
    Label* if_bailout, GetOwnPropertyMode mode) {
  VARIABLE(var_value, MachineRepresentation::kTagged, value);
  Label done(this), if_accessor_info(this, Label::kDeferred);

  Node* kind = DecodeWord32<PropertyDetails::KindField>(details);
  GotoIf(Word32Equal(kind, Int32Constant(kData)), &done);

  // Accessor case.
  GotoIfNot(IsAccessorPair(value), &if_accessor_info);

  // AccessorPair case.
  {
    if (mode == kCallJSGetter) {
      Node* accessor_pair = value;
      Node* getter =
          LoadObjectField(accessor_pair, AccessorPair::kGetterOffset);
      Node* getter_map = LoadMap(getter);
      Node* instance_type = LoadMapInstanceType(getter_map);
      // FunctionTemplateInfo getters are not supported yet.
      GotoIf(InstanceTypeEqual(instance_type, FUNCTION_TEMPLATE_INFO_TYPE),
             if_bailout);

      // Return undefined if the {getter} is not callable.
      var_value.Bind(UndefinedConstant());
      GotoIfNot(IsCallableMap(getter_map), &done);

      // Call the accessor.
      Callable callable = CodeFactory::Call(isolate());
      Node* result = CallJS(callable, context, getter, receiver);
      var_value.Bind(result);
    }
    Goto(&done);
  }

  // AccessorInfo case.
  BIND(&if_accessor_info);
  {
    Node* accessor_info = value;
    CSA_ASSERT(this, IsAccessorInfo(value));
    CSA_ASSERT(this, TaggedIsNotSmi(receiver));
    Label if_array(this), if_function(this), if_wrapper(this);

    // Dispatch based on {receiver} instance type.
    Node* receiver_map = LoadMap(receiver);
    Node* receiver_instance_type = LoadMapInstanceType(receiver_map);
    GotoIf(IsJSArrayInstanceType(receiver_instance_type), &if_array);
    GotoIf(IsJSFunctionInstanceType(receiver_instance_type), &if_function);
    Branch(IsJSPrimitiveWrapperInstanceType(receiver_instance_type),
           &if_wrapper, if_bailout);

    // JSArray AccessorInfo case.
    BIND(&if_array);
    {
      // We only deal with the "length" accessor on JSArray.
      GotoIfNot(IsLengthString(
                    LoadObjectField(accessor_info, AccessorInfo::kNameOffset)),
                if_bailout);
      var_value.Bind(LoadJSArrayLength(receiver));
      Goto(&done);
    }

    // JSFunction AccessorInfo case.
    BIND(&if_function);
    {
      // We only deal with the "prototype" accessor on JSFunction here.
      GotoIfNot(IsPrototypeString(
                    LoadObjectField(accessor_info, AccessorInfo::kNameOffset)),
                if_bailout);

      GotoIfPrototypeRequiresRuntimeLookup(CAST(receiver), CAST(receiver_map),
                                           if_bailout);
      var_value.Bind(LoadJSFunctionPrototype(receiver, if_bailout));
      Goto(&done);
    }

    // JSPrimitiveWrapper AccessorInfo case.
    BIND(&if_wrapper);
    {
      // We only deal with the "length" accessor on JSPrimitiveWrapper string
      // wrappers.
      GotoIfNot(IsLengthString(
                    LoadObjectField(accessor_info, AccessorInfo::kNameOffset)),
                if_bailout);
      Node* receiver_value = LoadJSPrimitiveWrapperValue(receiver);
      GotoIfNot(TaggedIsNotSmi(receiver_value), if_bailout);
      GotoIfNot(IsString(receiver_value), if_bailout);
      var_value.Bind(LoadStringLengthAsSmi(receiver_value));
      Goto(&done);
    }
  }

  BIND(&done);
  return UncheckedCast<Object>(var_value.value());
}

void CodeStubAssembler::TryGetOwnProperty(
    Node* context, Node* receiver, Node* object, Node* map, Node* instance_type,
    Node* unique_name, Label* if_found_value, Variable* var_value,
    Label* if_not_found, Label* if_bailout) {
  TryGetOwnProperty(context, receiver, object, map, instance_type, unique_name,
                    if_found_value, var_value, nullptr, nullptr, if_not_found,
                    if_bailout, kCallJSGetter);
}

void CodeStubAssembler::TryGetOwnProperty(
    Node* context, Node* receiver, Node* object, Node* map, Node* instance_type,
    Node* unique_name, Label* if_found_value, Variable* var_value,
    Variable* var_details, Variable* var_raw_value, Label* if_not_found,
    Label* if_bailout, GetOwnPropertyMode mode) {
  DCHECK_EQ(MachineRepresentation::kTagged, var_value->rep());
  Comment("TryGetOwnProperty");
  CSA_ASSERT(this, IsUniqueNameNoIndex(CAST(unique_name)));

  TVARIABLE(HeapObject, var_meta_storage);
  TVARIABLE(IntPtrT, var_entry);

  Label if_found_fast(this), if_found_dict(this), if_found_global(this);

  VARIABLE(local_var_details, MachineRepresentation::kWord32);
  if (!var_details) {
    var_details = &local_var_details;
  }
  Label if_found(this);

  TryLookupProperty(object, map, instance_type, unique_name, &if_found_fast,
                    &if_found_dict, &if_found_global, &var_meta_storage,
                    &var_entry, if_not_found, if_bailout);
  BIND(&if_found_fast);
  {
    TNode<DescriptorArray> descriptors = CAST(var_meta_storage.value());
    Node* name_index = var_entry.value();

    LoadPropertyFromFastObject(object, map, descriptors, name_index,
                               var_details, var_value);
    Goto(&if_found);
  }
  BIND(&if_found_dict);
  {
    Node* dictionary = var_meta_storage.value();
    Node* entry = var_entry.value();
    LoadPropertyFromNameDictionary(dictionary, entry, var_details, var_value);
    Goto(&if_found);
  }
  BIND(&if_found_global);
  {
    Node* dictionary = var_meta_storage.value();
    Node* entry = var_entry.value();

    LoadPropertyFromGlobalDictionary(dictionary, entry, var_details, var_value,
                                     if_not_found);
    Goto(&if_found);
  }
  // Here we have details and value which could be an accessor.
  BIND(&if_found);
  {
    // TODO(ishell): Execute C++ accessor in case of accessor info
    if (var_raw_value) {
      var_raw_value->Bind(var_value->value());
    }
    Node* value = CallGetterIfAccessor(var_value->value(), var_details->value(),
                                       context, receiver, if_bailout, mode);
    var_value->Bind(value);
    Goto(if_found_value);
  }
}

void CodeStubAssembler::TryLookupElement(Node* object, Node* map,
                                         SloppyTNode<Int32T> instance_type,
                                         SloppyTNode<IntPtrT> intptr_index,
                                         Label* if_found, Label* if_absent,
                                         Label* if_not_found,
                                         Label* if_bailout) {
  // Handle special objects in runtime.
  GotoIf(IsSpecialReceiverInstanceType(instance_type), if_bailout);

  Node* elements_kind = LoadMapElementsKind(map);

  // TODO(verwaest): Support other elements kinds as well.
  Label if_isobjectorsmi(this), if_isdouble(this), if_isdictionary(this),
      if_isfaststringwrapper(this), if_isslowstringwrapper(this), if_oob(this),
      if_typedarray(this);
  // clang-format off
  int32_t values[] = {
      // Handled by {if_isobjectorsmi}.
      PACKED_SMI_ELEMENTS, HOLEY_SMI_ELEMENTS, PACKED_ELEMENTS, HOLEY_ELEMENTS,
      PACKED_SEALED_ELEMENTS, HOLEY_SEALED_ELEMENTS, PACKED_FROZEN_ELEMENTS,
      HOLEY_FROZEN_ELEMENTS,
      // Handled by {if_isdouble}.
      PACKED_DOUBLE_ELEMENTS, HOLEY_DOUBLE_ELEMENTS,
      // Handled by {if_isdictionary}.
      DICTIONARY_ELEMENTS,
      // Handled by {if_isfaststringwrapper}.
      FAST_STRING_WRAPPER_ELEMENTS,
      // Handled by {if_isslowstringwrapper}.
      SLOW_STRING_WRAPPER_ELEMENTS,
      // Handled by {if_not_found}.
      NO_ELEMENTS,
      // Handled by {if_typed_array}.
      UINT8_ELEMENTS,
      INT8_ELEMENTS,
      UINT16_ELEMENTS,
      INT16_ELEMENTS,
      UINT32_ELEMENTS,
      INT32_ELEMENTS,
      FLOAT32_ELEMENTS,
      FLOAT64_ELEMENTS,
      UINT8_CLAMPED_ELEMENTS,
      BIGUINT64_ELEMENTS,
      BIGINT64_ELEMENTS,
  };
  Label* labels[] = {
      &if_isobjectorsmi, &if_isobjectorsmi, &if_isobjectorsmi,
      &if_isobjectorsmi, &if_isobjectorsmi, &if_isobjectorsmi,
      &if_isobjectorsmi, &if_isobjectorsmi,
      &if_isdouble, &if_isdouble,
      &if_isdictionary,
      &if_isfaststringwrapper,
      &if_isslowstringwrapper,
      if_not_found,
      &if_typedarray,
      &if_typedarray,
      &if_typedarray,
      &if_typedarray,
      &if_typedarray,
      &if_typedarray,
      &if_typedarray,
      &if_typedarray,
      &if_typedarray,
      &if_typedarray,
      &if_typedarray,
  };
  // clang-format on
  STATIC_ASSERT(arraysize(values) == arraysize(labels));
  Switch(elements_kind, if_bailout, values, labels, arraysize(values));

  BIND(&if_isobjectorsmi);
  {
    TNode<FixedArray> elements = CAST(LoadElements(object));
    TNode<IntPtrT> length = LoadAndUntagFixedArrayBaseLength(elements);

    GotoIfNot(UintPtrLessThan(intptr_index, length), &if_oob);

    TNode<Object> element = UnsafeLoadFixedArrayElement(elements, intptr_index);
    TNode<Oddball> the_hole = TheHoleConstant();
    Branch(WordEqual(element, the_hole), if_not_found, if_found);
  }
  BIND(&if_isdouble);
  {
    TNode<FixedArrayBase> elements = LoadElements(object);
    TNode<IntPtrT> length = LoadAndUntagFixedArrayBaseLength(elements);

    GotoIfNot(UintPtrLessThan(intptr_index, length), &if_oob);

    // Check if the element is a double hole, but don't load it.
    LoadFixedDoubleArrayElement(CAST(elements), intptr_index,
                                MachineType::None(), 0, INTPTR_PARAMETERS,
                                if_not_found);
    Goto(if_found);
  }
  BIND(&if_isdictionary);
  {
    // Negative keys must be converted to property names.
    GotoIf(IntPtrLessThan(intptr_index, IntPtrConstant(0)), if_bailout);

    TVARIABLE(IntPtrT, var_entry);
    TNode<NumberDictionary> elements = CAST(LoadElements(object));
    NumberDictionaryLookup(elements, intptr_index, if_found, &var_entry,
                           if_not_found);
  }
  BIND(&if_isfaststringwrapper);
  {
    CSA_ASSERT(this, HasInstanceType(object, JS_PRIMITIVE_WRAPPER_TYPE));
    Node* string = LoadJSPrimitiveWrapperValue(object);
    CSA_ASSERT(this, IsString(string));
    Node* length = LoadStringLengthAsWord(string);
    GotoIf(UintPtrLessThan(intptr_index, length), if_found);
    Goto(&if_isobjectorsmi);
  }
  BIND(&if_isslowstringwrapper);
  {
    CSA_ASSERT(this, HasInstanceType(object, JS_PRIMITIVE_WRAPPER_TYPE));
    Node* string = LoadJSPrimitiveWrapperValue(object);
    CSA_ASSERT(this, IsString(string));
    Node* length = LoadStringLengthAsWord(string);
    GotoIf(UintPtrLessThan(intptr_index, length), if_found);
    Goto(&if_isdictionary);
  }
  BIND(&if_typedarray);
  {
    TNode<JSArrayBuffer> buffer = LoadJSArrayBufferViewBuffer(CAST(object));
    GotoIf(IsDetachedBuffer(buffer), if_absent);

    TNode<UintPtrT> length = LoadJSTypedArrayLength(CAST(object));
    Branch(UintPtrLessThan(intptr_index, length), if_found, if_absent);
  }
  BIND(&if_oob);
  {
    // Positive OOB indices mean "not found", negative indices must be
    // converted to property names.
    GotoIf(IntPtrLessThan(intptr_index, IntPtrConstant(0)), if_bailout);
    Goto(if_not_found);
  }
}

void CodeStubAssembler::BranchIfMaybeSpecialIndex(TNode<String> name_string,
                                                  Label* if_maybe_special_index,
                                                  Label* if_not_special_index) {
  // TODO(cwhan.tunz): Implement fast cases more.

  // If a name is empty or too long, it's not a special index
  // Max length of canonical double: -X.XXXXXXXXXXXXXXXXX-eXXX
  const int kBufferSize = 24;
  TNode<Smi> string_length = LoadStringLengthAsSmi(name_string);
  GotoIf(SmiEqual(string_length, SmiConstant(0)), if_not_special_index);
  GotoIf(SmiGreaterThan(string_length, SmiConstant(kBufferSize)),
         if_not_special_index);

  // If the first character of name is not a digit or '-', or we can't match it
  // to Infinity or NaN, then this is not a special index.
  TNode<Int32T> first_char = StringCharCodeAt(name_string, IntPtrConstant(0));
  // If the name starts with '-', it can be a negative index.
  GotoIf(Word32Equal(first_char, Int32Constant('-')), if_maybe_special_index);
  // If the name starts with 'I', it can be "Infinity".
  GotoIf(Word32Equal(first_char, Int32Constant('I')), if_maybe_special_index);
  // If the name starts with 'N', it can be "NaN".
  GotoIf(Word32Equal(first_char, Int32Constant('N')), if_maybe_special_index);
  // Finally, if the first character is not a digit either, then we are sure
  // that the name is not a special index.
  GotoIf(Uint32LessThan(first_char, Int32Constant('0')), if_not_special_index);
  GotoIf(Uint32LessThan(Int32Constant('9'), first_char), if_not_special_index);
  Goto(if_maybe_special_index);
}

void CodeStubAssembler::TryPrototypeChainLookup(
    Node* receiver, Node* object, Node* key,
    const LookupInHolder& lookup_property_in_holder,
    const LookupInHolder& lookup_element_in_holder, Label* if_end,
    Label* if_bailout, Label* if_proxy) {
  // Ensure receiver is JSReceiver, otherwise bailout.
  GotoIf(TaggedIsSmi(receiver), if_bailout);
  CSA_ASSERT(this, TaggedIsNotSmi(object));

  Node* map = LoadMap(object);
  Node* instance_type = LoadMapInstanceType(map);
  {
    Label if_objectisreceiver(this);
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    STATIC_ASSERT(FIRST_JS_RECEIVER_TYPE == JS_PROXY_TYPE);
    Branch(IsJSReceiverInstanceType(instance_type), &if_objectisreceiver,
           if_bailout);
    BIND(&if_objectisreceiver);

    GotoIf(InstanceTypeEqual(instance_type, JS_PROXY_TYPE), if_proxy);
  }

  VARIABLE(var_index, MachineType::PointerRepresentation());
  VARIABLE(var_unique, MachineRepresentation::kTagged);

  Label if_keyisindex(this), if_iskeyunique(this);
  TryToName(key, &if_keyisindex, &var_index, &if_iskeyunique, &var_unique,
            if_bailout);

  BIND(&if_iskeyunique);
  {
    VARIABLE(var_holder, MachineRepresentation::kTagged, object);
    VARIABLE(var_holder_map, MachineRepresentation::kTagged, map);
    VARIABLE(var_holder_instance_type, MachineRepresentation::kWord32,
             instance_type);

    Variable* merged_variables[] = {&var_holder, &var_holder_map,
                                    &var_holder_instance_type};
    Label loop(this, arraysize(merged_variables), merged_variables);
    Goto(&loop);
    BIND(&loop);
    {
      Node* holder_map = var_holder_map.value();
      Node* holder_instance_type = var_holder_instance_type.value();

      Label next_proto(this), check_integer_indexed_exotic(this);
      lookup_property_in_holder(receiver, var_holder.value(), holder_map,
                                holder_instance_type, var_unique.value(),
                                &check_integer_indexed_exotic, if_bailout);

      BIND(&check_integer_indexed_exotic);
      {
        // Bailout if it can be an integer indexed exotic case.
        GotoIfNot(InstanceTypeEqual(holder_instance_type, JS_TYPED_ARRAY_TYPE),
                  &next_proto);
        GotoIfNot(IsString(var_unique.value()), &next_proto);
        BranchIfMaybeSpecialIndex(CAST(var_unique.value()), if_bailout,
                                  &next_proto);
      }

      BIND(&next_proto);

      Node* proto = LoadMapPrototype(holder_map);

      GotoIf(IsNull(proto), if_end);

      Node* map = LoadMap(proto);
      Node* instance_type = LoadMapInstanceType(map);

      var_holder.Bind(proto);
      var_holder_map.Bind(map);
      var_holder_instance_type.Bind(instance_type);
      Goto(&loop);
    }
  }
  BIND(&if_keyisindex);
  {
    VARIABLE(var_holder, MachineRepresentation::kTagged, object);
    VARIABLE(var_holder_map, MachineRepresentation::kTagged, map);
    VARIABLE(var_holder_instance_type, MachineRepresentation::kWord32,
             instance_type);

    Variable* merged_variables[] = {&var_holder, &var_holder_map,
                                    &var_holder_instance_type};
    Label loop(this, arraysize(merged_variables), merged_variables);
    Goto(&loop);
    BIND(&loop);
    {
      Label next_proto(this);
      lookup_element_in_holder(receiver, var_holder.value(),
                               var_holder_map.value(),
                               var_holder_instance_type.value(),
                               var_index.value(), &next_proto, if_bailout);
      BIND(&next_proto);

      Node* proto = LoadMapPrototype(var_holder_map.value());

      GotoIf(IsNull(proto), if_end);

      Node* map = LoadMap(proto);
      Node* instance_type = LoadMapInstanceType(map);

      var_holder.Bind(proto);
      var_holder_map.Bind(map);
      var_holder_instance_type.Bind(instance_type);
      Goto(&loop);
    }
  }
}

Node* CodeStubAssembler::HasInPrototypeChain(Node* context, Node* object,
                                             Node* prototype) {
  CSA_ASSERT(this, TaggedIsNotSmi(object));
  VARIABLE(var_result, MachineRepresentation::kTagged);
  Label return_false(this), return_true(this),
      return_runtime(this, Label::kDeferred), return_result(this);

  // Loop through the prototype chain looking for the {prototype}.
  VARIABLE(var_object_map, MachineRepresentation::kTagged, LoadMap(object));
  Label loop(this, &var_object_map);
  Goto(&loop);
  BIND(&loop);
  {
    // Check if we can determine the prototype directly from the {object_map}.
    Label if_objectisdirect(this), if_objectisspecial(this, Label::kDeferred);
    Node* object_map = var_object_map.value();
    TNode<Int32T> object_instance_type = LoadMapInstanceType(object_map);
    Branch(IsSpecialReceiverInstanceType(object_instance_type),
           &if_objectisspecial, &if_objectisdirect);
    BIND(&if_objectisspecial);
    {
      // The {object_map} is a special receiver map or a primitive map, check
      // if we need to use the if_objectisspecial path in the runtime.
      GotoIf(InstanceTypeEqual(object_instance_type, JS_PROXY_TYPE),
             &return_runtime);
      Node* object_bitfield = LoadMapBitField(object_map);
      int mask = Map::HasNamedInterceptorBit::kMask |
                 Map::IsAccessCheckNeededBit::kMask;
      Branch(IsSetWord32(object_bitfield, mask), &return_runtime,
             &if_objectisdirect);
    }
    BIND(&if_objectisdirect);

    // Check the current {object} prototype.
    Node* object_prototype = LoadMapPrototype(object_map);
    GotoIf(IsNull(object_prototype), &return_false);
    GotoIf(WordEqual(object_prototype, prototype), &return_true);

    // Continue with the prototype.
    CSA_ASSERT(this, TaggedIsNotSmi(object_prototype));
    var_object_map.Bind(LoadMap(object_prototype));
    Goto(&loop);
  }

  BIND(&return_true);
  var_result.Bind(TrueConstant());
  Goto(&return_result);

  BIND(&return_false);
  var_result.Bind(FalseConstant());
  Goto(&return_result);

  BIND(&return_runtime);
  {
    // Fallback to the runtime implementation.
    var_result.Bind(
        CallRuntime(Runtime::kHasInPrototypeChain, context, object, prototype));
  }
  Goto(&return_result);

  BIND(&return_result);
  return var_result.value();
}

Node* CodeStubAssembler::OrdinaryHasInstance(Node* context, Node* callable,
                                             Node* object) {
  VARIABLE(var_result, MachineRepresentation::kTagged);
  Label return_runtime(this, Label::kDeferred), return_result(this);

  GotoIfForceSlowPath(&return_runtime);

  // Goto runtime if {object} is a Smi.
  GotoIf(TaggedIsSmi(object), &return_runtime);

  // Goto runtime if {callable} is a Smi.
  GotoIf(TaggedIsSmi(callable), &return_runtime);

  // Load map of {callable}.
  Node* callable_map = LoadMap(callable);

  // Goto runtime if {callable} is not a JSFunction.
  Node* callable_instance_type = LoadMapInstanceType(callable_map);
  GotoIfNot(InstanceTypeEqual(callable_instance_type, JS_FUNCTION_TYPE),
            &return_runtime);

  GotoIfPrototypeRequiresRuntimeLookup(CAST(callable), CAST(callable_map),
                                       &return_runtime);

  // Get the "prototype" (or initial map) of the {callable}.
  Node* callable_prototype =
      LoadObjectField(callable, JSFunction::kPrototypeOrInitialMapOffset);
  {
    Label no_initial_map(this), walk_prototype_chain(this);
    VARIABLE(var_callable_prototype, MachineRepresentation::kTagged,
             callable_prototype);

    // Resolve the "prototype" if the {callable} has an initial map.
    GotoIfNot(IsMap(callable_prototype), &no_initial_map);
    var_callable_prototype.Bind(
        LoadObjectField(callable_prototype, Map::kPrototypeOffset));
    Goto(&walk_prototype_chain);

    BIND(&no_initial_map);
    // {callable_prototype} is the hole if the "prototype" property hasn't been
    // requested so far.
    Branch(WordEqual(callable_prototype, TheHoleConstant()), &return_runtime,
           &walk_prototype_chain);

    BIND(&walk_prototype_chain);
    callable_prototype = var_callable_prototype.value();
  }

  // Loop through the prototype chain looking for the {callable} prototype.
  CSA_ASSERT(this, IsJSReceiver(callable_prototype));
  var_result.Bind(HasInPrototypeChain(context, object, callable_prototype));
  Goto(&return_result);

  BIND(&return_runtime);
  {
    // Fallback to the runtime implementation.
    var_result.Bind(
        CallRuntime(Runtime::kOrdinaryHasInstance, context, callable, object));
  }
  Goto(&return_result);

  BIND(&return_result);
  return var_result.value();
}

TNode<IntPtrT> CodeStubAssembler::ElementOffsetFromIndex(Node* index_node,
                                                         ElementsKind kind,
                                                         ParameterMode mode,
                                                         int base_size) {
  CSA_SLOW_ASSERT(this, MatchesParameterMode(index_node, mode));
  int element_size_shift = ElementsKindToShiftSize(kind);
  int element_size = 1 << element_size_shift;
  int const kSmiShiftBits = kSmiShiftSize + kSmiTagSize;
  intptr_t index = 0;
  bool constant_index = false;
  if (mode == SMI_PARAMETERS) {
    element_size_shift -= kSmiShiftBits;
    Smi smi_index;
    constant_index = ToSmiConstant(index_node, &smi_index);
    if (constant_index) index = smi_index.value();
    index_node = BitcastTaggedSignedToWord(index_node);
  } else {
    DCHECK(mode == INTPTR_PARAMETERS);
    constant_index = ToIntPtrConstant(index_node, index);
  }
  if (constant_index) {
    return IntPtrConstant(base_size + element_size * index);
  }

  TNode<WordT> shifted_index =
      (element_size_shift == 0)
          ? UncheckedCast<WordT>(index_node)
          : ((element_size_shift > 0)
                 ? WordShl(index_node, IntPtrConstant(element_size_shift))
                 : WordSar(index_node, IntPtrConstant(-element_size_shift)));
  return IntPtrAdd(IntPtrConstant(base_size), Signed(shifted_index));
}

TNode<BoolT> CodeStubAssembler::IsOffsetInBounds(SloppyTNode<IntPtrT> offset,
                                                 SloppyTNode<IntPtrT> length,
                                                 int header_size,
                                                 ElementsKind kind) {
  // Make sure we point to the last field.
  int element_size = 1 << ElementsKindToShiftSize(kind);
  int correction = header_size - kHeapObjectTag - element_size;
  TNode<IntPtrT> last_offset =
      ElementOffsetFromIndex(length, kind, INTPTR_PARAMETERS, correction);
  return IntPtrLessThanOrEqual(offset, last_offset);
}

TNode<HeapObject> CodeStubAssembler::LoadFeedbackCellValue(
    SloppyTNode<JSFunction> closure) {
  TNode<FeedbackCell> feedback_cell =
      CAST(LoadObjectField(closure, JSFunction::kFeedbackCellOffset));
  return CAST(LoadObjectField(feedback_cell, FeedbackCell::kValueOffset));
}

TNode<HeapObject> CodeStubAssembler::LoadFeedbackVector(
    SloppyTNode<JSFunction> closure) {
  TVARIABLE(HeapObject, maybe_vector, LoadFeedbackCellValue(closure));
  Label done(this);

  // If the closure doesn't have a feedback vector allocated yet, return
  // undefined. FeedbackCell can contain Undefined / FixedArray (for lazy
  // allocations) / FeedbackVector.
  GotoIf(IsFeedbackVector(maybe_vector.value()), &done);

  // In all other cases return Undefined.
  maybe_vector = UndefinedConstant();
  Goto(&done);

  BIND(&done);
  return maybe_vector.value();
}

TNode<ClosureFeedbackCellArray> CodeStubAssembler::LoadClosureFeedbackArray(
    SloppyTNode<JSFunction> closure) {
  TVARIABLE(HeapObject, feedback_cell_array, LoadFeedbackCellValue(closure));
  Label end(this);

  // When feedback vectors are not yet allocated feedback cell contains a
  // an array of feedback cells used by create closures.
  GotoIf(HasInstanceType(feedback_cell_array.value(),
                         CLOSURE_FEEDBACK_CELL_ARRAY_TYPE),
         &end);

  // Load FeedbackCellArray from feedback vector.
  TNode<FeedbackVector> vector = CAST(feedback_cell_array.value());
  feedback_cell_array = CAST(
      LoadObjectField(vector, FeedbackVector::kClosureFeedbackCellArrayOffset));
  Goto(&end);

  BIND(&end);
  return CAST(feedback_cell_array.value());
}

TNode<FeedbackVector> CodeStubAssembler::LoadFeedbackVectorForStub() {
  TNode<JSFunction> function =
      CAST(LoadFromParentFrame(JavaScriptFrameConstants::kFunctionOffset));
  return CAST(LoadFeedbackVector(function));
}

void CodeStubAssembler::UpdateFeedback(Node* feedback, Node* maybe_vector,
                                       Node* slot_id) {
  Label end(this);
  // If feedback_vector is not valid, then nothing to do.
  GotoIf(IsUndefined(maybe_vector), &end);

  // This method is used for binary op and compare feedback. These
  // vector nodes are initialized with a smi 0, so we can simply OR
  // our new feedback in place.
  TNode<FeedbackVector> feedback_vector = CAST(maybe_vector);
  TNode<MaybeObject> feedback_element =
      LoadFeedbackVectorSlot(feedback_vector, slot_id);
  TNode<Smi> previous_feedback = CAST(feedback_element);
  TNode<Smi> combined_feedback = SmiOr(previous_feedback, CAST(feedback));

  GotoIf(SmiEqual(previous_feedback, combined_feedback), &end);
  {
    StoreFeedbackVectorSlot(feedback_vector, slot_id, combined_feedback,
                            SKIP_WRITE_BARRIER);
    ReportFeedbackUpdate(feedback_vector, slot_id, "UpdateFeedback");
    Goto(&end);
  }

  BIND(&end);
}

void CodeStubAssembler::ReportFeedbackUpdate(
    SloppyTNode<FeedbackVector> feedback_vector, SloppyTNode<IntPtrT> slot_id,
    const char* reason) {
  // Reset profiler ticks.
  StoreObjectFieldNoWriteBarrier(
      feedback_vector, FeedbackVector::kProfilerTicksOffset, Int32Constant(0),
      MachineRepresentation::kWord32);

#ifdef V8_TRACE_FEEDBACK_UPDATES
  // Trace the update.
  CallRuntime(Runtime::kInterpreterTraceUpdateFeedback, NoContextConstant(),
              LoadFromParentFrame(JavaScriptFrameConstants::kFunctionOffset),
              SmiTag(slot_id), StringConstant(reason));
#endif  // V8_TRACE_FEEDBACK_UPDATES
}

void CodeStubAssembler::OverwriteFeedback(Variable* existing_feedback,
                                          int new_feedback) {
  if (existing_feedback == nullptr) return;
  existing_feedback->Bind(SmiConstant(new_feedback));
}

void CodeStubAssembler::CombineFeedback(Variable* existing_feedback,
                                        int feedback) {
  if (existing_feedback == nullptr) return;
  existing_feedback->Bind(
      SmiOr(CAST(existing_feedback->value()), SmiConstant(feedback)));
}

void CodeStubAssembler::CombineFeedback(Variable* existing_feedback,
                                        Node* feedback) {
  if (existing_feedback == nullptr) return;
  existing_feedback->Bind(
      SmiOr(CAST(existing_feedback->value()), CAST(feedback)));
}

void CodeStubAssembler::CheckForAssociatedProtector(Node* name,
                                                    Label* if_protector) {
  // This list must be kept in sync with LookupIterator::UpdateProtector!
  // TODO(jkummerow): Would it be faster to have a bit in Symbol::flags()?
  GotoIf(WordEqual(name, LoadRoot(RootIndex::kconstructor_string)),
         if_protector);
  GotoIf(WordEqual(name, LoadRoot(RootIndex::kiterator_symbol)), if_protector);
  GotoIf(WordEqual(name, LoadRoot(RootIndex::knext_string)), if_protector);
  GotoIf(WordEqual(name, LoadRoot(RootIndex::kspecies_symbol)), if_protector);
  GotoIf(WordEqual(name, LoadRoot(RootIndex::kis_concat_spreadable_symbol)),
         if_protector);
  GotoIf(WordEqual(name, LoadRoot(RootIndex::kresolve_string)), if_protector);
  GotoIf(WordEqual(name, LoadRoot(RootIndex::kthen_string)), if_protector);
  // Fall through if no case matched.
}

TNode<Map> CodeStubAssembler::LoadReceiverMap(SloppyTNode<Object> receiver) {
  return Select<Map>(
      TaggedIsSmi(receiver),
      [=] { return CAST(LoadRoot(RootIndex::kHeapNumberMap)); },
      [=] { return LoadMap(UncheckedCast<HeapObject>(receiver)); });
}

TNode<IntPtrT> CodeStubAssembler::TryToIntptr(Node* key, Label* miss) {
  TVARIABLE(IntPtrT, var_intptr_key);
  Label done(this, &var_intptr_key), key_is_smi(this);
  GotoIf(TaggedIsSmi(key), &key_is_smi);
  // Try to convert a heap number to a Smi.
  GotoIfNot(IsHeapNumber(key), miss);
  {
    TNode<Float64T> value = LoadHeapNumberValue(key);
    TNode<Int32T> int_value = RoundFloat64ToInt32(value);
    GotoIfNot(Float64Equal(value, ChangeInt32ToFloat64(int_value)), miss);
    var_intptr_key = ChangeInt32ToIntPtr(int_value);
    Goto(&done);
  }

  BIND(&key_is_smi);
  {
    var_intptr_key = SmiUntag(key);
    Goto(&done);
  }

  BIND(&done);
  return var_intptr_key.value();
}

Node* CodeStubAssembler::EmitKeyedSloppyArguments(
    Node* receiver, Node* key, Node* value, Label* bailout,
    ArgumentsAccessMode access_mode) {
  // Mapped arguments are actual arguments. Unmapped arguments are values added
  // to the arguments object after it was created for the call. Mapped arguments
  // are stored in the context at indexes given by elements[key + 2]. Unmapped
  // arguments are stored as regular indexed properties in the arguments array,
  // held at elements[1]. See NewSloppyArguments() in runtime.cc for a detailed
  // look at argument object construction.
  //
  // The sloppy arguments elements array has a special format:
  //
  // 0: context
  // 1: unmapped arguments array
  // 2: mapped_index0,
  // 3: mapped_index1,
  // ...
  //
  // length is 2 + min(number_of_actual_arguments, number_of_formal_arguments).
  // If key + 2 >= elements.length then attempt to look in the unmapped
  // arguments array (given by elements[1]) and return the value at key, missing
  // to the runtime if the unmapped arguments array is not a fixed array or if
  // key >= unmapped_arguments_array.length.
  //
  // Otherwise, t = elements[key + 2]. If t is the hole, then look up the value
  // in the unmapped arguments array, as described above. Otherwise, t is a Smi
  // index into the context array given at elements[0]. Return the value at
  // context[t].

  GotoIfNot(TaggedIsSmi(key), bailout);
  key = SmiUntag(key);
  GotoIf(IntPtrLessThan(key, IntPtrConstant(0)), bailout);

  TNode<FixedArray> elements = CAST(LoadElements(receiver));
  TNode<IntPtrT> elements_length = LoadAndUntagFixedArrayBaseLength(elements);

  VARIABLE(var_result, MachineRepresentation::kTagged);
  if (access_mode == ArgumentsAccessMode::kStore) {
    var_result.Bind(value);
  } else {
    DCHECK(access_mode == ArgumentsAccessMode::kLoad ||
           access_mode == ArgumentsAccessMode::kHas);
  }
  Label if_mapped(this), if_unmapped(this), end(this, &var_result);
  Node* intptr_two = IntPtrConstant(2);
  Node* adjusted_length = IntPtrSub(elements_length, intptr_two);

  GotoIf(UintPtrGreaterThanOrEqual(key, adjusted_length), &if_unmapped);

  TNode<Object> mapped_index =
      LoadFixedArrayElement(elements, IntPtrAdd(key, intptr_two));
  Branch(WordEqual(mapped_index, TheHoleConstant()), &if_unmapped, &if_mapped);

  BIND(&if_mapped);
  {
    TNode<IntPtrT> mapped_index_intptr = SmiUntag(CAST(mapped_index));
    TNode<Context> the_context = CAST(LoadFixedArrayElement(elements, 0));
    if (access_mode == ArgumentsAccessMode::kLoad) {
      Node* result = LoadContextElement(the_context, mapped_index_intptr);
      CSA_ASSERT(this, WordNotEqual(result, TheHoleConstant()));
      var_result.Bind(result);
    } else if (access_mode == ArgumentsAccessMode::kHas) {
      CSA_ASSERT(this, Word32BinaryNot(IsTheHole(LoadContextElement(
                           the_context, mapped_index_intptr))));
      var_result.Bind(TrueConstant());
    } else {
      StoreContextElement(the_context, mapped_index_intptr, value);
    }
    Goto(&end);
  }

  BIND(&if_unmapped);
  {
    TNode<HeapObject> backing_store_ho =
        CAST(LoadFixedArrayElement(elements, 1));
    GotoIf(WordNotEqual(LoadMap(backing_store_ho), FixedArrayMapConstant()),
           bailout);
    TNode<FixedArray> backing_store = CAST(backing_store_ho);

    TNode<IntPtrT> backing_store_length =
        LoadAndUntagFixedArrayBaseLength(backing_store);
    if (access_mode == ArgumentsAccessMode::kHas) {
      Label out_of_bounds(this);
      GotoIf(UintPtrGreaterThanOrEqual(key, backing_store_length),
             &out_of_bounds);
      Node* result = LoadFixedArrayElement(backing_store, key);
      var_result.Bind(
          SelectBooleanConstant(WordNotEqual(result, TheHoleConstant())));
      Goto(&end);

      BIND(&out_of_bounds);
      var_result.Bind(FalseConstant());
      Goto(&end);
    } else {
      GotoIf(UintPtrGreaterThanOrEqual(key, backing_store_length), bailout);

      // The key falls into unmapped range.
      if (access_mode == ArgumentsAccessMode::kLoad) {
        Node* result = LoadFixedArrayElement(backing_store, key);
        GotoIf(WordEqual(result, TheHoleConstant()), bailout);
        var_result.Bind(result);
      } else {
        StoreFixedArrayElement(backing_store, key, value);
      }
      Goto(&end);
    }
  }

  BIND(&end);
  return var_result.value();
}

TNode<Context> CodeStubAssembler::LoadScriptContext(
    TNode<Context> context, TNode<IntPtrT> context_index) {
  TNode<Context> native_context = LoadNativeContext(context);
  TNode<ScriptContextTable> script_context_table = CAST(
      LoadContextElement(native_context, Context::SCRIPT_CONTEXT_TABLE_INDEX));

  TNode<Context> script_context = CAST(LoadFixedArrayElement(
      script_context_table, context_index,
      ScriptContextTable::kFirstContextSlotIndex * kTaggedSize));
  return script_context;
}

namespace {

// Converts typed array elements kind to a machine representations.
MachineRepresentation ElementsKindToMachineRepresentation(ElementsKind kind) {
  switch (kind) {
    case UINT8_CLAMPED_ELEMENTS:
    case UINT8_ELEMENTS:
    case INT8_ELEMENTS:
      return MachineRepresentation::kWord8;
    case UINT16_ELEMENTS:
    case INT16_ELEMENTS:
      return MachineRepresentation::kWord16;
    case UINT32_ELEMENTS:
    case INT32_ELEMENTS:
      return MachineRepresentation::kWord32;
    case FLOAT32_ELEMENTS:
      return MachineRepresentation::kFloat32;
    case FLOAT64_ELEMENTS:
      return MachineRepresentation::kFloat64;
    default:
      UNREACHABLE();
  }
}

}  // namespace

void CodeStubAssembler::StoreElement(Node* elements, ElementsKind kind,
                                     Node* index, Node* value,
                                     ParameterMode mode) {
  if (kind == BIGINT64_ELEMENTS || kind == BIGUINT64_ELEMENTS) {
    TNode<IntPtrT> offset = ElementOffsetFromIndex(index, kind, mode, 0);
    TVARIABLE(UintPtrT, var_low);
    // Only used on 32-bit platforms.
    TVARIABLE(UintPtrT, var_high);
    BigIntToRawBytes(CAST(value), &var_low, &var_high);

    MachineRepresentation rep = WordT::kMachineRepresentation;
#if defined(V8_TARGET_BIG_ENDIAN)
    if (!Is64()) {
      StoreNoWriteBarrier(rep, elements, offset, var_high.value());
      StoreNoWriteBarrier(rep, elements,
                          IntPtrAdd(offset, IntPtrConstant(kSystemPointerSize)),
                          var_low.value());
    } else {
      StoreNoWriteBarrier(rep, elements, offset, var_low.value());
    }
#else
    StoreNoWriteBarrier(rep, elements, offset, var_low.value());
    if (!Is64()) {
      StoreNoWriteBarrier(rep, elements,
                          IntPtrAdd(offset, IntPtrConstant(kSystemPointerSize)),
                          var_high.value());
    }
#endif
  } else if (IsTypedArrayElementsKind(kind)) {
    if (kind == UINT8_CLAMPED_ELEMENTS) {
      CSA_ASSERT(this,
                 Word32Equal(value, Word32And(Int32Constant(0xFF), value)));
    }
    Node* offset = ElementOffsetFromIndex(index, kind, mode, 0);
    // TODO(cbruni): Add OOB check once typed.
    MachineRepresentation rep = ElementsKindToMachineRepresentation(kind);
    StoreNoWriteBarrier(rep, elements, offset, value);
    return;
  } else if (IsDoubleElementsKind(kind)) {
    TNode<Float64T> value_float64 = UncheckedCast<Float64T>(value);
    StoreFixedDoubleArrayElement(CAST(elements), index, value_float64, mode);
  } else {
    WriteBarrierMode barrier_mode = IsSmiElementsKind(kind)
                                        ? UNSAFE_SKIP_WRITE_BARRIER
                                        : UPDATE_WRITE_BARRIER;
    StoreFixedArrayElement(CAST(elements), index, value, barrier_mode, 0, mode);
  }
}

Node* CodeStubAssembler::Int32ToUint8Clamped(Node* int32_value) {
  Label done(this);
  Node* int32_zero = Int32Constant(0);
  Node* int32_255 = Int32Constant(255);
  VARIABLE(var_value, MachineRepresentation::kWord32, int32_value);
  GotoIf(Uint32LessThanOrEqual(int32_value, int32_255), &done);
  var_value.Bind(int32_zero);
  GotoIf(Int32LessThan(int32_value, int32_zero), &done);
  var_value.Bind(int32_255);
  Goto(&done);
  BIND(&done);
  return var_value.value();
}

Node* CodeStubAssembler::Float64ToUint8Clamped(Node* float64_value) {
  Label done(this);
  VARIABLE(var_value, MachineRepresentation::kWord32, Int32Constant(0));
  GotoIf(Float64LessThanOrEqual(float64_value, Float64Constant(0.0)), &done);
  var_value.Bind(Int32Constant(255));
  GotoIf(Float64LessThanOrEqual(Float64Constant(255.0), float64_value), &done);
  {
    Node* rounded_value = Float64RoundToEven(float64_value);
    var_value.Bind(TruncateFloat64ToWord32(rounded_value));
    Goto(&done);
  }
  BIND(&done);
  return var_value.value();
}

Node* CodeStubAssembler::PrepareValueForWriteToTypedArray(
    TNode<Object> input, ElementsKind elements_kind, TNode<Context> context) {
  DCHECK(IsTypedArrayElementsKind(elements_kind));

  MachineRepresentation rep;
  switch (elements_kind) {
    case UINT8_ELEMENTS:
    case INT8_ELEMENTS:
    case UINT16_ELEMENTS:
    case INT16_ELEMENTS:
    case UINT32_ELEMENTS:
    case INT32_ELEMENTS:
    case UINT8_CLAMPED_ELEMENTS:
      rep = MachineRepresentation::kWord32;
      break;
    case FLOAT32_ELEMENTS:
      rep = MachineRepresentation::kFloat32;
      break;
    case FLOAT64_ELEMENTS:
      rep = MachineRepresentation::kFloat64;
      break;
    case BIGINT64_ELEMENTS:
    case BIGUINT64_ELEMENTS:
      return ToBigInt(context, input);
    default:
      UNREACHABLE();
  }

  VARIABLE(var_result, rep);
  VARIABLE(var_input, MachineRepresentation::kTagged, input);
  Label done(this, &var_result), if_smi(this), if_heapnumber_or_oddball(this),
      convert(this), loop(this, &var_input);
  Goto(&loop);
  BIND(&loop);
  GotoIf(TaggedIsSmi(var_input.value()), &if_smi);
  // We can handle both HeapNumber and Oddball here, since Oddball has the
  // same layout as the HeapNumber for the HeapNumber::value field. This
  // way we can also properly optimize stores of oddballs to typed arrays.
  GotoIf(IsHeapNumber(var_input.value()), &if_heapnumber_or_oddball);
  STATIC_ASSERT_FIELD_OFFSETS_EQUAL(HeapNumber::kValueOffset,
                                    Oddball::kToNumberRawOffset);
  Branch(HasInstanceType(var_input.value(), ODDBALL_TYPE),
         &if_heapnumber_or_oddball, &convert);

  BIND(&if_heapnumber_or_oddball);
  {
    Node* value = UncheckedCast<Float64T>(LoadObjectField(
        var_input.value(), HeapNumber::kValueOffset, MachineType::Float64()));
    if (rep == MachineRepresentation::kWord32) {
      if (elements_kind == UINT8_CLAMPED_ELEMENTS) {
        value = Float64ToUint8Clamped(value);
      } else {
        value = TruncateFloat64ToWord32(value);
      }
    } else if (rep == MachineRepresentation::kFloat32) {
      value = TruncateFloat64ToFloat32(value);
    } else {
      DCHECK_EQ(MachineRepresentation::kFloat64, rep);
    }
    var_result.Bind(value);
    Goto(&done);
  }

  BIND(&if_smi);
  {
    Node* value = SmiToInt32(var_input.value());
    if (rep == MachineRepresentation::kFloat32) {
      value = RoundInt32ToFloat32(value);
    } else if (rep == MachineRepresentation::kFloat64) {
      value = ChangeInt32ToFloat64(value);
    } else {
      DCHECK_EQ(MachineRepresentation::kWord32, rep);
      if (elements_kind == UINT8_CLAMPED_ELEMENTS) {
        value = Int32ToUint8Clamped(value);
      }
    }
    var_result.Bind(value);
    Goto(&done);
  }

  BIND(&convert);
  {
    var_input.Bind(CallBuiltin(Builtins::kNonNumberToNumber, context, input));
    Goto(&loop);
  }

  BIND(&done);
  return var_result.value();
}

void CodeStubAssembler::BigIntToRawBytes(TNode<BigInt> bigint,
                                         TVariable<UintPtrT>* var_low,
                                         TVariable<UintPtrT>* var_high) {
  Label done(this);
  *var_low = Unsigned(IntPtrConstant(0));
  *var_high = Unsigned(IntPtrConstant(0));
  TNode<Word32T> bitfield = LoadBigIntBitfield(bigint);
  TNode<Uint32T> length = DecodeWord32<BigIntBase::LengthBits>(bitfield);
  TNode<Uint32T> sign = DecodeWord32<BigIntBase::SignBits>(bitfield);
  GotoIf(Word32Equal(length, Int32Constant(0)), &done);
  *var_low = LoadBigIntDigit(bigint, 0);
  if (!Is64()) {
    Label load_done(this);
    GotoIf(Word32Equal(length, Int32Constant(1)), &load_done);
    *var_high = LoadBigIntDigit(bigint, 1);
    Goto(&load_done);
    BIND(&load_done);
  }
  GotoIf(Word32Equal(sign, Int32Constant(0)), &done);
  // Negative value. Simulate two's complement.
  if (!Is64()) {
    *var_high = Unsigned(IntPtrSub(IntPtrConstant(0), var_high->value()));
    Label no_carry(this);
    GotoIf(WordEqual(var_low->value(), IntPtrConstant(0)), &no_carry);
    *var_high = Unsigned(IntPtrSub(var_high->value(), IntPtrConstant(1)));
    Goto(&no_carry);
    BIND(&no_carry);
  }
  *var_low = Unsigned(IntPtrSub(IntPtrConstant(0), var_low->value()));
  Goto(&done);
  BIND(&done);
}

void CodeStubAssembler::EmitElementStore(Node* object, Node* key, Node* value,
                                         ElementsKind elements_kind,
                                         KeyedAccessStoreMode store_mode,
                                         Label* bailout, Node* context,
                                         Variable* maybe_converted_value) {
  CSA_ASSERT(this, Word32BinaryNot(IsJSProxy(object)));

  Node* elements = LoadElements(object);
  if (!(IsSmiOrObjectElementsKind(elements_kind) ||
        IsSealedElementsKind(elements_kind))) {
    CSA_ASSERT(this, Word32BinaryNot(IsFixedCOWArrayMap(LoadMap(elements))));
  } else if (!IsCOWHandlingStoreMode(store_mode)) {
    GotoIf(IsFixedCOWArrayMap(LoadMap(elements)), bailout);
  }

  // TODO(ishell): introduce TryToIntPtrOrSmi() and use OptimalParameterMode().
  ParameterMode parameter_mode = INTPTR_PARAMETERS;
  TNode<IntPtrT> intptr_key = TryToIntptr(key, bailout);

  if (IsTypedArrayElementsKind(elements_kind)) {
    Label done(this), update_value_and_bailout(this, Label::kDeferred);

    // IntegerIndexedElementSet converts value to a Number/BigInt prior to the
    // bounds check.
    Node* converted_value = PrepareValueForWriteToTypedArray(
        CAST(value), elements_kind, CAST(context));

    // There must be no allocations between the buffer load and
    // and the actual store to backing store, because GC may decide that
    // the buffer is not alive or move the elements.
    // TODO(ishell): introduce DisallowHeapAllocationCode scope here.

    // Check if buffer has been detached.
    TNode<JSArrayBuffer> buffer = LoadJSArrayBufferViewBuffer(CAST(object));
    if (maybe_converted_value) {
      GotoIf(IsDetachedBuffer(buffer), &update_value_and_bailout);
    } else {
      GotoIf(IsDetachedBuffer(buffer), bailout);
    }

    // Bounds check.
    TNode<UintPtrT> length = LoadJSTypedArrayLength(CAST(object));

    if (store_mode == STORE_IGNORE_OUT_OF_BOUNDS) {
      // Skip the store if we write beyond the length or
      // to a property with a negative integer index.
      GotoIfNot(UintPtrLessThan(intptr_key, length), &done);
    } else {
      DCHECK_EQ(store_mode, STANDARD_STORE);
      GotoIfNot(UintPtrLessThan(intptr_key, length), &update_value_and_bailout);
    }

    TNode<RawPtrT> backing_store = LoadJSTypedArrayBackingStore(CAST(object));
    StoreElement(backing_store, elements_kind, intptr_key, converted_value,
                 parameter_mode);
    Goto(&done);

    BIND(&update_value_and_bailout);
    // We already prepared the incoming value for storing into a typed array.
    // This might involve calling ToNumber in some cases. We shouldn't call
    // ToNumber again in the runtime so pass the converted value to the runtime.
    // The prepared value is an untagged value. Convert it to a tagged value
    // to pass it to runtime. It is not possible to do the detached buffer check
    // before we prepare the value, since ToNumber can detach the ArrayBuffer.
    // The spec specifies the order of these operations.
    if (maybe_converted_value != nullptr) {
      switch (elements_kind) {
        case UINT8_ELEMENTS:
        case INT8_ELEMENTS:
        case UINT16_ELEMENTS:
        case INT16_ELEMENTS:
        case UINT8_CLAMPED_ELEMENTS:
          maybe_converted_value->Bind(SmiFromInt32(converted_value));
          break;
        case UINT32_ELEMENTS:
          maybe_converted_value->Bind(ChangeUint32ToTagged(converted_value));
          break;
        case INT32_ELEMENTS:
          maybe_converted_value->Bind(ChangeInt32ToTagged(converted_value));
          break;
        case FLOAT32_ELEMENTS: {
          Label dont_allocate_heap_number(this), end(this);
          GotoIf(TaggedIsSmi(value), &dont_allocate_heap_number);
          GotoIf(IsHeapNumber(value), &dont_allocate_heap_number);
          {
            maybe_converted_value->Bind(AllocateHeapNumberWithValue(
                ChangeFloat32ToFloat64(converted_value)));
            Goto(&end);
          }
          BIND(&dont_allocate_heap_number);
          {
            maybe_converted_value->Bind(value);
            Goto(&end);
          }
          BIND(&end);
          break;
        }
        case FLOAT64_ELEMENTS: {
          Label dont_allocate_heap_number(this), end(this);
          GotoIf(TaggedIsSmi(value), &dont_allocate_heap_number);
          GotoIf(IsHeapNumber(value), &dont_allocate_heap_number);
          {
            maybe_converted_value->Bind(
                AllocateHeapNumberWithValue(converted_value));
            Goto(&end);
          }
          BIND(&dont_allocate_heap_number);
          {
            maybe_converted_value->Bind(value);
            Goto(&end);
          }
          BIND(&end);
          break;
        }
        case BIGINT64_ELEMENTS:
        case BIGUINT64_ELEMENTS:
          maybe_converted_value->Bind(converted_value);
          break;
        default:
          UNREACHABLE();
      }
    }
    Goto(bailout);

    BIND(&done);
    return;
  }
  DCHECK(IsFastElementsKind(elements_kind) ||
         IsSealedElementsKind(elements_kind));

  Node* length = SelectImpl(
      IsJSArray(object), [=]() { return LoadJSArrayLength(object); },
      [=]() { return LoadFixedArrayBaseLength(elements); },
      MachineRepresentation::kTagged);
  length = TaggedToParameter(length, parameter_mode);

  // In case value is stored into a fast smi array, assure that the value is
  // a smi before manipulating the backing store. Otherwise the backing store
  // may be left in an invalid state.
  if (IsSmiElementsKind(elements_kind)) {
    GotoIfNot(TaggedIsSmi(value), bailout);
  } else if (IsDoubleElementsKind(elements_kind)) {
    value = TryTaggedToFloat64(value, bailout);
  }

  if (IsGrowStoreMode(store_mode) && !IsSealedElementsKind(elements_kind)) {
    elements = CheckForCapacityGrow(object, elements, elements_kind, length,
                                    intptr_key, parameter_mode, bailout);
  } else {
    GotoIfNot(UintPtrLessThan(intptr_key, length), bailout);
  }

  // Cannot store to a hole in holey sealed elements so bailout.
  if (elements_kind == HOLEY_SEALED_ELEMENTS) {
    TNode<Object> target_value =
        LoadFixedArrayElement(CAST(elements), intptr_key);
    GotoIf(IsTheHole(target_value), bailout);
  }

  // If we didn't grow {elements}, it might still be COW, in which case we
  // copy it now.
  if (!(IsSmiOrObjectElementsKind(elements_kind) ||
        IsSealedElementsKind(elements_kind))) {
    CSA_ASSERT(this, Word32BinaryNot(IsFixedCOWArrayMap(LoadMap(elements))));
  } else if (IsCOWHandlingStoreMode(store_mode)) {
    elements = CopyElementsOnWrite(object, elements, elements_kind, length,
                                   parameter_mode, bailout);
  }

  CSA_ASSERT(this, Word32BinaryNot(IsFixedCOWArrayMap(LoadMap(elements))));
  StoreElement(elements, elements_kind, intptr_key, value, parameter_mode);
}

Node* CodeStubAssembler::CheckForCapacityGrow(Node* object, Node* elements,
                                              ElementsKind kind, Node* length,
                                              Node* key, ParameterMode mode,
                                              Label* bailout) {
  DCHECK(IsFastElementsKind(kind));
  VARIABLE(checked_elements, MachineRepresentation::kTagged);
  Label grow_case(this), no_grow_case(this), done(this),
      grow_bailout(this, Label::kDeferred);

  Node* condition;
  if (IsHoleyElementsKind(kind)) {
    condition = UintPtrGreaterThanOrEqual(key, length);
  } else {
    // We don't support growing here unless the value is being appended.
    condition = WordEqual(key, length);
  }
  Branch(condition, &grow_case, &no_grow_case);

  BIND(&grow_case);
  {
    Node* current_capacity =
        TaggedToParameter(LoadFixedArrayBaseLength(elements), mode);
    checked_elements.Bind(elements);
    Label fits_capacity(this);
    // If key is negative, we will notice in Runtime::kGrowArrayElements.
    GotoIf(UintPtrLessThan(key, current_capacity), &fits_capacity);

    {
      Node* new_elements = TryGrowElementsCapacity(
          object, elements, kind, key, current_capacity, mode, &grow_bailout);
      checked_elements.Bind(new_elements);
      Goto(&fits_capacity);
    }

    BIND(&grow_bailout);
    {
      Node* tagged_key = mode == SMI_PARAMETERS
                             ? key
                             : ChangeInt32ToTagged(TruncateIntPtrToInt32(key));
      Node* maybe_elements = CallRuntime(
          Runtime::kGrowArrayElements, NoContextConstant(), object, tagged_key);
      GotoIf(TaggedIsSmi(maybe_elements), bailout);
      CSA_ASSERT(this, IsFixedArrayWithKind(maybe_elements, kind));
      checked_elements.Bind(maybe_elements);
      Goto(&fits_capacity);
    }

    BIND(&fits_capacity);
    GotoIfNot(IsJSArray(object), &done);

    Node* new_length = IntPtrAdd(key, IntPtrOrSmiConstant(1, mode));
    StoreObjectFieldNoWriteBarrier(object, JSArray::kLengthOffset,
                                   ParameterToTagged(new_length, mode));
    Goto(&done);
  }

  BIND(&no_grow_case);
  {
    GotoIfNot(UintPtrLessThan(key, length), bailout);
    checked_elements.Bind(elements);
    Goto(&done);
  }

  BIND(&done);
  return checked_elements.value();
}

Node* CodeStubAssembler::CopyElementsOnWrite(Node* object, Node* elements,
                                             ElementsKind kind, Node* length,
                                             ParameterMode mode,
                                             Label* bailout) {
  VARIABLE(new_elements_var, MachineRepresentation::kTagged, elements);
  Label done(this);

  GotoIfNot(IsFixedCOWArrayMap(LoadMap(elements)), &done);
  {
    Node* capacity =
        TaggedToParameter(LoadFixedArrayBaseLength(elements), mode);
    Node* new_elements = GrowElementsCapacity(object, elements, kind, kind,
                                              length, capacity, mode, bailout);
    new_elements_var.Bind(new_elements);
    Goto(&done);
  }

  BIND(&done);
  return new_elements_var.value();
}

void CodeStubAssembler::TransitionElementsKind(Node* object, Node* map,
                                               ElementsKind from_kind,
                                               ElementsKind to_kind,
                                               Label* bailout) {
  DCHECK(!IsHoleyElementsKind(from_kind) || IsHoleyElementsKind(to_kind));
  if (AllocationSite::ShouldTrack(from_kind, to_kind)) {
    TrapAllocationMemento(object, bailout);
  }

  if (!IsSimpleMapChangeTransition(from_kind, to_kind)) {
    Comment("Non-simple map transition");
    Node* elements = LoadElements(object);

    Label done(this);
    GotoIf(WordEqual(elements, EmptyFixedArrayConstant()), &done);

    // TODO(ishell): Use OptimalParameterMode().
    ParameterMode mode = INTPTR_PARAMETERS;
    Node* elements_length = SmiUntag(LoadFixedArrayBaseLength(elements));
    Node* array_length = SelectImpl(
        IsJSArray(object),
        [=]() {
          CSA_ASSERT(this, IsFastElementsKind(LoadElementsKind(object)));
          return SmiUntag(LoadFastJSArrayLength(object));
        },
        [=]() { return elements_length; },
        MachineType::PointerRepresentation());

    CSA_ASSERT(this, WordNotEqual(elements_length, IntPtrConstant(0)));

    GrowElementsCapacity(object, elements, from_kind, to_kind, array_length,
                         elements_length, mode, bailout);
    Goto(&done);
    BIND(&done);
  }

  StoreMap(object, map);
}

void CodeStubAssembler::TrapAllocationMemento(Node* object,
                                              Label* memento_found) {
  Comment("[ TrapAllocationMemento");
  Label no_memento_found(this);
  Label top_check(this), map_check(this);

  TNode<ExternalReference> new_space_top_address = ExternalConstant(
      ExternalReference::new_space_allocation_top_address(isolate()));
  const int kMementoMapOffset = JSArray::kSize;
  const int kMementoLastWordOffset =
      kMementoMapOffset + AllocationMemento::kSize - kTaggedSize;

  // Bail out if the object is not in new space.
  TNode<IntPtrT> object_word = BitcastTaggedToWord(object);
  TNode<IntPtrT> object_page = PageFromAddress(object_word);
  {
    TNode<IntPtrT> page_flags =
        UncheckedCast<IntPtrT>(Load(MachineType::IntPtr(), object_page,
                                    IntPtrConstant(Page::kFlagsOffset)));
    GotoIf(WordEqual(
               WordAnd(page_flags,
                       IntPtrConstant(MemoryChunk::kIsInYoungGenerationMask)),
               IntPtrConstant(0)),
           &no_memento_found);
    // TODO(ulan): Support allocation memento for a large object by allocating
    // additional word for the memento after the large object.
    GotoIf(WordNotEqual(WordAnd(page_flags,
                                IntPtrConstant(MemoryChunk::kIsLargePageMask)),
                        IntPtrConstant(0)),
           &no_memento_found);
  }

  TNode<IntPtrT> memento_last_word = IntPtrAdd(
      object_word, IntPtrConstant(kMementoLastWordOffset - kHeapObjectTag));
  TNode<IntPtrT> memento_last_word_page = PageFromAddress(memento_last_word);

  TNode<IntPtrT> new_space_top = UncheckedCast<IntPtrT>(
      Load(MachineType::Pointer(), new_space_top_address));
  TNode<IntPtrT> new_space_top_page = PageFromAddress(new_space_top);

  // If the object is in new space, we need to check whether respective
  // potential memento object is on the same page as the current top.
  GotoIf(WordEqual(memento_last_word_page, new_space_top_page), &top_check);

  // The object is on a different page than allocation top. Bail out if the
  // object sits on the page boundary as no memento can follow and we cannot
  // touch the memory following it.
  Branch(WordEqual(object_page, memento_last_word_page), &map_check,
         &no_memento_found);

  // If top is on the same page as the current object, we need to check whether
  // we are below top.
  BIND(&top_check);
  {
    Branch(UintPtrGreaterThanOrEqual(memento_last_word, new_space_top),
           &no_memento_found, &map_check);
  }

  // Memento map check.
  BIND(&map_check);
  {
    TNode<Object> memento_map = LoadObjectField(object, kMementoMapOffset);
    Branch(WordEqual(memento_map, LoadRoot(RootIndex::kAllocationMementoMap)),
           memento_found, &no_memento_found);
  }
  BIND(&no_memento_found);
  Comment("] TrapAllocationMemento");
}

TNode<IntPtrT> CodeStubAssembler::PageFromAddress(TNode<IntPtrT> address) {
  return WordAnd(address, IntPtrConstant(~kPageAlignmentMask));
}

TNode<AllocationSite> CodeStubAssembler::CreateAllocationSiteInFeedbackVector(
    SloppyTNode<FeedbackVector> feedback_vector, TNode<Smi> slot) {
  TNode<IntPtrT> size = IntPtrConstant(AllocationSite::kSizeWithWeakNext);
  Node* site = Allocate(size, CodeStubAssembler::kPretenured);
  StoreMapNoWriteBarrier(site, RootIndex::kAllocationSiteWithWeakNextMap);
  // Should match AllocationSite::Initialize.
  TNode<WordT> field = UpdateWord<AllocationSite::ElementsKindBits>(
      IntPtrConstant(0), IntPtrConstant(GetInitialFastElementsKind()));
  StoreObjectFieldNoWriteBarrier(
      site, AllocationSite::kTransitionInfoOrBoilerplateOffset,
      SmiTag(Signed(field)));

  // Unlike literals, constructed arrays don't have nested sites
  TNode<Smi> zero = SmiConstant(0);
  StoreObjectFieldNoWriteBarrier(site, AllocationSite::kNestedSiteOffset, zero);

  // Pretenuring calculation field.
  StoreObjectFieldNoWriteBarrier(site, AllocationSite::kPretenureDataOffset,
                                 Int32Constant(0),
                                 MachineRepresentation::kWord32);

  // Pretenuring memento creation count field.
  StoreObjectFieldNoWriteBarrier(
      site, AllocationSite::kPretenureCreateCountOffset, Int32Constant(0),
      MachineRepresentation::kWord32);

  // Store an empty fixed array for the code dependency.
  StoreObjectFieldRoot(site, AllocationSite::kDependentCodeOffset,
                       RootIndex::kEmptyWeakFixedArray);

  // Link the object to the allocation site list
  TNode<ExternalReference> site_list = ExternalConstant(
      ExternalReference::allocation_sites_list_address(isolate()));
  TNode<Object> next_site =
      LoadBufferObject(ReinterpretCast<RawPtrT>(site_list), 0);

  // TODO(mvstanton): This is a store to a weak pointer, which we may want to
  // mark as such in order to skip the write barrier, once we have a unified
  // system for weakness. For now we decided to keep it like this because having
  // an initial write barrier backed store makes this pointer strong until the
  // next GC, and allocation sites are designed to survive several GCs anyway.
  StoreObjectField(site, AllocationSite::kWeakNextOffset, next_site);
  StoreFullTaggedNoWriteBarrier(site_list, site);

  StoreFeedbackVectorSlot(feedback_vector, slot, site, UPDATE_WRITE_BARRIER, 0,
                          SMI_PARAMETERS);
  return CAST(site);
}

TNode<MaybeObject> CodeStubAssembler::StoreWeakReferenceInFeedbackVector(
    SloppyTNode<FeedbackVector> feedback_vector, Node* slot,
    SloppyTNode<HeapObject> value, int additional_offset,
    ParameterMode parameter_mode) {
  TNode<MaybeObject> weak_value = MakeWeak(value);
  StoreFeedbackVectorSlot(feedback_vector, slot, weak_value,
                          UPDATE_WRITE_BARRIER, additional_offset,
                          parameter_mode);
  return weak_value;
}

TNode<BoolT> CodeStubAssembler::NotHasBoilerplate(
    TNode<Object> maybe_literal_site) {
  return TaggedIsSmi(maybe_literal_site);
}

TNode<Smi> CodeStubAssembler::LoadTransitionInfo(
    TNode<AllocationSite> allocation_site) {
  TNode<Smi> transition_info = CAST(LoadObjectField(
      allocation_site, AllocationSite::kTransitionInfoOrBoilerplateOffset));
  return transition_info;
}

TNode<JSObject> CodeStubAssembler::LoadBoilerplate(
    TNode<AllocationSite> allocation_site) {
  TNode<JSObject> boilerplate = CAST(LoadObjectField(
      allocation_site, AllocationSite::kTransitionInfoOrBoilerplateOffset));
  return boilerplate;
}

TNode<Int32T> CodeStubAssembler::LoadElementsKind(
    TNode<AllocationSite> allocation_site) {
  TNode<Smi> transition_info = LoadTransitionInfo(allocation_site);
  TNode<Int32T> elements_kind =
      Signed(DecodeWord32<AllocationSite::ElementsKindBits>(
          SmiToInt32(transition_info)));
  CSA_ASSERT(this, IsFastElementsKind(elements_kind));
  return elements_kind;
}

Node* CodeStubAssembler::BuildFastLoop(
    const CodeStubAssembler::VariableList& vars, Node* start_index,
    Node* end_index, const FastLoopBody& body, int increment,
    ParameterMode parameter_mode, IndexAdvanceMode advance_mode) {
  CSA_SLOW_ASSERT(this, MatchesParameterMode(start_index, parameter_mode));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(end_index, parameter_mode));
  MachineRepresentation index_rep = ParameterRepresentation(parameter_mode);
  VARIABLE(var, index_rep, start_index);
  VariableList vars_copy(vars.begin(), vars.end(), zone());
  vars_copy.push_back(&var);
  Label loop(this, vars_copy);
  Label after_loop(this);
  // Introduce an explicit second check of the termination condition before the
  // loop that helps turbofan generate better code. If there's only a single
  // check, then the CodeStubAssembler forces it to be at the beginning of the
  // loop requiring a backwards branch at the end of the loop (it's not possible
  // to force the loop header check at the end of the loop and branch forward to
  // it from the pre-header). The extra branch is slower in the case that the
  // loop actually iterates.
  Node* first_check = WordEqual(var.value(), end_index);
  int32_t first_check_val;
  if (ToInt32Constant(first_check, first_check_val)) {
    if (first_check_val) return var.value();
    Goto(&loop);
  } else {
    Branch(first_check, &after_loop, &loop);
  }

  BIND(&loop);
  {
    if (advance_mode == IndexAdvanceMode::kPre) {
      Increment(&var, increment, parameter_mode);
    }
    body(var.value());
    if (advance_mode == IndexAdvanceMode::kPost) {
      Increment(&var, increment, parameter_mode);
    }
    Branch(WordNotEqual(var.value(), end_index), &loop, &after_loop);
  }
  BIND(&after_loop);
  return var.value();
}

void CodeStubAssembler::BuildFastFixedArrayForEach(
    const CodeStubAssembler::VariableList& vars, Node* fixed_array,
    ElementsKind kind, Node* first_element_inclusive,
    Node* last_element_exclusive, const FastFixedArrayForEachBody& body,
    ParameterMode mode, ForEachDirection direction) {
  STATIC_ASSERT(FixedArray::kHeaderSize == FixedDoubleArray::kHeaderSize);
  CSA_SLOW_ASSERT(this, MatchesParameterMode(first_element_inclusive, mode));
  CSA_SLOW_ASSERT(this, MatchesParameterMode(last_element_exclusive, mode));
  CSA_SLOW_ASSERT(this, Word32Or(IsFixedArrayWithKind(fixed_array, kind),
                                 IsPropertyArray(fixed_array)));
  int32_t first_val;
  bool constant_first = ToInt32Constant(first_element_inclusive, first_val);
  int32_t last_val;
  bool constent_last = ToInt32Constant(last_element_exclusive, last_val);
  if (constant_first && constent_last) {
    int delta = last_val - first_val;
    DCHECK_GE(delta, 0);
    if (delta <= kElementLoopUnrollThreshold) {
      if (direction == ForEachDirection::kForward) {
        for (int i = first_val; i < last_val; ++i) {
          Node* index = IntPtrConstant(i);
          Node* offset =
              ElementOffsetFromIndex(index, kind, INTPTR_PARAMETERS,
                                     FixedArray::kHeaderSize - kHeapObjectTag);
          body(fixed_array, offset);
        }
      } else {
        for (int i = last_val - 1; i >= first_val; --i) {
          Node* index = IntPtrConstant(i);
          Node* offset =
              ElementOffsetFromIndex(index, kind, INTPTR_PARAMETERS,
                                     FixedArray::kHeaderSize - kHeapObjectTag);
          body(fixed_array, offset);
        }
      }
      return;
    }
  }

  Node* start =
      ElementOffsetFromIndex(first_element_inclusive, kind, mode,
                             FixedArray::kHeaderSize - kHeapObjectTag);
  Node* limit =
      ElementOffsetFromIndex(last_element_exclusive, kind, mode,
                             FixedArray::kHeaderSize - kHeapObjectTag);
  if (direction == ForEachDirection::kReverse) std::swap(start, limit);

  int increment = IsDoubleElementsKind(kind) ? kDoubleSize : kTaggedSize;
  BuildFastLoop(
      vars, start, limit,
      [fixed_array, &body](Node* offset) { body(fixed_array, offset); },
      direction == ForEachDirection::kReverse ? -increment : increment,
      INTPTR_PARAMETERS,
      direction == ForEachDirection::kReverse ? IndexAdvanceMode::kPre
                                              : IndexAdvanceMode::kPost);
}

void CodeStubAssembler::GotoIfFixedArraySizeDoesntFitInNewSpace(
    Node* element_count, Label* doesnt_fit, int base_size, ParameterMode mode) {
  GotoIf(FixedArraySizeDoesntFitInNewSpace(element_count, base_size, mode),
         doesnt_fit);
}

void CodeStubAssembler::InitializeFieldsWithRoot(Node* object,
                                                 Node* start_offset,
                                                 Node* end_offset,
                                                 RootIndex root_index) {
  CSA_SLOW_ASSERT(this, TaggedIsNotSmi(object));
  start_offset = IntPtrAdd(start_offset, IntPtrConstant(-kHeapObjectTag));
  end_offset = IntPtrAdd(end_offset, IntPtrConstant(-kHeapObjectTag));
  Node* root_value = LoadRoot(root_index);
  BuildFastLoop(
      end_offset, start_offset,
      [this, object, root_value](Node* current) {
        StoreNoWriteBarrier(MachineRepresentation::kTagged, object, current,
                            root_value);
      },
      -kTaggedSize, INTPTR_PARAMETERS,
      CodeStubAssembler::IndexAdvanceMode::kPre);
}

void CodeStubAssembler::BranchIfNumberRelationalComparison(
    Operation op, Node* left, Node* right, Label* if_true, Label* if_false) {
  CSA_SLOW_ASSERT(this, IsNumber(left));
  CSA_SLOW_ASSERT(this, IsNumber(right));

  Label do_float_comparison(this);
  TVARIABLE(Float64T, var_left_float);
  TVARIABLE(Float64T, var_right_float);

  Branch(
      TaggedIsSmi(left),
      [&] {
        TNode<Smi> smi_left = CAST(left);

        Branch(
            TaggedIsSmi(right),
            [&] {
              TNode<Smi> smi_right = CAST(right);

              // Both {left} and {right} are Smi, so just perform a fast
              // Smi comparison.
              switch (op) {
                case Operation::kEqual:
                  BranchIfSmiEqual(smi_left, smi_right, if_true, if_false);
                  break;
                case Operation::kLessThan:
                  BranchIfSmiLessThan(smi_left, smi_right, if_true, if_false);
                  break;
                case Operation::kLessThanOrEqual:
                  BranchIfSmiLessThanOrEqual(smi_left, smi_right, if_true,
                                             if_false);
                  break;
                case Operation::kGreaterThan:
                  BranchIfSmiLessThan(smi_right, smi_left, if_true, if_false);
                  break;
                case Operation::kGreaterThanOrEqual:
                  BranchIfSmiLessThanOrEqual(smi_right, smi_left, if_true,
                                             if_false);
                  break;
                default:
                  UNREACHABLE();
              }
            },
            [&] {
              CSA_ASSERT(this, IsHeapNumber(right));
              var_left_float = SmiToFloat64(smi_left);
              var_right_float = LoadHeapNumberValue(right);
              Goto(&do_float_comparison);
            });
      },
      [&] {
        CSA_ASSERT(this, IsHeapNumber(left));
        var_left_float = LoadHeapNumberValue(left);

        Branch(
            TaggedIsSmi(right),
            [&] {
              var_right_float = SmiToFloat64(right);
              Goto(&do_float_comparison);
            },
            [&] {
              CSA_ASSERT(this, IsHeapNumber(right));
              var_right_float = LoadHeapNumberValue(right);
              Goto(&do_float_comparison);
            });
      });

  BIND(&do_float_comparison);
  {
    switch (op) {
      case Operation::kEqual:
        Branch(Float64Equal(var_left_float.value(), var_right_float.value()),
               if_true, if_false);
        break;
      case Operation::kLessThan:
        Branch(Float64LessThan(var_left_float.value(), var_right_float.value()),
               if_true, if_false);
        break;
      case Operation::kLessThanOrEqual:
        Branch(Float64LessThanOrEqual(var_left_float.value(),
                                      var_right_float.value()),
               if_true, if_false);
        break;
      case Operation::kGreaterThan:
        Branch(
            Float64GreaterThan(var_left_float.value(), var_right_float.value()),
            if_true, if_false);
        break;
      case Operation::kGreaterThanOrEqual:
        Branch(Float64GreaterThanOrEqual(var_left_float.value(),
                                         var_right_float.value()),
               if_true, if_false);
        break;
      default:
        UNREACHABLE();
    }
  }
}

void CodeStubAssembler::GotoIfNumberGreaterThanOrEqual(Node* left, Node* right,
                                                       Label* if_true) {
  Label if_false(this);
  BranchIfNumberRelationalComparison(Operation::kGreaterThanOrEqual, left,
                                     right, if_true, &if_false);
  BIND(&if_false);
}

namespace {
Operation Reverse(Operation op) {
  switch (op) {
    case Operation::kLessThan:
      return Operation::kGreaterThan;
    case Operation::kLessThanOrEqual:
      return Operation::kGreaterThanOrEqual;
    case Operation::kGreaterThan:
      return Operation::kLessThan;
    case Operation::kGreaterThanOrEqual:
      return Operation::kLessThanOrEqual;
    default:
      break;
  }
  UNREACHABLE();
}
}  // anonymous namespace

Node* CodeStubAssembler::RelationalComparison(Operation op, Node* left,
                                              Node* right, Node* context,
                                              Variable* var_type_feedback) {
  Label return_true(this), return_false(this), do_float_comparison(this),
      end(this);
  TVARIABLE(Oddball, var_result);  // Actually only "true" or "false".
  TVARIABLE(Float64T, var_left_float);
  TVARIABLE(Float64T, var_right_float);

  // We might need to loop several times due to ToPrimitive and/or ToNumeric
  // conversions.
  VARIABLE(var_left, MachineRepresentation::kTagged, left);
  VARIABLE(var_right, MachineRepresentation::kTagged, right);
  VariableList loop_variable_list({&var_left, &var_right}, zone());
  if (var_type_feedback != nullptr) {
    // Initialize the type feedback to None. The current feedback is combined
    // with the previous feedback.
    var_type_feedback->Bind(SmiConstant(CompareOperationFeedback::kNone));
    loop_variable_list.push_back(var_type_feedback);
  }
  Label loop(this, loop_variable_list);
  Goto(&loop);
  BIND(&loop);
  {
    left = var_left.value();
    right = var_right.value();

    Label if_left_smi(this), if_left_not_smi(this);
    Branch(TaggedIsSmi(left), &if_left_smi, &if_left_not_smi);

    BIND(&if_left_smi);
    {
      TNode<Smi> smi_left = CAST(left);
      Label if_right_smi(this), if_right_heapnumber(this),
          if_right_bigint(this, Label::kDeferred),
          if_right_not_numeric(this, Label::kDeferred);
      GotoIf(TaggedIsSmi(right), &if_right_smi);
      Node* right_map = LoadMap(right);
      GotoIf(IsHeapNumberMap(right_map), &if_right_heapnumber);
      Node* right_instance_type = LoadMapInstanceType(right_map);
      Branch(IsBigIntInstanceType(right_instance_type), &if_right_bigint,
             &if_right_not_numeric);

      BIND(&if_right_smi);
      {
        TNode<Smi> smi_right = CAST(right);
        CombineFeedback(var_type_feedback,
                        CompareOperationFeedback::kSignedSmall);
        switch (op) {
          case Operation::kLessThan:
            BranchIfSmiLessThan(smi_left, smi_right, &return_true,
                                &return_false);
            break;
          case Operation::kLessThanOrEqual:
            BranchIfSmiLessThanOrEqual(smi_left, smi_right, &return_true,
                                       &return_false);
            break;
          case Operation::kGreaterThan:
            BranchIfSmiLessThan(smi_right, smi_left, &return_true,
                                &return_false);
            break;
          case Operation::kGreaterThanOrEqual:
            BranchIfSmiLessThanOrEqual(smi_right, smi_left, &return_true,
                                       &return_false);
            break;
          default:
            UNREACHABLE();
        }
      }

      BIND(&if_right_heapnumber);
      {
        CombineFeedback(var_type_feedback, CompareOperationFeedback::kNumber);
        var_left_float = SmiToFloat64(smi_left);
        var_right_float = LoadHeapNumberValue(right);
        Goto(&do_float_comparison);
      }

      BIND(&if_right_bigint);
      {
        OverwriteFeedback(var_type_feedback, CompareOperationFeedback::kAny);
        var_result = CAST(CallRuntime(Runtime::kBigIntCompareToNumber,
                                      NoContextConstant(),
                                      SmiConstant(Reverse(op)), right, left));
        Goto(&end);
      }

      BIND(&if_right_not_numeric);
      {
        OverwriteFeedback(var_type_feedback, CompareOperationFeedback::kAny);
        // Convert {right} to a Numeric; we don't need to perform the
        // dedicated ToPrimitive(right, hint Number) operation, as the
        // ToNumeric(right) will by itself already invoke ToPrimitive with
        // a Number hint.
        var_right.Bind(
            CallBuiltin(Builtins::kNonNumberToNumeric, context, right));
        Goto(&loop);
      }
    }

    BIND(&if_left_not_smi);
    {
      Node* left_map = LoadMap(left);

      Label if_right_smi(this), if_right_not_smi(this);
      Branch(TaggedIsSmi(right), &if_right_smi, &if_right_not_smi);

      BIND(&if_right_smi);
      {
        Label if_left_heapnumber(this), if_left_bigint(this, Label::kDeferred),
            if_left_not_numeric(this, Label::kDeferred);
        GotoIf(IsHeapNumberMap(left_map), &if_left_heapnumber);
        Node* left_instance_type = LoadMapInstanceType(left_map);
        Branch(IsBigIntInstanceType(left_instance_type), &if_left_bigint,
               &if_left_not_numeric);

        BIND(&if_left_heapnumber);
        {
          CombineFeedback(var_type_feedback, CompareOperationFeedback::kNumber);
          var_left_float = LoadHeapNumberValue(left);
          var_right_float = SmiToFloat64(right);
          Goto(&do_float_comparison);
        }

        BIND(&if_left_bigint);
        {
          OverwriteFeedback(var_type_feedback, CompareOperationFeedback::kAny);
          var_result = CAST(CallRuntime(Runtime::kBigIntCompareToNumber,
                                        NoContextConstant(), SmiConstant(op),
                                        left, right));
          Goto(&end);
        }

        BIND(&if_left_not_numeric);
        {
          OverwriteFeedback(var_type_feedback, CompareOperationFeedback::kAny);
          // Convert {left} to a Numeric; we don't need to perform the
          // dedicated ToPrimitive(left, hint Number) operation, as the
          // ToNumeric(left) will by itself already invoke ToPrimitive with
          // a Number hint.
          var_left.Bind(
              CallBuiltin(Builtins::kNonNumberToNumeric, context, left));
          Goto(&loop);
        }
      }

      BIND(&if_right_not_smi);
      {
        Node* right_map = LoadMap(right);

        Label if_left_heapnumber(this), if_left_bigint(this, Label::kDeferred),
            if_left_string(this, Label::kDeferred),
            if_left_other(this, Label::kDeferred);
        GotoIf(IsHeapNumberMap(left_map), &if_left_heapnumber);
        Node* left_instance_type = LoadMapInstanceType(left_map);
        GotoIf(IsBigIntInstanceType(left_instance_type), &if_left_bigint);
        Branch(IsStringInstanceType(left_instance_type), &if_left_string,
               &if_left_other);

        BIND(&if_left_heapnumber);
        {
          Label if_right_heapnumber(this),
              if_right_bigint(this, Label::kDeferred),
              if_right_not_numeric(this, Label::kDeferred);
          GotoIf(WordEqual(right_map, left_map), &if_right_heapnumber);
          Node* right_instance_type = LoadMapInstanceType(right_map);
          Branch(IsBigIntInstanceType(right_instance_type), &if_right_bigint,
                 &if_right_not_numeric);

          BIND(&if_right_heapnumber);
          {
            CombineFeedback(var_type_feedback,
                            CompareOperationFeedback::kNumber);
            var_left_float = LoadHeapNumberValue(left);
            var_right_float = LoadHeapNumberValue(right);
            Goto(&do_float_comparison);
          }

          BIND(&if_right_bigint);
          {
            OverwriteFeedback(var_type_feedback,
                              CompareOperationFeedback::kAny);
            var_result = CAST(CallRuntime(
                Runtime::kBigIntCompareToNumber, NoContextConstant(),
                SmiConstant(Reverse(op)), right, left));
            Goto(&end);
          }

          BIND(&if_right_not_numeric);
          {
            OverwriteFeedback(var_type_feedback,
                              CompareOperationFeedback::kAny);
            // Convert {right} to a Numeric; we don't need to perform
            // dedicated ToPrimitive(right, hint Number) operation, as the
            // ToNumeric(right) will by itself already invoke ToPrimitive with
            // a Number hint.
            var_right.Bind(
                CallBuiltin(Builtins::kNonNumberToNumeric, context, right));
            Goto(&loop);
          }
        }

        BIND(&if_left_bigint);
        {
          Label if_right_heapnumber(this), if_right_bigint(this),
              if_right_string(this), if_right_other(this);
          GotoIf(IsHeapNumberMap(right_map), &if_right_heapnumber);
          Node* right_instance_type = LoadMapInstanceType(right_map);
          GotoIf(IsBigIntInstanceType(right_instance_type), &if_right_bigint);
          Branch(IsStringInstanceType(right_instance_type), &if_right_string,
                 &if_right_other);

          BIND(&if_right_heapnumber);
          {
            OverwriteFeedback(var_type_feedback,
                              CompareOperationFeedback::kAny);
            var_result = CAST(CallRuntime(Runtime::kBigIntCompareToNumber,
                                          NoContextConstant(), SmiConstant(op),
                                          left, right));
            Goto(&end);
          }

          BIND(&if_right_bigint);
          {
            CombineFeedback(var_type_feedback,
                            CompareOperationFeedback::kBigInt);
            var_result = CAST(CallRuntime(Runtime::kBigIntCompareToBigInt,
                                          NoContextConstant(), SmiConstant(op),
                                          left, right));
            Goto(&end);
          }

          BIND(&if_right_string);
          {
            OverwriteFeedback(var_type_feedback,
                              CompareOperationFeedback::kAny);
            var_result = CAST(CallRuntime(Runtime::kBigIntCompareToString,
                                          NoContextConstant(), SmiConstant(op),
                                          left, right));
            Goto(&end);
          }

          // {right} is not a Number, BigInt, or String.
          BIND(&if_right_other);
          {
            OverwriteFeedback(var_type_feedback,
                              CompareOperationFeedback::kAny);
            // Convert {right} to a Numeric; we don't need to perform
            // dedicated ToPrimitive(right, hint Number) operation, as the
            // ToNumeric(right) will by itself already invoke ToPrimitive with
            // a Number hint.
            var_right.Bind(
                CallBuiltin(Builtins::kNonNumberToNumeric, context, right));
            Goto(&loop);
          }
        }

        BIND(&if_left_string);
        {
          Node* right_instance_type = LoadMapInstanceType(right_map);

          Label if_right_not_string(this, Label::kDeferred);
          GotoIfNot(IsStringInstanceType(right_instance_type),
                    &if_right_not_string);

          // Both {left} and {right} are strings.
          CombineFeedback(var_type_feedback, CompareOperationFeedback::kString);
          Builtins::Name builtin;
          switch (op) {
            case Operation::kLessThan:
              builtin = Builtins::kStringLessThan;
              break;
            case Operation::kLessThanOrEqual:
              builtin = Builtins::kStringLessThanOrEqual;
              break;
            case Operation::kGreaterThan:
              builtin = Builtins::kStringGreaterThan;
              break;
            case Operation::kGreaterThanOrEqual:
              builtin = Builtins::kStringGreaterThanOrEqual;
              break;
            default:
              UNREACHABLE();
          }
          var_result = CAST(CallBuiltin(builtin, context, left, right));
          Goto(&end);

          BIND(&if_right_not_string);
          {
            OverwriteFeedback(var_type_feedback,
                              CompareOperationFeedback::kAny);
            // {left} is a String, while {right} isn't. Check if {right} is
            // a BigInt, otherwise call ToPrimitive(right, hint Number) if
            // {right} is a receiver, or ToNumeric(left) and then
            // ToNumeric(right) in the other cases.
            STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
            Label if_right_bigint(this),
                if_right_receiver(this, Label::kDeferred);
            GotoIf(IsBigIntInstanceType(right_instance_type), &if_right_bigint);
            GotoIf(IsJSReceiverInstanceType(right_instance_type),
                   &if_right_receiver);

            var_left.Bind(
                CallBuiltin(Builtins::kNonNumberToNumeric, context, left));
            var_right.Bind(CallBuiltin(Builtins::kToNumeric, context, right));
            Goto(&loop);

            BIND(&if_right_bigint);
            {
              var_result = CAST(CallRuntime(
                  Runtime::kBigIntCompareToString, NoContextConstant(),
                  SmiConstant(Reverse(op)), right, left));
              Goto(&end);
            }

            BIND(&if_right_receiver);
            {
              Callable callable = CodeFactory::NonPrimitiveToPrimitive(
                  isolate(), ToPrimitiveHint::kNumber);
              var_right.Bind(CallStub(callable, context, right));
              Goto(&loop);
            }
          }
        }

        BIND(&if_left_other);
        {
          // {left} is neither a Numeric nor a String, and {right} is not a Smi.
          if (var_type_feedback != nullptr) {
            // Collect NumberOrOddball feedback if {left} is an Oddball
            // and {right} is either a HeapNumber or Oddball. Otherwise collect
            // Any feedback.
            Label collect_any_feedback(this), collect_oddball_feedback(this),
                collect_feedback_done(this);
            GotoIfNot(InstanceTypeEqual(left_instance_type, ODDBALL_TYPE),
                      &collect_any_feedback);

            GotoIf(IsHeapNumberMap(right_map), &collect_oddball_feedback);
            Node* right_instance_type = LoadMapInstanceType(right_map);
            Branch(InstanceTypeEqual(right_instance_type, ODDBALL_TYPE),
                   &collect_oddball_feedback, &collect_any_feedback);

            BIND(&collect_oddball_feedback);
            {
              CombineFeedback(var_type_feedback,
                              CompareOperationFeedback::kNumberOrOddball);
              Goto(&collect_feedback_done);
            }

            BIND(&collect_any_feedback);
            {
              OverwriteFeedback(var_type_feedback,
                                CompareOperationFeedback::kAny);
              Goto(&collect_feedback_done);
            }

            BIND(&collect_feedback_done);
          }

          // If {left} is a receiver, call ToPrimitive(left, hint Number).
          // Otherwise call ToNumeric(right) and then ToNumeric(left), the
          // order here is important as it's observable by user code.
          STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
          Label if_left_receiver(this, Label::kDeferred);
          GotoIf(IsJSReceiverInstanceType(left_instance_type),
                 &if_left_receiver);

          var_right.Bind(CallBuiltin(Builtins::kToNumeric, context, right));
          var_left.Bind(
              CallBuiltin(Builtins::kNonNumberToNumeric, context, left));
          Goto(&loop);

          BIND(&if_left_receiver);
          {
            Callable callable = CodeFactory::NonPrimitiveToPrimitive(
                isolate(), ToPrimitiveHint::kNumber);
            var_left.Bind(CallStub(callable, context, left));
            Goto(&loop);
          }
        }
      }
    }
  }

  BIND(&do_float_comparison);
  {
    switch (op) {
      case Operation::kLessThan:
        Branch(Float64LessThan(var_left_float.value(), var_right_float.value()),
               &return_true, &return_false);
        break;
      case Operation::kLessThanOrEqual:
        Branch(Float64LessThanOrEqual(var_left_float.value(),
                                      var_right_float.value()),
               &return_true, &return_false);
        break;
      case Operation::kGreaterThan:
        Branch(
            Float64GreaterThan(var_left_float.value(), var_right_float.value()),
            &return_true, &return_false);
        break;
      case Operation::kGreaterThanOrEqual:
        Branch(Float64GreaterThanOrEqual(var_left_float.value(),
                                         var_right_float.value()),
               &return_true, &return_false);
        break;
      default:
        UNREACHABLE();
    }
  }

  BIND(&return_true);
  {
    var_result = TrueConstant();
    Goto(&end);
  }

  BIND(&return_false);
  {
    var_result = FalseConstant();
    Goto(&end);
  }

  BIND(&end);
  return var_result.value();
}

TNode<Smi> CodeStubAssembler::CollectFeedbackForString(
    SloppyTNode<Int32T> instance_type) {
  TNode<Smi> feedback = SelectSmiConstant(
      Word32Equal(
          Word32And(instance_type, Int32Constant(kIsNotInternalizedMask)),
          Int32Constant(kInternalizedTag)),
      CompareOperationFeedback::kInternalizedString,
      CompareOperationFeedback::kString);
  return feedback;
}

void CodeStubAssembler::GenerateEqual_Same(Node* value, Label* if_equal,
                                           Label* if_notequal,
                                           Variable* var_type_feedback) {
  // In case of abstract or strict equality checks, we need additional checks
  // for NaN values because they are not considered equal, even if both the
  // left and the right hand side reference exactly the same value.

  Label if_smi(this), if_heapnumber(this);
  GotoIf(TaggedIsSmi(value), &if_smi);

  Node* value_map = LoadMap(value);
  GotoIf(IsHeapNumberMap(value_map), &if_heapnumber);

  // For non-HeapNumbers, all we do is collect type feedback.
  if (var_type_feedback != nullptr) {
    Node* instance_type = LoadMapInstanceType(value_map);

    Label if_string(this), if_receiver(this), if_oddball(this), if_symbol(this),
        if_bigint(this);
    GotoIf(IsStringInstanceType(instance_type), &if_string);
    GotoIf(IsJSReceiverInstanceType(instance_type), &if_receiver);
    GotoIf(IsOddballInstanceType(instance_type), &if_oddball);
    Branch(IsBigIntInstanceType(instance_type), &if_bigint, &if_symbol);

    BIND(&if_string);
    {
      CSA_ASSERT(this, IsString(value));
      CombineFeedback(var_type_feedback,
                      CollectFeedbackForString(instance_type));
      Goto(if_equal);
    }

    BIND(&if_symbol);
    {
      CSA_ASSERT(this, IsSymbol(value));
      CombineFeedback(var_type_feedback, CompareOperationFeedback::kSymbol);
      Goto(if_equal);
    }

    BIND(&if_receiver);
    {
      CSA_ASSERT(this, IsJSReceiver(value));
      CombineFeedback(var_type_feedback, CompareOperationFeedback::kReceiver);
      Goto(if_equal);
    }

    BIND(&if_bigint);
    {
      CSA_ASSERT(this, IsBigInt(value));
      CombineFeedback(var_type_feedback, CompareOperationFeedback::kBigInt);
      Goto(if_equal);
    }

    BIND(&if_oddball);
    {
      CSA_ASSERT(this, IsOddball(value));
      Label if_boolean(this), if_not_boolean(this);
      Branch(IsBooleanMap(value_map), &if_boolean, &if_not_boolean);

      BIND(&if_boolean);
      {
        CombineFeedback(var_type_feedback, CompareOperationFeedback::kAny);
        Goto(if_equal);
      }

      BIND(&if_not_boolean);
      {
        CSA_ASSERT(this, IsNullOrUndefined(value));
        CombineFeedback(var_type_feedback,
                        CompareOperationFeedback::kReceiverOrNullOrUndefined);
        Goto(if_equal);
      }
    }
  } else {
    Goto(if_equal);
  }

  BIND(&if_heapnumber);
  {
    CombineFeedback(var_type_feedback, CompareOperationFeedback::kNumber);
    Node* number_value = LoadHeapNumberValue(value);
    BranchIfFloat64IsNaN(number_value, if_notequal, if_equal);
  }

  BIND(&if_smi);
  {
    CombineFeedback(var_type_feedback, CompareOperationFeedback::kSignedSmall);
    Goto(if_equal);
  }
}

// ES6 section 7.2.12 Abstract Equality Comparison
Node* CodeStubAssembler::Equal(Node* left, Node* right, Node* context,
                               Variable* var_type_feedback) {
  // This is a slightly optimized version of Object::Equals. Whenever you
  // change something functionality wise in here, remember to update the
  // Object::Equals method as well.

  Label if_equal(this), if_notequal(this), do_float_comparison(this),
      do_right_stringtonumber(this, Label::kDeferred), end(this);
  VARIABLE(result, MachineRepresentation::kTagged);
  TVARIABLE(Float64T, var_left_float);
  TVARIABLE(Float64T, var_right_float);

  // We can avoid code duplication by exploiting the fact that abstract equality
  // is symmetric.
  Label use_symmetry(this);

  // We might need to loop several times due to ToPrimitive and/or ToNumber
  // conversions.
  VARIABLE(var_left, MachineRepresentation::kTagged, left);
  VARIABLE(var_right, MachineRepresentation::kTagged, right);
  VariableList loop_variable_list({&var_left, &var_right}, zone());
  if (var_type_feedback != nullptr) {
    // Initialize the type feedback to None. The current feedback will be
    // combined with the previous feedback.
    OverwriteFeedback(var_type_feedback, CompareOperationFeedback::kNone);
    loop_variable_list.push_back(var_type_feedback);
  }
  Label loop(this, loop_variable_list);
  Goto(&loop);
  BIND(&loop);
  {
    left = var_left.value();
    right = var_right.value();

    Label if_notsame(this);
    GotoIf(WordNotEqual(left, right), &if_notsame);
    {
      // {left} and {right} reference the exact same value, yet we need special
      // treatment for HeapNumber, as NaN is not equal to NaN.
      GenerateEqual_Same(left, &if_equal, &if_notequal, var_type_feedback);
    }

    BIND(&if_notsame);
    Label if_left_smi(this), if_left_not_smi(this);
    Branch(TaggedIsSmi(left), &if_left_smi, &if_left_not_smi);

    BIND(&if_left_smi);
    {
      Label if_right_smi(this), if_right_not_smi(this);
      Branch(TaggedIsSmi(right), &if_right_smi, &if_right_not_smi);

      BIND(&if_right_smi);
      {
        // We have already checked for {left} and {right} being the same value,
        // so when we get here they must be different Smis.
        CombineFeedback(var_type_feedback,
                        CompareOperationFeedback::kSignedSmall);
        Goto(&if_notequal);
      }

      BIND(&if_right_not_smi);
      Node* right_map = LoadMap(right);
      Label if_right_heapnumber(this), if_right_boolean(this),
          if_right_bigint(this, Label::kDeferred),
          if_right_receiver(this, Label::kDeferred);
      GotoIf(IsHeapNumberMap(right_map), &if_right_heapnumber);
      // {left} is Smi and {right} is not HeapNumber or Smi.
      if (var_type_feedback != nullptr) {
        var_type_feedback->Bind(SmiConstant(CompareOperationFeedback::kAny));
      }
      GotoIf(IsBooleanMap(right_map), &if_right_boolean);
      Node* right_type = LoadMapInstanceType(right_map);
      GotoIf(IsStringInstanceType(right_type), &do_right_stringtonumber);
      GotoIf(IsBigIntInstanceType(right_type), &if_right_bigint);
      Branch(IsJSReceiverInstanceType(right_type), &if_right_receiver,
             &if_notequal);

      BIND(&if_right_heapnumber);
      {
        var_left_float = SmiToFloat64(left);
        var_right_float = LoadHeapNumberValue(right);
        CombineFeedback(var_type_feedback, CompareOperationFeedback::kNumber);
        Goto(&do_float_comparison);
      }

      BIND(&if_right_boolean);
      {
        var_right.Bind(LoadObjectField(right, Oddball::kToNumberOffset));
        Goto(&loop);
      }

      BIND(&if_right_bigint);
      {
        result.Bind(CallRuntime(Runtime::kBigIntEqualToNumber,
                                NoContextConstant(), right, left));
        Goto(&end);
      }

      BIND(&if_right_receiver);
      {
        Callable callable = CodeFactory::NonPrimitiveToPrimitive(isolate());
        var_right.Bind(CallStub(callable, context, right));
        Goto(&loop);
      }
    }

    BIND(&if_left_not_smi);
    {
      GotoIf(TaggedIsSmi(right), &use_symmetry);

      Label if_left_symbol(this), if_left_number(this),
          if_left_string(this, Label::kDeferred),
          if_left_bigint(this, Label::kDeferred), if_left_oddball(this),
          if_left_receiver(this);

      Node* left_map = LoadMap(left);
      Node* right_map = LoadMap(right);
      Node* left_type = LoadMapInstanceType(left_map);
      Node* right_type = LoadMapInstanceType(right_map);

      GotoIf(IsStringInstanceType(left_type), &if_left_string);
      GotoIf(IsSymbolInstanceType(left_type), &if_left_symbol);
      GotoIf(IsHeapNumberInstanceType(left_type), &if_left_number);
      GotoIf(IsOddballInstanceType(left_type), &if_left_oddball);
      Branch(IsBigIntInstanceType(left_type), &if_left_bigint,
             &if_left_receiver);

      BIND(&if_left_string);
      {
        GotoIfNot(IsStringInstanceType(right_type), &use_symmetry);
        result.Bind(CallBuiltin(Builtins::kStringEqual, context, left, right));
        CombineFeedback(var_type_feedback,
                        SmiOr(CollectFeedbackForString(left_type),
                              CollectFeedbackForString(right_type)));
        Goto(&end);
      }

      BIND(&if_left_number);
      {
        Label if_right_not_number(this);
        GotoIf(Word32NotEqual(left_type, right_type), &if_right_not_number);

        var_left_float = LoadHeapNumberValue(left);
        var_right_float = LoadHeapNumberValue(right);
        CombineFeedback(var_type_feedback, CompareOperationFeedback::kNumber);
        Goto(&do_float_comparison);

        BIND(&if_right_not_number);
        {
          Label if_right_boolean(this);
          if (var_type_feedback != nullptr) {
            var_type_feedback->Bind(
                SmiConstant(CompareOperationFeedback::kAny));
          }
          GotoIf(IsStringInstanceType(right_type), &do_right_stringtonumber);
          GotoIf(IsBooleanMap(right_map), &if_right_boolean);
          GotoIf(IsBigIntInstanceType(right_type), &use_symmetry);
          Branch(IsJSReceiverInstanceType(right_type), &use_symmetry,
                 &if_notequal);

          BIND(&if_right_boolean);
          {
            var_right.Bind(LoadObjectField(right, Oddball::kToNumberOffset));
            Goto(&loop);
          }
        }
      }

      BIND(&if_left_bigint);
      {
        Label if_right_heapnumber(this), if_right_bigint(this),
            if_right_string(this), if_right_boolean(this);
        GotoIf(IsHeapNumberMap(right_map), &if_right_heapnumber);
        GotoIf(IsBigIntInstanceType(right_type), &if_right_bigint);
        GotoIf(IsStringInstanceType(right_type), &if_right_string);
        GotoIf(IsBooleanMap(right_map), &if_right_boolean);
        Branch(IsJSReceiverInstanceType(right_type), &use_symmetry,
               &if_notequal);

        BIND(&if_right_heapnumber);
        {
          if (var_type_feedback != nullptr) {
            var_type_feedback->Bind(
                SmiConstant(CompareOperationFeedback::kAny));
          }
          result.Bind(CallRuntime(Runtime::kBigIntEqualToNumber,
                                  NoContextConstant(), left, right));
          Goto(&end);
        }

        BIND(&if_right_bigint);
        {
          CombineFeedback(var_type_feedback, CompareOperationFeedback::kBigInt);
          result.Bind(CallRuntime(Runtime::kBigIntEqualToBigInt,
                                  NoContextConstant(), left, right));
          Goto(&end);
        }

        BIND(&if_right_string);
        {
          if (var_type_feedback != nullptr) {
            var_type_feedback->Bind(
                SmiConstant(CompareOperationFeedback::kAny));
          }
          result.Bind(CallRuntime(Runtime::kBigIntEqualToString,
                                  NoContextConstant(), left, right));
          Goto(&end);
        }

        BIND(&if_right_boolean);
        {
          if (var_type_feedback != nullptr) {
            var_type_feedback->Bind(
                SmiConstant(CompareOperationFeedback::kAny));
          }
          var_right.Bind(LoadObjectField(right, Oddball::kToNumberOffset));
          Goto(&loop);
        }
      }

      BIND(&if_left_oddball);
      {
        Label if_left_boolean(this), if_left_not_boolean(this);
        Branch(IsBooleanMap(left_map), &if_left_boolean, &if_left_not_boolean);

        BIND(&if_left_not_boolean);
        {
          // {left} is either Null or Undefined. Check if {right} is
          // undetectable (which includes Null and Undefined).
          Label if_right_undetectable(this), if_right_not_undetectable(this);
          Branch(IsUndetectableMap(right_map), &if_right_undetectable,
                 &if_right_not_undetectable);

          BIND(&if_right_undetectable);
          {
            if (var_type_feedback != nullptr) {
              // If {right} is undetectable, it must be either also
              // Null or Undefined, or a Receiver (aka document.all).
              var_type_feedback->Bind(SmiConstant(
                  CompareOperationFeedback::kReceiverOrNullOrUndefined));
            }
            Goto(&if_equal);
          }

          BIND(&if_right_not_undetectable);
          {
            if (var_type_feedback != nullptr) {
              // Track whether {right} is Null, Undefined or Receiver.
              var_type_feedback->Bind(SmiConstant(
                  CompareOperationFeedback::kReceiverOrNullOrUndefined));
              GotoIf(IsJSReceiverInstanceType(right_type), &if_notequal);
              GotoIfNot(IsBooleanMap(right_map), &if_notequal);
              var_type_feedback->Bind(
                  SmiConstant(CompareOperationFeedback::kAny));
            }
            Goto(&if_notequal);
          }
        }

        BIND(&if_left_boolean);
        {
          if (var_type_feedback != nullptr) {
            var_type_feedback->Bind(
                SmiConstant(CompareOperationFeedback::kAny));
          }

          // If {right} is a Boolean too, it must be a different Boolean.
          GotoIf(WordEqual(right_map, left_map), &if_notequal);

          // Otherwise, convert {left} to number and try again.
          var_left.Bind(LoadObjectField(left, Oddball::kToNumberOffset));
          Goto(&loop);
        }
      }

      BIND(&if_left_symbol);
      {
        Label if_right_receiver(this);
        GotoIf(IsJSReceiverInstanceType(right_type), &if_right_receiver);
        // {right} is not a JSReceiver and also not the same Symbol as {left},
        // so the result is "not equal".
        if (var_type_feedback != nullptr) {
          Label if_right_symbol(this);
          GotoIf(IsSymbolInstanceType(right_type), &if_right_symbol);
          var_type_feedback->Bind(SmiConstant(CompareOperationFeedback::kAny));
          Goto(&if_notequal);

          BIND(&if_right_symbol);
          {
            CombineFeedback(var_type_feedback,
                            CompareOperationFeedback::kSymbol);
            Goto(&if_notequal);
          }
        } else {
          Goto(&if_notequal);
        }

        BIND(&if_right_receiver);
        {
          // {left} is a Primitive and {right} is a JSReceiver, so swapping
          // the order is not observable.
          if (var_type_feedback != nullptr) {
            var_type_feedback->Bind(
                SmiConstant(CompareOperationFeedback::kAny));
          }
          Goto(&use_symmetry);
        }
      }

      BIND(&if_left_receiver);
      {
        CSA_ASSERT(this, IsJSReceiverInstanceType(left_type));
        Label if_right_receiver(this), if_right_not_receiver(this);
        Branch(IsJSReceiverInstanceType(right_type), &if_right_receiver,
               &if_right_not_receiver);

        BIND(&if_right_receiver);
        {
          // {left} and {right} are different JSReceiver references.
          CombineFeedback(var_type_feedback,
                          CompareOperationFeedback::kReceiver);
          Goto(&if_notequal);
        }

        BIND(&if_right_not_receiver);
        {
          // Check if {right} is undetectable, which means it must be Null
          // or Undefined, since we already ruled out Receiver for {right}.
          Label if_right_undetectable(this),
              if_right_not_undetectable(this, Label::kDeferred);
          Branch(IsUndetectableMap(right_map), &if_right_undetectable,
                 &if_right_not_undetectable);

          BIND(&if_right_undetectable);
          {
            // When we get here, {right} must be either Null or Undefined.
            CSA_ASSERT(this, IsNullOrUndefined(right));
            if (var_type_feedback != nullptr) {
              var_type_feedback->Bind(SmiConstant(
                  CompareOperationFeedback::kReceiverOrNullOrUndefined));
            }
            Branch(IsUndetectableMap(left_map), &if_equal, &if_notequal);
          }

          BIND(&if_right_not_undetectable);
          {
            // {right} is a Primitive, and neither Null or Undefined;
            // convert {left} to Primitive too.
            if (var_type_feedback != nullptr) {
              var_type_feedback->Bind(
                  SmiConstant(CompareOperationFeedback::kAny));
            }
            Callable callable = CodeFactory::NonPrimitiveToPrimitive(isolate());
            var_left.Bind(CallStub(callable, context, left));
            Goto(&loop);
          }
        }
      }
    }

    BIND(&do_right_stringtonumber);
    {
      var_right.Bind(CallBuiltin(Builtins::kStringToNumber, context, right));
      Goto(&loop);
    }

    BIND(&use_symmetry);
    {
      var_left.Bind(right);
      var_right.Bind(left);
      Goto(&loop);
    }
  }

  BIND(&do_float_comparison);
  {
    Branch(Float64Equal(var_left_float.value(), var_right_float.value()),
           &if_equal, &if_notequal);
  }

  BIND(&if_equal);
  {
    result.Bind(TrueConstant());
    Goto(&end);
  }

  BIND(&if_notequal);
  {
    result.Bind(FalseConstant());
    Goto(&end);
  }

  BIND(&end);
  return result.value();
}

TNode<Oddball> CodeStubAssembler::StrictEqual(SloppyTNode<Object> lhs,
                                              SloppyTNode<Object> rhs,
                                              Variable* var_type_feedback) {
  // Pseudo-code for the algorithm below:
  //
  // if (lhs == rhs) {
  //   if (lhs->IsHeapNumber()) return HeapNumber::cast(lhs)->value() != NaN;
  //   return true;
  // }
  // if (!lhs->IsSmi()) {
  //   if (lhs->IsHeapNumber()) {
  //     if (rhs->IsSmi()) {
  //       return Smi::ToInt(rhs) == HeapNumber::cast(lhs)->value();
  //     } else if (rhs->IsHeapNumber()) {
  //       return HeapNumber::cast(rhs)->value() ==
  //       HeapNumber::cast(lhs)->value();
  //     } else {
  //       return false;
  //     }
  //   } else {
  //     if (rhs->IsSmi()) {
  //       return false;
  //     } else {
  //       if (lhs->IsString()) {
  //         if (rhs->IsString()) {
  //           return %StringEqual(lhs, rhs);
  //         } else {
  //           return false;
  //         }
  //       } else if (lhs->IsBigInt()) {
  //         if (rhs->IsBigInt()) {
  //           return %BigIntEqualToBigInt(lhs, rhs);
  //         } else {
  //           return false;
  //         }
  //       } else {
  //         return false;
  //       }
  //     }
  //   }
  // } else {
  //   if (rhs->IsSmi()) {
  //     return false;
  //   } else {
  //     if (rhs->IsHeapNumber()) {
  //       return Smi::ToInt(lhs) == HeapNumber::cast(rhs)->value();
  //     } else {
  //       return false;
  //     }
  //   }
  // }

  Label if_equal(this), if_notequal(this), if_not_equivalent_types(this),
      end(this);
  TVARIABLE(Oddball, result);

  OverwriteFeedback(var_type_feedback, CompareOperationFeedback::kNone);

  // Check if {lhs} and {rhs} refer to the same object.
  Label if_same(this), if_notsame(this);
  Branch(WordEqual(lhs, rhs), &if_same, &if_notsame);

  BIND(&if_same);
  {
    // The {lhs} and {rhs} reference the exact same value, yet we need special
    // treatment for HeapNumber, as NaN is not equal to NaN.
    GenerateEqual_Same(lhs, &if_equal, &if_notequal, var_type_feedback);
  }

  BIND(&if_notsame);
  {
    // The {lhs} and {rhs} reference different objects, yet for Smi, HeapNumber,
    // BigInt and String they can still be considered equal.

    // Check if {lhs} is a Smi or a HeapObject.
    Label if_lhsissmi(this), if_lhsisnotsmi(this);
    Branch(TaggedIsSmi(lhs), &if_lhsissmi, &if_lhsisnotsmi);

    BIND(&if_lhsisnotsmi);
    {
      // Load the map of {lhs}.
      TNode<Map> lhs_map = LoadMap(CAST(lhs));

      // Check if {lhs} is a HeapNumber.
      Label if_lhsisnumber(this), if_lhsisnotnumber(this);
      Branch(IsHeapNumberMap(lhs_map), &if_lhsisnumber, &if_lhsisnotnumber);

      BIND(&if_lhsisnumber);
      {
        // Check if {rhs} is a Smi or a HeapObject.
        Label if_rhsissmi(this), if_rhsisnotsmi(this);
        Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

        BIND(&if_rhsissmi);
        {
          // Convert {lhs} and {rhs} to floating point values.
          Node* lhs_value = LoadHeapNumberValue(CAST(lhs));
          Node* rhs_value = SmiToFloat64(CAST(rhs));

          CombineFeedback(var_type_feedback, CompareOperationFeedback::kNumber);

          // Perform a floating point comparison of {lhs} and {rhs}.
          Branch(Float64Equal(lhs_value, rhs_value), &if_equal, &if_notequal);
        }

        BIND(&if_rhsisnotsmi);
        {
          TNode<HeapObject> rhs_ho = CAST(rhs);
          // Load the map of {rhs}.
          TNode<Map> rhs_map = LoadMap(rhs_ho);

          // Check if {rhs} is also a HeapNumber.
          Label if_rhsisnumber(this), if_rhsisnotnumber(this);
          Branch(IsHeapNumberMap(rhs_map), &if_rhsisnumber, &if_rhsisnotnumber);

          BIND(&if_rhsisnumber);
          {
            // Convert {lhs} and {rhs} to floating point values.
            Node* lhs_value = LoadHeapNumberValue(CAST(lhs));
            Node* rhs_value = LoadHeapNumberValue(CAST(rhs));

            CombineFeedback(var_type_feedback,
                            CompareOperationFeedback::kNumber);

            // Perform a floating point comparison of {lhs} and {rhs}.
            Branch(Float64Equal(lhs_value, rhs_value), &if_equal, &if_notequal);
          }

          BIND(&if_rhsisnotnumber);
          Goto(&if_not_equivalent_types);
        }
      }

      BIND(&if_lhsisnotnumber);
      {
        // Check if {rhs} is a Smi or a HeapObject.
        Label if_rhsissmi(this), if_rhsisnotsmi(this);
        Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

        BIND(&if_rhsissmi);
        Goto(&if_not_equivalent_types);

        BIND(&if_rhsisnotsmi);
        {
          // Load the instance type of {lhs}.
          Node* lhs_instance_type = LoadMapInstanceType(lhs_map);

          // Check if {lhs} is a String.
          Label if_lhsisstring(this, Label::kDeferred), if_lhsisnotstring(this);
          Branch(IsStringInstanceType(lhs_instance_type), &if_lhsisstring,
                 &if_lhsisnotstring);

          BIND(&if_lhsisstring);
          {
            // Load the instance type of {rhs}.
            Node* rhs_instance_type = LoadInstanceType(CAST(rhs));

            // Check if {rhs} is also a String.
            Label if_rhsisstring(this, Label::kDeferred),
                if_rhsisnotstring(this);
            Branch(IsStringInstanceType(rhs_instance_type), &if_rhsisstring,
                   &if_rhsisnotstring);

            BIND(&if_rhsisstring);
            {
              if (var_type_feedback != nullptr) {
                TNode<Smi> lhs_feedback =
                    CollectFeedbackForString(lhs_instance_type);
                TNode<Smi> rhs_feedback =
                    CollectFeedbackForString(rhs_instance_type);
                var_type_feedback->Bind(SmiOr(lhs_feedback, rhs_feedback));
              }
              result = CAST(CallBuiltin(Builtins::kStringEqual,
                                        NoContextConstant(), lhs, rhs));
              Goto(&end);
            }

            BIND(&if_rhsisnotstring);
            Goto(&if_not_equivalent_types);
          }

          BIND(&if_lhsisnotstring);
          {
            // Check if {lhs} is a BigInt.
            Label if_lhsisbigint(this), if_lhsisnotbigint(this);
            Branch(IsBigIntInstanceType(lhs_instance_type), &if_lhsisbigint,
                   &if_lhsisnotbigint);

            BIND(&if_lhsisbigint);
            {
              // Load the instance type of {rhs}.
              TNode<Uint16T> rhs_instance_type = LoadInstanceType(CAST(rhs));

              // Check if {rhs} is also a BigInt.
              Label if_rhsisbigint(this, Label::kDeferred),
                  if_rhsisnotbigint(this);
              Branch(IsBigIntInstanceType(rhs_instance_type), &if_rhsisbigint,
                     &if_rhsisnotbigint);

              BIND(&if_rhsisbigint);
              {
                CombineFeedback(var_type_feedback,
                                CompareOperationFeedback::kBigInt);
                result = CAST(CallRuntime(Runtime::kBigIntEqualToBigInt,
                                          NoContextConstant(), lhs, rhs));
                Goto(&end);
              }

              BIND(&if_rhsisnotbigint);
              Goto(&if_not_equivalent_types);
            }

            BIND(&if_lhsisnotbigint);
            if (var_type_feedback != nullptr) {
              // Load the instance type of {rhs}.
              TNode<Map> rhs_map = LoadMap(CAST(rhs));
              TNode<Uint16T> rhs_instance_type = LoadMapInstanceType(rhs_map);

              Label if_lhsissymbol(this), if_lhsisreceiver(this),
                  if_lhsisoddball(this);
              GotoIf(IsJSReceiverInstanceType(lhs_instance_type),
                     &if_lhsisreceiver);
              GotoIf(IsBooleanMap(lhs_map), &if_not_equivalent_types);
              GotoIf(IsOddballInstanceType(lhs_instance_type),
                     &if_lhsisoddball);
              Branch(IsSymbolInstanceType(lhs_instance_type), &if_lhsissymbol,
                     &if_not_equivalent_types);

              BIND(&if_lhsisreceiver);
              {
                GotoIf(IsBooleanMap(rhs_map), &if_not_equivalent_types);
                OverwriteFeedback(var_type_feedback,
                                  CompareOperationFeedback::kReceiver);
                GotoIf(IsJSReceiverInstanceType(rhs_instance_type),
                       &if_notequal);
                OverwriteFeedback(
                    var_type_feedback,
                    CompareOperationFeedback::kReceiverOrNullOrUndefined);
                GotoIf(IsOddballInstanceType(rhs_instance_type), &if_notequal);
                Goto(&if_not_equivalent_types);
              }

              BIND(&if_lhsisoddball);
              {
                STATIC_ASSERT(LAST_PRIMITIVE_TYPE == ODDBALL_TYPE);
                GotoIf(IsBooleanMap(rhs_map), &if_not_equivalent_types);
                GotoIf(Int32LessThan(rhs_instance_type,
                                     Int32Constant(ODDBALL_TYPE)),
                       &if_not_equivalent_types);
                OverwriteFeedback(
                    var_type_feedback,
                    CompareOperationFeedback::kReceiverOrNullOrUndefined);
                Goto(&if_notequal);
              }

              BIND(&if_lhsissymbol);
              {
                GotoIfNot(IsSymbolInstanceType(rhs_instance_type),
                          &if_not_equivalent_types);
                OverwriteFeedback(var_type_feedback,
                                  CompareOperationFeedback::kSymbol);
                Goto(&if_notequal);
              }
            } else {
              Goto(&if_notequal);
            }
          }
        }
      }
    }

    BIND(&if_lhsissmi);
    {
      // We already know that {lhs} and {rhs} are not reference equal, and {lhs}
      // is a Smi; so {lhs} and {rhs} can only be strictly equal if {rhs} is a
      // HeapNumber with an equal floating point value.

      // Check if {rhs} is a Smi or a HeapObject.
      Label if_rhsissmi(this), if_rhsisnotsmi(this);
      Branch(TaggedIsSmi(rhs), &if_rhsissmi, &if_rhsisnotsmi);

      BIND(&if_rhsissmi);
      CombineFeedback(var_type_feedback,
                      CompareOperationFeedback::kSignedSmall);
      Goto(&if_notequal);

      BIND(&if_rhsisnotsmi);
      {
        // Load the map of the {rhs}.
        TNode<Map> rhs_map = LoadMap(CAST(rhs));

        // The {rhs} could be a HeapNumber with the same value as {lhs}.
        Label if_rhsisnumber(this), if_rhsisnotnumber(this);
        Branch(IsHeapNumberMap(rhs_map), &if_rhsisnumber, &if_rhsisnotnumber);

        BIND(&if_rhsisnumber);
        {
          // Convert {lhs} and {rhs} to floating point values.
          TNode<Float64T> lhs_value = SmiToFloat64(CAST(lhs));
          TNode<Float64T> rhs_value = LoadHeapNumberValue(CAST(rhs));

          CombineFeedback(var_type_feedback, CompareOperationFeedback::kNumber);

          // Perform a floating point comparison of {lhs} and {rhs}.
          Branch(Float64Equal(lhs_value, rhs_value), &if_equal, &if_notequal);
        }

        BIND(&if_rhsisnotnumber);
        Goto(&if_not_equivalent_types);
      }
    }
  }

  BIND(&if_equal);
  {
    result = TrueConstant();
    Goto(&end);
  }

  BIND(&if_not_equivalent_types);
  {
    OverwriteFeedback(var_type_feedback, CompareOperationFeedback::kAny);
    Goto(&if_notequal);
  }

  BIND(&if_notequal);
  {
    result = FalseConstant();
    Goto(&end);
  }

  BIND(&end);
  return result.value();
}

// ECMA#sec-samevalue
// This algorithm differs from the Strict Equality Comparison Algorithm in its
// treatment of signed zeroes and NaNs.
void CodeStubAssembler::BranchIfSameValue(Node* lhs, Node* rhs, Label* if_true,
                                          Label* if_false, SameValueMode mode) {
  VARIABLE(var_lhs_value, MachineRepresentation::kFloat64);
  VARIABLE(var_rhs_value, MachineRepresentation::kFloat64);
  Label do_fcmp(this);

  // Immediately jump to {if_true} if {lhs} == {rhs}, because - unlike
  // StrictEqual - SameValue considers two NaNs to be equal.
  GotoIf(WordEqual(lhs, rhs), if_true);

  // Check if the {lhs} is a Smi.
  Label if_lhsissmi(this), if_lhsisheapobject(this);
  Branch(TaggedIsSmi(lhs), &if_lhsissmi, &if_lhsisheapobject);

  BIND(&if_lhsissmi);
  {
    // Since {lhs} is a Smi, the comparison can only yield true
    // iff the {rhs} is a HeapNumber with the same float64 value.
    Branch(TaggedIsSmi(rhs), if_false, [&] {
      GotoIfNot(IsHeapNumber(rhs), if_false);
      var_lhs_value.Bind(SmiToFloat64(lhs));
      var_rhs_value.Bind(LoadHeapNumberValue(rhs));
      Goto(&do_fcmp);
    });
  }

  BIND(&if_lhsisheapobject);
  {
    // Check if the {rhs} is a Smi.
    Branch(
        TaggedIsSmi(rhs),
        [&] {
          // Since {rhs} is a Smi, the comparison can only yield true
          // iff the {lhs} is a HeapNumber with the same float64 value.
          GotoIfNot(IsHeapNumber(lhs), if_false);
          var_lhs_value.Bind(LoadHeapNumberValue(lhs));
          var_rhs_value.Bind(SmiToFloat64(rhs));
          Goto(&do_fcmp);
        },
        [&] {
          // Now this can only yield true if either both {lhs} and {rhs} are
          // HeapNumbers with the same value, or both are Strings with the
          // same character sequence, or both are BigInts with the same
          // value.
          Label if_lhsisheapnumber(this), if_lhsisstring(this),
              if_lhsisbigint(this);
          Node* const lhs_map = LoadMap(lhs);
          GotoIf(IsHeapNumberMap(lhs_map), &if_lhsisheapnumber);
          if (mode != SameValueMode::kNumbersOnly) {
            Node* const lhs_instance_type = LoadMapInstanceType(lhs_map);
            GotoIf(IsStringInstanceType(lhs_instance_type), &if_lhsisstring);
            GotoIf(IsBigIntInstanceType(lhs_instance_type), &if_lhsisbigint);
          }
          Goto(if_false);

          BIND(&if_lhsisheapnumber);
          {
            GotoIfNot(IsHeapNumber(rhs), if_false);
            var_lhs_value.Bind(LoadHeapNumberValue(lhs));
            var_rhs_value.Bind(LoadHeapNumberValue(rhs));
            Goto(&do_fcmp);
          }

          if (mode != SameValueMode::kNumbersOnly) {
            BIND(&if_lhsisstring);
            {
              // Now we can only yield true if {rhs} is also a String
              // with the same sequence of characters.
              GotoIfNot(IsString(rhs), if_false);
              Node* const result = CallBuiltin(Builtins::kStringEqual,
                                               NoContextConstant(), lhs, rhs);
              Branch(IsTrue(result), if_true, if_false);
            }

            BIND(&if_lhsisbigint);
            {
              GotoIfNot(IsBigInt(rhs), if_false);
              Node* const result = CallRuntime(Runtime::kBigIntEqualToBigInt,
                                               NoContextConstant(), lhs, rhs);
              Branch(IsTrue(result), if_true, if_false);
            }
          }
        });
  }

  BIND(&do_fcmp);
  {
    TNode<Float64T> lhs_value = UncheckedCast<Float64T>(var_lhs_value.value());
    TNode<Float64T> rhs_value = UncheckedCast<Float64T>(var_rhs_value.value());
    BranchIfSameNumberValue(lhs_value, rhs_value, if_true, if_false);
  }
}

void CodeStubAssembler::BranchIfSameNumberValue(TNode<Float64T> lhs_value,
                                                TNode<Float64T> rhs_value,
                                                Label* if_true,
                                                Label* if_false) {
  Label if_equal(this), if_notequal(this);
  Branch(Float64Equal(lhs_value, rhs_value), &if_equal, &if_notequal);

  BIND(&if_equal);
  {
    // We still need to handle the case when {lhs} and {rhs} are -0.0 and
    // 0.0 (or vice versa). Compare the high word to
    // distinguish between the two.
    Node* const lhs_hi_word = Float64ExtractHighWord32(lhs_value);
    Node* const rhs_hi_word = Float64ExtractHighWord32(rhs_value);

    // If x is +0 and y is -0, return false.
    // If x is -0 and y is +0, return false.
    Branch(Word32Equal(lhs_hi_word, rhs_hi_word), if_true, if_false);
  }

  BIND(&if_notequal);
  {
    // Return true iff both {rhs} and {lhs} are NaN.
    GotoIf(Float64Equal(lhs_value, lhs_value), if_false);
    Branch(Float64Equal(rhs_value, rhs_value), if_false, if_true);
  }
}

TNode<Oddball> CodeStubAssembler::HasProperty(SloppyTNode<Context> context,
                                              SloppyTNode<Object> object,
                                              SloppyTNode<Object> key,
                                              HasPropertyLookupMode mode) {
  Label call_runtime(this, Label::kDeferred), return_true(this),
      return_false(this), end(this), if_proxy(this, Label::kDeferred);

  CodeStubAssembler::LookupInHolder lookup_property_in_holder =
      [this, &return_true](Node* receiver, Node* holder, Node* holder_map,
                           Node* holder_instance_type, Node* unique_name,
                           Label* next_holder, Label* if_bailout) {
        TryHasOwnProperty(holder, holder_map, holder_instance_type, unique_name,
                          &return_true, next_holder, if_bailout);
      };

  CodeStubAssembler::LookupInHolder lookup_element_in_holder =
      [this, &return_true, &return_false](
          Node* receiver, Node* holder, Node* holder_map,
          Node* holder_instance_type, Node* index, Label* next_holder,
          Label* if_bailout) {
        TryLookupElement(holder, holder_map, holder_instance_type, index,
                         &return_true, &return_false, next_holder, if_bailout);
      };

  TryPrototypeChainLookup(object, object, key, lookup_property_in_holder,
                          lookup_element_in_holder, &return_false,
                          &call_runtime, &if_proxy);

  TVARIABLE(Oddball, result);

  BIND(&if_proxy);
  {
    TNode<Name> name = CAST(CallBuiltin(Builtins::kToName, context, key));
    switch (mode) {
      case kHasProperty:
        GotoIf(IsPrivateSymbol(name), &return_false);

        result = CAST(
            CallBuiltin(Builtins::kProxyHasProperty, context, object, name));
        Goto(&end);
        break;
      case kForInHasProperty:
        Goto(&call_runtime);
        break;
    }
  }

  BIND(&return_true);
  {
    result = TrueConstant();
    Goto(&end);
  }

  BIND(&return_false);
  {
    result = FalseConstant();
    Goto(&end);
  }

  BIND(&call_runtime);
  {
    Runtime::FunctionId fallback_runtime_function_id;
    switch (mode) {
      case kHasProperty:
        fallback_runtime_function_id = Runtime::kHasProperty;
        break;
      case kForInHasProperty:
        fallback_runtime_function_id = Runtime::kForInHasProperty;
        break;
    }

    result =
        CAST(CallRuntime(fallback_runtime_function_id, context, object, key));
    Goto(&end);
  }

  BIND(&end);
  CSA_ASSERT(this, IsBoolean(result.value()));
  return result.value();
}

Node* CodeStubAssembler::Typeof(Node* value) {
  VARIABLE(result_var, MachineRepresentation::kTagged);

  Label return_number(this, Label::kDeferred), if_oddball(this),
      return_function(this), return_undefined(this), return_object(this),
      return_string(this), return_bigint(this), return_result(this);

  GotoIf(TaggedIsSmi(value), &return_number);

  Node* map = LoadMap(value);

  GotoIf(IsHeapNumberMap(map), &return_number);

  Node* instance_type = LoadMapInstanceType(map);

  GotoIf(InstanceTypeEqual(instance_type, ODDBALL_TYPE), &if_oddball);

  Node* callable_or_undetectable_mask = Word32And(
      LoadMapBitField(map),
      Int32Constant(Map::IsCallableBit::kMask | Map::IsUndetectableBit::kMask));

  GotoIf(Word32Equal(callable_or_undetectable_mask,
                     Int32Constant(Map::IsCallableBit::kMask)),
         &return_function);

  GotoIfNot(Word32Equal(callable_or_undetectable_mask, Int32Constant(0)),
            &return_undefined);

  GotoIf(IsJSReceiverInstanceType(instance_type), &return_object);

  GotoIf(IsStringInstanceType(instance_type), &return_string);

  GotoIf(IsBigIntInstanceType(instance_type), &return_bigint);

  CSA_ASSERT(this, InstanceTypeEqual(instance_type, SYMBOL_TYPE));
  result_var.Bind(HeapConstant(isolate()->factory()->symbol_string()));
  Goto(&return_result);

  BIND(&return_number);
  {
    result_var.Bind(HeapConstant(isolate()->factory()->number_string()));
    Goto(&return_result);
  }

  BIND(&if_oddball);
  {
    Node* type = LoadObjectField(value, Oddball::kTypeOfOffset);
    result_var.Bind(type);
    Goto(&return_result);
  }

  BIND(&return_function);
  {
    result_var.Bind(HeapConstant(isolate()->factory()->function_string()));
    Goto(&return_result);
  }

  BIND(&return_undefined);
  {
    result_var.Bind(HeapConstant(isolate()->factory()->undefined_string()));
    Goto(&return_result);
  }

  BIND(&return_object);
  {
    result_var.Bind(HeapConstant(isolate()->factory()->object_string()));
    Goto(&return_result);
  }

  BIND(&return_string);
  {
    result_var.Bind(HeapConstant(isolate()->factory()->string_string()));
    Goto(&return_result);
  }

  BIND(&return_bigint);
  {
    result_var.Bind(HeapConstant(isolate()->factory()->bigint_string()));
    Goto(&return_result);
  }

  BIND(&return_result);
  return result_var.value();
}

TNode<Object> CodeStubAssembler::GetSuperConstructor(
    SloppyTNode<Context> context, SloppyTNode<JSFunction> active_function) {
  Label is_not_constructor(this, Label::kDeferred), out(this);
  TVARIABLE(Object, result);

  TNode<Map> map = LoadMap(active_function);
  TNode<Object> prototype = LoadMapPrototype(map);
  TNode<Map> prototype_map = LoadMap(CAST(prototype));
  GotoIfNot(IsConstructorMap(prototype_map), &is_not_constructor);

  result = prototype;
  Goto(&out);

  BIND(&is_not_constructor);
  {
    CallRuntime(Runtime::kThrowNotSuperConstructor, context, prototype,
                active_function);
    Unreachable();
  }

  BIND(&out);
  return result.value();
}

TNode<JSReceiver> CodeStubAssembler::SpeciesConstructor(
    SloppyTNode<Context> context, SloppyTNode<Object> object,
    SloppyTNode<JSReceiver> default_constructor) {
  Isolate* isolate = this->isolate();
  TVARIABLE(JSReceiver, var_result, default_constructor);

  // 2. Let C be ? Get(O, "constructor").
  TNode<Object> constructor =
      GetProperty(context, object, isolate->factory()->constructor_string());

  // 3. If C is undefined, return defaultConstructor.
  Label out(this);
  GotoIf(IsUndefined(constructor), &out);

  // 4. If Type(C) is not Object, throw a TypeError exception.
  ThrowIfNotJSReceiver(context, constructor,
                       MessageTemplate::kConstructorNotReceiver);

  // 5. Let S be ? Get(C, @@species).
  TNode<Object> species =
      GetProperty(context, constructor, isolate->factory()->species_symbol());

  // 6. If S is either undefined or null, return defaultConstructor.
  GotoIf(IsNullOrUndefined(species), &out);

  // 7. If IsConstructor(S) is true, return S.
  Label throw_error(this);
  GotoIf(TaggedIsSmi(species), &throw_error);
  GotoIfNot(IsConstructorMap(LoadMap(CAST(species))), &throw_error);
  var_result = CAST(species);
  Goto(&out);

  // 8. Throw a TypeError exception.
  BIND(&throw_error);
  ThrowTypeError(context, MessageTemplate::kSpeciesNotConstructor);

  BIND(&out);
  return var_result.value();
}

Node* CodeStubAssembler::InstanceOf(Node* object, Node* callable,
                                    Node* context) {
  VARIABLE(var_result, MachineRepresentation::kTagged);
  Label if_notcallable(this, Label::kDeferred),
      if_notreceiver(this, Label::kDeferred), if_otherhandler(this),
      if_nohandler(this, Label::kDeferred), return_true(this),
      return_false(this), return_result(this, &var_result);

  // Ensure that the {callable} is actually a JSReceiver.
  GotoIf(TaggedIsSmi(callable), &if_notreceiver);
  GotoIfNot(IsJSReceiver(callable), &if_notreceiver);

  // Load the @@hasInstance property from {callable}.
  Node* inst_of_handler =
      GetProperty(context, callable, HasInstanceSymbolConstant());

  // Optimize for the likely case where {inst_of_handler} is the builtin
  // Function.prototype[@@hasInstance] method, and emit a direct call in
  // that case without any additional checking.
  Node* native_context = LoadNativeContext(context);
  Node* function_has_instance =
      LoadContextElement(native_context, Context::FUNCTION_HAS_INSTANCE_INDEX);
  GotoIfNot(WordEqual(inst_of_handler, function_has_instance),
            &if_otherhandler);
  {
    // Call to Function.prototype[@@hasInstance] directly.
    Callable builtin(BUILTIN_CODE(isolate(), FunctionPrototypeHasInstance),
                     CallTrampolineDescriptor{});
    Node* result = CallJS(builtin, context, inst_of_handler, callable, object);
    var_result.Bind(result);
    Goto(&return_result);
  }

  BIND(&if_otherhandler);
  {
    // Check if there's actually an {inst_of_handler}.
    GotoIf(IsNull(inst_of_handler), &if_nohandler);
    GotoIf(IsUndefined(inst_of_handler), &if_nohandler);

    // Call the {inst_of_handler} for {callable} and {object}.
    Node* result = CallJS(
        CodeFactory::Call(isolate(), ConvertReceiverMode::kNotNullOrUndefined),
        context, inst_of_handler, callable, object);

    // Convert the {result} to a Boolean.
    BranchIfToBooleanIsTrue(result, &return_true, &return_false);
  }

  BIND(&if_nohandler);
  {
    // Ensure that the {callable} is actually Callable.
    GotoIfNot(IsCallable(callable), &if_notcallable);

    // Use the OrdinaryHasInstance algorithm.
    Node* result =
        CallBuiltin(Builtins::kOrdinaryHasInstance, context, callable, object);
    var_result.Bind(result);
    Goto(&return_result);
  }

  BIND(&if_notcallable);
  { ThrowTypeError(context, MessageTemplate::kNonCallableInInstanceOfCheck); }

  BIND(&if_notreceiver);
  { ThrowTypeError(context, MessageTemplate::kNonObjectInInstanceOfCheck); }

  BIND(&return_true);
  var_result.Bind(TrueConstant());
  Goto(&return_result);

  BIND(&return_false);
  var_result.Bind(FalseConstant());
  Goto(&return_result);

  BIND(&return_result);
  return var_result.value();
}

TNode<Number> CodeStubAssembler::NumberInc(SloppyTNode<Number> value) {
  TVARIABLE(Number, var_result);
  TVARIABLE(Float64T, var_finc_value);
  Label if_issmi(this), if_isnotsmi(this), do_finc(this), end(this);
  Branch(TaggedIsSmi(value), &if_issmi, &if_isnotsmi);

  BIND(&if_issmi);
  {
    Label if_overflow(this);
    TNode<Smi> smi_value = CAST(value);
    TNode<Smi> one = SmiConstant(1);
    var_result = TrySmiAdd(smi_value, one, &if_overflow);
    Goto(&end);

    BIND(&if_overflow);
    {
      var_finc_value = SmiToFloat64(smi_value);
      Goto(&do_finc);
    }
  }

  BIND(&if_isnotsmi);
  {
    TNode<HeapNumber> heap_number_value = CAST(value);

    // Load the HeapNumber value.
    var_finc_value = LoadHeapNumberValue(heap_number_value);
    Goto(&do_finc);
  }

  BIND(&do_finc);
  {
    TNode<Float64T> finc_value = var_finc_value.value();
    TNode<Float64T> one = Float64Constant(1.0);
    TNode<Float64T> finc_result = Float64Add(finc_value, one);
    var_result = AllocateHeapNumberWithValue(finc_result);
    Goto(&end);
  }

  BIND(&end);
  return var_result.value();
}

TNode<Number> CodeStubAssembler::NumberDec(SloppyTNode<Number> value) {
  TVARIABLE(Number, var_result);
  TVARIABLE(Float64T, var_fdec_value);
  Label if_issmi(this), if_isnotsmi(this), do_fdec(this), end(this);
  Branch(TaggedIsSmi(value), &if_issmi, &if_isnotsmi);

  BIND(&if_issmi);
  {
    TNode<Smi> smi_value = CAST(value);
    TNode<Smi> one = SmiConstant(1);
    Label if_overflow(this);
    var_result = TrySmiSub(smi_value, one, &if_overflow);
    Goto(&end);

    BIND(&if_overflow);
    {
      var_fdec_value = SmiToFloat64(smi_value);
      Goto(&do_fdec);
    }
  }

  BIND(&if_isnotsmi);
  {
    TNode<HeapNumber> heap_number_value = CAST(value);

    // Load the HeapNumber value.
    var_fdec_value = LoadHeapNumberValue(heap_number_value);
    Goto(&do_fdec);
  }

  BIND(&do_fdec);
  {
    TNode<Float64T> fdec_value = var_fdec_value.value();
    TNode<Float64T> minus_one = Float64Constant(-1.0);
    TNode<Float64T> fdec_result = Float64Add(fdec_value, minus_one);
    var_result = AllocateHeapNumberWithValue(fdec_result);
    Goto(&end);
  }

  BIND(&end);
  return var_result.value();
}

TNode<Number> CodeStubAssembler::NumberAdd(SloppyTNode<Number> a,
                                           SloppyTNode<Number> b) {
  TVARIABLE(Number, var_result);
  Label float_add(this, Label::kDeferred), end(this);
  GotoIf(TaggedIsNotSmi(a), &float_add);
  GotoIf(TaggedIsNotSmi(b), &float_add);

  // Try fast Smi addition first.
  var_result = TrySmiAdd(CAST(a), CAST(b), &float_add);
  Goto(&end);

  BIND(&float_add);
  {
    var_result = ChangeFloat64ToTagged(
        Float64Add(ChangeNumberToFloat64(a), ChangeNumberToFloat64(b)));
    Goto(&end);
  }

  BIND(&end);
  return var_result.value();
}

TNode<Number> CodeStubAssembler::NumberSub(SloppyTNode<Number> a,
                                           SloppyTNode<Number> b) {
  TVARIABLE(Number, var_result);
  Label float_sub(this, Label::kDeferred), end(this);
  GotoIf(TaggedIsNotSmi(a), &float_sub);
  GotoIf(TaggedIsNotSmi(b), &float_sub);

  // Try fast Smi subtraction first.
  var_result = TrySmiSub(CAST(a), CAST(b), &float_sub);
  Goto(&end);

  BIND(&float_sub);
  {
    var_result = ChangeFloat64ToTagged(
        Float64Sub(ChangeNumberToFloat64(a), ChangeNumberToFloat64(b)));
    Goto(&end);
  }

  BIND(&end);
  return var_result.value();
}

void CodeStubAssembler::GotoIfNotNumber(Node* input, Label* is_not_number) {
  Label is_number(this);
  GotoIf(TaggedIsSmi(input), &is_number);
  Branch(IsHeapNumber(input), &is_number, is_not_number);
  BIND(&is_number);
}

void CodeStubAssembler::GotoIfNumber(Node* input, Label* is_number) {
  GotoIf(TaggedIsSmi(input), is_number);
  GotoIf(IsHeapNumber(input), is_number);
}

TNode<Number> CodeStubAssembler::BitwiseOp(Node* left32, Node* right32,
                                           Operation bitwise_op) {
  switch (bitwise_op) {
    case Operation::kBitwiseAnd:
      return ChangeInt32ToTagged(Signed(Word32And(left32, right32)));
    case Operation::kBitwiseOr:
      return ChangeInt32ToTagged(Signed(Word32Or(left32, right32)));
    case Operation::kBitwiseXor:
      return ChangeInt32ToTagged(Signed(Word32Xor(left32, right32)));
    case Operation::kShiftLeft:
      if (!Word32ShiftIsSafe()) {
        right32 = Word32And(right32, Int32Constant(0x1F));
      }
      return ChangeInt32ToTagged(Signed(Word32Shl(left32, right32)));
    case Operation::kShiftRight:
      if (!Word32ShiftIsSafe()) {
        right32 = Word32And(right32, Int32Constant(0x1F));
      }
      return ChangeInt32ToTagged(Signed(Word32Sar(left32, right32)));
    case Operation::kShiftRightLogical:
      if (!Word32ShiftIsSafe()) {
        right32 = Word32And(right32, Int32Constant(0x1F));
      }
      return ChangeUint32ToTagged(Unsigned(Word32Shr(left32, right32)));
    default:
      break;
  }
  UNREACHABLE();
}

// ES #sec-createarrayiterator
TNode<JSArrayIterator> CodeStubAssembler::CreateArrayIterator(
    TNode<Context> context, TNode<Object> object, IterationKind kind) {
  TNode<Context> native_context = LoadNativeContext(context);
  TNode<Map> iterator_map = CAST(LoadContextElement(
      native_context, Context::INITIAL_ARRAY_ITERATOR_MAP_INDEX));
  Node* iterator = Allocate(JSArrayIterator::kSize);
  StoreMapNoWriteBarrier(iterator, iterator_map);
  StoreObjectFieldRoot(iterator, JSArrayIterator::kPropertiesOrHashOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldRoot(iterator, JSArrayIterator::kElementsOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldNoWriteBarrier(
      iterator, JSArrayIterator::kIteratedObjectOffset, object);
  StoreObjectFieldNoWriteBarrier(iterator, JSArrayIterator::kNextIndexOffset,
                                 SmiConstant(0));
  StoreObjectFieldNoWriteBarrier(
      iterator, JSArrayIterator::kKindOffset,
      SmiConstant(Smi::FromInt(static_cast<int>(kind))));
  return CAST(iterator);
}

TNode<JSObject> CodeStubAssembler::AllocateJSIteratorResult(
    SloppyTNode<Context> context, SloppyTNode<Object> value,
    SloppyTNode<Oddball> done) {
  CSA_ASSERT(this, IsBoolean(done));
  Node* native_context = LoadNativeContext(context);
  Node* map =
      LoadContextElement(native_context, Context::ITERATOR_RESULT_MAP_INDEX);
  Node* result = Allocate(JSIteratorResult::kSize);
  StoreMapNoWriteBarrier(result, map);
  StoreObjectFieldRoot(result, JSIteratorResult::kPropertiesOrHashOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldRoot(result, JSIteratorResult::kElementsOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldNoWriteBarrier(result, JSIteratorResult::kValueOffset, value);
  StoreObjectFieldNoWriteBarrier(result, JSIteratorResult::kDoneOffset, done);
  return CAST(result);
}

Node* CodeStubAssembler::AllocateJSIteratorResultForEntry(Node* context,
                                                          Node* key,
                                                          Node* value) {
  Node* native_context = LoadNativeContext(context);
  Node* length = SmiConstant(2);
  int const elements_size = FixedArray::SizeFor(2);
  TNode<FixedArray> elements = UncheckedCast<FixedArray>(
      Allocate(elements_size + JSArray::kSize + JSIteratorResult::kSize));
  StoreObjectFieldRoot(elements, FixedArray::kMapOffset,
                       RootIndex::kFixedArrayMap);
  StoreObjectFieldNoWriteBarrier(elements, FixedArray::kLengthOffset, length);
  StoreFixedArrayElement(elements, 0, key);
  StoreFixedArrayElement(elements, 1, value);
  Node* array_map = LoadContextElement(
      native_context, Context::JS_ARRAY_PACKED_ELEMENTS_MAP_INDEX);
  TNode<HeapObject> array = InnerAllocate(elements, elements_size);
  StoreMapNoWriteBarrier(array, array_map);
  StoreObjectFieldRoot(array, JSArray::kPropertiesOrHashOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldNoWriteBarrier(array, JSArray::kElementsOffset, elements);
  StoreObjectFieldNoWriteBarrier(array, JSArray::kLengthOffset, length);
  Node* iterator_map =
      LoadContextElement(native_context, Context::ITERATOR_RESULT_MAP_INDEX);
  TNode<HeapObject> result = InnerAllocate(array, JSArray::kSize);
  StoreMapNoWriteBarrier(result, iterator_map);
  StoreObjectFieldRoot(result, JSIteratorResult::kPropertiesOrHashOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldRoot(result, JSIteratorResult::kElementsOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldNoWriteBarrier(result, JSIteratorResult::kValueOffset, array);
  StoreObjectFieldRoot(result, JSIteratorResult::kDoneOffset,
                       RootIndex::kFalseValue);
  return result;
}

TNode<JSReceiver> CodeStubAssembler::ArraySpeciesCreate(TNode<Context> context,
                                                        TNode<Object> o,
                                                        TNode<Number> len) {
  TNode<JSReceiver> constructor =
      CAST(CallRuntime(Runtime::kArraySpeciesConstructor, context, o));
  return Construct(context, constructor, len);
}

TNode<BoolT> CodeStubAssembler::IsDetachedBuffer(TNode<JSArrayBuffer> buffer) {
  TNode<Uint32T> buffer_bit_field = LoadJSArrayBufferBitField(buffer);
  return IsSetWord32<JSArrayBuffer::WasDetachedBit>(buffer_bit_field);
}

void CodeStubAssembler::ThrowIfArrayBufferIsDetached(
    SloppyTNode<Context> context, TNode<JSArrayBuffer> array_buffer,
    const char* method_name) {
  Label if_detached(this, Label::kDeferred), if_not_detached(this);
  Branch(IsDetachedBuffer(array_buffer), &if_detached, &if_not_detached);
  BIND(&if_detached);
  ThrowTypeError(context, MessageTemplate::kDetachedOperation, method_name);
  BIND(&if_not_detached);
}

void CodeStubAssembler::ThrowIfArrayBufferViewBufferIsDetached(
    SloppyTNode<Context> context, TNode<JSArrayBufferView> array_buffer_view,
    const char* method_name) {
  TNode<JSArrayBuffer> buffer = LoadJSArrayBufferViewBuffer(array_buffer_view);
  ThrowIfArrayBufferIsDetached(context, buffer, method_name);
}

TNode<Uint32T> CodeStubAssembler::LoadJSArrayBufferBitField(
    TNode<JSArrayBuffer> array_buffer) {
  return LoadObjectField<Uint32T>(array_buffer, JSArrayBuffer::kBitFieldOffset);
}

TNode<RawPtrT> CodeStubAssembler::LoadJSArrayBufferBackingStore(
    TNode<JSArrayBuffer> array_buffer) {
  return LoadObjectField<RawPtrT>(array_buffer,
                                  JSArrayBuffer::kBackingStoreOffset);
}

TNode<JSArrayBuffer> CodeStubAssembler::LoadJSArrayBufferViewBuffer(
    TNode<JSArrayBufferView> array_buffer_view) {
  return LoadObjectField<JSArrayBuffer>(array_buffer_view,
                                        JSArrayBufferView::kBufferOffset);
}

TNode<UintPtrT> CodeStubAssembler::LoadJSArrayBufferViewByteLength(
    TNode<JSArrayBufferView> array_buffer_view) {
  return LoadObjectField<UintPtrT>(array_buffer_view,
                                   JSArrayBufferView::kByteLengthOffset);
}

TNode<UintPtrT> CodeStubAssembler::LoadJSArrayBufferViewByteOffset(
    TNode<JSArrayBufferView> array_buffer_view) {
  return LoadObjectField<UintPtrT>(array_buffer_view,
                                   JSArrayBufferView::kByteOffsetOffset);
}

TNode<UintPtrT> CodeStubAssembler::LoadJSTypedArrayLength(
    TNode<JSTypedArray> typed_array) {
  return LoadObjectField<UintPtrT>(typed_array, JSTypedArray::kLengthOffset);
}

CodeStubArguments::CodeStubArguments(
    CodeStubAssembler* assembler, Node* argc, Node* fp,
    CodeStubAssembler::ParameterMode param_mode, ReceiverMode receiver_mode)
    : assembler_(assembler),
      argc_mode_(param_mode),
      receiver_mode_(receiver_mode),
      argc_(argc),
      base_(),
      fp_(fp != nullptr ? fp : assembler_->LoadFramePointer()) {
  Node* offset = assembler_->ElementOffsetFromIndex(
      argc_, SYSTEM_POINTER_ELEMENTS, param_mode,
      (StandardFrameConstants::kFixedSlotCountAboveFp - 1) *
          kSystemPointerSize);
  base_ =
      assembler_->UncheckedCast<RawPtrT>(assembler_->IntPtrAdd(fp_, offset));
}

TNode<Object> CodeStubArguments::GetReceiver() const {
  DCHECK_EQ(receiver_mode_, ReceiverMode::kHasReceiver);
  return assembler_->UncheckedCast<Object>(assembler_->LoadFullTagged(
      base_, assembler_->IntPtrConstant(kSystemPointerSize)));
}

void CodeStubArguments::SetReceiver(TNode<Object> object) const {
  DCHECK_EQ(receiver_mode_, ReceiverMode::kHasReceiver);
  assembler_->StoreFullTaggedNoWriteBarrier(
      base_, assembler_->IntPtrConstant(kSystemPointerSize), object);
}

TNode<WordT> CodeStubArguments::AtIndexPtr(
    Node* index, CodeStubAssembler::ParameterMode mode) const {
  using Node = compiler::Node;
  Node* negated_index = assembler_->IntPtrOrSmiSub(
      assembler_->IntPtrOrSmiConstant(0, mode), index, mode);
  Node* offset = assembler_->ElementOffsetFromIndex(
      negated_index, SYSTEM_POINTER_ELEMENTS, mode, 0);
  return assembler_->IntPtrAdd(assembler_->UncheckedCast<IntPtrT>(base_),
                               offset);
}

TNode<Object> CodeStubArguments::AtIndex(
    Node* index, CodeStubAssembler::ParameterMode mode) const {
  DCHECK_EQ(argc_mode_, mode);
  CSA_ASSERT(assembler_,
             assembler_->UintPtrOrSmiLessThan(index, GetLength(mode), mode));
  return assembler_->UncheckedCast<Object>(
      assembler_->LoadFullTagged(AtIndexPtr(index, mode)));
}

TNode<Object> CodeStubArguments::AtIndex(int index) const {
  return AtIndex(assembler_->IntPtrConstant(index));
}

TNode<Object> CodeStubArguments::GetOptionalArgumentValue(
    int index, TNode<Object> default_value) {
  CodeStubAssembler::TVariable<Object> result(assembler_);
  CodeStubAssembler::Label argument_missing(assembler_),
      argument_done(assembler_, &result);

  assembler_->GotoIf(assembler_->UintPtrOrSmiGreaterThanOrEqual(
                         assembler_->IntPtrOrSmiConstant(index, argc_mode_),
                         argc_, argc_mode_),
                     &argument_missing);
  result = AtIndex(index);
  assembler_->Goto(&argument_done);

  assembler_->BIND(&argument_missing);
  result = default_value;
  assembler_->Goto(&argument_done);

  assembler_->BIND(&argument_done);
  return result.value();
}

TNode<Object> CodeStubArguments::GetOptionalArgumentValue(
    TNode<IntPtrT> index, TNode<Object> default_value) {
  CodeStubAssembler::TVariable<Object> result(assembler_);
  CodeStubAssembler::Label argument_missing(assembler_),
      argument_done(assembler_, &result);

  assembler_->GotoIf(
      assembler_->UintPtrOrSmiGreaterThanOrEqual(
          assembler_->IntPtrToParameter(index, argc_mode_), argc_, argc_mode_),
      &argument_missing);
  result = AtIndex(index);
  assembler_->Goto(&argument_done);

  assembler_->BIND(&argument_missing);
  result = default_value;
  assembler_->Goto(&argument_done);

  assembler_->BIND(&argument_done);
  return result.value();
}

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

void CodeStubArguments::PopAndReturn(Node* value) {
  Node* pop_count;
  if (receiver_mode_ == ReceiverMode::kHasReceiver) {
    pop_count = assembler_->IntPtrOrSmiAdd(
        argc_, assembler_->IntPtrOrSmiConstant(1, argc_mode_), argc_mode_);
  } else {
    pop_count = argc_;
  }

  assembler_->PopAndReturn(assembler_->ParameterToIntPtr(pop_count, argc_mode_),
                           value);
}

TNode<BoolT> CodeStubAssembler::IsFastElementsKind(
    TNode<Int32T> elements_kind) {
  STATIC_ASSERT(FIRST_ELEMENTS_KIND == FIRST_FAST_ELEMENTS_KIND);
  return Uint32LessThanOrEqual(elements_kind,
                               Int32Constant(LAST_FAST_ELEMENTS_KIND));
}

TNode<BoolT> CodeStubAssembler::IsDoubleElementsKind(
    TNode<Int32T> elements_kind) {
  STATIC_ASSERT(FIRST_ELEMENTS_KIND == FIRST_FAST_ELEMENTS_KIND);
  STATIC_ASSERT((PACKED_DOUBLE_ELEMENTS & 1) == 0);
  STATIC_ASSERT(PACKED_DOUBLE_ELEMENTS + 1 == HOLEY_DOUBLE_ELEMENTS);
  return Word32Equal(Word32Shr(elements_kind, Int32Constant(1)),
                     Int32Constant(PACKED_DOUBLE_ELEMENTS / 2));
}

TNode<BoolT> CodeStubAssembler::IsFastSmiOrTaggedElementsKind(
    TNode<Int32T> elements_kind) {
  STATIC_ASSERT(FIRST_ELEMENTS_KIND == FIRST_FAST_ELEMENTS_KIND);
  STATIC_ASSERT(PACKED_DOUBLE_ELEMENTS > TERMINAL_FAST_ELEMENTS_KIND);
  STATIC_ASSERT(HOLEY_DOUBLE_ELEMENTS > TERMINAL_FAST_ELEMENTS_KIND);
  return Uint32LessThanOrEqual(elements_kind,
                               Int32Constant(TERMINAL_FAST_ELEMENTS_KIND));
}

TNode<BoolT> CodeStubAssembler::IsFastSmiElementsKind(
    SloppyTNode<Int32T> elements_kind) {
  return Uint32LessThanOrEqual(elements_kind,
                               Int32Constant(HOLEY_SMI_ELEMENTS));
}

TNode<BoolT> CodeStubAssembler::IsHoleyFastElementsKind(
    TNode<Int32T> elements_kind) {
  CSA_ASSERT(this, IsFastElementsKind(elements_kind));

  STATIC_ASSERT(HOLEY_SMI_ELEMENTS == (PACKED_SMI_ELEMENTS | 1));
  STATIC_ASSERT(HOLEY_ELEMENTS == (PACKED_ELEMENTS | 1));
  STATIC_ASSERT(HOLEY_DOUBLE_ELEMENTS == (PACKED_DOUBLE_ELEMENTS | 1));
  return IsSetWord32(elements_kind, 1);
}

TNode<BoolT> CodeStubAssembler::IsHoleyFastElementsKindForRead(
    TNode<Int32T> elements_kind) {
  CSA_ASSERT(this,
             Uint32LessThanOrEqual(elements_kind,
                                   Int32Constant(LAST_FROZEN_ELEMENTS_KIND)));

  STATIC_ASSERT(HOLEY_SMI_ELEMENTS == (PACKED_SMI_ELEMENTS | 1));
  STATIC_ASSERT(HOLEY_ELEMENTS == (PACKED_ELEMENTS | 1));
  STATIC_ASSERT(HOLEY_DOUBLE_ELEMENTS == (PACKED_DOUBLE_ELEMENTS | 1));
  STATIC_ASSERT(HOLEY_SEALED_ELEMENTS == (PACKED_SEALED_ELEMENTS | 1));
  STATIC_ASSERT(HOLEY_FROZEN_ELEMENTS == (PACKED_FROZEN_ELEMENTS | 1));
  return IsSetWord32(elements_kind, 1);
}

TNode<BoolT> CodeStubAssembler::IsElementsKindGreaterThan(
    TNode<Int32T> target_kind, ElementsKind reference_kind) {
  return Int32GreaterThan(target_kind, Int32Constant(reference_kind));
}

TNode<BoolT> CodeStubAssembler::IsElementsKindLessThanOrEqual(
    TNode<Int32T> target_kind, ElementsKind reference_kind) {
  return Int32LessThanOrEqual(target_kind, Int32Constant(reference_kind));
}

TNode<BoolT> CodeStubAssembler::IsElementsKindInRange(
    TNode<Int32T> target_kind, ElementsKind lower_reference_kind,
    ElementsKind higher_reference_kind) {
  return Uint32LessThanOrEqual(
      Int32Sub(target_kind, Int32Constant(lower_reference_kind)),
      Int32Constant(higher_reference_kind - lower_reference_kind));
}

Node* CodeStubAssembler::IsDebugActive() {
  Node* is_debug_active = Load(
      MachineType::Uint8(),
      ExternalConstant(ExternalReference::debug_is_active_address(isolate())));
  return Word32NotEqual(is_debug_active, Int32Constant(0));
}

Node* CodeStubAssembler::IsPromiseHookEnabled() {
  Node* const promise_hook = Load(
      MachineType::Pointer(),
      ExternalConstant(ExternalReference::promise_hook_address(isolate())));
  return WordNotEqual(promise_hook, IntPtrConstant(0));
}

Node* CodeStubAssembler::HasAsyncEventDelegate() {
  Node* const async_event_delegate =
      Load(MachineType::Pointer(),
           ExternalConstant(
               ExternalReference::async_event_delegate_address(isolate())));
  return WordNotEqual(async_event_delegate, IntPtrConstant(0));
}

Node* CodeStubAssembler::IsPromiseHookEnabledOrHasAsyncEventDelegate() {
  Node* const promise_hook_or_async_event_delegate =
      Load(MachineType::Uint8(),
           ExternalConstant(
               ExternalReference::promise_hook_or_async_event_delegate_address(
                   isolate())));
  return Word32NotEqual(promise_hook_or_async_event_delegate, Int32Constant(0));
}

Node* CodeStubAssembler::
    IsPromiseHookEnabledOrDebugIsActiveOrHasAsyncEventDelegate() {
  Node* const promise_hook_or_debug_is_active_or_async_event_delegate = Load(
      MachineType::Uint8(),
      ExternalConstant(
          ExternalReference::
              promise_hook_or_debug_is_active_or_async_event_delegate_address(
                  isolate())));
  return Word32NotEqual(promise_hook_or_debug_is_active_or_async_event_delegate,
                        Int32Constant(0));
}

TNode<Code> CodeStubAssembler::LoadBuiltin(TNode<Smi> builtin_id) {
  CSA_ASSERT(this, SmiGreaterThanOrEqual(builtin_id, SmiConstant(0)));
  CSA_ASSERT(this,
             SmiLessThan(builtin_id, SmiConstant(Builtins::builtin_count)));

  int const kSmiShiftBits = kSmiShiftSize + kSmiTagSize;
  int index_shift = kSystemPointerSizeLog2 - kSmiShiftBits;
  TNode<WordT> table_index =
      index_shift >= 0
          ? WordShl(BitcastTaggedSignedToWord(builtin_id), index_shift)
          : WordSar(BitcastTaggedSignedToWord(builtin_id), -index_shift);

  return CAST(
      Load(MachineType::TaggedPointer(),
           ExternalConstant(ExternalReference::builtins_address(isolate())),
           table_index));
}

TNode<Code> CodeStubAssembler::GetSharedFunctionInfoCode(
    SloppyTNode<SharedFunctionInfo> shared_info, Label* if_compile_lazy) {
  TNode<Object> sfi_data =
      LoadObjectField(shared_info, SharedFunctionInfo::kFunctionDataOffset);

  TVARIABLE(Code, sfi_code);

  Label done(this);
  Label check_instance_type(this);

  // IsSmi: Is builtin
  GotoIf(TaggedIsNotSmi(sfi_data), &check_instance_type);
  if (if_compile_lazy) {
    GotoIf(SmiEqual(CAST(sfi_data), SmiConstant(Builtins::kCompileLazy)),
           if_compile_lazy);
  }
  sfi_code = LoadBuiltin(CAST(sfi_data));
  Goto(&done);

  // Switch on data's instance type.
  BIND(&check_instance_type);
  TNode<Int32T> data_type = LoadInstanceType(CAST(sfi_data));

  int32_t case_values[] = {BYTECODE_ARRAY_TYPE,
                           WASM_EXPORTED_FUNCTION_DATA_TYPE,
                           ASM_WASM_DATA_TYPE,
                           UNCOMPILED_DATA_WITHOUT_PREPARSE_DATA_TYPE,
                           UNCOMPILED_DATA_WITH_PREPARSE_DATA_TYPE,
                           FUNCTION_TEMPLATE_INFO_TYPE,
                           WASM_JS_FUNCTION_DATA_TYPE,
                           WASM_CAPI_FUNCTION_DATA_TYPE};
  Label check_is_bytecode_array(this);
  Label check_is_exported_function_data(this);
  Label check_is_asm_wasm_data(this);
  Label check_is_uncompiled_data_without_preparse_data(this);
  Label check_is_uncompiled_data_with_preparse_data(this);
  Label check_is_function_template_info(this);
  Label check_is_interpreter_data(this);
  Label check_is_wasm_js_function_data(this);
  Label check_is_wasm_capi_function_data(this);
  Label* case_labels[] = {&check_is_bytecode_array,
                          &check_is_exported_function_data,
                          &check_is_asm_wasm_data,
                          &check_is_uncompiled_data_without_preparse_data,
                          &check_is_uncompiled_data_with_preparse_data,
                          &check_is_function_template_info,
                          &check_is_wasm_js_function_data,
                          &check_is_wasm_capi_function_data};
  STATIC_ASSERT(arraysize(case_values) == arraysize(case_labels));
  Switch(data_type, &check_is_interpreter_data, case_values, case_labels,
         arraysize(case_labels));

  // IsBytecodeArray: Interpret bytecode
  BIND(&check_is_bytecode_array);
  sfi_code = HeapConstant(BUILTIN_CODE(isolate(), InterpreterEntryTrampoline));
  Goto(&done);

  // IsWasmExportedFunctionData: Use the wrapper code
  BIND(&check_is_exported_function_data);
  sfi_code = CAST(LoadObjectField(
      CAST(sfi_data), WasmExportedFunctionData::kWrapperCodeOffset));
  Goto(&done);

  // IsAsmWasmData: Instantiate using AsmWasmData
  BIND(&check_is_asm_wasm_data);
  sfi_code = HeapConstant(BUILTIN_CODE(isolate(), InstantiateAsmJs));
  Goto(&done);

  // IsUncompiledDataWithPreparseData | IsUncompiledDataWithoutPreparseData:
  // Compile lazy
  BIND(&check_is_uncompiled_data_with_preparse_data);
  Goto(&check_is_uncompiled_data_without_preparse_data);
  BIND(&check_is_uncompiled_data_without_preparse_data);
  sfi_code = HeapConstant(BUILTIN_CODE(isolate(), CompileLazy));
  Goto(if_compile_lazy ? if_compile_lazy : &done);

  // IsFunctionTemplateInfo: API call
  BIND(&check_is_function_template_info);
  sfi_code = HeapConstant(BUILTIN_CODE(isolate(), HandleApiCall));
  Goto(&done);

  // IsInterpreterData: Interpret bytecode
  BIND(&check_is_interpreter_data);
  // This is the default branch, so assert that we have the expected data type.
  CSA_ASSERT(this,
             Word32Equal(data_type, Int32Constant(INTERPRETER_DATA_TYPE)));
  sfi_code = CAST(LoadObjectField(
      CAST(sfi_data), InterpreterData::kInterpreterTrampolineOffset));
  Goto(&done);

  // IsWasmJSFunctionData: Use the wrapper code.
  BIND(&check_is_wasm_js_function_data);
  sfi_code = CAST(
      LoadObjectField(CAST(sfi_data), WasmJSFunctionData::kWrapperCodeOffset));
  Goto(&done);

  // IsWasmCapiFunctionData: Use the wrapper code.
  BIND(&check_is_wasm_capi_function_data);
  sfi_code = CAST(LoadObjectField(CAST(sfi_data),
                                  WasmCapiFunctionData::kWrapperCodeOffset));
  Goto(&done);

  BIND(&done);
  return sfi_code.value();
}

Node* CodeStubAssembler::AllocateFunctionWithMapAndContext(Node* map,
                                                           Node* shared_info,
                                                           Node* context) {
  CSA_SLOW_ASSERT(this, IsMap(map));

  Node* const code = GetSharedFunctionInfoCode(shared_info);

  // TODO(ishell): All the callers of this function pass map loaded from
  // Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX. So we can remove
  // map parameter.
  CSA_ASSERT(this, Word32BinaryNot(IsConstructorMap(map)));
  CSA_ASSERT(this, Word32BinaryNot(IsFunctionWithPrototypeSlotMap(map)));
  Node* const fun = Allocate(JSFunction::kSizeWithoutPrototype);
  STATIC_ASSERT(JSFunction::kSizeWithoutPrototype == 7 * kTaggedSize);
  StoreMapNoWriteBarrier(fun, map);
  StoreObjectFieldRoot(fun, JSObject::kPropertiesOrHashOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldRoot(fun, JSObject::kElementsOffset,
                       RootIndex::kEmptyFixedArray);
  StoreObjectFieldRoot(fun, JSFunction::kFeedbackCellOffset,
                       RootIndex::kManyClosuresCell);
  StoreObjectFieldNoWriteBarrier(fun, JSFunction::kSharedFunctionInfoOffset,
                                 shared_info);
  StoreObjectFieldNoWriteBarrier(fun, JSFunction::kContextOffset, context);
  StoreObjectFieldNoWriteBarrier(fun, JSFunction::kCodeOffset, code);
  return fun;
}

void CodeStubAssembler::CheckPrototypeEnumCache(Node* receiver,
                                                Node* receiver_map,
                                                Label* if_fast,
                                                Label* if_slow) {
  VARIABLE(var_object, MachineRepresentation::kTagged, receiver);
  VARIABLE(var_object_map, MachineRepresentation::kTagged, receiver_map);

  Label loop(this, {&var_object, &var_object_map}), done_loop(this);
  Goto(&loop);
  BIND(&loop);
  {
    // Check that there are no elements on the current {object}.
    Label if_no_elements(this);
    Node* object = var_object.value();
    Node* object_map = var_object_map.value();

    // The following relies on the elements only aliasing with JSProxy::target,
    // which is a Javascript value and hence cannot be confused with an elements
    // backing store.
    STATIC_ASSERT(static_cast<int>(JSObject::kElementsOffset) ==
                  static_cast<int>(JSProxy::kTargetOffset));
    Node* object_elements = LoadObjectField(object, JSObject::kElementsOffset);
    GotoIf(IsEmptyFixedArray(object_elements), &if_no_elements);
    GotoIf(IsEmptySlowElementDictionary(object_elements), &if_no_elements);

    // It might still be an empty JSArray.
    GotoIfNot(IsJSArrayMap(object_map), if_slow);
    Node* object_length = LoadJSArrayLength(object);
    Branch(WordEqual(object_length, SmiConstant(0)), &if_no_elements, if_slow);

    // Continue with the {object}s prototype.
    BIND(&if_no_elements);
    object = LoadMapPrototype(object_map);
    GotoIf(IsNull(object), if_fast);

    // For all {object}s but the {receiver}, check that the cache is empty.
    var_object.Bind(object);
    object_map = LoadMap(object);
    var_object_map.Bind(object_map);
    Node* object_enum_length = LoadMapEnumLength(object_map);
    Branch(WordEqual(object_enum_length, IntPtrConstant(0)), &loop, if_slow);
  }
}

Node* CodeStubAssembler::CheckEnumCache(Node* receiver, Label* if_empty,
                                        Label* if_runtime) {
  Label if_fast(this), if_cache(this), if_no_cache(this, Label::kDeferred);
  Node* receiver_map = LoadMap(receiver);

  // Check if the enum length field of the {receiver} is properly initialized,
  // indicating that there is an enum cache.
  Node* receiver_enum_length = LoadMapEnumLength(receiver_map);
  Branch(WordEqual(receiver_enum_length,
                   IntPtrConstant(kInvalidEnumCacheSentinel)),
         &if_no_cache, &if_cache);

  BIND(&if_no_cache);
  {
    // Avoid runtime-call for empty dictionary receivers.
    GotoIfNot(IsDictionaryMap(receiver_map), if_runtime);
    TNode<NameDictionary> properties = CAST(LoadSlowProperties(receiver));
    TNode<Smi> length = GetNumberOfElements(properties);
    GotoIfNot(WordEqual(length, SmiConstant(0)), if_runtime);
    // Check that there are no elements on the {receiver} and its prototype
    // chain. Given that we do not create an EnumCache for dict-mode objects,
    // directly jump to {if_empty} if there are no elements and no properties
    // on the {receiver}.
    CheckPrototypeEnumCache(receiver, receiver_map, if_empty, if_runtime);
  }

  // Check that there are no elements on the fast {receiver} and its
  // prototype chain.
  BIND(&if_cache);
  CheckPrototypeEnumCache(receiver, receiver_map, &if_fast, if_runtime);

  BIND(&if_fast);
  return receiver_map;
}

TNode<Object> CodeStubAssembler::GetArgumentValue(TorqueStructArguments args,
                                                  TNode<IntPtrT> index) {
  return CodeStubArguments(this, args).GetOptionalArgumentValue(index);
}

TorqueStructArguments CodeStubAssembler::GetFrameArguments(
    TNode<RawPtrT> frame, TNode<IntPtrT> argc) {
  return CodeStubArguments(this, argc, frame, INTPTR_PARAMETERS)
      .GetTorqueArguments();
}

void CodeStubAssembler::Print(const char* s) {
  std::string formatted(s);
  formatted += "\n";
  CallRuntime(Runtime::kGlobalPrint, NoContextConstant(),
              StringConstant(formatted.c_str()));
}

void CodeStubAssembler::Print(const char* prefix, Node* tagged_value) {
  if (prefix != nullptr) {
    std::string formatted(prefix);
    formatted += ": ";
    Handle<String> string = isolate()->factory()->NewStringFromAsciiChecked(
        formatted.c_str(), AllocationType::kOld);
    CallRuntime(Runtime::kGlobalPrint, NoContextConstant(),
                HeapConstant(string));
  }
  CallRuntime(Runtime::kDebugPrint, NoContextConstant(), tagged_value);
}

void CodeStubAssembler::PerformStackCheck(TNode<Context> context) {
  Label ok(this), stack_check_interrupt(this, Label::kDeferred);

  // The instruction sequence below is carefully crafted to hit our pattern
  // matcher for stack checks within instruction selection.
  // See StackCheckMatcher::Matched and JSGenericLowering::LowerJSStackCheck.

  TNode<UintPtrT> sp = UncheckedCast<UintPtrT>(LoadStackPointer());
  TNode<UintPtrT> stack_limit = UncheckedCast<UintPtrT>(Load(
      MachineType::Pointer(),
      ExternalConstant(ExternalReference::address_of_stack_limit(isolate()))));
  TNode<BoolT> sp_within_limit = UintPtrLessThan(stack_limit, sp);

  Branch(sp_within_limit, &ok, &stack_check_interrupt);

  BIND(&stack_check_interrupt);
  CallRuntime(Runtime::kStackGuard, context);
  Goto(&ok);

  BIND(&ok);
}

void CodeStubAssembler::InitializeFunctionContext(Node* native_context,
                                                  Node* context, int slots) {
  DCHECK_GE(slots, Context::MIN_CONTEXT_SLOTS);
  StoreMapNoWriteBarrier(context, RootIndex::kFunctionContextMap);
  StoreObjectFieldNoWriteBarrier(context, FixedArray::kLengthOffset,
                                 SmiConstant(slots));

  Node* const empty_scope_info =
      LoadContextElement(native_context, Context::SCOPE_INFO_INDEX);
  StoreContextElementNoWriteBarrier(context, Context::SCOPE_INFO_INDEX,
                                    empty_scope_info);
  StoreContextElementNoWriteBarrier(context, Context::PREVIOUS_INDEX,
                                    UndefinedConstant());
  StoreContextElementNoWriteBarrier(context, Context::EXTENSION_INDEX,
                                    TheHoleConstant());
  StoreContextElementNoWriteBarrier(context, Context::NATIVE_CONTEXT_INDEX,
                                    native_context);
}

TNode<JSArray> CodeStubAssembler::ArrayCreate(TNode<Context> context,
                                              TNode<Number> length) {
  TVARIABLE(JSArray, array);
  Label allocate_js_array(this);

  Label done(this), next(this), runtime(this, Label::kDeferred);
  TNode<Smi> limit = SmiConstant(JSArray::kInitialMaxFastElementArray);
  CSA_ASSERT_BRANCH(this, [=](Label* ok, Label* not_ok) {
    BranchIfNumberRelationalComparison(Operation::kGreaterThanOrEqual, length,
                                       SmiConstant(0), ok, not_ok);
  });
  // This check also transitively covers the case where length is too big
  // to be representable by a SMI and so is not usable with
  // AllocateJSArray.
  BranchIfNumberRelationalComparison(Operation::kGreaterThanOrEqual, length,
                                     limit, &runtime, &next);

  BIND(&runtime);
  {
    TNode<Context> native_context = LoadNativeContext(context);
    TNode<JSFunction> array_function =
        CAST(LoadContextElement(native_context, Context::ARRAY_FUNCTION_INDEX));
    array = CAST(CallRuntime(Runtime::kNewArray, context, array_function,
                             length, array_function, UndefinedConstant()));
    Goto(&done);
  }

  BIND(&next);
  CSA_ASSERT(this, TaggedIsSmi(length));

  TNode<Map> array_map = CAST(LoadContextElement(
      context, Context::JS_ARRAY_PACKED_SMI_ELEMENTS_MAP_INDEX));

  // TODO(delphick): Consider using
  // AllocateUninitializedJSArrayWithElements to avoid initializing an
  // array and then writing over it.
  array =
      AllocateJSArray(PACKED_SMI_ELEMENTS, array_map, length, SmiConstant(0),
                      nullptr, ParameterMode::SMI_PARAMETERS);
  Goto(&done);

  BIND(&done);
  return array.value();
}

void CodeStubAssembler::SetPropertyLength(TNode<Context> context,
                                          TNode<Object> array,
                                          TNode<Number> length) {
  Label fast(this), runtime(this), done(this);
  // There's no need to set the length, if
  // 1) the array is a fast JS array and
  // 2) the new length is equal to the old length.
  // as the set is not observable. Otherwise fall back to the run-time.

  // 1) Check that the array has fast elements.
  // TODO(delphick): Consider changing this since it does an an unnecessary
  // check for SMIs.
  // TODO(delphick): Also we could hoist this to after the array construction
  // and copy the args into array in the same way as the Array constructor.
  BranchIfFastJSArray(array, context, &fast, &runtime);

  BIND(&fast);
  {
    TNode<JSArray> fast_array = CAST(array);

    TNode<Smi> length_smi = CAST(length);
    TNode<Smi> old_length = LoadFastJSArrayLength(fast_array);
    CSA_ASSERT(this, TaggedIsPositiveSmi(old_length));

    // 2) If the created array's length matches the required length, then
    //    there's nothing else to do. Otherwise use the runtime to set the
    //    property as that will insert holes into excess elements or shrink
    //    the backing store as appropriate.
    Branch(SmiNotEqual(length_smi, old_length), &runtime, &done);
  }

  BIND(&runtime);
  {
    SetPropertyStrict(context, array, CodeStubAssembler::LengthStringConstant(),
                      length);
    Goto(&done);
  }

  BIND(&done);
}

void CodeStubAssembler::GotoIfInitialPrototypePropertyModified(
    TNode<Map> object_map, TNode<Map> initial_prototype_map, int descriptor,
    RootIndex field_name_root_index, Label* if_modified) {
  DescriptorIndexAndName index_name{descriptor, field_name_root_index};
  GotoIfInitialPrototypePropertiesModified(
      object_map, initial_prototype_map,
      Vector<DescriptorIndexAndName>(&index_name, 1), if_modified);
}

void CodeStubAssembler::GotoIfInitialPrototypePropertiesModified(
    TNode<Map> object_map, TNode<Map> initial_prototype_map,
    Vector<DescriptorIndexAndName> properties, Label* if_modified) {
  TNode<Map> prototype_map = LoadMap(LoadMapPrototype(object_map));
  GotoIfNot(WordEqual(prototype_map, initial_prototype_map), if_modified);

  // We need to make sure that relevant properties in the prototype have
  // not been tampered with. We do this by checking that their slots
  // in the prototype's descriptor array are still marked as const.
  TNode<DescriptorArray> descriptors = LoadMapDescriptors(prototype_map);

  TNode<Uint32T> combined_details;
  for (int i = 0; i < properties.length(); i++) {
    // Assert the descriptor index is in-bounds.
    int descriptor = properties[i].descriptor_index;
    CSA_ASSERT(this, Int32LessThan(Int32Constant(descriptor),
                                   LoadNumberOfDescriptors(descriptors)));
    // Assert that the name is correct. This essentially checks that
    // the descriptor index corresponds to the insertion order in
    // the bootstrapper.
    CSA_ASSERT(this,
               WordEqual(LoadKeyByDescriptorEntry(descriptors, descriptor),
                         LoadRoot(properties[i].name_root_index)));

    TNode<Uint32T> details =
        DescriptorArrayGetDetails(descriptors, Uint32Constant(descriptor));
    if (i == 0) {
      combined_details = details;
    } else {
      combined_details = Word32And(combined_details, details);
    }
  }

  TNode<Uint32T> constness =
      DecodeWord32<PropertyDetails::ConstnessField>(combined_details);

  GotoIfNot(
      Word32Equal(constness,
                  Int32Constant(static_cast<int>(PropertyConstness::kConst))),
      if_modified);
}

TNode<String> CodeStubAssembler::TaggedToDirectString(TNode<Object> value,
                                                      Label* fail) {
  ToDirectStringAssembler to_direct(state(), value);
  to_direct.TryToDirect(fail);
  to_direct.PointerToData(fail);
  return CAST(value);
}

}  // namespace internal
}  // namespace v8
