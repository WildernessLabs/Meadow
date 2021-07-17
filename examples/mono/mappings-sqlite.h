#ifndef __INC_SQLITE_FUNCTIONS__
#define __INC_SQLITE_FUNCTIONS__

#include "../sqlite/sqlite3.h"

extern int sqlite3_open(const char *, void **);
extern int sqlite3_close(void *);
extern int sqlite3_close_v2(void *);
extern int sqlite3_enable_shared_cache(int enable);
extern void sqlite3_interrupt(void *);
extern int sqlite3_finalize(void *);
extern int sqlite3_reset(void *);
extern int sqlite3_clear_bindings(void*);
extern int sqlite3_stmt_status(void*, int op, int resetFlg);
extern char* sqlite3_bind_parameter_name(void*, int index);
extern char* sqlite3_column_database_name(void*, int index);
extern char* sqlite3_column_decltype(void*, int index);
extern char* sqlite3_column_name(void*, int index);
extern char* sqlite3_column_origin_name(void*, int index);
extern char* sqlite3_column_table_name(void*, int index);
extern char* sqlite3_column_text(void*, int index);
extern char* sqlite3_errmsg(void *);

// NOTE: commented out definitions appear in the sqlite_pcl_raw P/Invokes, but yield "undefined symbol" when compiling here
MonoDlMapping sqlite_mappings[] = {
	{ "sqlite3_open", sqlite3_open },
	{ "sqlite3_close", sqlite3_close },
	{ "sqlite3_close_v2", sqlite3_close_v2 },
	{ "sqlite3_enable_shared_cache", sqlite3_enable_shared_cache },
	{ "sqlite3_interrupt", sqlite3_interrupt },
	{ "sqlite3_finalize", sqlite3_finalize },
	{ "sqlite3_reset", sqlite3_reset },
	{ "sqlite3_clear_bindings", sqlite3_clear_bindings },
	{ "sqlite3_stmt_status", sqlite3_stmt_status },
	{ "sqlite3_bind_parameter_name", sqlite3_bind_parameter_name },
//	{ "sqlite3_column_database_name", sqlite3_column_database_name },
	{ "sqlite3_column_decltype", sqlite3_column_decltype },
	{ "sqlite3_column_name", sqlite3_column_name },
//	{ "sqlite3_column_origin_name", sqlite3_column_origin_name },
//	{ "sqlite3_column_table_name", sqlite3_column_table_name },
	{ "sqlite3_column_text", sqlite3_column_text },
	{ "sqlite3_errmsg", sqlite3_errmsg },
    { NULL, NULL }
};

/*
extern int sqlite3_clear_bindings(void*);
extern int sqlite3_stmt_status(sqlite3_stmt stm, int op, int resetFlg);
extern char* sqlite3_bind_parameter_name(void*, int index);
extern char* sqlite3_column_database_name(void*, int index);
extern char* sqlite3_column_decltype(void*, int index);
extern char* sqlite3_column_name(void*, int index);
extern char* sqlite3_column_origin_name(void*, int index);
extern char* sqlite3_column_table_name(void*, int index);
extern char* sqlite3_column_text(void*, int index);
extern char* sqlite3_errmsg(void *);

extern int sqlite3_db_readonly(void *, char* dbName);
extern char* sqlite3_db_filename(void *, char* att);
extern int sqlite3_prepare_v2(void *, char* pSql, int nBytes, out IntPtr stmt, out char* ptrRemain);
extern int sqlite3_prepare_v3(void *, char* pSql, int nBytes, uint flags, out IntPtr stmt, out char* ptrRemain);
extern int sqlite3_db_status(void *, int op, out int current, out int highest, int resetFlg);
extern int sqlite3_complete(char* pSql);
extern int sqlite3_compileoption_used(char* pSql);
extern char* sqlite3_compileoption_get(int n);
extern int sqlite3_table_column_metadata(void *, char* dbName, char* tblName, char* colName, out char* ptrDataType, out char* ptrCollSeq, out int notNull, out int primaryKey, out int autoInc);
extern char* sqlite3_value_text(IntPtr p);
extern int sqlite3_enable_load_extension(void *, int enable);
extern int sqlite3_limit(void *, int id, int newVal);
extern int sqlite3_initialize();
extern int sqlite3_shutdown();
extern char* sqlite3_libversion();
extern int sqlite3_libversion_number();
extern int sqlite3_threadsafe();
extern char* sqlite3_sourceid();
extern IntPtr sqlite3_malloc(int n);
extern IntPtr sqlite3_realloc(IntPtr p, int n);
extern void sqlite3_free(IntPtr p);
extern int sqlite3_stricmp(IntPtr p, IntPtr q);
extern int sqlite3_strnicmp(IntPtr p, IntPtr q, int n);
extern int sqlite3_open(char* filename, out IntPtr db);
extern int sqlite3_open_v2(char* filename, out IntPtr db, int flags, char* vfs);
extern IntPtr sqlite3_vfs_find(char* vfs);
extern long sqlite3_last_insert_rowid(void *);
extern int sqlite3_changes(void *);
extern int sqlite3_total_changes(void *);
extern long sqlite3_memory_used();
extern long sqlite3_memory_highwater(int resetFlag);
extern long sqlite3_soft_heap_limit64(long n);
extern long sqlite3_hard_heap_limit64(long n);
extern int sqlite3_status(int op, out int current, out int highwater, int resetFlag);
extern int sqlite3_busy_timeout(void *, int ms);
extern int sqlite3_bind_blob(void*, int index, char* val, int nSize, IntPtr nTransient);
extern int sqlite3_bind_zeroblob(void*, int index, int size);
extern int sqlite3_bind_double(void*, int index, double val);
extern int sqlite3_bind_int(void*, int index, int val);
extern int sqlite3_bind_int64(void*, int index, long val);
extern int sqlite3_bind_null(void*, int index);
extern int sqlite3_bind_text(void*, int index, char* val, int nlen, IntPtr pvReserved);
extern int sqlite3_bind_text16(void*, int index, char* val, int nlen, IntPtr pvReserved);
extern int sqlite3_bind_parameter_count(void*);
extern int sqlite3_bind_parameter_index(void*, char* strName);
extern int sqlite3_column_count(void*);
extern int sqlite3_data_count(void*);
extern int sqlite3_step(void*);
extern char* sqlite3_sql(void*);
extern double sqlite3_column_double(void*, int index);
extern int sqlite3_column_int(void*, int index);
extern long sqlite3_column_int64(void*, int index);
extern IntPtr sqlite3_column_blob(void*, int index);
extern int sqlite3_column_bytes(void*, int index);
extern int sqlite3_column_type(void*, int index);
extern int sqlite3_aggregate_count(IntPtr context);
extern IntPtr sqlite3_value_blob(IntPtr p);
extern int sqlite3_value_bytes(IntPtr p);
extern double sqlite3_value_double(IntPtr p);
extern int sqlite3_value_int(IntPtr p);
extern long sqlite3_value_int64(IntPtr p);
extern int sqlite3_value_type(IntPtr p);
extern IntPtr sqlite3_user_data(IntPtr context);
extern void sqlite3_result_blob(IntPtr context, IntPtr val, int nSize, IntPtr pvReserved);
extern void sqlite3_result_double(IntPtr context, double val);
extern void sqlite3_result_error(IntPtr context, char* strErr, int nLen);
extern void sqlite3_result_int(IntPtr context, int val);
extern void sqlite3_result_int64(IntPtr context, long val);
extern void sqlite3_result_null(IntPtr context);
extern void sqlite3_result_text(IntPtr context, char* val, int nLen, IntPtr pvReserved);
extern void sqlite3_result_zeroblob(IntPtr context, int n);
extern void sqlite3_result_error_toobig(IntPtr context);
extern void sqlite3_result_error_nomem(IntPtr context);
extern void sqlite3_result_error_code(IntPtr context, int code);
extern IntPtr sqlite3_aggregate_context(IntPtr context, int nBytes);

[DllImport(SQLITE_DLL, ExactSpelling=true, EntryPoint = "sqlite3_config", CallingConvention = CALLING_CONVENTION)]
extern int sqlite3_config_none(int op);

[DllImport(SQLITE_DLL, ExactSpelling=true, EntryPoint = "sqlite3_config", CallingConvention = CALLING_CONVENTION)]
extern int sqlite3_config_int(int op, int val);

[DllImport(SQLITE_DLL, ExactSpelling=true, EntryPoint = "sqlite3_config", CallingConvention = CALLING_CONVENTION)]
extern int sqlite3_config_log(int op, NativeMethods.callback_log func, hook_handle pvUser);

[DllImport(SQLITE_DLL, ExactSpelling=true, EntryPoint = "sqlite3_db_config", CallingConvention = CALLING_CONVENTION)]
extern int sqlite3_db_config_charptr(void *, int op, char* val);

[DllImport(SQLITE_DLL, ExactSpelling=true, EntryPoint = "sqlite3_db_config", CallingConvention = CALLING_CONVENTION)]
extern int sqlite3_db_config_int_outint(void *, int op, int val, int* result);

[DllImport(SQLITE_DLL, ExactSpelling=true, EntryPoint = "sqlite3_db_config", CallingConvention = CALLING_CONVENTION)]
extern int sqlite3_db_config_intptr_int_int(void *, int op, IntPtr ptr, int int0, int int1);
extern int sqlite3_create_collation(void *, byte[] strName, int nType, hook_handle pvUser, NativeMethods.callback_collation func);
extern IntPtr sqlite3_update_hook(void *, NativeMethods.callback_update func, hook_handle pvUser);
extern IntPtr sqlite3_commit_hook(void *, NativeMethods.callback_commit func, hook_handle pvUser);
extern IntPtr sqlite3_profile(void *, NativeMethods.callback_profile func, hook_handle pvUser);
extern void sqlite3_progress_handler(void *, int instructions, NativeMethods.callback_progress_handler func, hook_handle pvUser);
extern IntPtr sqlite3_trace(void *, NativeMethods.callback_trace func, hook_handle pvUser);
extern IntPtr sqlite3_rollback_hook(void *, NativeMethods.callback_rollback func, hook_handle pvUser);
extern IntPtr sqlite3_db_handle(IntPtr stmt);
extern IntPtr sqlite3_next_stmt(void *, IntPtr stmt);
extern int sqlite3_stmt_isexplain(void*);
extern int sqlite3_stmt_busy(void*);
extern int sqlite3_stmt_readonly(void*);
extern int sqlite3_exec(void *, char* strSql, NativeMethods.callback_exec cb, hook_handle pvParam, out IntPtr errMsg);
extern int sqlite3_get_autocommit(void *);
extern int sqlite3_extended_result_codes(void *, int onoff);
extern int sqlite3_errcode(void *);
extern int sqlite3_extended_errcode(void *);
extern char* sqlite3_errstr(int rc);
extern void sqlite3_log(int iErrCode, char* zFormat);
extern int sqlite3_file_control(void *, byte[] zDbName, int op, IntPtr pArg);
extern sqlite3_backup sqlite3_backup_init(sqlite3 destDb, char* zDestName, sqlite3 sourceDb, char* zSourceName);
extern int sqlite3_backup_step(sqlite3_backup backup, int nPage);
extern int sqlite3_backup_remaining(sqlite3_backup backup);
extern int sqlite3_backup_pagecount(sqlite3_backup backup);
extern int sqlite3_backup_finish(IntPtr backup);
extern int sqlite3_snapshot_get(void *, char* schema, out IntPtr snap);
extern int sqlite3_snapshot_open(void *, char* schema, sqlite3_snapshot snap);
extern int sqlite3_snapshot_recover(void *, char* name);
extern int sqlite3_snapshot_cmp(sqlite3_snapshot p1, sqlite3_snapshot p2);
extern void sqlite3_snapshot_free(IntPtr snap);
extern int sqlite3_blob_open(void *, char* sdb, char* table, char* col, long rowid, int flags, out IntPtr blob);
extern int sqlite3_blob_write(sqlite3_blob blob, char* b, int n, int offset);
extern int sqlite3_blob_read(sqlite3_blob blob, char* b, int n, int offset);
extern int sqlite3_blob_bytes(sqlite3_blob blob);
extern int sqlite3_blob_reopen(sqlite3_blob blob, long rowid);
extern int sqlite3_blob_close(IntPtr blob);
extern int sqlite3_wal_autocheckpoint(void *, int n);
extern int sqlite3_wal_checkpoint(void *, char* dbName);
extern int sqlite3_wal_checkpoint_v2(void *, char* dbName, int eMode, out int logSize, out int framesCheckPointed);
extern int sqlite3_set_authorizer(void *, NativeMethods.callback_authorizer cb, hook_handle pvUser);
extern int sqlite3_create_function_v2(void *, byte[] strName, int nArgs, int nType, hook_handle pvUser, NativeMethods.callback_scalar_function func, NativeMethods.callback_agg_function_step fstep, NativeMethods.callback_agg_function_final ffinal, NativeMethods.callback_destroy fdestroy);
extern int sqlite3_keyword_count();
extern int sqlite3_keyword_name(int i, out char* name, out int length);
*/
#endif // __INC_SQLITE_FUNCTIONS__
