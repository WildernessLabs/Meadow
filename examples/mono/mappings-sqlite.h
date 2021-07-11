#ifndef __INC_SQLITE_FUNCTIONS__
#define __INC_SQLITE_FUNCTIONS__

#include "../sqlite/sqlite3.h"

extern int sqlite3_open(const char *, struct sqlite3 **);
extern int sqlite3_os_init(void);

MonoDlMapping sqlite_mappings[] = {
	{ "sqlite3_open", sqlite3_open },
	{ "sqlite3_os_init", sqlite3_os_init },
    { NULL, NULL }
};

#endif // __INC_SQLITE_FUNCTIONS__
