set -ex
make -C ../../../nuttx/tools -f Makefile.host mksymtab
cat ../../../nuttx/syscall/syscall.csv ../../../nuttx/libc/libc.csv | sort > symtab.csv
../../../nuttx/tools/mksymtab symtab.csv symtab.inc
rm symtab.csv
