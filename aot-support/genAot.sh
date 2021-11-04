#!/bin/bash


#
# Use monolinker to produce a subset of the DLLs needed at runtime
#
function build_dll()
{
	echo "Running the mono linker"
	export PATH=/opt/mono/bin:${OLD_PATH}
	RC=0
	ARGS=`ls $* | awk '{print "-a "$1" "}'`
	monolinker -l all -c link -o ${OUTPUT} ${ARGS[@]}
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
	export MONO_PATH=${OUTPUT}
	if [ ${KEEP} -eq 1 ]; then
		KEEPOPT=",keep-temps"
	else
		KEEPOPT=""
	fi
	MONO_LDFLAGS="${LDFLAGS} -Map=${OUTPUT}/${FN}.map"
	mono --aot="llvm,${LLVMOPTS},ld-flags=${MONO_LDFLAGS}${KEEPOPT}" ${OUTPUT}/${FN}
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
	export MONO_PATH=${OUTPUT}
	if [ ${KEEP} -eq 1 ]; then
		KEEPOPT="keep-temps"
	else
		KEEPOPT=""
	fi
	mono --aot="${KEEPOPT}" ${OUTPUT}/${FN}
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
	export MONO_PATH=${OUTPUT}
	mono --aot="llvmonly,asmonly,interp,llvm-outfile=${OUTPUT}/${FN}.bc,llvmllc=-exception-model=dwarf" ${OUTPUT}/${FN}
	if [ -f ${OUTPUT}/${FN}.bc ]; then
		echo "Clanging ${FN}"
		clang   -fpic -O1 -fno-optimize-sibling-calls -Wno-override-module --target=thumb2-none-eabi \
			-mcpu=cortex-m7 -mfloat-abi=hard -mfpu=fpv5-d16 -c -o ${OUTPUT}/${FN}.o ${OUTPUT}/${FN}.bc
		if [ -f ${OUTPUT}/${FN}.o ]; then
			if [ ${KEEP} -eq 0 ];then 
				rm -f ${OUTPUT}/${FN}.bc
			fi
			echo "Linking ${FN}"
			arm-none-eabi-ld -shared -G -Bsymbolic -z max-page-size=1024 \
				-E \
				-z common-page-size=1024 --no-dynamic-linker -pie \
				--strip-debug -no-enum-size-warning \
				--export-dynamic-symbol=mono_aot_* \
				-z combreloc \
				-o ${OUTPUT}/${FN}.so ${OUTPUT}/${FN}.o \
				--entry mono_aot_file_info \
				--start-group \
				-L${PWD} -lbuiltin \
				-L${ARM_LIB} -lsupc++ \
				-L${ARM_GCCLIB} -lgcc \
				--end-group -Map=${OUTPUT}/${FN}.map
			RC=$?
			if [ -f ${OUTPUT}/${FN}.so ]; then
				if [ ${KEEP} -eq 0 ];then 
					rm -f ${OUTPUT}/${FN}.o
				fi
				arm-none-eabi-strip --wildcard --strip-symbol=\$a.* \
					--strip-symbol=\$d.* --strip-symbol=\$t.* \
					--strip-symbol=\$a --strip-symbol=\$d --strip-symbol=\$t ${OUTPUT}/${FN}.so
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
	echo "genAOT -t <type> -k -o <dir> <input> ..." >&2
	echo >&2
	echo "where:" >&2
	echo "   -t <type>   Type of AOT: 'llvm', 'jit', 'llvmonly'" >&2
	echo "   -o <dir>    Output directory" >&2
	echo "   -k          Keep any intermediate files" >&2
        echo >&2
	echo "This script will put the input files through the mono linker and then" >&2
	echo "put the resulting artifacts through the AOT process" >&2
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
OUTPUT=""
while getopts "t:o:k" opt
do
	case "${opt}" in
	o)
		OUTPUT=${OPTARG}
		;;
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

if [ $# -eq 0 ]; then
	usage "No input files specified"
fi

if [ -z "${OUTPUT}" ]; then
	usage "Missing output directory"
fi

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

if [ ! -d "${OUTPUT}" ]; then
	RSP=`mkdir -p ${OUTPUT} 2>&1`
	if [ $? -ne 0 ]; then
		usage ${RSP}
	fi
fi

rm -rf ${OUTPUT}/*
build_dll $*

#
# For all the artifacts produced by the linker - AOT them
#
if [ $? -eq 0 ]; then
	#
	# We do mscorlib last as it takes the longest - especially in LLVMONLY mode
	SOS=`ls ${OUTPUT}/*.{exe,dll} | grep -v mscorlib`
	for so in ${SOS} 
	do
		build_so ${TYPE} ${so}
	done
	build_so ${OUTPUT}/mscorlib.dll
fi
