//
// Copyright (C) 2017 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

package clangprebuilts

import (
	"path"
	"strings"

	"github.com/google/blueprint/proptools"

	"android/soong/android"
	"android/soong/cc"
	"android/soong/cc/config"
	"android/soong/genrule"
)

const libclangCppSoName = "libclang-cpp.so"
const libcxxSoName = "libc++.so"
const libcxxabiSoName = "libc++abi.so"

var (
	// Files included in the llvm-tools filegroup in ../Android.bp
	llvmToolsFiles = []string{
		"bin/llvm-symbolizer",
		"bin/llvm-cxxfilt",
		"lib/libc++.so",
	}
)

// This module is used to generate libfuzzer, libomp static libraries and
// libclang_rt.* shared libraries. When LLVM_PREBUILTS_VERSION and
// LLVM_RELEASE_VERSION are set, the library will generated from the given
// path.
func init() {
	android.RegisterModuleType("llvm_host_defaults",
		llvmHostDefaultsFactory)
	android.RegisterModuleType("llvm_host_prebuilt_library_shared",
		llvmHostPrebuiltLibrarySharedFactory)
	android.RegisterModuleType("llvm_prebuilt_library_static",
		llvmPrebuiltLibraryStaticFactory)
	android.RegisterModuleType("libclang_rt_prebuilt_library_shared",
		libClangRtPrebuiltLibrarySharedFactory)
	android.RegisterModuleType("libclang_rt_prebuilt_library_static",
		libClangRtPrebuiltLibraryStaticFactory)
	android.RegisterModuleType("libclang_rt_prebuilt_object",
		libClangRtPrebuiltObjectFactory)
	android.RegisterModuleType("llvm_darwin_filegroup",
		llvmDarwinFileGroupFactory)
	android.RegisterModuleType("clang_builtin_headers",
		clangBuiltinHeadersFactory)
	android.RegisterModuleType("llvm_tools_filegroup",
		llvmToolsFilegroupFactory)
}

func getClangPrebuiltDir(ctx android.LoadHookContext) string {
	return path.Join(
		"./",
		ctx.Config().GetenvWithDefault("LLVM_PREBUILTS_VERSION", config.ClangDefaultVersion),
	)
}

func getClangResourceDir(ctx android.LoadHookContext) string {
	clangDir := getClangPrebuiltDir(ctx)
	releaseVersion := ctx.Config().GetenvWithDefault("LLVM_RELEASE_VERSION",
		config.ClangDefaultShortVersion)
	return path.Join(clangDir, "lib", "clang", releaseVersion, "lib", "linux")
}

func getSymbolFilePath(ctx android.LoadHookContext) string {
	libDir := getClangResourceDir(ctx)
	return path.Join(libDir, strings.TrimSuffix(ctx.ModuleName(), ".llndk")+".map.txt")
}

func trimVersionNumbers(ver string, retain int) string {
	sep := "."
	versions := strings.Split(ver, sep)
	return strings.Join(versions[0:retain], sep)
}

func getHostLibrary(ctx android.LoadHookContext) string {
	switch ctx.ModuleName() {
	case "prebuilt_libclang-cpp_host":
		return libclangCppSoName
	case "prebuilt_libc++_host":
		return libcxxSoName
	case "prebuilt_libc++abi_host":
		return libcxxabiSoName
	default:
		ctx.ModuleErrorf("unsupported host LLVM module: " + ctx.ModuleName())
		return ""
	}
}

func llvmHostPrebuiltLibraryShared(ctx android.LoadHookContext) {
	moduleName := ctx.ModuleName()
	enabled := ctx.Config().IsEnvTrue("LLVM_BUILD_HOST_TOOLS")

	clangDir := getClangPrebuiltDir(ctx)

	headerDir := path.Join(clangDir, "include")
	if moduleName == "prebuilt_libc++_host" {
		headerDir = path.Join(headerDir, "c++", "v1")
	}

	linuxLibrary := path.Join(clangDir, "lib", getHostLibrary(ctx))
	darwinFileGroup := strings.TrimSuffix(strings.TrimPrefix(
		moduleName, "prebuilt_"), "_host") + "_darwin"

	type props struct {
		Enabled             *bool
		Export_include_dirs []string
		Target              struct {
			Linux_glibc_x86_64 struct {
				Srcs []string
			}
			Darwin_x86_64 struct {
				Srcs []string
			}
			Windows struct {
				Enabled *bool
			}
		}
		Stl *string
	}

	p := &props{}
	p.Enabled = proptools.BoolPtr(enabled)
	p.Export_include_dirs = []string{headerDir}
	p.Target.Linux_glibc_x86_64.Srcs = []string{linuxLibrary}
	p.Target.Darwin_x86_64.Srcs = []string{":" + darwinFileGroup}
	p.Target.Windows.Enabled = proptools.BoolPtr(false)
	p.Stl = proptools.StringPtr("none")
	ctx.AppendProperties(p)
}

type archInnerProps struct {
	Srcs []string
	Stem *string
}
type archProps struct {
	Android_arm         archInnerProps
	Android_arm64       archInnerProps
	Android_riscv64     archInnerProps
	Android_x86         archInnerProps
	Android_x86_64      archInnerProps
	Linux_bionic_arm64  archInnerProps
	Linux_bionic_x86_64 archInnerProps
	Glibc_x86           archInnerProps
	Glibc_x86_64        archInnerProps
	Linux_musl_x86      archInnerProps
	Linux_musl_x86_64   archInnerProps
	Linux_musl_arm      archInnerProps
	Linux_musl_arm64    archInnerProps
	Darwin              archInnerProps
	Windows_x86_64      archInnerProps
}

func llvmPrebuiltLibraryStatic(ctx android.LoadHookContext) {
	libDir := getClangResourceDir(ctx)
	name := strings.TrimPrefix(ctx.ModuleName(), "prebuilt_") + ".a"

	type props struct {
		Export_include_dirs []string
		Target              archProps
	}

	p := &props{}

	if name == "libFuzzer.a" {
		headerDir := path.Join(getClangPrebuiltDir(ctx), "prebuilt_include", "llvm", "lib", "Fuzzer")
		p.Export_include_dirs = []string{headerDir}
	} else if name == "libsimpleperf_readelf.a" {
		headerDir := path.Join(getClangPrebuiltDir(ctx), "include")
		p.Export_include_dirs = []string{headerDir}
	}

	p.Target.Android_arm.Srcs = []string{path.Join(libDir, "arm", name)}
	p.Target.Android_arm64.Srcs = []string{path.Join(libDir, "aarch64", name)}
	p.Target.Android_riscv64.Srcs = []string{path.Join(libDir, "riscv64", name)}
	p.Target.Android_x86.Srcs = []string{path.Join(libDir, "i386", name)}
	p.Target.Android_x86_64.Srcs = []string{path.Join(libDir, "x86_64", name)}
	p.Target.Linux_bionic_arm64.Srcs = []string{path.Join(libDir, "aarch64", name)}
	p.Target.Linux_bionic_x86_64.Srcs = []string{path.Join(libDir, "x86_64", name)}

	if name == "libsimpleperf_readelf.a" {
		p.Target.Glibc_x86_64.Srcs = []string{path.Join(getClangPrebuiltDir(ctx), "lib/x86_64-unknown-linux-gnu", name)}
		p.Target.Windows_x86_64.Srcs = []string{path.Join(getClangPrebuiltDir(ctx), "lib/x86_64-w64-windows-gnu", name)}
		p.Target.Darwin.Srcs = []string{":libsimpleperf_readelf_darwin"}
		p.Target.Linux_musl_x86_64.Srcs = []string{path.Join(libDir, "x86_64-unknown-linux-musl/lib", name)}
		p.Target.Linux_musl_arm64.Srcs = []string{path.Join(libDir, "aarch64-unknown-linux-musl/lib", name)}
	} else {
		p.Target.Linux_musl_x86.Srcs = []string{path.Join(libDir, "i686-unknown-linux-musl/lib", name)}
		p.Target.Linux_musl_x86_64.Srcs = []string{path.Join(libDir, "x86_64-unknown-linux-musl/lib", name)}
		p.Target.Linux_musl_arm.Srcs = []string{path.Join(libDir, "arm-unknown-linux-musleabihf/lib", name)}
		p.Target.Linux_musl_arm64.Srcs = []string{path.Join(libDir, "aarch64-unknown-linux-musl/lib", name)}
	}

	ctx.AppendProperties(p)
}

type prebuiltLibrarySharedProps struct {
	Is_llndk *bool

	Shared_libs []string
}

type prebuiltLibraryProps struct {
	Lib_name *string

	Suffix *string
}

func libClangRtPrebuiltLibraryShared(ctx android.LoadHookContext, libProps *prebuiltLibraryProps,
	sharedProps *prebuiltLibrarySharedProps) {

	if ctx.Config().IsEnvTrue("FORCE_BUILD_SANITIZER_SHARED_OBJECTS") {
		return
	}

	libDir := getClangResourceDir(ctx)

	type props struct {
		Target             archProps
		System_shared_libs []string
		No_libcrt          *bool
		Sanitize           struct {
			Never *bool
		}
		Strip struct {
			None *bool
		}
		Pack_relocations *bool
		Stl              *string
		Stubs            struct {
			Symbol_file *string
			Versions    []string
		}
		Llndk struct {
			Symbol_file *string
		}
	}

	p := &props{}

	name := proptools.StringDefault(libProps.Lib_name, strings.TrimPrefix(ctx.ModuleName(), "prebuilt_"))
	suffix := proptools.String(libProps.Suffix)

	p.Target.Android_arm.Srcs = []string{path.Join(libDir, name+"-arm-android"+suffix+".so")}
	p.Target.Android_arm.Stem = proptools.StringPtr(name + "-arm-android" + suffix)
	p.Target.Android_arm.Srcs = []string{path.Join(libDir, name+"-arm-android"+suffix+".so")}
	p.Target.Android_arm.Stem = proptools.StringPtr(name + "-arm-android" + suffix)
	p.Target.Android_arm64.Srcs = []string{path.Join(libDir, name+"-aarch64-android"+suffix+".so")}
	p.Target.Android_arm64.Stem = proptools.StringPtr(name + "-aarch64-android" + suffix)
	p.Target.Android_riscv64.Srcs = []string{path.Join(libDir, name+"-riscv64-android"+suffix+".so")}
	p.Target.Android_riscv64.Stem = proptools.StringPtr(name + "-riscv64-android" + suffix)
	p.Target.Android_x86.Srcs = []string{path.Join(libDir, name+"-i686-android"+suffix+".so")}
	p.Target.Android_x86.Stem = proptools.StringPtr(name + "-i686-android" + suffix)
	p.Target.Android_x86_64.Srcs = []string{path.Join(libDir, name+"-x86_64-android"+suffix+".so")}
	p.Target.Android_x86_64.Stem = proptools.StringPtr(name + "-x86_64-android" + suffix)
	p.Target.Linux_bionic_arm64.Srcs = []string{path.Join(libDir, name+"-aarch64-android"+suffix+".so")}
	p.Target.Linux_bionic_arm64.Stem = proptools.StringPtr(name + "-aarch64-android" + suffix)
	p.Target.Linux_bionic_x86_64.Srcs = []string{path.Join(libDir, name+"-x86_64-android"+suffix+".so")}
	p.Target.Linux_bionic_x86_64.Stem = proptools.StringPtr(name + "-x86_64-android" + suffix)
	p.Target.Glibc_x86.Srcs = []string{path.Join(libDir, name+"-i386"+suffix+".so")}
	p.Target.Glibc_x86.Stem = proptools.StringPtr(name + "-i386" + suffix)
	p.Target.Glibc_x86_64.Srcs = []string{path.Join(libDir, name+"-x86_64"+suffix+".so")}
	p.Target.Glibc_x86_64.Stem = proptools.StringPtr(name + "-x86_64" + suffix)
	p.Target.Linux_musl_x86.Srcs = []string{path.Join(libDir, "i686-unknown-linux-musl/lib/linux", name+"-i386"+suffix+".so")}
	p.Target.Linux_musl_x86.Stem = proptools.StringPtr(name + "-i386" + suffix)
	p.Target.Linux_musl_x86_64.Srcs = []string{path.Join(libDir, "x86_64-unknown-linux-musl/lib/linux", name+"-x86_64"+suffix+".so")}
	p.Target.Linux_musl_x86_64.Stem = proptools.StringPtr(name + "-x86_64" + suffix)
	p.Target.Linux_musl_arm.Srcs = []string{path.Join(libDir, "arm-unknown-linux-musleabihf/lib/linux", name+"-armhf"+suffix+".so")}
	p.Target.Linux_musl_arm.Stem = proptools.StringPtr(name + "-armhf" + suffix)
	p.Target.Linux_musl_arm64.Srcs = []string{path.Join(libDir, "aarch64-unknown-linux-musl/lib/linux", name+"-aarch64"+suffix+".so")}
	p.Target.Linux_musl_arm64.Stem = proptools.StringPtr(name + "-aarch64" + suffix)

	p.System_shared_libs = []string{}
	p.No_libcrt = proptools.BoolPtr(true)
	p.Sanitize.Never = proptools.BoolPtr(true)
	p.Strip.None = proptools.BoolPtr(true)
	disable := false
	p.Pack_relocations = &disable
	p.Stl = proptools.StringPtr("none")

	if proptools.Bool(sharedProps.Is_llndk) {
		p.Stubs.Versions = []string{"29", "10000"}
		p.Stubs.Symbol_file = proptools.StringPtr(getSymbolFilePath(ctx))
		p.Llndk.Symbol_file = proptools.StringPtr(getSymbolFilePath(ctx))
	}

	ctx.AppendProperties(p)
}

func libClangRtPrebuiltLibraryStatic(ctx android.LoadHookContext, libProps *prebuiltLibraryProps) {
	libDir := getClangResourceDir(ctx)

	type props struct {
		Target             archProps
		System_shared_libs []string
		No_libcrt          *bool
		Stl                *string
	}

	name := proptools.StringDefault(libProps.Lib_name, strings.TrimPrefix(ctx.ModuleName(), "prebuilt_"))
	name = strings.TrimSuffix(name, ".static")
	suffix := proptools.String(libProps.Suffix)

	p := &props{}

	p.Target.Android_arm.Srcs = []string{path.Join(libDir, name+"-arm-android"+suffix+".a")}
	p.Target.Android_arm64.Srcs = []string{path.Join(libDir, name+"-aarch64-android"+suffix+".a")}
	p.Target.Android_riscv64.Srcs = []string{path.Join(libDir, name+"-riscv64-android"+suffix+".a")}
	p.Target.Android_x86.Srcs = []string{path.Join(libDir, name+"-i686-android"+suffix+".a")}
	p.Target.Android_x86_64.Srcs = []string{path.Join(libDir, name+"-x86_64-android"+suffix+".a")}
	p.Target.Linux_bionic_arm64.Srcs = []string{path.Join(libDir, name+"-aarch64-android"+suffix+".a")}
	p.Target.Linux_bionic_x86_64.Srcs = []string{path.Join(libDir, name+"-x86_64-android"+suffix+".a")}
	p.Target.Glibc_x86.Srcs = []string{path.Join(libDir, name+"-i386"+suffix+".a")}
	p.Target.Glibc_x86_64.Srcs = []string{path.Join(libDir, name+"-x86_64"+suffix+".a")}
	p.Target.Linux_musl_x86.Srcs = []string{path.Join(libDir, "i686-unknown-linux-musl/lib/linux", name+"-i386"+suffix+".a")}
	p.Target.Linux_musl_x86_64.Srcs = []string{path.Join(libDir, "x86_64-unknown-linux-musl/lib/linux", name+"-x86_64"+suffix+".a")}
	p.Target.Linux_musl_arm.Srcs = []string{path.Join(libDir, "arm-unknown-linux-musleabihf/lib/linux", name+"-armhf"+suffix+".a")}
	p.Target.Linux_musl_arm64.Srcs = []string{path.Join(libDir, "aarch64-unknown-linux-musl/lib/linux", name+"-aarch64"+suffix+".a")}
	p.System_shared_libs = []string{}
	p.No_libcrt = proptools.BoolPtr(true)
	p.Stl = proptools.StringPtr("none")
	ctx.AppendProperties(p)
}

func libClangRtPrebuiltObject(ctx android.LoadHookContext) {
	libDir := getClangResourceDir(ctx)

	type props struct {
		Arch struct {
			X86 struct {
				Srcs []string
			}
			X86_64 struct {
				Srcs []string
			}
			Arm struct {
				Srcs []string
			}
			Arm64 struct {
				Srcs []string
			}
		}
		System_shared_libs []string
		Stl                *string
	}

	name := strings.TrimPrefix(ctx.ModuleName(), "prebuilt_")

	p := &props{}
	p.Arch.X86.Srcs = []string{path.Join(libDir, name+"-i386.o")}
	p.Arch.X86_64.Srcs = []string{path.Join(libDir, name+"-x86_64.o")}
	p.Arch.Arm.Srcs = []string{path.Join(libDir, "arm-unknown-linux-musleabihf/lib/linux", name+"-armhf.o")}
	p.Arch.Arm64.Srcs = []string{path.Join(libDir, "aarch64-unknown-linux-musl/lib/linux", name+"-aarch64.o")}
	p.System_shared_libs = []string{}
	p.Stl = proptools.StringPtr("none")
	ctx.AppendProperties(p)
}

func llvmDarwinFileGroup(ctx android.LoadHookContext) {
	clangDir := getClangPrebuiltDir(ctx)
	libName := strings.TrimSuffix(ctx.ModuleName(), "_darwin")
	if libName == "libc++" || libName == "libc++abi" {
		libName += ".1"
	}
	if libName == "libsimpleperf_readelf" {
		libName += ".a"
	} else {
		libName += ".dylib"
	}
	lib := path.Join(clangDir, "lib", libName)

	type props struct {
		Srcs []string
	}

	libPath := android.ExistentPathForSource(ctx, ctx.ModuleDir(), lib)
	if libPath.Valid() {
		p := &props{}
		p.Srcs = []string{lib}
		ctx.AppendProperties(p)
	}
}

func llvmPrebuiltLibraryStaticFactory() android.Module {
	module, _ := cc.NewPrebuiltStaticLibrary(android.HostAndDeviceSupported)
	android.AddLoadHook(module, llvmPrebuiltLibraryStatic)
	return module.Init()
}

func llvmHostPrebuiltLibrarySharedFactory() android.Module {
	module, _ := cc.NewPrebuiltSharedLibrary(android.HostSupported)
	android.AddLoadHook(module, llvmHostPrebuiltLibraryShared)
	return module.Init()
}

func libClangRtPrebuiltLibrarySharedFactory() android.Module {
	module, _ := cc.NewPrebuiltSharedLibrary(android.HostAndDeviceSupported)
	props := &prebuiltLibraryProps{}
	sharedProps := &prebuiltLibrarySharedProps{}
	module.AddProperties(props, sharedProps)
	android.AddLoadHook(module, func(ctx android.LoadHookContext) {
		libClangRtPrebuiltLibraryShared(ctx, props, sharedProps)
	})
	return module.Init()
}

func libClangRtPrebuiltLibraryStaticFactory() android.Module {
	module, _ := cc.NewPrebuiltStaticLibrary(android.HostAndDeviceSupported)
	props := &prebuiltLibraryProps{}
	module.AddProperties(props)
	android.AddLoadHook(module, func(ctx android.LoadHookContext) {
		libClangRtPrebuiltLibraryStatic(ctx, props)
	})
	return module.Init()
}

func libClangRtPrebuiltObjectFactory() android.Module {
	module := cc.NewPrebuiltObject(android.HostAndDeviceSupported)
	android.AddLoadHook(module, libClangRtPrebuiltObject)
	return module.Init()
}

func llvmDarwinFileGroupFactory() android.Module {
	module := android.FileGroupFactory()
	android.AddLoadHook(module, llvmDarwinFileGroup)
	return module
}

func llvmHostDefaults(ctx android.LoadHookContext) {
	type props struct {
		Enabled *bool
	}

	p := &props{}
	if !ctx.Config().IsEnvTrue("LLVM_BUILD_HOST_TOOLS") {
		p.Enabled = proptools.BoolPtr(false)
	}
	ctx.AppendProperties(p)
}

func llvmHostDefaultsFactory() android.Module {
	module := cc.DefaultsFactory()
	android.AddLoadHook(module, llvmHostDefaults)
	return module
}

func clangBuiltinHeaders(ctx android.LoadHookContext) {
	type props struct {
		Cmd  *string
		Srcs []string
	}

	p := &props{}
	builtinHeadersDir := path.Join(
		getClangPrebuiltDir(ctx), "lib", "clang",
		ctx.Config().GetenvWithDefault("LLVM_RELEASE_VERSION",
			config.ClangDefaultShortVersion), "include")
	s := "$(location) " + path.Join(ctx.ModuleDir(), builtinHeadersDir) + " $(in) >$(out)"
	p.Cmd = &s

	p.Srcs = []string{path.Join(builtinHeadersDir, "**", "*.h")}
	ctx.AppendProperties(p)
}

func clangBuiltinHeadersFactory() android.Module {
	module := genrule.GenRuleFactory()
	android.AddLoadHook(module, clangBuiltinHeaders)
	return module
}

func llvmToolsFileGroup(ctx android.LoadHookContext) {
	type props struct {
		Srcs []string
	}

	p := &props{}
	prebuiltDir := path.Join(getClangPrebuiltDir(ctx))
	for _, src := range llvmToolsFiles {
		p.Srcs = append(p.Srcs, path.Join(prebuiltDir, src))
	}
	ctx.AppendProperties(p)
}

func llvmToolsFilegroupFactory() android.Module {
	module := android.FileGroupFactory()
	android.AddLoadHook(module, llvmToolsFileGroup)
	return module
}
