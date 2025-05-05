# Adding a new `syscall`

In all cases, use the format (style) of the file being changed.

## Edit `syscall_stublookup.c`

Locate the #if defined for the specified CONFIG variable.  Create a new one if it does not exist.

## Edit `syscall.csv`

Add a new entry for the specified function with the format:

* Function name
* Header file
* defined for the CONFIG variable
* Return type
* Parameter types

Keep the function names in alphabetical order.

## Edit the file `nuttx/include/sys/syscall.h`

Locate the definition for the CONFIG variable.  If it does not exist then add a new entry after the `#if defined(CONFIG_MEASURE_FREQUENCY_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)` statement.

Add and entry for the method using the same format as the rest of the file.

## Edit syscall_lookup.h

Locate the definition for the CONFIG variable.  If it does not exist then add a new one above CONFIG_ARCH_BOARD_MEADOW.

Add a new SYSCALL_LOOKUP entry indented by two spaces.  The format of the entry is:

* Function name
* Number of parameters
* STUB name as created in `syscall_stublookup.c`
