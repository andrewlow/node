// Copyright (c) 1994-2006 Sun Microsystems Inc.
// All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// - Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// - Redistribution in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the
// distribution.
//
// - Neither the name of Sun Microsystems or the names of contributors may
// be used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.

// The original source code covered by the above license above has been
// modified significantly by Google Inc.
// Copyright 2012 the V8 project authors. All rights reserved.

//
// Copyright IBM Corp. 2012-2014. All rights reserved.
//

// A light-weight S390 Assembler
// Generates user mode instructions for the S390 architecture

#ifndef V8_S390_ASSEMBLER_S390_H_
#define V8_S390_ASSEMBLER_S390_H_
#include <stdio.h>

#if V8_HOST_ARCH_S390 && V8_OS_LINUX
// elf.h include is required for auxv check for STFLE facility used
// for hardware detection, which is sensible only on s390 hosts.
#include <elf.h>
#endif

#include <fcntl.h>
#include <unistd.h>
#if V8_OS_ZOS
// xlC defines cds in stdlib.h.
#undef cds
#endif
#include "src/assembler.h"
#include "src/s390/constants-s390.h"
#include "src/serialize.h"

#define ABI_USES_FUNCTION_DESCRIPTORS V8_OS_ZOS

#define ABI_PASSES_HANDLES_IN_REGS 1

#define ABI_RETURNS_HANDLES_IN_REGS \
  (!V8_HOST_ARCH_S390 || V8_OS_ZOS || (V8_TARGET_LITTLE_ENDIAN))

#define ABI_RETURNS_OBJECT_PAIRS_IN_REGS \
  (!V8_HOST_ARCH_S390 || V8_OS_ZOS || (V8_TARGET_LITTLE_ENDIAN))
#define INSTR_AND_DATA_CACHE_COHERENCY LWSYNC
namespace v8 {
namespace internal {

// CPU Registers.
//
// 1) We would prefer to use an enum, but enum values are assignment-
// compatible with int, which has caused code-generation bugs.
//
// 2) We would prefer to use a class instead of a struct but we don't like
// the register initialization to depend on the particular initialization
// order (which appears to be different on OS X, Linux, and Windows for the
// installed versions of C++ we tried). Using a struct permits C-style
// "initialization". Also, the Register objects cannot be const as this
// forces initialization stubs in MSVC, making us dependent on initialization
// order.
//
// 3) By not using an enum, we are possibly preventing the compiler from
// doing certain constant folds, which may significantly reduce the
// code generated for some assembly instructions (because they boil down
// to a few constants). If this is a problem, we could change the code
// such that we use an enum in optimized mode, and the struct in debug
// mode. This way we get the compile-time error checking in debug mode
// and best performance in optimized code.

// Core register
struct Register {
  static const int kNumRegisters = 16;
  static const int kSizeInBytes = kPointerSize;

#if V8_TARGET_LITTLE_ENDIAN
  static const int kMantissaOffset = 0;
  static const int kExponentOffset = 4;
#else
  static const int kMantissaOffset = 4;
  static const int kExponentOffset = 0;
#endif

  static const int kAllocatableRangeBegin  = 2;
  static const int kAllocatableRangeEnd    = 9;
  static const int kAllocatableContext        = 13;  // cp
  static const int kNumAllocatable =
      kAllocatableRangeEnd - kAllocatableRangeBegin + 1;
  static const int kMaxNumAllocatableRegisters =
      kNumAllocatable + 1;  // cp
  static int NumAllocatableRegisters() { return kMaxNumAllocatableRegisters; }

  static int ToAllocationIndex(Register reg) {
    int index;
    int code = reg.code();
    if (code == kAllocatableContext) {
      // Context is the last index
      index = NumAllocatableRegisters() - 1;
    } else {
      // r0-r1 are skipped
      index = code - kAllocatableRangeBegin;
    }
    DCHECK(index >= 0 && index < kMaxNumAllocatableRegisters);
    return index;
  }

  static Register FromAllocationIndex(int index) {
    DCHECK(index >= 0 && index < kMaxNumAllocatableRegisters);
    return index == kMaxNumAllocatableRegisters - 1 ?
      from_code(kAllocatableContext) :  // Last index is always 'cp' register.
      from_code(index + kAllocatableRangeBegin);  // r0-r1 are skipped
  }

  static const char* AllocationIndexToString(int index) {
    DCHECK(index >= 0 && index < kMaxNumAllocatableRegisters);
    const char* const names[] = {
        "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "cp",
    };
    return names[index];
  }

  static Register from_code(int code) {
    Register r = { code };
    return r;
  }

  bool is_valid() const { return 0 <= code_ && code_ < kNumRegisters; }

  bool is(Register reg) const { return code_ == reg.code_; }
  int code() const {
    DCHECK(is_valid());
    return code_;
  }

  int bit() const {
    DCHECK(is_valid());
    return 1 << code_;
  }

  void set_code(int code) {
    code_ = code;
    DCHECK(is_valid());
  }

  // Unfortunately we can't make this private in a struct.
  int code_;
};

typedef struct Register Register;


// These constants are used in several locations, including static initializers
const int kRegister_no_reg_Code = -1;
const int kRegister_r0_Code  =  0;  // general scratch
const int kRegister_r1_Code  =  1;
const int kRegister_r2_Code  =  2;
const int kRegister_r3_Code  =  3;
const int kRegister_r4_Code  =  4;
const int kRegister_r5_Code  =  5;
const int kRegister_r6_Code  =  6;
const int kRegister_r7_Code  =  7;
const int kRegister_r8_Code  =  8;
const int kRegister_r9_Code  =  9;
const int kRegister_r10_Code  =  10;  // roots array pointer
const int kRegister_fp_Code  =  11;  // frame pointer
const int kRegister_r12_Code  =  12;  // ip (general scratch)
const int kRegister_r13_Code  =  13;
const int kRegister_r14_Code  =  14;
const int kRegister_sp_Code  =  15;  // stack pointer

const Register no_reg = { kRegister_no_reg_Code };

// Give alias names to registers
const Register cp = { kRegister_r13_Code };  // JavaScript context pointer
const Register kRootRegister = { kRegister_r10_Code };  // Roots array pointer.

const Register r0  = { kRegister_r0_Code };
// Lithium scratch register - defined in lithium-codegen-s390.h
const Register r1  = { kRegister_r1_Code };
const Register r2  = { kRegister_r2_Code };
const Register r3  = { kRegister_r3_Code };
const Register r4  = { kRegister_r4_Code };
const Register r5  = { kRegister_r5_Code };
const Register r6  = { kRegister_r6_Code };
const Register r7  = { kRegister_r7_Code };
const Register r8  = { kRegister_r8_Code };
const Register r9  = { kRegister_r9_Code };
// Used as roots register.
const Register r10 = { kRegister_r10_Code };
const Register fp  = { kRegister_fp_Code };
// IP - Intra procedural register
const Register ip  = { kRegister_r12_Code };
// CP - Context Register
const Register r13  = { kRegister_r13_Code };
const Register r14  = { kRegister_r14_Code };
const Register sp   = { kRegister_sp_Code };

// Double word FP register.
struct DoubleRegister {
  static const int kNumRegisters = 16;
  static const int kMaxNumRegisters = kNumRegisters;
#ifdef V8_TARGET_ARCH_S390X
  static const int kNumVolatileRegisters = 8;     // d0-d7
#else
  static const int kNumVolatileRegisters = 14;     // d0-d15 except d4 and d6
#endif
  static const int kAllocatableRangeBegin  = 1;
  static const int kAllocatableRangeEnd    = 12;
  static const int kNumAllocatable =
      kAllocatableRangeEnd - kAllocatableRangeBegin + 1;
  static const int kMaxNumAllocatableRegisters =
      kNumAllocatable;
  static int NumAllocatableRegisters() { return kMaxNumAllocatableRegisters; }
  static int ToAllocationIndex(DoubleRegister reg) {
    int code = reg.code();
    int index = code - kAllocatableRangeBegin;
    DCHECK(index < kMaxNumAllocatableRegisters);
    return index;
  }

  static DoubleRegister FromAllocationIndex(int index) {
    DCHECK(index >= 0 && index < kMaxNumAllocatableRegisters);
    return (from_code(index + kAllocatableRangeBegin));  // d0 is skipped
  }

  static const char* AllocationIndexToString(int index);
  static DoubleRegister from_code(int code) {
    DoubleRegister r = { code };
    return r;
  }

  bool is_valid() const { return 0 <= code_ && code_ < kNumRegisters; }
  bool is(DoubleRegister reg) const { return code_ == reg.code_; }

  int code() const {
    DCHECK(is_valid());
    return code_;
  }
  int bit() const {
    DCHECK(is_valid());
    return 1 << code_;
  }
  void split_code(int* vm, int* m) const {
    DCHECK(is_valid());
    *m = (code_ & 0x10) >> 4;
    *vm = code_ & 0x0F;
  }

  int code_;
};


typedef DoubleRegister DoubleRegister;

const DoubleRegister no_dreg = { -1 };
const DoubleRegister d0  = {  0 };
const DoubleRegister d1  = {  1 };
const DoubleRegister d2  = {  2 };
const DoubleRegister d3  = {  3 };
const DoubleRegister d4  = {  4 };
const DoubleRegister d5  = {  5 };
const DoubleRegister d6  = {  6 };
const DoubleRegister d7  = {  7 };
const DoubleRegister d8  = {  8 };
const DoubleRegister d9  = {  9 };
const DoubleRegister d10 = { 10 };
const DoubleRegister d11 = { 11 };
const DoubleRegister d12 = { 12 };
const DoubleRegister d13 = { 13 };
const DoubleRegister d14 = { 14 };
const DoubleRegister d15 = { 15 };

// Aliases for double registers.  Defined using #define instead of
// "static const DoubleRegister&" because Clang complains otherwise when a
// compilation unit that includes this header doesn't use the variables.
#define kDoubleRegZero d14
#define kScratchDoubleReg d13

Register ToRegister(int num);

// Coprocessor register
struct CRegister {
  bool is_valid() const { return 0 <= code_ && code_ < 16; }
  bool is(CRegister creg) const { return code_ == creg.code_; }
  int code() const {
    DCHECK(is_valid());
    return code_;
  }
  int bit() const {
    DCHECK(is_valid());
    return 1 << code_;
  }

  // Unfortunately we can't make this private in a struct.
  int code_;
};


const CRegister no_creg = { -1 };

const CRegister cr0  = {  0 };
const CRegister cr1  = {  1 };
const CRegister cr2  = {  2 };
const CRegister cr3  = {  3 };
const CRegister cr4  = {  4 };
const CRegister cr5  = {  5 };
const CRegister cr6  = {  6 };
const CRegister cr7  = {  7 };
const CRegister cr8  = {  8 };
const CRegister cr9  = {  9 };
const CRegister cr10 = { 10 };
const CRegister cr11 = { 11 };
const CRegister cr12 = { 12 };
const CRegister cr13 = { 13 };
const CRegister cr14 = { 14 };
const CRegister cr15 = { 15 };

// -----------------------------------------------------------------------------
// Machine instruction Operands

#if V8_TARGET_ARCH_S390X
const RelocInfo::Mode kRelocInfo_NONEPTR = RelocInfo::NONE64;
#else
const RelocInfo::Mode kRelocInfo_NONEPTR = RelocInfo::NONE32;
#endif

// Class Operand represents a shifter operand in data processing instructions
// defining immediate numbers and masks
typedef uint8_t Length;

struct Mask {
  uint8_t mask;
  uint8_t value() {return mask;}
  static Mask from_value(uint8_t input) {
    DCHECK(input <= 0x0F);
    Mask m = {input};
    return m;
  }
};

class Operand BASE_EMBEDDED {
 public:
  // immediate
  INLINE(explicit Operand(intptr_t immediate,
         RelocInfo::Mode rmode = kRelocInfo_NONEPTR));
  INLINE(static Operand Zero()) {
    return Operand(static_cast<intptr_t>(0));
  }
  INLINE(explicit Operand(const ExternalReference& f));
  explicit Operand(Handle<Object> handle);
  INLINE(explicit Operand(Smi* value));

  // rm
  INLINE(explicit Operand(Register rm));

  // Return true if this is a register operand.
  INLINE(bool is_reg() const);

  // For mov.  Return the number of actual instructions required to
  // load the operand into a register.  This can be anywhere from
  // one (constant pool small section) to five instructions (full
  // 64-bit sequence).
  //
  // The value returned is only valid as long as no entries are added to the
  // constant pool between this call and the actual instruction being emitted.
  bool must_output_reloc_info(const Assembler* assembler) const;

  inline intptr_t immediate() const {
    DCHECK(!rm_.is_valid());
    return imm_;
  }

  inline void setBits(int n) {
    imm_ = (static_cast<uint32_t>(imm_) << (32 - n)) >> (32 - n);
  }

  Register rm() const { return rm_; }

 private:
  Register rm_;
  intptr_t imm_;  // valid if rm_ == no_reg
  RelocInfo::Mode rmode_;

  friend class Assembler;
  friend class MacroAssembler;
};

typedef int32_t Disp;

// Class MemOperand represents a memory operand in load and store instructions
// On S390, we have various flavours of memory operands:
//   1) a base register + 16 bit unsigned displacement
//   2) a base register + index register + 16 bit unsigned displacement
//   3) a base register + index register + 20 bit signed displacement
class MemOperand BASE_EMBEDDED {
 public:
  explicit MemOperand(Register rx, Disp offset = 0);
  explicit MemOperand(Register rx, Register rb, Disp offset = 0);

  int32_t offset() const {
    return offset_;
  }
  uint32_t getDisplacement() const { return offset(); }

  // Base register
  Register rb() const {
    DCHECK(!baseRegister.is(no_reg));
    return baseRegister;
  }

  Register getBaseRegister() const { return rb(); }

  // Index Register
  Register rx() const {
    DCHECK(!indexRegister.is(no_reg));
    return indexRegister;
  }
  Register getIndexRegister() const { return rx(); }

 private:
  Register baseRegister;     // base
  int32_t offset_;  // offset
  Register indexRegister;     // index

  friend class Assembler;
};

class Assembler : public AssemblerBase {
 public:
  // Create an assembler. Instructions and relocation information are emitted
  // into a buffer, with the instructions starting from the beginning and the
  // relocation information starting from the end of the buffer. See CodeDesc
  // for a detailed comment on the layout (globals.h).
  //
  // If the provided buffer is NULL, the assembler allocates and grows its own
  // buffer, and buffer_size determines the initial buffer size. The buffer is
  // owned by the assembler and deallocated upon destruction of the assembler.
  //
  // If the provided buffer is not NULL, the assembler uses the provided buffer
  // for code generation and assumes its size to be buffer_size. If the buffer
  // is too small, a fatal error occurs. No deallocation of the buffer is done
  // upon destruction of the assembler.
  Assembler(Isolate* isolate, void* buffer, int buffer_size);
  virtual ~Assembler() { }

  // GetCode emits any pending (non-emitted) code and fills the descriptor
  // desc. GetCode() is idempotent; it returns the same result if no other
  // Assembler functions are invoked in between GetCode() calls.
  void GetCode(CodeDesc* desc);

  // Label operations & relative jumps (PPUM Appendix D)
  //
  // Takes a branch opcode (cc) and a label (L) and generates
  // either a backward branch or a forward branch and links it
  // to the label fixup chain. Usage:
  //
  // Label L;    // unbound label
  // j(cc, &L);  // forward branch to unbound label
  // bind(&L);   // bind label to the current pc
  // j(cc, &L);  // backward branch to bound label
  // bind(&L);   // illegal: a label may be bound only once
  //
  // Note: The same Label can be used for forward and backward branches
  // but it may be bound only once.

  void bind(Label* L);  // binds an unbound label L to the current code position
  // Determines if Label is bound and near enough so that a single
  // branch instruction can be used to reach it.
  bool is_near(Label* L, Condition cond);

  // Returns the branch offset to the given label from the current code position
  // Links the label to the current position if it is still unbound
  // Manages the jump elimination optimization if the second parameter is true.
  int branch_offset(Label* L, bool jump_elimination_allowed);

  // Puts a labels target address at the given position.
  // The high 8 bits are set to zero.
  void label_at_put(Label* L, int at_offset);
  void load_label_offset(Register r1, Label* L);

  // Read/Modify the code target address in the branch/call instruction at pc.
  INLINE(static Address target_address_at(Address pc, Address constant_pool));
  INLINE(static void set_target_address_at(Address pc,
                                           Address constant_pool,
                                           Address target,
                                           ICacheFlushMode icache_flush_mode =
                                               FLUSH_ICACHE_IF_NEEDED));
  INLINE(static Address target_address_at(Address pc, Code* code)) {
    Address constant_pool = code ? code->constant_pool() : NULL;
    return target_address_at(pc, constant_pool);
  }
  INLINE(static void set_target_address_at(Address pc,
                                           Code* code,
                                           Address target,
                                           ICacheFlushMode icache_flush_mode =
                                               FLUSH_ICACHE_IF_NEEDED)) {
    Address constant_pool = code ? code->constant_pool() : NULL;
    set_target_address_at(pc, constant_pool, target, icache_flush_mode);
  }

  // Return the code target address at a call site from the return address
  // of that call in the instruction stream.
  inline static Address target_address_from_return_address(Address pc);

  // Given the address of the beginning of a call, return the address
  // in the instruction stream that the call will return to.
  INLINE(static Address return_address_from_call_start(Address pc));

  // Return the code target address of the patch debug break slot
  INLINE(static Address break_address_from_return_address(Address pc));

  // This sets the branch destination.
  // This is for calls and branches within generated code.
  inline static void deserialization_set_special_target_at(
      Address instruction_payload, Code* code, Address target);

  // Size of an instruction.
  static const int kInstrSize = sizeof(Instr);

  // Here we are patching the address in the LUI/ORI instruction pair.
  // These values are used in the serialization process and must be zero for
  // PPC platform, as Code, Embedded Object or External-reference pointers
  // are split across two consecutive instructions and don't exist separately
  // in the code, so the serializer should not step forwards in memory after
  // a target is resolved and written.
  static const int kSpecialTargetSize = 0;

  // Number of bytes for instructions used to store pointer sized constant.
#if V8_TARGET_ARCH_S390X
  static const int kBytesForPtrConstant = 12;  // iihf + iilf
#else
  static const int kBytesForPtrConstant = 6;   // iilf
#endif

  // Distance between the instruction referring to the address of the call
  // target and the return address.

  // Call sequence is a FIXED_SEQUENCE:
  // iihf    r8, 2148      @ call address hi  // <64-bit only>
  // iilf    r8, 5728      @ call address lo
  // basr    r14, r8
  //                      @ return address
#if V8_TARGET_ARCH_S390X
  static const int kCallTargetAddressOffset = 14;
#else
  static const int kCallTargetAddressOffset = 8;
#endif

  // Distance between start of patched return sequence and the emitted address
  // to jump to.
  // Patched return sequence is a FIXED_SEQUENCE:
  //   mov r0, <address>
  //   mtlr r0
  //   blrl
  static const int kPatchReturnSequenceAddressOffset =  0 * kInstrSize;

  // Distance between start of patched debug break slot and the emitted address
  // to jump to.
  // Patched debug break slot code is a FIXED_SEQUENCE:
  //   mov r0, <address>
  //   mtlr r0
  //   blrl
  static const int kPatchDebugBreakSlotAddressOffset =  0 * kInstrSize;

  // This is the length of the BreakLocationIterator::SetDebugBreakAtReturn()
  // code patch FIXED_SEQUENCE in bytes!
#if V8_TARGET_ARCH_S390X
  static const int kJSReturnSequenceLength = 16;
#else
  static const int kJSReturnSequenceLength = 10;
#endif

  // This is the length of the code sequence from SetDebugBreakAtSlot()
  // FIXED_SEQUENCE in bytes!
#if V8_TARGET_ARCH_S390X
  static const int kDebugBreakSlotLength = 14;
#else
  static const int kDebugBreakSlotLength = 8;
#endif
  static const int kPatchDebugBreakSlotReturnOffset = kDebugBreakSlotLength;

  static inline int encode_crbit(const CRegister& cr, enum CRBit crbit) {
    return ((cr.code() * CRWIDTH) + crbit);
  }

  // ---------------------------------------------------------------------------
  // Code generation

  // Helper for unconditional branch to Label with update to save register
  void b(Register r, Label* l) {
    positions_recorder()->WriteRecordedPositions();
    int32_t halfwords = branch_offset(l, false) / 2;
    brasl(r, Operand(halfwords));
  }

  // Conditional Branch Instruction - Generates either BRC / BRCL
  void branchOnCond(Condition c, int branch_offset, bool is_bound = false);

  // Helpers for conditional branch to Label
  void b(Condition cond, Label* l, Label::Distance dist = Label::kFar) {
    branchOnCond(cond, branch_offset(l, false),
                 l->is_bound() || (dist == Label::kNear));
  }

  // Helpers for conditional branch to Label
  void beq(Label * l, Label::Distance dist = Label::kFar) { b(eq, l, dist); }
  void bne(Label * l, Label::Distance dist = Label::kFar) { b(ne, l, dist); }
  void blt(Label * l, Label::Distance dist = Label::kFar) { b(lt, l, dist); }
  void ble(Label * l, Label::Distance dist = Label::kFar) { b(le, l, dist); }
  void bgt(Label * l, Label::Distance dist = Label::kFar) { b(gt, l, dist); }
  void bge(Label * l, Label::Distance dist = Label::kFar) { b(ge, l, dist); }
  void b(Label * l, Label::Distance dist = Label::kFar)   { b(al, l, dist); }
  void jmp(Label * l, Label::Distance dist = Label::kFar) { b(al, l, dist); }
  void bunordered(Label* l, Label::Distance dist = Label::kFar) {
                                                     b(unordered, l, dist); }
  void bordered(Label* l, Label::Distance dist = Label::kFar) {
                                                       b(ordered, l, dist); }

  // Helpers for conditional indirect branch off register
  void b(Condition cond, Register r) { bcr(cond, r); }
  void beq(Register r) { b(eq, r); }
  void bne(Register r) { b(ne, r); }
  void blt(Register r) { b(lt, r); }
  void ble(Register r) { b(le, r); }
  void bgt(Register r) { b(gt, r); }
  void bge(Register r) { b(ge, r); }
  void b(Register r)   { b(al, r); }
  void b(Register r, Disp d) { bc(al, r0, r, d); }

  void jmp(Register r) { b(al, r); }
  void jmp(Register r, Disp d) { bc(al, r0, r, d); }
  void bunordered(Register r) { b(unordered, r); }
  void bordered(Register r)   { b(ordered, r);   }

  // S390 native instructions
  // Indirect Conditional Branch via register
  void bcr(Condition m, Register target);
  // ---------------------------------------------------------------------------
  // Code generation

  // Insert the smallest number of nop instructions
  // possible to align the pc offset to a multiple
  // of m. m must be a power of 2 (>= 4).
  void Align(int m);
  // Aligns code to something that's optimal for a jump target for the platform.
  void CodeTargetAlign();

  void breakpoint(bool do_print) {
    if (do_print) {
      printf("DebugBreak is inserted to %p\n", pc_);
    }
#if V8_HOST_ARCH_64_BIT
    int64_t value = reinterpret_cast<uint64_t>(&v8::base::OS::DebugBreak);
    int32_t hi_32 = static_cast<int64_t>(value) >> 32;
    int32_t lo_32 = static_cast<int32_t>(value);

    iihf(r1, Operand(hi_32));
    iilf(r1, Operand(lo_32));
#else
    iilf(r1, Operand(reinterpret_cast<uint32_t>(&v8::base::OS::DebugBreak)));
#endif
    basr(r14, r1);
  }

  // Load address relative long.
  void larl(Register r, Label *l) {
    larl(r, Operand(branch_offset(l, false)));
  }

  void mvc(const MemOperand& opnd1, const MemOperand& opnd2, uint32_t length);
  void asi(const MemOperand&, const Operand&);
  void agsi(const MemOperand&, const Operand&);

  // Data-processing instructions

  // S390 instruction generation
#define E_FORM(name)\
void name()

#define IE_FORM(name)\
void name(const Operand& i1, const Operand& i2)

#define I_FORM(name)\
void name(const Operand& i)

#define RR_FORM(name)\
void name(Register r1, Register r2)

#define RR2_FORM(name)\
void name(Condition m1, Register r2)

#define RX_FORM(name)\
void name(Register r1, Register x2, Register b2, \
                 Disp d2);\
void name(Register r1, const MemOperand& opnd)

#define RX_b_FORM(name)\
void name(Condition m1, Register x2, Register b2, \
                 Disp d2);\
void name(Condition m, const MemOperand& opnd)

#define RI1_FORM(name)\
void name(Register r,  const Operand& i)

#define RI2_FORM(name)\
void name(Condition m, const Operand& i)

#define RIE_FORM(name)\
void name(Register r1, Register R3, const Operand& i)

#define RIE_F_FORM(name)\
void name(Register r1, Register r2, const Operand &i3, \
                     const Operand& i4, const Operand& i5)

#define RIL1_FORM(name)\
void name(Register r1, const Operand& i2)

#define RIL2_FORM(name)\
void name(Condition m1, const Operand& i2)

#define RXE_FORM(name)\
void name(Register r1, const MemOperand& opnd);\
void name(Register r1, Register b2, Register x2, \
          Disp d2)

#define RXF_FORM(name)\
void name(Register r1, Register r3, const MemOperand& opnd);\
void name(Register r1, Register r3, Register b2, \
                 Register x2, Disp d2)

#define RXY_FORM(name)\
void name(Register r1, Register x2, Register b2, \
                 Disp d2);\
void name(Register r1, const MemOperand& opnd)

#define RSI_FORM(name)\
void name(Register r1, Register r3, const Operand& i)

#define RIS_FORM(name)\
void name(Register r1, Condition m3, Register b4, \
          Disp d4, const Operand& i2);\
void name(Register r1, const Operand& i2, Condition m3, \
          const MemOperand& opnd)

#define SI_FORM(name)\
void name(const MemOperand& opnd, const Operand& i);\
void name(const Operand& i2, Register b1, Disp d1)

#define SIL_FORM(name)\
void name(Register b1, Disp d1, const Operand& i2);\
void name(const MemOperand& opnd, const Operand& i2)

#define RRE_FORM(name)\
void name(Register r1, Register r2)

#define RRF1_FORM(name)\
void name(Register r1, Register r2, Register r3)

#define RRF2_FORM(name)\
void name(Condition m1, Register r1, Register r2)

#define RRF3_FORM(name)\
void name(Register r3, Condition m4, Register r1, Register r2)

#define RS1_FORM(name)\
void name(Register r1, Register r3, const MemOperand& opnd);\
void name(Register r1, Register r3, Register b2, Disp d2)

#define RS2_FORM(name)\
void name(Register r1, Condition m3, const MemOperand& opnd);\
void name(Register r1, Condition m3, Register b2, Disp d2)

#define RSE_FORM(name)\
void name(Register r1, Register r3, const MemOperand& opnd);\
void name(Register r1, Register r3, Register b2, Disp d2)

#define RSL_FORM(name)\
void name(Length l, Register b2, Disp d2);\
void name(const MemOperand& opnd)

#define RSY1_FORM(name)\
void name(Register r1, Register r3, Register b2, Disp d2);\
void name(Register r1, Register r3, const MemOperand& opnd)

#define RSY2_FORM(name)\
void name(Register r1, Condition m3, Register b2, Disp d2);\
void name(Register r1, Condition m3, const MemOperand& opnd)

#define RRD_FORM(name)\
void name(Register r1, Register r3, Register r2)

#define RRS_FORM(name)\
void name(Register r1, Register r2, Register b4, \
          Disp d4, Condition m3);\
void name(Register r1, Register r2, Condition m3, \
          const MemOperand& opnd)

#define S_FORM(name)\
void name(Register b2, Disp d2);\
void name(const MemOperand& opnd)

#define SIY_FORM(name)\
void name(const Operand& i2, Register b1, Disp d1);\
void name(const MemOperand& opnd, const Operand& i)

#define SS1_FORM(name)\
void name(Register b1, Disp d1, \
          Register b3, Disp d2, Length length);\
void name(const MemOperand& opnd1, const MemOperand& opnd2, Length length)

#define SS2_FORM(name)\
void name(const MemOperand& opnd1, const MemOperand& opnd2, \
          Length length1, Length length2);\
void name(Register b1, \
          Disp d1, Register b2, Disp d2, Length l1, Length l2)

#define SS3_FORM(name)\
void name(const MemOperand& opnd1, const MemOperand& opnd2, Length length);\
void name(const Operand& i3, Register b1, \
          Disp d1, Register b2, Disp d2, Length l1)

#define SS4_FORM(name)\
void name(const MemOperand& opnd1, const MemOperand& opnd2);\
void name(Register r1, Register r3, Register b1, \
          Disp d1, Register b2, Disp d2)

#define SS5_FORM(name)\
void name(const MemOperand& opnd1, const MemOperand& opnd2);\
void name(Register r1, Register r3, Register b3, \
          Disp d2, Register b4, Disp d4)

#define SSE_FORM(name)\
void name(Register b1, Disp d1, \
          Register b2, Disp d2);\
void name(const MemOperand& opnd1, const MemOperand& opnd2)

#define SSF_FORM(name)\
void name(Register r3, Register b1, Disp d1, \
          Register b2, Disp d2);\
void name(Register r3, const MemOperand& opnd1, const MemOperand& opnd2)

// S390 instruction sets
RX_FORM(a);
RXE_FORM(adb);
RRE_FORM(adbr);
RRF1_FORM(adtr);
RRF1_FORM(adtra);
RXE_FORM(aeb);
RRE_FORM(aebr);
RIL1_FORM(afi);
RXY_FORM(ag);
RXY_FORM(agf);
RIL1_FORM(agfi);
RRE_FORM(agfr);
RI1_FORM(aghi);
RRE_FORM(agr);
RX_FORM(ah);
RRF1_FORM(ahhhr);
RRF1_FORM(ahhlr);
RI1_FORM(ahi);
RXY_FORM(ahy);
RIL1_FORM(aih);
RX_FORM(al_z);
RXY_FORM(alc);
RXY_FORM(alcg);
RRE_FORM(alcgr);
RRE_FORM(alcr);
RIL1_FORM(alfi);
RXY_FORM(alg);
RXY_FORM(algf);
RIL1_FORM(algfi);
RRE_FORM(algfr);
RIE_FORM(alghsik);
RRE_FORM(algr);
SIY_FORM(algsi);
RRF1_FORM(alhhhr);
RRF1_FORM(alhhlr);
RIE_FORM(alhsik);
RR_FORM(alr);
SIY_FORM(alsi);
RIL1_FORM(alsih);
RIL1_FORM(alsihn);
RXY_FORM(aly);
SS2_FORM(ap);
RR_FORM(ar);
RRE_FORM(axbr);
RRF1_FORM(axtr);
RRF1_FORM(axtra);
RXY_FORM(ay);
RX_FORM(bal);
RR_FORM(balr);
RX_FORM(bas);
RR_FORM(basr);
RR_FORM(bassm);
RX_b_FORM(bc);
RX_FORM(bct);
RXY_FORM(bctg);
RRE_FORM(bctgr);
RR_FORM(bctr);
RI1_FORM(bras);
RIL1_FORM(brasl);
RI2_FORM(brc);
RIL2_FORM(brcl);
RIL1_FORM(brcth);
RSI_FORM(brxh);
RIE_FORM(brxhg);
RSI_FORM(brxle);
RIE_FORM(brxlg);
RR_FORM(bsm);
RS1_FORM(bxle);
RSY1_FORM(bxleg);
RX_FORM(c);
RX_FORM(cd);
RRE_FORM(cdr);
RXE_FORM(cdb);
RRF2_FORM(cdfbra);
RRE_FORM(cdftr);
RRF2_FORM(cdgbra);
RRE_FORM(cdgtr);
RRF2_FORM(cdgtra);
RRF2_FORM(cdlftr);
RRF2_FORM(cdlgtr);
RS1_FORM(cds);
RSY1_FORM(cdsg);
RRE_FORM(cdstr);
RSY1_FORM(cdsy);
RRE_FORM(cdtr);
RRE_FORM(cdutr);
RSL_FORM(cdzt);
RXE_FORM(ceb);
RRE_FORM(cebr);
RRE_FORM(cedtr);
RRE_FORM(cefbr);
RRF2_FORM(cefbra);
RRE_FORM(cegbr);
RRF2_FORM(cegbra);
RRF2_FORM(celfbr);
RRF2_FORM(celgbr);
RRE_FORM(cextr);
S_FORM(cfc);
RRF2_FORM(cfdbra);
RRF2_FORM(cfdr);
RRF2_FORM(cfdtr);
RRF2_FORM(cfebr);
RRF2_FORM(cfebra);
RRF2_FORM(cfer);
RIL1_FORM(cfi);
RRF2_FORM(cfxbr);
RRF2_FORM(cfxbra);
RRF2_FORM(cfxr);
RRF2_FORM(cfxtr);
RXY_FORM(cg);
RRF2_FORM(cgdbra);
RRF2_FORM(cgdr);
RRF2_FORM(cgdtr);
RRF2_FORM(cgdtra);
RRF2_FORM(cgebr);
RRF2_FORM(cgebra);
RRF2_FORM(cger);
RXY_FORM(cgf);
RIL1_FORM(cgfi);
RRE_FORM(cgfr);
RIL1_FORM(cgfrl);
RXY_FORM(cgh);
RI1_FORM(cghi);
RIL1_FORM(cghrl);
SIL_FORM(cghsi);
RIS_FORM(cgib);
RIE_FORM(cgij);
RIE_FORM(cgit);
RRS_FORM(cgrb);
RIE_FORM(cgrj);
RIL1_FORM(cgrl);
RRF2_FORM(cgrt);
RRF2_FORM(cgxbr);
RRF2_FORM(cgxbra);
RRF2_FORM(cgxr);
RRF2_FORM(cgxtr);
RRF2_FORM(cgxtra);
RX_FORM(ch);
RXY_FORM(chf);
RRE_FORM(chhr);
SIL_FORM(chhsi);
RI1_FORM(chi);
RRE_FORM(chlr);
RIL1_FORM(chrl);
SIL_FORM(chsi);
RXY_FORM(chy);
RIS_FORM(cib);
RIL1_FORM(cih);
RIE_FORM(cij);
RIE_FORM(cit);
RRE_FORM(cksm);
RX_FORM(cl);
SS1_FORM(clc);
RR_FORM(clcl);
RS1_FORM(clcle);
RSY1_FORM(clclu);
RRF2_FORM(clfebr);
SIL_FORM(clfhsi);
RIL1_FORM(clfi);
RIE_FORM(clfit);
RRF2_FORM(clfxbr);
RRF2_FORM(clfxtr);
RXY_FORM(clg);
RRF2_FORM(clgdtr);
RRF2_FORM(clgebr);
RXY_FORM(clgf);
RIL1_FORM(clgfi);
RXY_FORM(cly);
RR_FORM(cr_z);
SSF_FORM(csst);
RRF2_FORM(csxtr);
RSY1_FORM(csy);
RRF2_FORM(cu12);
RRF2_FORM(cu14);
RRF2_FORM(cu21);
RRF2_FORM(cu24);
RRE_FORM(cu41);
RRE_FORM(cu42);
RRE_FORM(cudtr);
RRE_FORM(cuse);
RRF2_FORM(cutfu);
RRF2_FORM(cuutf);
RRE_FORM(cuxtr);
RX_FORM(cvb);
RXY_FORM(cvbg);
RXY_FORM(cvby);
RX_FORM(cvd);
RXY_FORM(cvdg);
RXY_FORM(cvdy);
RRE_FORM(cxbr);
RRE_FORM(cxfbr);
RRF2_FORM(cxfbra);
RRE_FORM(cxftr);
RRE_FORM(cxgbr);
RRF2_FORM(cxgbra);
RRE_FORM(cxgtr);
RRF2_FORM(cxgtra);
RRF2_FORM(cxlfbr);
RRF2_FORM(cxlftr);
RRF2_FORM(cxlgbr);
RRF2_FORM(cxlgtr);
RRE_FORM(cxstr);
RRE_FORM(cxtr);
RRE_FORM(cxutr);
RSL_FORM(cxzt);
RXY_FORM(cy);
RSL_FORM(czdt);
RSL_FORM(czxt);
RX_FORM(d);
RXE_FORM(ddb);
RRE_FORM(ddbr);
RRF1_FORM(ddtr);
RRF1_FORM(ddtra);
RXE_FORM(deb);
RRE_FORM(debr);
RRF1_FORM(didbr);
RRF1_FORM(diebr);
RXY_FORM(dl);
RXY_FORM(dlg);
RRE_FORM(dlgr);
RRE_FORM(dlr);
SS2_FORM(dp);
RR_FORM(dr);
RXY_FORM(dsg);
RXY_FORM(dsgf);
RRE_FORM(dsgfr);
RRE_FORM(dsgr);
RRE_FORM(dxbr);
RRF1_FORM(dxtr);
RRF1_FORM(dxtra);
RRE_FORM(ear);
RSY1_FORM(ecag);
SSF_FORM(ectg);
SS1_FORM(ed);
SS1_FORM(edmk);
RRE_FORM(eedtr);
RRE_FORM(eextr);
RRE_FORM(efpc);
RRE_FORM(epair);
RRE_FORM(epsw);
RRE_FORM(esdtr);
RRE_FORM(esxtr);
RRE_FORM(etnd);
RX_FORM(ex);
RIL1_FORM(exrl);
RRF2_FORM(fidbr);
RRF2_FORM(fidbra);
RRF2_FORM(fidtr);
RRF2_FORM(fiebr);
RRF2_FORM(fiebra);
RRF2_FORM(fixbr);
RRF2_FORM(fixbra);
RRF2_FORM(fixtr);
RRE_FORM(flogr);
S_FORM(hsch);
RX_FORM(ic_z);
RS2_FORM(icm);
RSY2_FORM(icmh);
RSY2_FORM(icmy);
RXY_FORM(icy);
RRF1_FORM(iedtr);
RRF1_FORM(iextr);
RIL1_FORM(iihf);
RI1_FORM(iihh);
RI1_FORM(iihl);
RIL1_FORM(iilf);
RI1_FORM(iilh);
RI1_FORM(iill);
RRE_FORM(ipm);
RXE_FORM(kdb);
RRE_FORM(kdbr);
RRE_FORM(kdtr);
RXE_FORM(keb);
RRE_FORM(kebr);
RRE_FORM(kimd);
RRE_FORM(klmd);
RRE_FORM(km);
RRE_FORM(kmac);
RRE_FORM(kmc);
RRF1_FORM(kmctr);
RRE_FORM(kmf);
RRE_FORM(kmo);
RRE_FORM(kxbr);
RRE_FORM(kxtr);
RX_FORM(l);
RSY1_FORM(laa);
RSY1_FORM(laag);
RSY1_FORM(laal);
RSY1_FORM(laalg);
RX_FORM(lae);
RXY_FORM(laey);
RSY1_FORM(lan);
RSY1_FORM(lang);
RSY1_FORM(lao);
RSY1_FORM(laog);
RIL1_FORM(larl);
RXY_FORM(lat);
RSY1_FORM(lax);
RSY1_FORM(laxg);
RXY_FORM(lb);
RXY_FORM(lbh);
RRE_FORM(lbr);
RRE_FORM(lcdfr);
RRE_FORM(lcebr);
RRE_FORM(lcgfr);
RRE_FORM(lcgr);
RR_FORM(lcr);
RRE_FORM(lcxbr);
RRF2_FORM(ldetr);
RRE_FORM(ldxbr);
RRF2_FORM(ldxbra);
RRF2_FORM(ldxtr);
RX_FORM(le_z);
RRF2_FORM(ledbra);
RRF2_FORM(ledtr);
RR_FORM(ler);
RRE_FORM(lexbr);
RRF2_FORM(lexbra);
RXY_FORM(ley);
S_FORM(lfas);
RXY_FORM(lfh);
RXY_FORM(lfhat);
S_FORM(lfpc);
RXY_FORM(lg);
RXY_FORM(lgat);
RXY_FORM(lgb);
RRE_FORM(lgbr);
RXY_FORM(lgf);
RIL1_FORM(lgfi);
RRE_FORM(lgfr);
RIL1_FORM(lgfrl);
RXY_FORM(lgh);
RI1_FORM(lghi);
RRE_FORM(lghr);
RIL1_FORM(lghrl);
RRE_FORM(lgr);
RIL1_FORM(lgrl);
RX_FORM(lh);
RXY_FORM(lhh);
RRE_FORM(lhr);
RIL1_FORM(lhrl);
RXY_FORM(lhy);
RXY_FORM(llch);
RRE_FORM(llcr);
RRE_FORM(llgcr);
RXY_FORM(llgf);
RXY_FORM(llgfat);
RRE_FORM(llgfr);
RIL1_FORM(llgfrl);
RXY_FORM(llgh);
RRE_FORM(llghr);
RIL1_FORM(llghrl);
RXY_FORM(llgt);
RXY_FORM(llgtat);
RRE_FORM(llgtr);
RXY_FORM(llh);
RXY_FORM(llhh);
RRE_FORM(llhr);
RIL1_FORM(llhrl);
RIL1_FORM(llihf);
RI1_FORM(llihh);
RI1_FORM(llihl);
RIL1_FORM(llilf);
RI1_FORM(llilh);
RI1_FORM(llill);
SS5_FORM(lmd);
RSY1_FORM(lmh);
RRE_FORM(lndfr);
RRE_FORM(lnebr);
RRE_FORM(lngfr);
RRE_FORM(lngr);
RR_FORM(lnr);
RRE_FORM(lnxbr);
RSY1_FORM(loc);
RSY1_FORM(locg);
RRF2_FORM(locgr);
RRF2_FORM(locr);
SSF_FORM(lpd);
RRE_FORM(lpdfr);
SSF_FORM(lpdg);
RRE_FORM(lpebr);
RRE_FORM(lpgfr);
RRE_FORM(lpgr);
RXY_FORM(lpq);
RR_FORM(lpr);
RRE_FORM(lpxbr);
RR_FORM(lr);
RIL1_FORM(lrl);
RXY_FORM(lrv);
RXY_FORM(lrvg);
RRE_FORM(lrvgr);
RXY_FORM(lrvh);
RRE_FORM(lrvr);
RRE_FORM(ltdbr);
RRE_FORM(ltdtr);
RRE_FORM(ltebr);
RXY_FORM(ltgf);
RRE_FORM(ltgfr);
RRE_FORM(ltgr);
RR_FORM(ltr);
RRE_FORM(ltxbr);
RRE_FORM(ltxtr);
RXE_FORM(lxdb);
RRE_FORM(lxdbr);
RRF2_FORM(lxdtr);
RXE_FORM(lxeb);
RRE_FORM(lxebr);
RRE_FORM(lxr);
RXY_FORM(ly);
RRE_FORM(lzer);
RRE_FORM(lzxr);
RX_FORM(m);
RXF_FORM(madb);
RXF_FORM(maeb);
RRD_FORM(maebr);
SI_FORM(mc);
RXE_FORM(mdb);
RRE_FORM(mdbr);
RXE_FORM(mdeb);
RRE_FORM(mdebr);
RRF1_FORM(mdtr);
RRF1_FORM(mdtra);
RXE_FORM(meeb);
RRE_FORM(meebr);
RXY_FORM(mfy);
RI1_FORM(mghi);
RX_FORM(mh);
RI1_FORM(mhi);
RXY_FORM(mhy);
RXY_FORM(ml);
RXY_FORM(mlg);
RRE_FORM(mlgr);
RRE_FORM(mlr);
SS2_FORM(mp);
RR_FORM(mr_z);
RX_FORM(ms);
S_FORM(msch);
RXF_FORM(msdb);
RXF_FORM(mseb);
RRD_FORM(msebr);
RIL1_FORM(msfi);
RXY_FORM(msg);
RXY_FORM(msgf);
RIL1_FORM(msgfi);
RRE_FORM(msgfr);
RRE_FORM(msgr);
RRE_FORM(msr);
RXY_FORM(msy);
SS1_FORM(mvcin);
SS4_FORM(mvck);
RR_FORM(mvcl);
RS1_FORM(mvcle);
RSY1_FORM(mvclu);
SSF_FORM(mvcos);
SS4_FORM(mvcp);
SSE_FORM(mvcdk);
SS4_FORM(mvcs);
SIL_FORM(mvhhi);
SI_FORM(mvi);
SIY_FORM(mviy);
SS1_FORM(mvn);
SS2_FORM(mvo);
RRE_FORM(mvst);
SS1_FORM(mvz);
RRE_FORM(mxbr);
RXE_FORM(mxdb);
RRE_FORM(mxdbr);
RRF1_FORM(mxtr);
RRF1_FORM(mxtra);
RX_FORM(n);
SS1_FORM(nc);
RXY_FORM(ng);
RRE_FORM(ngr);
SI_FORM(ni);
IE_FORM(niai);
RIL1_FORM(nihf);
RI1_FORM(nihh);
RI1_FORM(nihl);
RIL1_FORM(nilf);
RI1_FORM(nilh);
RI1_FORM(nill);
SIY_FORM(niy);
RR_FORM(nr);
RXY_FORM(ntstg);
RXY_FORM(ny);
RX_FORM(o);
SS1_FORM(oc);
RXY_FORM(og);
RRE_FORM(ogr);
SI_FORM(oi);
RIL1_FORM(oihf);
RI1_FORM(oihh);
RI1_FORM(oihl);
RIL1_FORM(oilf);
RI1_FORM(oilh);
RI1_FORM(oill);
SIY_FORM(oiy);
RR_FORM(or_z);
RXY_FORM(oy);
SS2_FORM(pack);
RRE_FORM(pcc);
RXY_FORM(pfd);
RIL2_FORM(pfdrl);
E_FORM(pfpo);
SS1_FORM(pka);
SS1_FORM(pku);
SS5_FORM(plo);
RRE_FORM(popcnt);
RRF1_FORM(ppa);
RRF1_FORM(qadtr);
RRF1_FORM(qaxtr);
S_FORM(rchp);
RIE_FORM(risbhg);
RIE_FORM(risblg);
// RSY1_FORM(rll);
// RSY1_FORM(rllg);
RIE_FORM(rnsbg);
RIE_FORM(rosbg);
S_FORM(rp);
RRF1_FORM(rrdtr);
RRF1_FORM(rrxtr);
S_FORM(rsch);
RIE_FORM(rxsbg);
RX_FORM(s);
S_FORM(sal);
RRE_FORM(sar);
S_FORM(schm);
RXE_FORM(sdb);
RRE_FORM(sdbr);
RRF1_FORM(sdtr);
RRF1_FORM(sdtra);
RXE_FORM(seb);
RRE_FORM(sebr);
RRE_FORM(sfasr);
RRE_FORM(sfpc);
RXY_FORM(sg);
RXY_FORM(sgf);
RRE_FORM(sgfr);
RRE_FORM(sgr);
RX_FORM(sh);
RRF1_FORM(shhhr);
RRF1_FORM(shhlr);
RXY_FORM(shy);
RX_FORM(sl);
RXY_FORM(slb);
RXY_FORM(slbg);
RRE_FORM(slbgr);
RRE_FORM(slbr);
RS1_FORM(slda);
RS1_FORM(sldl);
RXF_FORM(sldt);
RIL1_FORM(slfi);
RXY_FORM(slg);
RXY_FORM(slgf);
RIL1_FORM(slgfi);
RRE_FORM(slgfr);
RRE_FORM(slgr);
RRF1_FORM(slhhhr);
RRF1_FORM(slhhlr);
RR_FORM(slr);
RXF_FORM(slxt);
RXY_FORM(sly);
SS2_FORM(sp_z);
RR_FORM(spm);
RXE_FORM(sqeb);
RRE_FORM(sqebr);
RRE_FORM(sqxbr);
RR_FORM(sr);
RS1_FORM(srdl);
RXF_FORM(srdt);
S_FORM(srnm);
S_FORM(srnmb);
S_FORM(srnmt);
SS3_FORM(srp);
RRE_FORM(srst);
RRE_FORM(srstu);
RXF_FORM(srxt);
S_FORM(ssch);
RXY_FORM(stch);
S_FORM(stck);
S_FORM(stcke);
S_FORM(stckf);
RS2_FORM(stcm);
RSY2_FORM(stcmh);
RSY2_FORM(stcmy);
S_FORM(stcps);
S_FORM(stcrw);
RX_FORM(ste);
RXY_FORM(stey);
RXY_FORM(stfh);
S_FORM(stfle);
S_FORM(stfpc);
RIL1_FORM(stgrl);
RXY_FORM(sthh);
RIL1_FORM(sthrl);
RSY1_FORM(stmh);
RSY2_FORM(stoc);
RSY2_FORM(stocg);
RXY_FORM(stpq);
RIL1_FORM(strl);
RXY_FORM(strv);
RXY_FORM(strvg);
RXY_FORM(strvh);
S_FORM(stsch);
I_FORM(svc);
RRE_FORM(sxbr);
RRF1_FORM(sxtr);
RRF1_FORM(sxtra);
RXY_FORM(sy);
S_FORM(tabort);
RRF2_FORM(tbdr);
RRF2_FORM(tbedr);
SIL_FORM(tbegin);
SIL_FORM(tbeginc);
RXE_FORM(tcdb);
RXE_FORM(tceb);
RXE_FORM(tcxb);
RXE_FORM(tdcdt);
RXE_FORM(tdcet);
RXE_FORM(tdcxt);
RXE_FORM(tdgdt);
RXE_FORM(tdget);
RXE_FORM(tdgxt);
S_FORM(tend);
RRE_FORM(thder);
RRE_FORM(thdr);
RI1_FORM(tmh);
RI1_FORM(tmhh);
RI1_FORM(tmhl);
RI1_FORM(tml);
RI1_FORM(tmlh);
RI1_FORM(tmll);
RSL_FORM(tp);
S_FORM(tpi);
SS1_FORM(tr);
RRE_FORM(tre);
RRF2_FORM(troo);
RRF2_FORM(trot);
SS1_FORM(trt);
RRF2_FORM(trte);
RRF2_FORM(trto);
SS1_FORM(trtr);
RRF2_FORM(trtre);
RRF2_FORM(trtt);
S_FORM(ts);
S_FORM(tsch);
SS2_FORM(unpk);
SS1_FORM(unpka);
SS1_FORM(unpku);
E_FORM(upt);
RX_FORM(x);
SS1_FORM(xc);
RXY_FORM(xg);
RRE_FORM(xgr);
SI_FORM(xi);
RIL1_FORM(xihf);
RIL1_FORM(xilf);
SIY_FORM(xiy);
RR_FORM(xr);
S_FORM(xsch);
RXY_FORM(xy);
SS2_FORM(zap);


  // Load Address Instructions
  void la(Register r1, const MemOperand& src);
  void lay(Register r1, const MemOperand& src);

  // Load Instructions
  void lt_z(Register r1, const MemOperand& src);
  void ltg(Register r1, const MemOperand& src);

  // Load Logical Byte Instructions (aka. chars)
  void llc(Register r1, const MemOperand& src);
  void llgc(Register r1, const MemOperand& src);

  // Load Multiple Instructions
  void lm(Register r1, Register r2, const MemOperand& src);
  void lmy(Register r1, Register r2, const MemOperand& src);
  void lmg(Register r1, Register r2, const MemOperand& src);

  // Store Instructions
  void stm(Register r1, Register r2, const MemOperand& src);
  void stmy(Register r1, Register r2, const MemOperand& src);
  void stmg(Register r1, Register r2, const MemOperand& src);
  void st(Register dst, const MemOperand& src);
  void sty(Register dst, const MemOperand& src);
  void sth(Register dst, const MemOperand& src);
  void sthy(Register dst, const MemOperand& src);
  void stc(Register dst, const MemOperand& src);
  void stcy(Register dst, const MemOperand& src);

  // Compare Instructions
  void cr(Register r1, Register r2);
  void cgr(Register r1, Register r2);
  void clr(Register r1, Register r2);
  void clgr(Register r1, Register r2);
  void cli(const MemOperand& mem, const Operand& imm);
  void cliy(const MemOperand& mem, const Operand& imm);

  // Test Under Mask Instructions
  void tm(const MemOperand& mem, const Operand& imm);
  void tmy(const MemOperand& mem, const Operand& imm);

  // Rotate Instruction
  void rll(Register r1, Register r3, Register opnd);
  void rll(Register r1, Register r3, const Operand& opnd);
  void rll(Register r1, Register r3, Register r2, const Operand& opnd);
  void rllg(Register r1, Register r3, const Operand& opnd);
  void rllg(Register r1, Register r3, const Register opnd);
  void rllg(Register r1, Register r3, Register r2, const Operand& opnd);

  // Shift Instruction (32)
  void sll(Register r1, Register opnd);
  void sll(Register r1, const Operand& opnd);
  void sllk(Register r1, Register r3, Register opnd);
  void sllk(Register r1, Register r3, const Operand& opnd);
  void srl(Register r1, Register opnd);
  void srl(Register r1, const Operand& opnd);
  void srlk(Register r1, Register r3, Register opnd);
  void srlk(Register r1, Register r3, const Operand& opnd);
  void sra(Register r1, Register opnd);
  void sra(Register r1, const Operand& opnd);
  void srak(Register r1, Register r3, Register opnd);
  void srak(Register r1, Register r3, const Operand& opnd);
  void sla(Register r1, Register opnd);
  void sla(Register r1, const Operand& opnd);
  void slak(Register r1, Register r3, Register opnd);
  void slak(Register r1, Register r3, const Operand& opnd);

  // Data-processing instructions

  void sub(Register dst, Register src1, Register src2,
           OEBit s = LeaveOE, RCBit r = LeaveRC);

  // Shift Instructions (64)
  void sllg(Register r1, Register r3, const Operand& opnd);
  void sllg(Register r1, Register r3, const Register opnd);
  void srlg(Register r1, Register r3, const Operand& opnd);
  void srlg(Register r1, Register r3, const Register opnd);
  void srag(Register r1, Register r3, const Operand& opnd);
  void srag(Register r1, Register r3, const Register opnd);
  void srda(Register r1, const Operand& opnd);
  void slag(Register r1, Register r3, const Operand& opnd);
  void slag(Register r1, Register r3, const Register opnd);

  // Rotate and Insert Selected Bits
  void risbg(Register dst, Register src, const Operand& startBit,
             const Operand& endBit, const Operand& shiftAmt,
             bool zeroBits = true);
  void risbgn(Register dst, Register src, const Operand& startBit,
              const Operand& endBit, const Operand& shiftAmt,
              bool zeroBits = true);

  // Arithmetic Instructions
  void ahik(Register r1, Register r3, const Operand& opnd);
  void ark(Register r1, Register r2, Register r3);
  void alrk(Register r1, Register r2, Register r3);
  void aghik(Register r1, Register r3, const Operand& opnd);
  void agrk(Register r1, Register r2, Register r3);
  void algrk(Register r1, Register r2, Register r3);
  void srk(Register r1, Register r2, Register r3);
  void slrk(Register r1, Register r2, Register r3);
  void sgrk(Register r1, Register r2, Register r3);
  void slgrk(Register r1, Register r2, Register r3);

  // Bitwise Instructions
  void nrk(Register r1, Register r2, Register r3);
  void ngrk(Register r1, Register r2, Register r3);
  void ork(Register r1, Register r2, Register r3);
  void ogrk(Register r1, Register r2, Register r3);
  void xrk(Register r1, Register r2, Register r3);
  void xgrk(Register r1, Register r2, Register r3);


  // GPR <-> FPR conversions
  void lgdr(Register r1, DoubleRegister f2);
  void ldgr(DoubleRegister f1, Register r2);

  // floating point instructions
  void ld(DoubleRegister r1, const MemOperand& opnd);
  void ldy(DoubleRegister r1, const MemOperand& opnd);
  void ldr(DoubleRegister r1, DoubleRegister r2);
  void std(DoubleRegister r1, const MemOperand& opnd);
  void stdy(DoubleRegister r1, const MemOperand& opnd);
  void ste(DoubleRegister r1, const MemOperand& opnd);
  void stey(DoubleRegister r1, const MemOperand& opnd);

  void ledbr(DoubleRegister r1, DoubleRegister r2);
  void ldebr(DoubleRegister r1, DoubleRegister r2);
  void lpdbr(DoubleRegister r1,  DoubleRegister r2);
  // double type conversion
  void cfdbr(Condition m, Register fixReg, DoubleRegister fltReg);
  void cdfbr(DoubleRegister fltReg, Register fixReg);
  void cgdbr(Condition m, Register fixReg, DoubleRegister fltReg);
  void cdgbr(DoubleRegister fltReg, Register fixReg);
  void cdlfbr(Condition m3, Condition m4,
              DoubleRegister fltReg, Register fixReg);
  void cdlgbr(Condition m3, Condition m4,
              DoubleRegister fltReg, Register fixReg);
  void clfdbr(Condition m3, Condition m4,
              Register fixReg, DoubleRegister fltReg);
  void clgdbr(Condition m3, Condition m4,
              Register fixReg, DoubleRegister fltReg);

  // float type conversion
  void cfebr(Register fixReg, DoubleRegister fltReg);
  void cefbr(DoubleRegister fltReg, Register fixReg);

  void cdb(DoubleRegister r1, const MemOperand& opnd);
  void cdbr(DoubleRegister r1, DoubleRegister r2);
  void adb(DoubleRegister r1, const MemOperand& opnd);
  void adbr(DoubleRegister r1, DoubleRegister r2);
  void lzdr(DoubleRegister r1);
  void sdb(DoubleRegister r1, const MemOperand& opnd);
  void sdbr(DoubleRegister r1, DoubleRegister r2);
  void mdb(DoubleRegister r1, const MemOperand& opnd);
  void mdbr(DoubleRegister r1, DoubleRegister r2);
  void ddb(DoubleRegister r1, const MemOperand& opnd);
  void ddbr(DoubleRegister r1, DoubleRegister r2);
  void madbr(DoubleRegister r1, DoubleRegister r2, DoubleRegister r3);
  void msdbr(DoubleRegister r1, DoubleRegister r2, DoubleRegister r3);

  void sqdb(DoubleRegister r1, const MemOperand& opnd);
  void sqdbr(DoubleRegister r1, DoubleRegister r2);
  void lcdbr(DoubleRegister r1, DoubleRegister r2);
  void ldeb(DoubleRegister r1, const MemOperand& opnd);

  // Branch Instructions
  void brct(Register r1, const Operand& opnd);
  void brctg(Register r1, const Operand& opnd);

  // Move integer
  void mvhi(const MemOperand& opnd1, const Operand& i2);
  void mvghi(const MemOperand& opnd1, const Operand& i2);

  void lhi(Register dst, const Operand& imm);

  void stg(Register rs, const MemOperand &src);

  // Exception-generating instructions and debugging support
  void stop(const char* msg,
            Condition cond = al,
            int32_t code = kDefaultStopCode,
            CRegister cr = cr7);

  void bkpt(uint32_t imm16);  // v5 and above

  // Different nop operations are used by the code generator to detect certain
  // states of the generated code.
  enum NopMarkerTypes {
    NON_MARKING_NOP = 0,
    GROUP_ENDING_NOP,
    DEBUG_BREAK_NOP,
    BASR_CALL_TYPE_NOP,
    BRAS_CALL_TYPE_NOP,
    // IC markers.
    PROPERTY_ACCESS_INLINED,
    PROPERTY_ACCESS_INLINED_CONTEXT,
    PROPERTY_ACCESS_INLINED_CONTEXT_DONT_DELETE,
    // Helper values.
    LAST_CODE_MARKER,
    FIRST_IC_MARKER = PROPERTY_ACCESS_INLINED
  };

  void nop(int type = 0);   // 0 is the default non-marking type.

  // Check the code size generated from label to here.
  int SizeOfCodeGeneratedSince(Label* label) {
    return pc_offset() - label->pos();
  }

  // Class for scoping postponing the trampoline pool generation.
  class BlockTrampolinePoolScope {
   public:
    explicit BlockTrampolinePoolScope(Assembler* assem) : assem_(assem) {
      assem_->StartBlockTrampolinePool();
    }
    ~BlockTrampolinePoolScope() {
      assem_->EndBlockTrampolinePool();
    }

   private:
    Assembler* assem_;

    DISALLOW_IMPLICIT_CONSTRUCTORS(BlockTrampolinePoolScope);
  };

  // Debugging

  // Mark address of the ExitJSFrame code.
  void RecordJSReturn();

  // Mark address of a debug break slot.
  void RecordDebugBreakSlot();

  // Record the AST id of the CallIC being compiled, so that it can be placed
  // in the relocation information.
  void SetRecordedAstId(TypeFeedbackId ast_id) {
// PPC - this shouldn't be failing roohack   DCHECK(recorded_ast_id_.IsNone());
    recorded_ast_id_ = ast_id;
  }

  TypeFeedbackId RecordedAstId() {
    // roohack - another issue??? DCHECK(!recorded_ast_id_.IsNone());
    return recorded_ast_id_;
  }

  void ClearRecordedAstId() { recorded_ast_id_ = TypeFeedbackId::None(); }

  // Record a comment relocation entry that can be used by a disassembler.
  // Use --code-comments to enable.
  void RecordComment(const char* msg);

  // Writes a single byte or word of data in the code stream.  Used
  // for inline tables, e.g., jump-tables.
  void db(uint8_t data);
  void dd(uint32_t data);
  void emit_ptr(uintptr_t data);


  PositionsRecorder* positions_recorder() { return &positions_recorder_; }

  // Read/patch instructions
  SixByteInstr instr_at(int pos) {
    return Instruction::InstructionBits(buffer_ + pos);
  }
  template<typename T>
  void instr_at_put(int pos, T instr) {
    Instruction::SetInstructionBits<T>(buffer_ + pos, instr);
  }

  // Decodes instruction at pos, and returns its length
  int32_t instr_length_at(int pos) {
    return Instruction::InstructionLength(buffer_ + pos);
  }

  static SixByteInstr instr_at(byte* pc) {
    return Instruction::InstructionBits(pc);
  }

  static Condition GetCondition(Instr instr);

  static bool IsBranch(Instr instr);
  static Register GetRA(Instr instr);
  static Register GetRB(Instr instr);
#if V8_TARGET_ARCH_S390X
  static bool Is64BitLoadIntoIP(SixByteInstr instr1, SixByteInstr instr2);
#else
  static bool Is32BitLoadIntoIP(SixByteInstr instr);
#endif


  static bool IsCmpRegister(Address addr);
  static bool IsCmpImmediate(Instr instr);
  static bool IsRlwinm(Instr instr);
  static bool IsNop(SixByteInstr instr, int type = NON_MARKING_NOP);

  // Postpone the generation of the trampoline pool for the specified number of
  // instructions.
  void CheckTrampolinePool();

  int instructions_required_for_mov(const Operand& x) const;
  // Allocate a constant pool of the correct size for the generated code.
  Handle<ConstantPoolArray> NewConstantPool(Isolate* isolate);

  // Generate the constant pool for the generated code.
  void PopulateConstantPool(ConstantPoolArray* constant_pool);

#ifdef V8_OS_ZOS
  // Generate function descirptor for z/OS.
  void function_descriptor();

  static void RelocateInternalReference(Address pc, intptr_t delta,
                                        Address code_start,
                                        ICacheFlushMode icache_flush_mode =
                                            FLUSH_ICACHE_IF_NEEDED);

  static int DecodeInternalReference(Vector<char> buffer, Address pc);
#endif

 public:
  byte* buffer_pos() const { return buffer_; }

 protected:
  // Relocation for a type-recording IC has the AST id added to it.  This
  // member variable is a way to pass the information from the call site to
  // the relocation info.
  TypeFeedbackId recorded_ast_id_;

  int buffer_space() const { return reloc_info_writer.pos() - pc_; }

  // Decode branch instruction at pos and return branch target pos
  int target_at(int pos);

  // Patch branch instruction at pos to branch to given branch target pos
  void target_at_put(int pos, int target_pos);

  // Record reloc info for current pc_
  void RecordRelocInfo(RelocInfo::Mode rmode, intptr_t data = 0);
  void RecordRelocInfo(const RelocInfo& rinfo);

  // Block the emission of the trampoline pool before pc_offset.
  void BlockTrampolinePoolBefore(int pc_offset) {
    if (no_trampoline_pool_before_ < pc_offset)
      no_trampoline_pool_before_ = pc_offset;
  }

  void StartBlockTrampolinePool() {
    trampoline_pool_blocked_nesting_++;
  }

  void EndBlockTrampolinePool() {
    trampoline_pool_blocked_nesting_--;
  }

  bool is_trampoline_pool_blocked() const {
    return trampoline_pool_blocked_nesting_ > 0;
  }

  bool has_exception() const {
    return internal_trampoline_exception_;
  }

  bool is_trampoline_emitted() const {
    return trampoline_emitted_;
  }


 private:
  // Code generation
  // The relocation writer's position is at least kGap bytes below the end of
  // the generated instructions. This is so that multi-instruction sequences do
  // not have to check for overflow. The same is true for writes of large
  // relocation info entries.
  static const int kGap = 32;

  // Repeated checking whether the trampoline pool should be emitted is rather
  // expensive. By default we only check again once a number of instructions
  // has been generated.
  int next_buffer_check_;  // pc offset of next buffer check.

  // Emission of the trampoline pool may be blocked in some code sequences.
  int trampoline_pool_blocked_nesting_;  // Block emission if this is not zero.
  int no_trampoline_pool_before_;  // Block emission before this pc offset.

  // Relocation info generation
  // Each relocation is encoded as a variable size value
  static const int kMaxRelocSize = RelocInfoWriter::kMaxSize;
  RelocInfoWriter reloc_info_writer;

  // The bound position, before this we cannot do instruction elimination.
  int last_bound_pos_;

  // Code emission
  inline void CheckBuffer();
  void GrowBuffer();

  // S390 emitting helpers
  inline void emit2bytes(uint16_t x);
  inline void emit4bytes(uint32_t x);
  inline void emit6bytes(uint64_t x);

  inline void i_form(Opcode op, const Operand& i);
  inline void e_form(Opcode op);
  inline void ie_form(Opcode op, const Operand& i1, const Operand& i2);
  inline void rr_form(Opcode op, Register r1, Register r2);
  inline void rr_form(Opcode op, DoubleRegister r1, DoubleRegister r2);
  inline void rr_form(Opcode op, Condition m1, Register r2);

  inline void rr2_form(uint8_t op, Condition m1, Register r2);
  inline void rx_form(Opcode op,
                     Register r1,
                     Register x2,
                     Register b2,
                     Disp d2);
  inline void rx_form(Opcode op, DoubleRegister r1,
                      Register x2, Register b2, Disp d2);
  inline void rx_b_form(Opcode op,
                     Condition m1,
                     Register x2,
                     Register b2,
                     Disp d2);


// RI1 format: <insn> R1,I2
//    +--------+----+----+------------------+
//    | OpCode | R1 |OpCd|        I2        |
//    +--------+----+----+------------------+
//    0        8    12   16                31
  inline void ri_form(Opcode op, Register r1, const Operand& i2);
// RI2 format: <insn> M1,I2
//    +--------+----+----+------------------+
//    | OpCode | M1 |OpCd|        I2        |
//    +--------+----+----+------------------+
//    0        8    12   16                31
  inline void ri_form(Opcode op, Condition m1, const Operand& i2);
  inline void rie_form(Opcode op, Register r1, Register r3,
                     const Operand& i2);
  inline void rie_f_form(Opcode op, Register r1, Register r2, const Operand &i3,
                     const Operand& i4, const Operand& i5);
  inline void ril_form(Opcode op, Register r1, const Operand& i2);
  inline void ril_form(Opcode op, Condition m1, const Operand& i2);
  inline void rre_form(Opcode op, Register r1, Register r2);
  inline void rre_form(Opcode op, DoubleRegister r1,
                       DoubleRegister r2);
  inline void rrd_form(Opcode op, Register r1, Register r3,
                       Register r2);
// RS format: <insn> R1,M3,D2(B2)
//     +--------+----+----+----+-------------+
//     | OpCode | R1 | M3 | B2 |     D2      |
//     +--------+----+----+----+-------------+
//     0        8    12   16   20           31
//
  inline void rs_form(Opcode op,
                        Register r1,
                        Condition m3,
                        Register b2,
                        const Disp d2);
// RS format: <insn> R1,R3,D2(B2)
//    +--------+----+----+----+-------------+
//    | OpCode | R1 | R3 | B2 |     D2      |
//    +--------+----+----+----+-------------+
//    0        8    12   16   20           31
//
  inline void rs_form(Opcode op,
                        Register r1,
                        Register r3,
                        Register b2,
                        const Disp d2);

  inline void rsi_form(Opcode op, Register r1, Register r3, const Operand& i2);
  inline void rsl_form(Opcode op, Length l1, Register b2, Disp d2);

// RSY format: <insn> R1,R3,D2(B2)
//     +--------+----+----+----+-------------+--------+--------+
//     | OpCode | R1 | R3 | B2 |    DL2      |  DH2   | OpCode |
//     +--------+----+----+----+-------------+--------+--------+
//     0        8    12   16   20            32       40      47
  inline void rsy_form(Opcode op,
                        Register r1,
                        Register r3,
                        Register b2,
                        const Disp d2);

// RSY format: <insn> R1,M3,D2(B2)
//     +--------+----+----+----+-------------+--------+--------+
//     | OpCode | R1 | M3 | B2 |    DL2      |  DH2   | OpCode |
//     +--------+----+----+----+-------------+--------+--------+
//     0        8    12   16   20            32       40      47
  inline void rsy_form(Opcode op,
                        Register r1,
                        Condition m3,
                        Register b2,
                        const Disp d2);

  inline void rxe_form(Opcode op, Register r1, Register x2, Register b2,
                     Disp d2);
  inline void rxy_form(Opcode op, Register r1, Register x2, Register b2,
                     Disp d2);
  inline void rxy_form(Opcode op, DoubleRegister r1, Register x2,
                       Register b2, Disp d2);
  inline void rrs_form(Opcode op, Register r1, Register r2, Register b4,
                     Disp d4, Condition m3);
  inline void ris_form(Opcode op, Register r1, Condition m3, Register b4, \
                     Disp d4, const Operand& i2);
  inline void s_form(Opcode op, Register b1, Disp d2);
  inline void si_form(Opcode op, const Operand& i2, Register b1,
                     Disp d1);
  inline void siy_form(Opcode op, const Operand& i2, Register b1, \
                     Disp d1);
  inline void sil_form(Opcode op, Register b1, Disp d1,
                     const Operand& i2);
  inline void rxf_form(Opcode op, Register r1, Register r3, Register b2, \
                     Register x2, Disp d2);
  inline void ss_form(Opcode op, Length l, Register b1, Disp d1, \
                     Register b2, Disp d2);
  inline void ss_form(Opcode op, Length l1, Length l2, Register b1,
                     Disp d1, Register b2, Disp d2);
  inline void ss_form(Opcode op, Length l1, const Operand& i3, Register b1,
                     Disp d1, Register b2, Disp d2);
  inline void ss_form(Opcode op, Register r1, Register r2, Register b1,
                     Disp d1, Register b2, Disp d2);
  inline void sse_form(Opcode op, Register b1, Disp d1, Register b2,
                     Disp d2);
  inline void ssf_form(Opcode op, Register r3, Register b1, Disp d1,
                     Register b2, Disp d2);
  inline void rrf1_form(Opcode op, Register r1, Register r2, Register r3);
  inline void rrf1_form(uint32_t x);
  inline void rrf2_form(uint32_t x);
  inline void rrf3_form(uint32_t x);
  inline void rrfe_form(Opcode op, Condition m3, Condition m4, Register r1,
                        Register r2);

  inline void CheckTrampolinePoolQuick();

  // Labels
  void print(Label* L);
  int  max_reach_from(int pos);
  void bind_to(Label* L, int pos);
  void next(Label* L);

  class Trampoline {
   public:
    Trampoline() {
      next_slot_ = 0;
      free_slot_count_ = 0;
    }
    Trampoline(int start, int slot_count) {
      next_slot_ = start;
      free_slot_count_ = slot_count;
    }
    int take_slot() {
      int trampoline_slot = kInvalidSlotPos;
      if (free_slot_count_ <= 0) {
        // We have run out of space on trampolines.
        // Make sure we fail in debug mode, so we become aware of each case
        // when this happens.
        DCHECK(0);
        // Internal exception will be caught.
      } else {
        trampoline_slot = next_slot_;
        free_slot_count_--;
        next_slot_ += kTrampolineSlotsSize;
      }
      return trampoline_slot;
    }

   private:
    int next_slot_;
    int free_slot_count_;
  };

  int32_t get_trampoline_entry();
  int unbound_labels_count_;
  // If trampoline is emitted, generated code is becoming large. As
  // this is already a slow case which can possibly break our code
  // generation for the extreme case, we use this information to
  // trigger different mode of branch instruction generation, where we
  // no longer use a single branch instruction.
  bool trampoline_emitted_;
  static const int kTrampolineSlotsSize = kInstrSize;
  static const int kMaxCondBranchReach = (1 << (16 - 1)) - 1;
  static const int kMaxBlockTrampolineSectionSize = 64 * kInstrSize;
  static const int kInvalidSlotPos = -1;

  Trampoline trampoline_;
  bool internal_trampoline_exception_;

  friend class RegExpMacroAssemblerS390;
  friend class RelocInfo;
  friend class CodePatcher;
  friend class BlockTrampolinePoolScope;

  PositionsRecorder positions_recorder_;
  friend class PositionsRecorder;
  friend class EnsureSpace;
};


class EnsureSpace BASE_EMBEDDED {
 public:
  explicit EnsureSpace(Assembler* assembler) {
    assembler->CheckBuffer();
  }
};

} }  // namespace v8::internal

#endif  // V8_S390_ASSEMBLER_S390_H_
