#!/bin/bash


#
# Use monolinker to produce a subset of the DLLs needed at runtime
#
function build_dll()
{
	export PATH=/opt/mono/bin:${OLD_PATH}
	RC=0
	ARGS=`ls input/*.{exe,dll} | awk '{print "-a "$1" "}'`
	monolinker -l all -c link -o output ${ARGS[@]}
	RC=$?
	export PATH=${OLD_PATH}
	return ${RC}
}

#
# Build the AOT shared objects in LLVM mode
#
function llvm_build_so()
{
	FN=`basename $1`
	echo "Compiling ${FN}"
	export PATH=/opt/mono.jit/bin:${OLD_PATH}
	export MONO_PATH=${PWD}/output
	if [ ${KEEP} -eq 1 ]; then
		KEEPOPT=",keep-temps"
	else
		KEEPOPT=""
	fi
	MONO_LDFLAGS="${LDFLAGS} -Map=output/${FN}.map"
	mono --aot="llvm,${LLVMOPTS},ld-flags=${MONO_LDFLAGS}${KEEPOPT}" output/${FN}
	if [ $? -ne 0 ]; then
		echo "Error compiling ${FN}" >&2
		exit 1
	fi
	export MONO_PATH=""
	export PATH=${OLD_PATH}
}

#
# Build the AOT shared objects in JIT mode
#
function jit_build_so()
{
	FN=`basename $1`
	echo "Compiling ${FN}"
	export PATH=/opt/mono.jit/bin:${OLD_PATH}
	export MONO_PATH=${PWD}/output
	if [ ${KEEP} -eq 1 ]; then
		KEEPOPT="keep-temps"
	else
		KEEPOPT=""
	fi
	mono --aot="${KEEPOPT}" output/${FN}
	if [ $? -ne 0 ]; then
		echo "Error compiling ${FN}" >&2
		exit 1
	fi
	export MONO_PATH=""
	export PATH=${OLD_PATH}
}

#
# Build the AOT shared objects in JIT mode
#
function llvmonly_build_so()
{
	FN=`basename $1`
	echo "Compiling ${FN}"
	export PATH=/opt/mono.jit/bin:/opt/llvm/bin:${OLD_PATH}
	export MONO_PATH=${PWD}/output
	mono --aot="llvmonly,asmonly,interp,llvm-outfile=output/${FN}.bc,llvmllc=-exception-model=dwarf" output/${FN}
	if [ -f output/${FN}.bc ]; then
		echo "Clanging ${FN}"
		clang   -fpic -O1 -fno-optimize-sibling-calls -Wno-override-module --target=thumb2-none-eabi \
			-mcpu=cortex-m7 -mfloat-abi=hard -mfpu=fpv5-d16 -c -o output/${FN}.o output/${FN}.bc
		if [ -f output/${FN}.o ]; then
			if [ ${KEEP} -eq 0 ];then 
				rm -f output/${FN}.bc
			fi
			echo "Linking ${FN}"
			arm-none-eabi-ld -shared -G -Bsymbolic -z max-page-size=1024 \
				-E \
				-z common-page-size=1024 --no-dynamic-linker -pie \
				--strip-debug -no-enum-size-warning \
				--export-dynamic-symbol=mono_aot_* \
				-z combreloc \
				-o output/${FN}.so output/${FN}.o \
				--entry mono_aot_file_info \
				--start-group \
				-L${PWD} -lbuiltin \
				-L${ARM_LIB} -lsupc++ \
				-L${ARM_GCCLIB} -lgcc \
				--end-group -Map=output/${FN}.map
			RC=$?
			if [ -f output/${FN}.so ]; then
				if [ ${KEEP} -eq 0 ];then 
					rm -f output/${FN}.o
				fi
				arm-none-eabi-strip --wildcard --strip-symbol=\$a.* \
					--strip-symbol=\$d.* --strip-symbol=\$t.* \
					--strip-symbol=\$a --strip-symbol=\$d --strip-symbol=\$t output/${FN}.so
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

#
# Call the appropriate AOT build mode
#
function build_so()
{
	case ${1} in
	llvm)
		llvm_build_so $2
		;;
	jit)
		jit_build_so $2
		;;
	llvmonly)
		llvmonly_build_so $2
		;;
	esac
}

#
# Display usage information and any (optional) error message
#
usage()
{
	if [ -n "$1" ]; then
		echo $1 >&2
		echo >&2
	fi
	echo "genAOT -t <type> -k -o -s" >&2
	echo >&2
	echo "where:" >&2
	echo "   -t <type>   Type of AOT: 'llvm', 'jit', 'llvmonly'" >&2
	echo "   -k          Keep any intermediate files" >&2
	echo "   -s          Skip monolinker step" >&2
	exit 1
}

#
# Where to find the thumb2 library archives
#
export ARM_LIB=/usr/arm-none-eabi/lib/thumb/v7e-m/fpv5/hard

OLD_PATH=${PATH}
KEEP=0
SKIP=0
TYPE=""
while getopts "t:ks" opt
do
	case "${opt}" in
	k)
		KEEP=1;
		;;
	s)
		SKIP=1;
		;;
	t)
		TYPE=${OPTARG}
		;;
	esac
done
shift $(( OPTIND - 1 ))

#
# Set any AOT mode-specific flags
#
case ${TYPE} in
llvm)
	LDFLAGS="-E -z max-page-size=1024 -z common-page-size=1024 --no-dynamic-linker -pie --strip-debug -no-enum-size-warning --export-dynamic-symbol=mono_aot_* -z combreloc --start-group -L${PWD} -lbuiltin -L${ARM_LIB_1} -lsupc++_nano -L${ARM_LIB_2} -lgcc  --end-group --long-plt --entry mono_aot_file_info"
	LLVMOPTS="llvmopts=-mtriple=arm-none-eabi-thumb,llvmopts=-float-abi=hard,llvmopts=-mattr=+vfp4,llvmopts=-mattr=+armv7e-m,llvmllc=-mattr=+armv7e-m"
	;;
llvmonly)
	export ARM_GCCLIB="/usr/lib/gcc/arm-none-eabi/7.1.0/thumb/v7e-m/fpv5/hard"
	;;
jit)
	;;
*)
	usage "Unknown AOT method: ${TYPE} - valid methods: llvm, jit, or llvmonly" 
	exit 1
	;;
esac

#
# If we've already run the monolinker then we can ask to skip this step
#
if [ ${SKIP} -eq 1 ]; then
	cp input/App.exe output
	SOS=`ls output/*.{exe,dll}`
	for so in ${SOS} 
	do
		build_so ${TYPE} ${so}
	done
else
	rm -rf output 
	mkdir output
	build_dll

	#
	# For all the artifacts produced by the linker - AOT them
	#
	if [ $? -eq 0 ]; then
		#
		# We do mscorlib last as it takes the longest - especially in LLVMONLY mode
		SOS=`ls output/*.{exe,dll} | grep -v mscorlib`
		for so in ${SOS} 
		do
			build_so ${TYPE} ${so}
		done
		build_so output/mscorlib.dll
	fi
fi
