# Release Notes

## clang-r383902

### Upstream Cherry-picks
Patches with start_version: 383902 in https://android.googlesource.com/toolchain/llvm_android/+/master/patches/PATCHES.json.

### Local Changes
Revert 90c31b0f428f [X86] Custom lower ISD::FROUND with SSE4.1 to avoid a libcall.

### Notes
New Clang warnings encountered:
- -Wdeprecated-copy
- -Wrange-loop-construct
- -Wmisleading-indentation
- -Wzero-as-null-pointer-constant (not new, but seems to have stricter rule)
- -Wdeprecated-anon-enum-enum-conversion
- -Wdeprecated-enum-enum-conversion
- -Wstring-compare
- -Wenum-enum-conversion
- -Wenum-float-conversion
- -Wpessimizing-move

New Clang-tidy warnings encountered:
- -bugprone-reserved-identifier*,-cert-dcl51-cpp,-cert-dcl37-c
- -readability-qualified-auto
- -bugprone-signed-char-misuse
- -bugprone-branch-clone

Clang detects new compilation errors for not-instantiated template member functions.
Switch from python wrapper to go wrapper.

### Enabled
May 01 2020

------
## clang-r377782d

### Upstream Cherry-picks
- 6c732461 [ELF] Fix a null pointer dereference when --emit-relocs and --strip-debug are used together

### Enabled
Apr 04 2020

------
## clang-r377782c

### Upstream Cherry-picks
- 41206b61e30c [DebugInfo] Re-instate LiveDebugVariables scope trimming
- 1de10705594c [DAGCombine] Fix alias analysis for unaligned accesses

### Enabled
Mar 19 2020

------
## clang-r377782b

### Upstream Cherry-picks
- a3b22da4e0ea [CFG] Fix a flaky crash in CFGBlock::getLastCondition().
- d32484f40cbe [lldb][CMake] Fix build for the case of custom libedit installation
- 21f26470e974 Revert 3f91705ca54 ARM-NEON: make type modifiers orthogonal and allow multiple modifiers
- 90b8bc003caa IRGen: Call SetLLVMFunctionAttributes{,ForDefinition} on __cfi_check_fail
- acc79aa0e747 Revert "Revert 1689ad27af5"[builtins] Implement rounding mode support for i386/x86_64
- c5b890e92243 PR44268: Fix crash if __builtin_object_size is applied to a heap allocation.
- cd40bd0a32e2 hwasan: Move .note.hwasan.globals note to hwasan.module_ctor comdat.
- 4f38ab250ff4 [LLD][ELF][ARM] Do not insert interworking thunks for non STT_FUNC symbols
- 6c4a8bc0a9f6 Make llvm::crc32() work also for input sizes larger than 32 bits.
- f8c9ceb1ce9c [SimplifyLibCalls] Add __strlen_chk.

### Local Changes
- Revert two changes that break Android builds
- Add stubs and headers for nl_types APIs
- Add a new library libclang cxx
- Do not record function addresses if value profiling is disabled

### Notes
New Clang warnings encountered:
- -Wbitwise-conditional-parentheses
- -Wbool-operation
- -Wint-in-bool-context
- -Wsizeof-array-div
- -Wtautological-bitwise-compare
- -Wtautological-overlap-compare

### Enabled
Feb 18 2020

------
## clang-r370808

### Upstream Cherry-picks
- 1689ad27af5c [builtins] Implement rounding mode support for i386/x86_64
- 9e7ce07a8866 ARM: Don't emit R_ARM_NONE relocations to compact unwinding decoders in .ARM.exidx on Android.
- 51adeae1c90c remove redundant LLVM version from version string when setting CLANG_VENDOR
- 8ea148dc0cbf [Builtins] Fix bug where powerpc builtins specializations didn't remove generic implementations.
- 7a2b704bf0cf [Sema][Typo Correction] Fix another infinite loop on ambiguity
- r370981 [DebugInfo] Emit DW_TAG_enumeration_type for referenced global enumerator.
- r371003 Revert "Revert "[builtins] Rounding mode support for addxf3/subxf3""
- r371090 Fix windows-x86-debug compilation with python enabled using multi-target generator
- r371111 [IfConversion] Fix diamond conversion with unanalyzable branches.
- r371202 Revert r370635, it caused PR43241.
- r371215 Revert "Revert r370635, it caused PR43241."
- r371216 Reland D66717 [ELF] Do not ICF two sections with different output sections (by SECTIONS commands)
- r371262 [IR] CallBrInst: scan+update arg list when indirect dest list changes
- r371434 [IfConversion] Correctly handle cases where analyzeBranch fails.
- r371643 [IRMover] Don't map globals if their types are the same
- r371804 [ELF] Delete a redundant assignment to SectionBase::assigned. NFC
- r371859 [Sema][Typo Correction] Fix potential infite loop on ambiguity checks
- r371957 [ELF] Map the ELF header at imageBase
- r372047 Fix swig python package path
- r372194 Cache PYTHON_EXECUTABLE for windows
- r372364 Revert "Fix swig python package path"
- r372400 [ELF] Error if the linked-to section of a SHF_LINK_ORDER section is discarded
- r372482 [LLDB] Fix compilation for MinGW, remove redundant class name on inline member
- r372493 Use _WIN32 instead of _MSC_VER
- r372587 [LLDB] Add a missing specification of linking against dbghelp
- r372734 [ELF] Make MergeInputSection merging aware of output sections
- r372735 [ELF] Delete SectionBase::assigned
- r372835 [lldb] [cmake] Fix installing Python modules on systems using /usr/lib
- r372996 [ELF] Set SectionBase::partition in processSectionCommands
- r373022 Always rebuild a DeclRefExpr if its FoundDecl would change.
- r373035 hwasan: Compatibility fixes for short granules.
- r373255 ELF: Don't merge SHF_LINK_ORDER sections for different output sections in relocatable links.
- r373929 Fix Calling Convention through aliases
- r375166 libhwasan initialisation include kernel syscall ABI relaxation
- r375298 hwasan: Add missing SANITIZER_INTERFACE_ATTRIBUTE on __hwasan_personality_wrapper.

### Notes
New Clang warnings encountered:
- -Wreorder-init-list
- -Walloca
- -Wc99-designator
- -Wdangling-gsl
- -Wimplicit-fallthrough
- -Wimplicit-int-float-conversion
- -Wincomplete-setjmp-declaration
- -Wpointer-compare
- -Wxor-used-as-pow

-Wimplicit fallthrough was previously only checked for C++ code, but Clang can
now for for C code. `__attribute__((__fallthrough__))` should be used to
explicitly specify intentional fallthrough to silence the warning.

Lots of cherry picks are fixes for LLDB and HWASAN.

r373035 is slightly different from upstream due to not additionally cherry
picking r372338.

Writes to variables declared const through casts to non-const pointers
(explicitly undefined behavior) are now removed. The next release of Clang adds
UBSAN support for catching such mistakes.

### Created
Nov 11 2019

-----
## clang-r365631c

### Upstream Cherry-picks
- r366130 [LoopUnroll+LoopUnswitch] do not transform loops containing callbr
- r369761 [llvm-objcopy] Strip debug sections when running with --strip-unneeded.
- r370981 [DebugInfo] Emit DW_TAG_enumeration_type for referenced global enumerator.
- r372047 Fix swig python package path
- r372194 Cache PYTHON_EXECUTABLE for windows
- r372364 Revert "Fix swig python package path"
- r372587 [LLDB] Add a missing specification of linking against dbghelp
- r372835 [lldb] [cmake] Fix installing Python modules on systems using /usr/lib

### Notes
Fixes for:
- asm goto + LTO in Android Linux kernels
- debug info missing for enums
- NDK fixes for:
  - stripping debug sections w/ llvm-objcopy/llvm-strip
  - LLDB

### Created
Sep 26 2019

-----
## Older Releases
Release notes not available.
