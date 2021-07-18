/****************************************************************************
 * \Meadow\Meadow.OS\apps\examples\sqlite\sqlite_compile_sqlite3.c
 * 
 *   Copyright (C) 2020 Wilderness Labs. All rights reserved.
 *   Author:  Wilderness Labs
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

// This file is used to BUILD sqlite3.c (see end of file)

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include<nuttx/config.h>
#if defined (CONFIG_EXAMPLES_MEADOW_SQLITE)

#define SQLITE_OS_OTHER 1

// Nuttx has a usleep function with millsec resolution
#define HAVE_USLEEP 1       

// 0 = single threaded, 1 = serialized, 2 = Multi-threaded
// The setting of 0 is an initial value to get things initially
// working. see https://sqlite.org/threadsafe.html for details
#define SQLITE_THREADSAFE 0

// Omit write-ahead log (optional to using journal files)
// If defined as 0 sqlite3.c will not compile. When not defined or set
// to 1 sqlite3.c will compile. Decided to make it explicit by defining
// as 1. I did not explore the root cause of why it wouldn't build. 
// Probably a number of settings need to agree.
// see https://sqlite.org/wal.html
#define SQLITE_OMIT_WAL 1

// Temporary files. The user must configure SQLite to use in-memory
// temp files when the Meadow VFS. The easiest way to do this is to
// compile with:
#define SQLITE_TEMP_STORE 3

// Below are the defines that which when not defined cause compiler
// warnings. An attempt was made to define these  but this seemed to
// be a rabbit hole. 
//
// #define SQLITE_32BIT_ROWID 0
// #define SQLITE_4_BYTE_ALIGNED_MALLOC 0
// #define SQLITE_64BIT_STATS 0
// #define SQLITE_ALLOW_COVERING_INDEX_SCAN 1 // 0 is deprecated
// #define SQLITE_ALLOW_URI_AUTHORITY 0
// #define SQLITE_BUG_COMPATIBLE_20160819 0
// #define SQLITE_CASE_SENSITIVE_LIKE 0
// #define SQLITE_CHECK_PAGES 0
// #define SQLITE_COVERAGE_TEST 0
// //#define SQLITE_DEBUG 0    // Do not define
// #define SQLITE_DEFAULT_AUTOMATIC_INDEX 0
// #define SQLITE_DEFAULT_AUTOVACUUM 0
// #define SQLITE_DEFAULT_CKPTFULLFSYNC 0
// #define SQLITE_DEFAULT_FOREIGN_KEYS 0
// #define SQLITE_DEFAULT_MEMSTATUS 0
// #define SQLITE_DEFAULT_RECURSIVE_TRIGGERS 0
// #define SQLITE_DIRECT_OVERFLOW_READ 0
// #define SQLITE_DISABLE_DIRSYNC 0
// #define SQLITE_DISABLE_FTS3_UNICODE 0
// #define SQLITE_DISABLE_FTS4_DEFERRED 0
// #define SQLITE_DISABLE_INTRINSIC 0
// #define SQLITE_DISABLE_LFS 0
// #define SQLITE_DISABLE_PAGECACHE_OVERFLOW_STATS 0
// #define SQLITE_DISABLE_SKIPAHEAD_DISTINCT 0
// #define SQLITE_ENABLE_API_ARMOR 0
// #define SQLITE_ENABLE_ATOMIC_WRITE 0
// #define SQLITE_ENABLE_BATCH_ATOMIC_WRITE 0
// #define SQLITE_ENABLE_BYTECODE_VTAB 0
// #define SQLITE_ENABLE_CEROD 0
// #define SQLITE_ENABLE_COLUMN_METADATA 0
// #define SQLITE_ENABLE_COLUMN_USED_MASK 0
// #define SQLITE_ENABLE_COSTMULT 0
// #define SQLITE_ENABLE_CURSOR_HINTS 0
// #define SQLITE_ENABLE_DBSTAT_VTAB 0
// #define SQLITE_ENABLE_EXPENSIVE_ASSERT 0
// #define SQLITE_ENABLE_FTS1 0
// #define SQLITE_ENABLE_FTS2 0
// #define SQLITE_ENABLE_FTS3 0
// #define SQLITE_ENABLE_FTS3_PARENTHESIS 0
// #define SQLITE_ENABLE_FTS3_TOKENIZER 0
// #define SQLITE_ENABLE_FTS4 0
// #define SQLITE_ENABLE_FTS5 0
// #define SQLITE_ENABLE_GEOPOLY 0
// #define SQLITE_ENABLE_HIDDEN_COLUMNS 0
// #define SQLITE_ENABLE_ICU 0
// #define SQLITE_ENABLE_IOTRACE 0
// #define SQLITE_ENABLE_JSON1 0
// #define SQLITE_ENABLE_LOAD_EXTENSION 0
// #define SQLITE_ENABLE_MEMORY_MANAGEMENT 0
// #define SQLITE_ENABLE_MEMSYS3 0
// #define SQLITE_ENABLE_MEMSYS5 0
// #define SQLITE_ENABLE_MULTIPLEX 0
// #define SQLITE_ENABLE_NORMALIZE 0
// #define SQLITE_ENABLE_NULL_TRIM 0
// #define SQLITE_ENABLE_OVERSIZE_CELL_CHECK 0
// #define SQLITE_ENABLE_PREUPDATE_HOOK 0
// #define SQLITE_ENABLE_QPSG 0
// #define SQLITE_ENABLE_RBU 0
// #define SQLITE_ENABLE_RTREE 0
// #define SQLITE_ENABLE_SELECTTRACE 0
// #define SQLITE_ENABLE_SESSION 0
// #define SQLITE_ENABLE_SNAPSHOT 0
// #define SQLITE_ENABLE_SORTER_REFERENCES 0
// #define SQLITE_ENABLE_SQLLOG 0
// #define SQLITE_ENABLE_STMTVTAB 0
// #define SQLITE_ENABLE_STMT_SCANSTATUS 0
// #define SQLITE_ENABLE_UNKNOWN_SQL_FUNCTION 0
// #define SQLITE_ENABLE_UNLOCK_NOTIFY 0
// #define SQLITE_ENABLE_UPDATE_DELETE_LIMIT 0
// #define SQLITE_ENABLE_URI_00_ERROR 0
// #define SQLITE_ENABLE_VFSTRACE 0
// #define SQLITE_ENABLE_WHERETRACE 0
// #define SQLITE_ENABLE_ZIPVFS 0
// #define SQLITE_EXPLAIN_ESTIMATED_ROWS 0
// #define SQLITE_EXTRA_IFNULLROW 0
// #define SQLITE_FTS5_ENABLE_TEST_MI 0
// #define SQLITE_FTS5_NO_WITHOUT_ROWID 0
// #define SQLITE_HOMEGROWN_RECURSIVE_MUTEX 0
// #define SQLITE_IGNORE_AFP_LOCK_ERRORS 0
// #define SQLITE_IGNORE_FLOCK_LOCK_ERRORS 0
// #define SQLITE_INLINE_MEMCPY 0
// #define SQLITE_INT64_TYPE 0
// #define SQLITE_LIKE_DOESNT_MATCH_BLOBS 0
// #define SQLITE_LOCK_TRACE 0
// #define SQLITE_LOG_CACHE_SPILL 0
// #define SQLITE_MEMDEBUG 0
// #define SQLITE_MIXED_ENDIAN_64BIT_FLOAT 0
// #define SQLITE_MMAP_READWRITE 0
// #define SQLITE_MUTEX_NOOP 0
// #define SQLITE_MUTEX_NREF 0
// #define SQLITE_MUTEX_OMIT 0
// #define SQLITE_MUTEX_PTHREADS 0
// #define SQLITE_MUTEX_W32 0
// #define SQLITE_NEED_ERR_NAME 0
// #define SQLITE_NOINLINE 0
// #define SQLITE_NO_SYNC 0
// #define SQLITE_OMIT_ALTERTABLE 0
// #define SQLITE_OMIT_ANALYZE 0
// #define SQLITE_OMIT_ATTACH 0
// #define SQLITE_OMIT_AUTHORIZATION 0
// #define SQLITE_OMIT_AUTOINCREMENT 0
// #define SQLITE_OMIT_AUTOINIT 0
// #define SQLITE_OMIT_AUTOMATIC_INDEX 0
// #define SQLITE_OMIT_AUTORESET 0
// #define SQLITE_OMIT_AUTOVACUUM 0
// #define SQLITE_OMIT_BETWEEN_OPTIMIZATION 0
// #define SQLITE_OMIT_BLOB_LITERAL 0
// #define SQLITE_OMIT_CAST 0
// #define SQLITE_OMIT_CHECK 0
// #define SQLITE_OMIT_COMPLETE 0
// #define SQLITE_OMIT_COMPOUND_SELECT 0
// #define SQLITE_OMIT_CONFLICT_CLAUSE 0
// #define SQLITE_OMIT_CTE 0
// #define SQLITE_OMIT_DATETIME_FUNCS 0
// #define SQLITE_OMIT_DECLTYPE 0
// #define SQLITE_OMIT_DEPRECATED 0
// #define SQLITE_OMIT_DISKIO 0
// #define SQLITE_OMIT_EXPLAIN 0
// #define SQLITE_OMIT_FLAG_PRAGMAS 0
// #define SQLITE_OMIT_FLOATING_POINT 0
// #define SQLITE_OMIT_FOREIGN_KEY 0
// #define SQLITE_OMIT_GET_TABLE 0
// #define SQLITE_OMIT_HEX_INTEGER 0
// #define SQLITE_OMIT_INCRBLOB 0
// #define SQLITE_OMIT_INTEGRITY_CHECK 0
// #define SQLITE_OMIT_LIKE_OPTIMIZATION 0
// #define SQLITE_OMIT_LOAD_EXTENSION 0
// #define SQLITE_OMIT_LOCALTIME 0
// #define SQLITE_OMIT_LOOKASIDE 0
// #define SQLITE_OMIT_MEMORYDB 0
// #define SQLITE_OMIT_OR_OPTIMIZATION 0
// #define SQLITE_OMIT_PAGER_PRAGMAS 0
// #define SQLITE_OMIT_PARSER_TRACE 0
// #define SQLITE_OMIT_POPEN 0
// #define SQLITE_OMIT_PRAGMA 0
// #define SQLITE_OMIT_PROGRESS_CALLBACK 0
// #define SQLITE_OMIT_QUICKBALANCE 0
// #define SQLITE_OMIT_REINDEX 0
// #define SQLITE_OMIT_SCHEMA_PRAGMAS 0
// #define SQLITE_OMIT_SCHEMA_VERSION_PRAGMAS 0
// #define SQLITE_OMIT_SHARED_CACHE 0
// #define SQLITE_OMIT_SHUTDOWN_DIRECTORIES 0
// #define SQLITE_OMIT_SUBQUERY 0
// #define SQLITE_OMIT_TCL_VARIABLE 0
// #define SQLITE_OMIT_TEMPDB 0
// #define SQLITE_OMIT_TEST_CONTROL 0
// #define SQLITE_OMIT_TRACE 0
// #define SQLITE_OMIT_TRIGGER 0
// #define SQLITE_OMIT_TRUNCATE_OPTIMIZATION 0
// #define SQLITE_OMIT_UTF16 0
// #define SQLITE_OMIT_VACUUM 0
// #define SQLITE_OMIT_VIEW 0
// #define SQLITE_OMIT_VIRTUALTABLE 0
// #define SQLITE_OMIT_WSD 0
// #define SQLITE_OMIT_XFER_OPT 0
// #define SQLITE_PCACHE_SEPARATE_HEADER 0
// #define SQLITE_PERFORMANCE_TRACE 0
// #define SQLITE_POWERSAFE_OVERWRITE 0
// #define SQLITE_PREFER_PROXY_LOCKING 0
// #define SQLITE_PROXY_DEBUG 0
// #define SQLITE_REVERSE_UNORDERED_SELECTS 0
// #define SQLITE_RTREE_INT_ONLY 0
// #define SQLITE_SECURE_DELETE 0
// #define SQLITE_SMALL_STACK 0
// #define SQLITE_SOUNDEX 0
// #define SQLITE_SUBSTR_COMPATIBILITY 0
// #define SQLITE_SYSTEM_MALLOC 0
// #define SQLITE_TCL 0
// #define SQLITE_TEST 0
// #define SQLITE_UNLINK_AFTER_CLOSE 0
// #define SQLITE_UNTESTABLE 0
// #define SQLITE_USER_AUTHENTICATION 0
// #define SQLITE_USE_ALLOCA 0
// #define SQLITE_USE_FCNTL_TRACE 0
// #define SQLITE_USE_URI 0
// #define SQLITE_VDBE_COVERAGE 0
// #define SQLITE_WIN32_MALLOC 0
// #define SQLITE_ZERO_MALLOC 0
// #define SQLITE_ENABLE_HIDDEN_COLUMNS 0
// #define HAVE_MALLOC_H 0 && HAVE_MALLOC_USABLE_SIZE
// #define HAVE_STRCHRNUL 0
// #define SQLITE_ENABLE_STAT4 0
// #define SQLITE_OMIT_SUBQUERY 0
// #define SQLITE_USER_AUTHENTICATION 0
// #define SQLITE_ENABLE_HIDDEN_COLUMNS 0
// #define SQLITE_USER_AUTHENTICATION 0
// #define WHERETRACE_ENABLED 0 /* 0x20800 */
// #define WHERETRACE_ENABLED 0
// #define WHERETRACE_ENABLED 0 /* 0x8 */
// #define SQLITE_USER_AUTHENTICATION 0
// #define SQLITE_DEFAULT_CKPTFULLFSYNC 0
// #define SQLITE_ENABLE_API_ARMOR 0

// https://sqlite.org/testing.html section 11
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wundef"

// These are called by initialization, but are not defined unless SQLITE_OS_UNIX is defined
// If you define SQLITE_OS_UNIX, it fails because Nuttx doesn't implement things like fchmod
int sqlite3_os_init(void) { return 0; }
int sqlite3_os_end(void) { return 0; }

// Compile sqlite
#include "sqlite3.c"

#pragma GCC diagnostic pop

#endif 