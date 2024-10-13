As of Android S, this deprecation is fully complete (except for ARCH=arm on
android-4.19-stable) and step 9 which is being cleaned up for Android T.

The relevant kernel commits are:
v5.7-rc1:
commit a0d1c951ef08 ("kbuild: support LLVM=1 to switch the default tools to Clang/LLVM")
v5.15-rc1:
commit f12b034afeb3 ("scripts/Makefile.clang: default to LLVM_IAS=1")

The original version of this document is retained below:

---

In the effort to improve test coverage of the LLVM substitutes when building
Linux kernels for Android distributions, as well as minimize build
dependencies, we plan to phase kernel builds over to use the LLVM substitutes
distributed through AOSP for Android S. This will remove the kernels from being
dependent on binutils in Android. The NDK is currently the other largest
dependency.

Invoking a kernel build with all of the substitutes is tedious; see
https://www.kernel.org/doc/html/latest/kbuild/llvm.html#llvm-utilities, so
`make LLVM=1` was introduced in:
commit a0d1c951ef08 ("kbuild: support LLVM=1 to switch the default tools to Clang/LLVM")
which first landed in v5.7-rc1 and will need to be backported to at least 5.4.

Support for using Clang's "integrated assembler" is a risk; we have gotten it
working upstream, but then small changes to assembly quickly uncover missing
support. Thus `LLVM_IAS=1` is a separate flag from `LLVM=1`.

The plan for S is:

1. Ensure a0d1c951ef08 is backported to all supported kernel version branches
   for S.
2. Wire up support in Android common kernel's build/build.sh to forward
   `LLVM=1` to `make` from a supplied config. `LLVM=1` needs to be specified
   for all invocations of `make`.
3. Update kernel configs to use `LLVM=1` and remove the individual flags like
   `CC=clang`, `NM=llvm-nm`, and such.
4. Create a branch of
   https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/
   that only contains the GNU as (assembler) binary.
   (https://android.googlesource.com/platform/prebuilts/gas/linux-x86/)
5. Update repo manifests for kernels to use the branch from 4, and ensure
   kernels build cleanly.
6. Remove GNU binutils from Android kernel manifests.

5 is potentially a lot of work, depending on whether we have a lot of fixes to
backport or certain configs are broken that haven't been tested upstream.

A stretch goal, assuming we get LLVM_IAS=1 in shape:

7. Wire up support in Android common kernel's build/build.sh to forward
   `LLVM_IAS=1` to `make` from a supplied config. `LLVM_IAS=1` needs to be
   specified for all invocations of `make`. Alternatively, we can just modify
   the top level Makefile to do what LLVM_IAS=1 would do anyways, though having
   flexibility of turning it off quickly should it regress in mainline is
   probably a safer bet.
8. Update kernel configs to use `LLVM_IAS=1`, unless we simply modified the top
   level Makefile in 6.  At this point we will be using Clang's integrated
   assembler.
9. Remove prebuilts/gas/linux-x86 (from step 4) from Android kernel manifests.

I think we can get all 9 done for S, but 7,8,9 are a risk, and aren't critical
to solve for S. Luckily, our comrades over at CrOS LLVM are helping whip
Clang's integrated assembler into shape. In fact, they may end up beating us to
the punch.

See this public hotlist for some of the outstanding issues to be resolved.
https://github.com/ClangBuiltLinux/linux/issues?q=is%3Aissue+is%3Aopen+label%3A%22%5BTOOL%5D+integrated-as%22
(Not all of those are blockers).

The version of GNU binutils distributed by AOSP is version 2.27.0.20170315 (ToT
GNU binutils is 2.35) plus cherry picks. The 5.9-rc1 linux kernel currently
requires binutils 2.23 or newer.

## Contingency Plans

Let's say a kernel change doesn't work with Clang's integrated assembler. The
immediate question is "does this change need to land now, or can we bear a
revert until Clang's integrated assembler is fixed?"

With the answer to the above question being "Yes, revert now please" then the
contingency plan is:
1. file a bug, or at least email android-llvm@googlegroups.com and
   ndesaulniers@google.com.
2. revert step 9 above.
3. revert step 8 above.
4. Wait for toolchain feature or bug to be fixed in upstream LLVM.
5. Wait for AOSP LLVM release containing fix.
6. Move kernel builds to newer toolchains.
7. Reapply step 8 above.
8. Reapply step 9 above.

With the answer to the above question being "No, we can wait for assembler
support" then the contingency plan is:
1. file a bug, or at least email android-llvm@googlegroups.com and
   ndesaulniers@google.com.
2. Wait for toolchain feature or bug to be fixed in upstream LLVM.
3. Wait for AOSP LLVM release containing fix.
4. Move kernel builds to newer toolchains.
