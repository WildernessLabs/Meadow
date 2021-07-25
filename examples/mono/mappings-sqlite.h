#ifndef __INC_SQLITE_FUNCTIONS__
#define __INC_SQLITE_FUNCTIONS__

#include "../sqlite/sqlite3.h"

extern int sqlite3_close(sqlite3 *);
extern int sqlite3_close_v2(sqlite3 *);
extern int sqlite3_enable_shared_cache(int enable);
extern void sqlite3_interrupt(sqlite3 *);
extern int sqlite3_finalize(sqlite3_stmt *);
extern int sqlite3_reset(sqlite3_stmt *);
extern int sqlite3_clear_bindings(sqlite3_stmt*);
extern int sqlite3_stmt_status(sqlite3_stmt*, int op, int resetFlg);
extern const char* sqlite3_bind_parameter_name(sqlite3_stmt*, int index);
extern const char* sqlite3_column_database_name(sqlite3_stmt*, int index);
extern const char* sqlite3_column_decltype(sqlite3_stmt*, int index);
extern const char* sqlite3_column_name(sqlite3_stmt*, int index);
extern const void *sqlite3_column_name16(sqlite3_stmt*, int N);
extern const char* sqlite3_column_origin_name(sqlite3_stmt*, int index);
extern const char* sqlite3_column_table_name(sqlite3_stmt*, int index);
extern const unsigned char* sqlite3_column_text(sqlite3_stmt*, int index);
extern const char* sqlite3_errmsg(sqlite3 *);
extern int sqlite3_db_readonly(sqlite3 *, const char* dbName);
extern const char* sqlite3_db_filename(sqlite3 *, const char* att);
extern int sqlite3_prepare_v2(sqlite3 *, const char* pSql, int nBytes, sqlite3_stmt** stmt, const char** ptrRemain);
extern int sqlite3_prepare_v3(sqlite3 *, const char* pSql, int nBytes, uint flags, sqlite3_stmt** stmt, const char** ptrRemain);
extern int sqlite3_db_status(sqlite3 *, int op, int* current, int* highest, int resetFlg);
extern int sqlite3_complete(const char* pSql);
extern int sqlite3_compileoption_used(const char* pSql);
extern const char* sqlite3_compileoption_get(int n);
extern int sqlite3_table_column_metadata(sqlite3 *, const char* dbName, const char* tblName, const char* colName, const char** ptrDataType, const char** ptrCollSeq, int* notNull, int* primaryKey, int* autoInc);
extern const unsigned char* sqlite3_value_text(sqlite3_value * p);
extern int sqlite3_enable_load_extension(sqlite3 *, int enable);
extern int sqlite3_limit(sqlite3 *, int id, int newVal);
extern int sqlite3_initialize(void);
extern int sqlite3_shutdown(void);
extern const char* sqlite3_libversion(void);
extern int sqlite3_libversion_number(void);
extern int sqlite3_threadsafe(void);
extern const char* sqlite3_sourceid(void);
extern void* sqlite3_malloc(int n);
extern void* sqlite3_realloc(void* p, int n);
extern void sqlite3_free(void* p);
extern int sqlite3_stricmp(const char* p, const char* q);
extern int sqlite3_strnicmp(const char* p, const char* q, int n);
extern int sqlite3_open(const char* filename, sqlite3** db);
extern int sqlite3_open_v2(const char* filename, sqlite3** db, int flags, const char* vfs);
extern sqlite3_vfs* sqlite3_vfs_find(const char* vfs);
extern sqlite3_int64 sqlite3_last_insert_rowid(sqlite3 *);
extern int sqlite3_changes(sqlite3 *);
extern int sqlite3_total_changes(sqlite3 *);
extern sqlite3_int64 sqlite3_memory_used();
extern sqlite3_int64 sqlite3_memory_highwater(int resetFlag);
extern sqlite3_int64 sqlite3_soft_heap_limit64(sqlite3_int64 n);
extern sqlite3_int64 sqlite3_hard_heap_limit64(sqlite3_int64 n);
extern int sqlite3_status(int op, int* current, int* highwater, int resetFlag);
extern int sqlite3_busy_timeout(sqlite3 *, int ms);
extern int sqlite3_bind_blob(sqlite3_stmt*, int index, const void* val, int nSize, void(*)(void*));
extern int sqlite3_bind_zeroblob(sqlite3_stmt*, int index, int size);
extern int sqlite3_bind_double(sqlite3_stmt*, int index, double val);
extern int sqlite3_bind_int(sqlite3_stmt*, int index, int val);
extern int sqlite3_bind_int64(sqlite3_stmt*, int index, sqlite3_int64 val);
extern int sqlite3_bind_null(sqlite3_stmt*, int index);
extern int sqlite3_bind_text(sqlite3_stmt*,int,const char*,int,void(*)(void*));
extern int sqlite3_bind_text16(sqlite3_stmt*, int, const void*, int, void(*)(void*));
extern int sqlite3_bind_parameter_count(sqlite3_stmt*);
extern int sqlite3_bind_parameter_index(sqlite3_stmt*, const char* strName);
extern int sqlite3_column_count(sqlite3_stmt*);
extern int sqlite3_data_count(sqlite3_stmt*);
extern int sqlite3_step(sqlite3_stmt*);
extern const char* sqlite3_sql(sqlite3_stmt*);
extern double sqlite3_column_double(sqlite3_stmt*, int index);
extern int sqlite3_column_int(sqlite3_stmt*, int index);
extern sqlite3_int64 sqlite3_column_int64(sqlite3_stmt*, int index);
extern const void* sqlite3_column_blob(sqlite3_stmt*, int index);
extern int sqlite3_column_bytes(sqlite3_stmt*, int index);
extern int sqlite3_column_type(sqlite3_stmt*, int index);
extern int sqlite3_aggregate_count(sqlite3_context* context);
extern const void* sqlite3_value_blob(sqlite3_value* p);
extern int sqlite3_value_bytes(sqlite3_value* p);
extern double sqlite3_value_double(sqlite3_value* p);
extern int sqlite3_value_int(sqlite3_value* p);
extern sqlite3_int64 sqlite3_value_int64(sqlite3_value* p);
extern int sqlite3_value_type(sqlite3_value* p);
extern void* sqlite3_user_data(sqlite3_context* context);
extern void sqlite3_result_blob(sqlite3_context*, const void*, int, void(*)(void*));
extern void sqlite3_result_double(sqlite3_context* context, double val);
extern void sqlite3_result_error(sqlite3_context* context, const char* strErr, int nLen);
extern void sqlite3_result_int(sqlite3_context* context, int val);
extern void sqlite3_result_int64(sqlite3_context* context, sqlite3_int64 val);
extern void sqlite3_result_null(sqlite3_context* context);
extern void sqlite3_result_text(sqlite3_context*, const char*, int, void(*)(void*));
extern void sqlite3_result_zeroblob(sqlite3_context* context, int n);
extern void sqlite3_result_error_toobig(sqlite3_context* context);
extern void sqlite3_result_error_nomem(sqlite3_context* context);
extern void sqlite3_result_error_code(sqlite3_context* context, int code);
extern void* sqlite3_aggregate_context(sqlite3_context* context, int nBytes);
extern int sqlite3_config(int op, ...);
extern int sqlite3_db_config(sqlite3 *, int op, ...);
extern int sqlite3_create_collation(sqlite3 *, const char *zName, int eTextRep, void *pArg, int(*xCompare)(void*,int,const void*,int,const void*));
extern void* sqlite3_update_hook(sqlite3 *, void(*)(void *,int , char const *, char const *,sqlite3_int64),void*);
extern void* sqlite3_commit_hook(sqlite3 *, int(*)(void*), void*);
extern void* sqlite3_profile(sqlite3 *, void(*xProfile)(void*,const char*,sqlite3_uint64), void*);
extern void sqlite3_progress_handler(sqlite3 *, int, int(*)(void*), void*);
extern void* sqlite3_trace(sqlite3 *, void(*xTrace)(void*,const char*), void*);
extern void* sqlite3_rollback_hook(sqlite3 *, void(*)(void *), void*);
extern sqlite3* sqlite3_db_handle(sqlite3_stmt* stmt);
extern sqlite3_stmt* sqlite3_next_stmt(sqlite3 *, sqlite3_stmt*);
extern int sqlite3_stmt_isexplain(sqlite3_stmt*);
extern int sqlite3_stmt_busy(sqlite3_stmt*);
extern int sqlite3_stmt_readonly(sqlite3_stmt*);
extern int sqlite3_exec(sqlite3 *, const char* strSql, int (*callback)(void*,int,char**,char**), void*, char**);
extern int sqlite3_get_autocommit(sqlite3 *);
extern int sqlite3_extended_result_codes(sqlite3 *, int onoff);
extern int sqlite3_errcode(sqlite3 *);
extern int sqlite3_extended_errcode(sqlite3 *);
extern const char* sqlite3_errstr(int rc);
extern void sqlite3_log(int iErrCode, const char* zFormat, ...);
extern int sqlite3_file_control(sqlite3 *, const char* zDbName, int op, void* pArg);
extern sqlite3_backup* sqlite3_backup_init(sqlite3* destDb, const char* zDestName, sqlite3* sourceDb, const char* zSourceName);
extern int sqlite3_backup_step(sqlite3_backup* backup, int nPage);
extern int sqlite3_backup_remaining(sqlite3_backup* backup);
extern int sqlite3_backup_pagecount(sqlite3_backup* backup);
extern int sqlite3_backup_finish(sqlite3_backup* backup);
//extern int sqlite3_snapshot_get(void *, char* schema, void** snap);
//extern int sqlite3_snapshot_open(void *, char* schema, void* snap);
//extern int sqlite3_snapshot_recover(void *, char* name);
//extern int sqlite3_snapshot_cmp(void* p1, void* p2);
//extern void sqlite3_snapshot_free(void* snap);
extern int sqlite3_blob_open(sqlite3 *, const char* sdb, const char* table, const char* col, sqlite3_int64 rowid, int flags, sqlite3_blob** blob);
extern int sqlite3_blob_write(sqlite3_blob *, const void *z, int n, int iOffset);
extern int sqlite3_blob_read(sqlite3_blob *, void *Z, int N, int iOffset);
extern int sqlite3_blob_bytes(sqlite3_blob* blob);
extern int sqlite3_blob_reopen(sqlite3_blob* blob, sqlite3_int64 rowid);
extern int sqlite3_blob_close(sqlite3_blob* blob);
extern int sqlite3_wal_autocheckpoint(sqlite3 *, int n);
extern int sqlite3_wal_checkpoint(sqlite3 *, const char* dbName);
extern int sqlite3_wal_checkpoint_v2(sqlite3 *, const char* dbName, int eMode, int* logSize, int* framesCheckPointed);
extern int sqlite3_set_authorizer(sqlite3 *, int (*xAuth)(void*,int,const char*,const char*,const char*,const char*), void*);
extern int sqlite3_create_function_v2(sqlite3 *, const char* strName, int nArgs, int nType, void*, void (*xFunc)(sqlite3_context*,int,sqlite3_value**), void (*xStep)(sqlite3_context*,int,sqlite3_value**), void (*xFinal)(sqlite3_context*), void(*xDestroy)(void*));
extern int sqlite3_keyword_count();
extern int sqlite3_keyword_name(int i, const char** name, int* length);

extern sqlite3_vfs * sqlite3_demovfs(void);
extern int sqlite3_vfs_register(sqlite3_vfs *pVfs, int makeDflt);

// NOTE: commented out definitions appear in the sqlite_pcl_raw P/Invokes, but yield "undefined symbol" when compiling here
MonoDlMapping sqlite_mappings[] = {
	{ "sqlite3_demovfs", sqlite3_demovfs },
	{ "sqlite3_vfs_register", sqlite3_vfs_register },

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
	{ "sqlite3_column_name16", sqlite3_column_name16 },	
//	{ "sqlite3_column_origin_name", sqlite3_column_origin_name },
//	{ "sqlite3_column_table_name", sqlite3_column_table_name },
	{ "sqlite3_column_text", sqlite3_column_text },
	{ "sqlite3_errmsg", sqlite3_errmsg },
	{ "sqlite3_db_filename", sqlite3_db_filename },
	{ "sqlite3_prepare_v2", sqlite3_prepare_v2 },
	{ "sqlite3_prepare_v3", sqlite3_prepare_v3 },
	{ "sqlite3_db_status", sqlite3_db_status },
	{ "sqlite3_complete", sqlite3_complete },
	{ "sqlite3_compsqlite3_compileoption_usedlete", sqlite3_compileoption_used },
	{ "sqlite3_compileoption_get", sqlite3_compileoption_get },
	{ "sqlite3_table_column_metadata", sqlite3_table_column_metadata },
	{ "sqlite3_value_text", sqlite3_value_text },
	{ "sqlite3_enable_load_extension", sqlite3_enable_load_extension },
	{ "sqlite3_limit", sqlite3_limit },
	{ "sqlite3_initialize", sqlite3_initialize },
	{ "sqlite3_shutdown", sqlite3_shutdown },
	{ "sqlite3_libversion", sqlite3_libversion },
	{ "sqlite3_libversion_number", sqlite3_libversion_number },
	{ "sqlite3_threadsafe", sqlite3_threadsafe },
	{ "sqlite3_sourceid", sqlite3_sourceid },
	{ "sqlite3_malloc", sqlite3_malloc },
	{ "sqlite3_realloc", sqlite3_realloc },
	{ "sqlite3_free", sqlite3_free },
	{ "sqlite3_stricmp", sqlite3_stricmp },
	{ "sqlite3_strnicmp", sqlite3_strnicmp },
	{ "sqlite3_open", sqlite3_open },
	{ "sqlite3_open_v2", sqlite3_open_v2 },
	{ "sqlite3_vfs_find", sqlite3_vfs_find },
	{ "sqlite3_last_insert_rowid", sqlite3_last_insert_rowid },
	{ "sqlite3_changes", sqlite3_changes },
	{ "sqlite3_total_changes", sqlite3_total_changes },
	{ "sqlite3_memory_used", sqlite3_memory_used },
	{ "sqlite3_memory_highwater", sqlite3_memory_highwater },
	{ "sqlite3_soft_heap_limit64", sqlite3_soft_heap_limit64 },
	{ "sqlite3_hard_heap_limit64", sqlite3_hard_heap_limit64 },
	{ "sqlite3_status", sqlite3_status },
	{ "sqlite3_busy_timeout", sqlite3_busy_timeout },
	{ "sqlite3_bind_blob", sqlite3_bind_blob },
	{ "sqlite3_bind_zeroblob", sqlite3_bind_zeroblob },
	{ "sqlite3_bind_double", sqlite3_bind_double },
	{ "sqlite3_bind_int", sqlite3_bind_int },
	{ "sqlite3_bind_int64", sqlite3_bind_int64 },
	{ "sqlite3_bind_null", sqlite3_bind_null },
	{ "sqlite3_bind_text", sqlite3_bind_text },
	{ "sqlite3_bind_text16", sqlite3_bind_text16 },
	{ "sqlite3_bind_parameter_count", sqlite3_bind_parameter_count },
	{ "sqlite3_bind_parameter_index", sqlite3_bind_parameter_index },
	{ "sqlite3_column_count", sqlite3_column_count },
	{ "sqlite3_data_count", sqlite3_data_count },
	{ "sqlite3_step", sqlite3_step },
	{ "sqlite3_sql", sqlite3_sql },
	{ "sqlite3_column_double", sqlite3_column_double },
	{ "sqlite3_column_int", sqlite3_column_int },
	{ "sqlite3_column_int64", sqlite3_column_int64 },
	{ "sqlite3_column_blob", sqlite3_column_blob },
	{ "sqlite3_close", sqlite3_column_bytes },
	{ "sqlite3_close", sqlite3_column_type },
	{ "sqlite3_aggregate_count", sqlite3_aggregate_count },
	{ "sqlite3_value_blob", sqlite3_value_blob },
	{ "sqlite3_value_bytes", sqlite3_value_bytes },
	{ "sqlite3_value_double", sqlite3_value_double },
	{ "sqlite3_value_int", sqlite3_value_int },
	{ "sqlite3_value_int64", sqlite3_value_int64 },
	{ "sqlite3_value_type", sqlite3_value_type },
	{ "sqlite3_user_data", sqlite3_user_data },
	{ "sqlite3_result_blob", sqlite3_result_blob },
	{ "sqlite3_result_double", sqlite3_result_double },
	{ "sqlite3_result_error", sqlite3_result_error },
	{ "sqlite3_result_int", sqlite3_result_int },
	{ "sqlite3_result_int64", sqlite3_result_int64 },
	{ "sqlite3_result_null", sqlite3_result_null },
	{ "sqlite3_result_text", sqlite3_result_text },
	{ "sqlite3_result_zeroblob", sqlite3_result_zeroblob },
	{ "sqlite3_result_error_toobig", sqlite3_result_error_toobig },
	{ "sqlite3_result_error_nomem", sqlite3_result_error_nomem },
	{ "sqlite3_result_error_code", sqlite3_result_error_code },
	{ "sqlite3_aggregate_context", sqlite3_aggregate_context },
	{ "sqlite3_config", sqlite3_config },
	{ "sqlite3_db_config", sqlite3_db_config },
	{ "sqlite3_create_collation", sqlite3_create_collation },
	{ "sqlite3_update_hook", sqlite3_update_hook },
	{ "sqlite3_commit_hook", sqlite3_commit_hook },
	{ "sqlite3_profile", sqlite3_profile },
	{ "sqlite3_progress_handler", sqlite3_progress_handler },
	{ "sqlite3_trace", sqlite3_trace },
	{ "sqlite3_rollback_hook", sqlite3_rollback_hook },
	{ "sqlite3_db_handle", sqlite3_db_handle },
	{ "sqlite3_next_stmt", sqlite3_next_stmt },
	{ "sqlite3_stmt_isexplain", sqlite3_stmt_isexplain },
	{ "sqlite3_stmt_busy", sqlite3_stmt_busy },
	{ "sqlite3_stmt_readonly", sqlite3_stmt_readonly },
	{ "sqlite3_exec", sqlite3_exec },
	{ "sqlite3_get_autocommit", sqlite3_get_autocommit },
	{ "sqlite3_extended_result_codes", sqlite3_extended_result_codes },
	{ "sqlite3_errcode", sqlite3_errcode },
	{ "sqlite3_extended_errcode", sqlite3_extended_errcode },
	{ "sqlite3_errstr", sqlite3_errstr },
	{ "sqlite3_log", sqlite3_log },
	{ "sqlite3_file_control", sqlite3_file_control },
	{ "sqlite3_backup_init", sqlite3_backup_init },
	{ "sqlite3_backup_step", sqlite3_backup_step },
	{ "sqlite3_backup_remaining", sqlite3_backup_remaining },
	{ "sqlite3_backup_pagecount", sqlite3_backup_pagecount },
	{ "sqlite3_backup_finish", sqlite3_backup_finish },
//	{ "sqlite3_snapshot_get", sqlite3_snapshot_get },
//	{ "sqlite3_snapshot_open", sqlite3_snapshot_open },
//	{ "sqlite3_snapshot_recover", sqlite3_snapshot_recover },
//	{ "sqlite3_snapshot_cmp", sqlite3_snapshot_cmp },
//	{ "sqlite3_snapshot_free", sqlite3_snapshot_free },
	{ "sqlite3_blob_open", sqlite3_blob_open },
	{ "sqlite3_blob_write", sqlite3_blob_write },
	{ "sqlite3_blob_read", sqlite3_blob_read },
	{ "sqlite3_blob_bytes", sqlite3_blob_bytes },
	{ "sqlite3_blob_reopen", sqlite3_blob_reopen },
	{ "sqlite3_blob_close", sqlite3_blob_close },
	{ "sqlite3_wal_autocheckpoint", sqlite3_wal_autocheckpoint },
	{ "sqlite3_wal_checkpoint", sqlite3_wal_checkpoint },
	{ "sqlite3_wal_checkpoint_v2", sqlite3_wal_checkpoint_v2 },
	{ "sqlite3_set_authorizer", sqlite3_set_authorizer },
	{ "sqlite3_create_function_v2", sqlite3_create_function_v2 },
	{ "sqlite3_keyword_count", sqlite3_keyword_count },
	{ "sqlite3_keyword_name", sqlite3_keyword_name },
    { NULL, NULL }
};

#endif // __INC_SQLITE_FUNCTIONS__
