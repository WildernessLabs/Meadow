# Table of Contents
1. [Ahead of Time Compilation](#aot)
	1. [LLVMONLY](#llvmonly)
	2. [AOT Process](#aotprocess)
		1. [Components](#components)
		2. [Limitations](#limitations)
	3. [Artifacts](#artifacts)
		1. [Post Link](#postlink)
		2. [Post AOT](#postaot)
		3. [Sample Run of Benchmark](#benchmark)
			1. [AOT Mode](#aotmode)
			2. [Interpreter Mode](#intmode)
	4. [Build Script](#buildscript)
	5. [Using the Shared Objects](#usingso)
		1. [NuttX Support](#nuttxsup)
		2. [Open Questions](#openq)
	6. [Building the Cross-Compiler](#buildcc)
	7. [Building Mono with LLVM runtime Support](#buildllvm)
	8. [Pull Requests](#prs)
# Ahead of Time Compilation <a name="aot"></a>
The AOT process will convert objects subject to interpretation into native code that mono can invoke. There are a number of AOT options:
1. Compiling the JITted code – not an option as there is no thumb2 JIT
2. Using mono’s llvm support to compile, assemble, link, and strip. This requires execution of a cross-compiler in a 32-bit environment.
3. Using mono’s llvm support to compile to bitcode which is then post-processed to create the required artifacts.
Option (3) is what is described here.
## LLVMONLY <a name="llvmonly"></a>
From [1] “Mono LLVMONLY/Bitcode description: [https://www.mono-project.com/docs/advanced/runtime/docs/BITCODE/](https://www.mono-project.com/docs/advanced/runtime/docs/BITCODE/)”
“Bitcode imposes the following major restrictions:
* No inline assembly/machine code
* Compilation using stock clang
To enable the runtime to operate in this environment, a new execution mode ‘llvmonly’ was implemented. In this mode:
* Everything is compiled to llvm bitcode, then compiled to native code using clang.
* No trampolines, etc. are used”
## AOT Process <a name="aotprocess"></a>
1. Use mono linker to reduce DLLs to only the content required
2. Run mono with `--aot=llvmonly,asmonly` to create bitcode (.bc)
3. Use clang++ to convert bitcode to arm thumb2 code
4. Link the object into a self-contained shared object
5. Strip the object of symbols to reduce size
### Components <a name="components"></a>
1. monolinker – to create DLLs with only the code required for the application
2. mono – to perform the AOT process which creates the bitcode
3. LLVM 11.0.1 – for clang which converts bitcode to thumb2 object code
4. arm-none-eabi-binutils-cs:
 * `arm-none-eabi-ld` – to link object code into self-container shared object
 * `arm-none-eabi-strip` – to strip unnecessary symbols from shared object
5. `arm-none-eabi-gcc-cs`- for libstdc++.a
### Limitations <a name="limitations"></a>
The llvmonly process is not able convert filter clauses and will print messages such as:
 
```LLVM failed for 'NetworkStream.Read': non-finally/catch/fault clause.``` 

This means this code will fall back to interpretation.
## Artifacts <a name="artifacts"></a>
The AOT process will create both `.dll` and `.so` objects both of which are required. The former for metadata and for that code not converted, the latter for the native code.
### Post Link <a name="postlink"></a>
|Size|Object|
|---:|------|
|3.5K|app.exe|
|664K|I18N.CJK.dll|
|39K|I18N.dll|
|32K	|I18N.MidEast.dll|
|36K	|I18N.Other.dll|
|188K|	I18N.Rare.dll|
|71K	|I18N.West.dll|
|210K|Meadow.dll|
|116K|Mono.Security.dll|
|2.2M|	mscorlib.dll|
|51K	|System.Configuration.dll|
|28K	|System.Core.dll|
|855K|	System.dll|
|25K	|System.Numerics.dll|
### Post AOT <a name="postaot"></a>
|Size|Object|
|---:|------|
|234K|	app.exe.so|
|306K|	I18N.CJK.dll.so|
|298K|	I18N.dll.so|
|253K|	I18N.MidEast.dll.so|
|266K|	I18N.Other.dll.so|
|372K|	I18N.Rare.dll.so|
|284K|	I18N.West.dll.so|
|1.2M|	Meadow.dll.so|
|476K|	Mono.Security.dll.so|
|5.9M|	mscorlib.dll.so|
|353K|	System.Configuration.dll.so|
|469K|	System.Core.dll.so|
|2.8M|System.dll.so|
|341K|	System.Numerics.dll.so|
|2.4M|	System.Xml.dll.so|
### Sample Run of Benchmark <a name="benchmark"></a>
The MeadowLabs benchmark was used to exercise the code.
#### AOT Mode <a name="aotmode"></a>
The benchmark program was put through the AOT process and run
##### Integer List 
|Test|Time|
|----|---:|
| List instantiation | 5ms |
| List population | 21ms |
| List summation | 21ms |
| List clearing | 12758ms |
##### Port Test
|Test|Time|
|----|---:|
| Port initialization | 1165ms |
| 300 Port writes | Æ21ms | 
| Average time per write | 0.4033333ms |
##### Pi Calculation
|Test|Time|
|----|---:|
| 50 digit Pi calc | 660ms
| 100 digit Pi calc | 3160ms
| 150 digit Pi calc | 7455ms 
#### Interpreter Mode <a name="intmode"></a>
The benchmark program was put through the AOT process and run
##### Integer List
|Test|Time|
|----|---:|
| List instantiation | 30ms |
| List population | 67ms |
| List summation | 27ms |
| List clearing | 8757ms |
##### Port Test
|Test|Time|
|----|---:|
| Port initialization | 4192ms |
| 300 Port writes | 554ms |
| Average time per write | 1.846667ms |
##### Pi Calculation
|Test|Time|
|----|---:|
| 50 digit Pi calc | 2505ms
| 100 digit Pi calc | 11998ms
| 150 digit Pi calc | 27404ms
## Build Script <a name="buildscript"></a>
```
#!/bin/bash

function build_dll()
{
	export PATH=/opt/mono/bin:/opt/llvm/bin:${OLD_PATH} [15]
	RC=0
	ARGS=`ls input/*.{exe,dll} | awk '{print "-a "$1" "}'`
	monolinker -l all -c link -o output ${ARGS[@]}
	RC=$?
	export PATH=${OLD_PATH}
	return ${RC}
}

function build_so()
{
	FN=`basename $1`
	echo "Compiling ${FN}"
	export PATH=/opt/mono.arm/bin:/opt/llvm/bin:${OLD_PATH} [16]
	export MONO_PATH=${PWD}/output [17]
	mono --aot="llvmonly,asmonly,interp,llvm-outfile=output/${FN}.bc,llvmllc=-exception-model=dwarf" output/${FN} [2]
	if [ -f output/${FN}.bc ]; then
		echo "Clanging ${FN}"
		clang   -fpic -O2 -fno-optimize-sibling-calls -Wno-override-module --target=thumb2-none-eabi \
			-mcpu=cortex-m7 -mfloat-abi=hard -mfpu=fpv5-d16 -c -o output/${FN}.o output/${FN}.bc [3]
		if [ -f output/${FN}.o ]; then
			if [ ${KEEP} -eq 0 ];then
				rm -f output/${FN}.bc
			fi
			echo "Linking ${FN}"
			arm-none-eabi-ld [4] -shared [5] -G -Bsymbolic -z max-page-size=1024 [6] \
				-z common-page-size=1024 --no-dynamic-linker [7] -pie [8] \
				--strip-debug [9] -no-enum-size-warning [10] \
				--export-dynamic-symbol=mono_aot_* [11] \
				-z combreloc [12] \
				-o output/${FN}.so output/${FN}.o \
				--start-group [13] \
				-L${PWD} -lapps -lbinfmt -lbuiltin -lconfigs -lcrypto \
				-ldrivers -lfs -lkarch -lkc -lkmm -lnet -lpass1 -lproxies \
				-lsched -luarch -luc -lumm -lxx \
				-L${ARM_LIB} -lstdc++ \
				--end-group
			RC=$?
			if [ -f output/${FN}.so ]; then
				if [ ${KEEP} -eq 0 ];then
					rm -f output/${FN}.o
				fi
				arm-none-eabi-strip --wildcard --strip-symbol=\$a.* \
					--strip-symbol=\$d.* --strip-symbol=\$t.* \
					--strip-symbol=\$a --strip-symbol=\$d --strip-symbol=\$t output/${FN}.so [14]
			else
				echo "Error linking ${FN} - rc: ${RC}" >&2
				exit 3
			fi
		else
			echo "Error clanging ${FN}" >&2
			exit 2
		fi
	else
		echo "Error compiling ${FN}" >&2
		exit 1
	fi
	export MONO_PATH=""
	export PATH=${OLD_PATH}
}

export ARM_LIB=/usr/arm-none-eabi/lib/thumb/v7e-m/fpv5/hard

OLD_PATH=${PATH}
KEEP=0
ONLY=0
SKIP=0
while getopts "kos" opt [17]
do
	case "${opt}" in
	k)
		KEEP=1;
		;;
	o)
		if [ -d output ]; then
			ONLY=1;
		fi
		;;
	s)
		SKIP=1;
		;;
	esac
done
shift $(( OPTIND - 1 ))

if [ ${ONLY} -eq 1 ]; then
	cp input/App.exe output
	SOS=`ls output/*.exe`
	for so in ${SOS}
	do
		build_so ${so}
	done
else
	if [ ${SKIP} -eq 0 ]; then
		rm -rf output
		mkdir output
		build_dll
	fi
	if [ $? -eq 0 ]; then
		SOS=`ls output/*.{exe,dll} | grep -v mscorlib`
		for so in ${SOS}
		do
			build_so ${so}
		done
		build_so output/mscorlib.dll [18]
	fi
fi
```
### Notes
1. 	Run the monlinker to create DLLs with only what’s required
2.	 Run mono with llvmonly,asmonly to get it to use LLVM backend and create bitcode (.bc) output
3. 	Run clang++ to convert bitcode to thumb2 object with floating point hardware support
4.	 Link the object 
5.	 Create shared object
6.	 Reduce the gap between text and data segments to 1K instead of default 64K
7.	 No need to specify the name of the dynamic loader as we only have the one we wrote
8.	 Create position independent executable code
9.	 Don’t create debug metadata
10.	Eliminate warning messages about enum sizes
11.	Only export `mono_aot_*` symbols in dynamic symbol table
12.	Combine relocation tables
13.	Search through these static libraries to resolve undefined symbols. Most of these libraries are from the NuttX staging directory and provide the libc-type functions. The libstdc++ library is only required for a couple of gcc-related symbols.
14.	Strip out all symbols starting with `$a`, `$d`, or `$t` to reduce the size of the object.
15.	The cross-compile version of mono is installed in `/opt/mono` and llvm 11.01 is in `/opt/llvm`
16.	There are a number of libstdc++.a from which to choose, we pick the one that most closely matches our device 
17.	The script provides the option to preserve the intermediate products of the AOT process, use existing artifacts, or skip the link. 
18.	mscorlib takes the longest to process so I leave it to last 
## Using the Shared Objects <a name="usingso"></a>
The resulting .dll and .so files need to be placed onto the Meadow board using the `meadow --WriteFile` command.
The mono_main function in `Meadow.OS//apps/examples/mono/mono_main.c` is updated to start mono with the `--llvmonly-interp` option:

```
#ifdef CONFIG_BUILD_KERNEL
  char *mono_argv[] = {"mono", "--interp", app_path};
#else
  char *mono_argv[] = {"mono", "--llvmonly-interp", "--debug", app_path};
#endif
```

The `--debug` option is being used during development and may be removed. 
### NuttX Support <a name="nuttxsupport"></a>
Code has been added to NuttX to load shared objects. The existing `dlopen()` code only supported relocatable objects to dynamically relocatable objects. The new code adds this support including additional relocation types. 
### Open Questions <a name="openq"></a>
This mode of operation also raises some questions.
#### Packaging
How to package for use by end-users: there are a number of components for this process that are required to perform the building of the artifacts. The mono used to AOT compile the code must be at the same level of the mono used to run the application. The mono AOT runtime will check these levels.
#### Cross-Build Host
The cross-compiler needs to be built for a 32-bit host as the size of structures is different due to pointer size differences. The script following assumes being on a Linux system that has 32-bit support. This means hosts such as RHEL8/CentOS8 are unable to run this. I am unsure as to the status of Mac OS X or Windows. 
## Building the Cross Compiler <a name="buildcc"></a>
This script will build a cross-compiler instance of mono that is used in the AOT process. The source is identical to that which is used to create mono that runs on the board. Note, the source tree must be completely clean of objects created by previous builds. A `make clean` does not get rid of all `.o` files (most notably in btls and llvm) and will cause the build to fail. 

```
#!/bin/bash

scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

# Check if the shell is interactive.
if [[ $- == *i* ]]; then
  red=`tput setaf 1`
  green=`tput setaf 2`
  reset=`tput sgr0`
fi

VERBOSE=true
FORCE=false
CLEAN=false
DEBUG=false

for i in "$@"
do
case $i in
    -v|--verbose)
    VERBOSE=true
    ;;
    -f|--force)
    FORCE=true
    ;;
    -c|--clean)
    CLEAN=true
    ;;
    -d|--debug)
    DEBUG=true
    ;;
    *)
    # unknown option
    ;;
esac
done

run_command() {
  if $VERBOSE; then
    echo
    $1
  else
    $1 &>/dev/null
  fi
}

check_command_status() {
  exit_status=$?
  if [ $exit_status -ne 0 ]; then
    printf " ${red}error${reset}\n"
    if ! $VERBOSE; then
        printf "Re-run the script with --verbose flag to see the output.\n"
    fi
    exit 1
  else
    printf " ${green}success${reset}\n"
  fi
}

#
# Build Mono
#

CFLAGS="-m32 -D__THUMB__"
CXXFLAGS="$CFLAGS"
LDFLAGS="-m32"
export CMAKE_C_FLAGS=-"$CFLAGS"
export CMAKE_CXX_FLAGS="$CXXFLAGS"
export LLVM_CMAKE_ARGS="-DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32"

if $DEBUG; then
  DEBUG_CFLAGS="-ggdb"
  CFLAGS="$CFLAGS $DEBUG_CFLAGS"
fi

cd $scriptdir/mono

AUTOGEN="./autogen.sh
	--target=arm-linux-eabi
	--prefix=/opt/mono.arm
	--with-runtime-preset=fullaotinterp_llvm
	--enable-llvm --enable-mcs
	--host=i686-pc-linux-gnu
	--build=i686-pc-linux-gnu
	--with-csc=roslyn
	--disable-boehm
	--disable-executables
	--disable-support-build
	--enable-cooperative-suspend
	--enable-interpreter
	--enable-nls=no
	--enable-minimal=profiler,pinvoke,debug,appdomains,verifier,large_code,logging,\
com,attach,perfcounters,normalization,desktop_loader,shared_perfcounters,remoting,security,\
lldb,mdb,shadowcopy"

if [ ! -f $scriptdir/mono/Makefile ] || $FORCE || $CLEAN; then
    printf "Configuring Mono AOT compiler...\n"

    # This step does not use run_command because of bash string escaping issues.
    if $VERBOSE; then
        $AUTOGEN CFLAGS="$CFLAGS" CPPFLAGS="$CPPFLAGS" CXXFLAGS="$CXXFLAGS" LDFLAGS="$LDFLAGS"
    else
        $AUTOGEN CFLAGS="$CFLAGS" CPPFLAGS="$CPPFLAGS" CXXFLAGS="$CXXFLAGS" LDFLAGS="$LDFLAGS" &>/dev/null
    fi
    check_command_status
else
    printf "Mono already configured (use --force to override)\n"
fi

printf "Building Mono AOT compiler...\n"
run_command "make -C $scriptdir/mono -j8"
check_command_status

printf "Packaging Mono AOT compiler...\n"
mkdir -p $scriptdir/mono/libs

cp $scriptdir/mono/mono/mini/mono-sgen \
  $scriptdir/mono/libs

exit 0
```

## Building Mono with LLVM runtime Support <a name="buildllvm"></a>

The existing `build-mono.sh` is updated so that the runtime provides support for AOT objects created by LLVM.

```
--- a/Meadow.OS/build-mono.sh
+++ b/Meadow.OS/build-mono.sh
@@ -105,6 +105,8 @@ CONFIGURE="./configure
     --enable-cooperative-suspend
     --enable-interpreter
     --enable-nls=no
+    --with-runtime-preset=fullaotinterp_llvm
+    --enable-llvm-runtime
     --enable-minimal=jit,profiler,pinvoke,debug,appdomains,verifier,large_code,logging,\
 com,attach,simd,perfcounters,normalization,desktop_loader,shared_perfcounters,\
 remoting,security,lldb,mdb,shadowcopy"
```

## Pull Requests <a name="prs"></a>
1. [Apps - Enable AOT Operation](https://github.com/WildernessLabs/apps/pull/62)
2. [Mono - Add AOT support via LLVM](https://github.com/WildernessLabs/mono/pull/8)
3. [NuttX - Enable loading of shared objects](https://github.com/WildernessLabs/nuttx/pull/84)
4. [Meadow - AOT Support](https://github.com/WildernessLabs/Meadow/pull/29)
