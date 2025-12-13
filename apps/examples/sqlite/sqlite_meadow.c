#include "sqlite3.h"
#include "syslog.h"

// These are called by initialization, but are not defined unless SQLITE_OS_UNIX is defined
// If you define SQLITE_OS_UNIX, it fails because Nuttx doesn't implement things like fchmod
SQLITE_API sqlite3_os_init(void) 
{
    // we must register the `demovfs` that Meadow uses
    sqlite3_vfs *pVfs = sqlite3_demovfs();
    if(pVfs == 0)
    {
        syslog(LOG_ERR, "Failed to create sqlite3_demovfs\n");
        return SQLITE_ERROR;
    }
    int ret = sqlite3_vfs_register(pVfs, 1);
    if(ret)
    {
        syslog(LOG_ERR, "Failed to register sqlite3_demovfs\n");
        return SQLITE_ERROR;
    }

    return SQLITE_OK; 
}

SQLITE_API sqlite3_os_end(void) 
{ 
    return SQLITE_OK; 
}