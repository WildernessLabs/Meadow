#ifndef __INC_SQLITE_FUNCTIONS__
#define __INC_SQLITE_FUNCTIONS__

#include "../sqlite/sqlite3.h"

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
extern int sqlite3_db_readonly(void *, char* dbName);
extern char* sqlite3_db_filename(void *, char* att);
extern int sqlite3_prepare_v2(void *, char* pSql, int nBytes, void* stmt, char* ptrRemain);
extern int sqlite3_prepare_v3(void *, char* pSql, int nBytes, uint flags, void* stmt, char* ptrRemain);
extern int sqlite3_db_status(void *, int op, int* current, int* highest, int resetFlg);
extern int sqlite3_complete(char* pSql);
extern int sqlite3_compileoption_used(char* pSql);
extern char* sqlite3_compileoption_get(int n);
extern int sqlite3_table_column_metadata(void *, char* dbName, char* tblName, char* colName, char* ptrDataType, char* ptrCollSeq, int* notNull, int* primaryKey, int* autoInc);
extern char* sqlite3_value_text(void * p);
extern int sqlite3_enable_load_extension(void *, int enable);
extern int sqlite3_limit(void *, int id, int newVal);
extern int sqlite3_initialize(void);
extern int sqlite3_shutdown(void);
extern char* sqlite3_libversion(void);
extern int sqlite3_libversion_number(void);
extern int sqlite3_threadsafe(void);
extern char* sqlite3_sourceid(void);
extern void* sqlite3_malloc(int n);
extern void* sqlite3_realloc(void* p, int n);
extern void sqlite3_free(void* p);
extern int sqlite3_stricmp(void* p, void* q);
extern int sqlite3_strnicmp(void* p, void* q, int n);
extern int sqlite3_open(char* filename, void** db);
extern int sqlite3_open_v2(char* filename, void** db, int flags, char* vfs);
extern void* sqlite3_vfs_find(char* vfs);
extern long sqlite3_last_insert_rowid(void *);
extern int sqlite3_changes(void *);
extern int sqlite3_total_changes(void *);
extern long sqlite3_memory_used();
extern long sqlite3_memory_highwater(int resetFlag);
extern long sqlite3_soft_heap_limit64(long n);
extern long sqlite3_hard_heap_limit64(long n);
extern int sqlite3_status(int op, int* current, int* highwater, int resetFlag);
extern int sqlite3_busy_timeout(void *, int ms);
extern int sqlite3_bind_blob(void*, int index, char* val, int nSize, void* nTransient);
extern int sqlite3_bind_zeroblob(void*, int index, int size);
extern int sqlite3_bind_double(void*, int index, double val);
extern int sqlite3_bind_int(void*, int index, int val);
extern int sqlite3_bind_int64(void*, int index, long val);
extern int sqlite3_bind_null(void*, int index);
extern int sqlite3_bind_text(void*, int index, char* val, int nlen, void* pvReserved);
extern int sqlite3_bind_text16(void*, int index, char* val, int nlen, void* pvReserved);
extern int sqlite3_bind_parameter_count(void*);
extern int sqlite3_bind_parameter_index(void*, char* strName);
extern int sqlite3_column_count(void*);
extern int sqlite3_data_count(void*);
extern int sqlite3_step(void*);
extern char* sqlite3_sql(void*);
extern double sqlite3_column_double(void*, int index);
extern int sqlite3_column_int(void*, int index);
extern long sqlite3_column_int64(void*, int index);
extern void* sqlite3_column_blob(void*, int index);
extern int sqlite3_column_bytes(void*, int index);
extern int sqlite3_column_type(void*, int index);
extern int sqlite3_aggregate_count(void* context);
extern void* sqlite3_value_blob(void* p);
extern int sqlite3_value_bytes(void* p);
extern double sqlite3_value_double(void* p);
extern int sqlite3_value_int(void* p);
extern long sqlite3_value_int64(void* p);
extern int sqlite3_value_type(void* p);
extern void* sqlite3_user_data(void* context);
extern void sqlite3_result_blob(void* context, void* val, int nSize, void* pvReserved);
extern void sqlite3_result_double(void* context, double val);
extern void sqlite3_result_error(void* context, char* strErr, int nLen);
extern void sqlite3_result_int(void* context, int val);
extern void sqlite3_result_int64(void* context, long val);
extern void sqlite3_result_null(void* context);
extern void sqlite3_result_text(void* context, char* val, int nLen, void* pvReserved);
extern void sqlite3_result_zeroblob(void* context, int n);
extern void sqlite3_result_error_toobig(void* context);
extern void sqlite3_result_error_nomem(void* context);
extern void sqlite3_result_error_code(void* context, int code);
extern void* sqlite3_aggregate_context(void* context, int nBytes);
extern int sqlite3_config(int op, int func, int pvUser);
extern int sqlite3_db_config(void *, int op, void* ptr, int int0, int int1);
extern int sqlite3_create_collation(void *, const char *zName, int eTextRep, void *pArg, int(*xCompare)(void*,int,const void*,int,const void*));
extern void* sqlite3_update_hook(void *, void(*)(void *,int ,char const *,char const *,long),void*);
extern void* sqlite3_commit_hook(void *, int(*)(void*), void*);
extern void* sqlite3_profile(void *, void(*xProfile)(void*,const char*,long), void*);
extern void sqlite3_progress_handler(void *, int, int(*)(void*), void*);
extern void* sqlite3_trace(void *, void(*xTrace)(void*,const char*), void*);
extern void* sqlite3_rollback_hook(void *, void(*)(void *), void*);
extern void* sqlite3_db_handle(void* stmt);
extern void* sqlite3_next_stmt(void *, void* stmt);
extern int sqlite3_stmt_isexplain(void*);
extern int sqlite3_stmt_busy(void*);
extern int sqlite3_stmt_readonly(void*);
extern int sqlite3_exec(void *, char* strSql, int (*callback)(void*,int,char**,char**), void*, char**);
extern int sqlite3_get_autocommit(void *);
extern int sqlite3_extended_result_codes(void *, int onoff);
extern int sqlite3_errcode(void *);
extern int sqlite3_extended_errcode(void *);
extern char* sqlite3_errstr(int rc);
extern void sqlite3_log(int iErrCode, char* zFormat);
extern int sqlite3_file_control(void *, char* zDbName, int op, void* pArg);
extern void* sqlite3_backup_init(void* destDb, char* zDestName, void* sourceDb, char* zSourceName);
extern int sqlite3_backup_step(void* backup, int nPage);
extern int sqlite3_backup_remaining(void* backup);
extern int sqlite3_backup_pagecount(void* backup);
extern int sqlite3_backup_finish(void* backup);
//extern int sqlite3_snapshot_get(void *, char* schema, void** snap);
//extern int sqlite3_snapshot_open(void *, char* schema, void* snap);
//extern int sqlite3_snapshot_recover(void *, char* name);
//extern int sqlite3_snapshot_cmp(void* p1, void* p2);
//extern void sqlite3_snapshot_free(void* snap);
extern int sqlite3_blob_open(void *, char* sdb, char* table, char* col, long rowid, int flags, void** blob);
extern int sqlite3_blob_write(void* blob, char* b, int n, int offset);
extern int sqlite3_blob_read(void* blob, char* b, int n, int offset);
extern int sqlite3_blob_bytes(void* blob);
extern int sqlite3_blob_reopen(void* blob, long rowid);
extern int sqlite3_blob_close(void* blob);
extern int sqlite3_wal_autocheckpoint(void *, int n);
extern int sqlite3_wal_checkpoint(void *, char* dbName);
extern int sqlite3_wal_checkpoint_v2(void *, char* dbName, int eMode, int* logSize, int* framesCheckPointed);
extern int sqlite3_set_authorizer(void *, int (*xAuth)(void*,int,const char*,const char*,const char*,const char*), void*);
extern int sqlite3_create_function_v2(void *, char* strName, int nArgs, int nType, void*, void (*xFunc)(void*,int,void**), void (*xStep)(void*,int,void**), void (*xFinal)(void*), void(*xDestroy)(void*));
extern int sqlite3_keyword_count();
extern int sqlite3_keyword_name(int i, char** name, int* length);

// NOTE: commented out definitions appear in the sqlite_pcl_raw P/Invokes, but yield "undefined symbol" when compiling here
MonoDlMapping sqlite_mappings[] = {
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
