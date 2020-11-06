/****************************************************************************
 * \Meadow\Meadow.OS\apps\examples\sqlite\sqlite_meadow_tests.c
 * 
 *   Copyright (C) 2019 - 2020 Wilderness Labs. All rights reserved.
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

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include<nuttx/config.h>
#if defined(CONFIG_EXAMPLES_SQLITE_TESTS)

#include "sqlite3.h"

#include "../hcom/hcom_common.h"
#include "sqlite_meadow.h"

#define DATABASE_TEST_NAME "/meadow0/Test1.db"

sqlite3_vfs *sqlite3_demovfs(void);

static int execute_sqlite_populate_database(void);
static int execute_sqlite_query1_experiment(void);
static int execute_sqlite_query2_experiment(void);

//==============================================================
// Callable from CLI
void hcom_meadow_sqlite_tests(uint32_t userData)
{
  syslog(1, "SQLite test:Entered hcom_meadow_sqlite_tests switch, userData:%d\n", userData);
  
  switch(userData)
  {
    case 1:
    execute_sqlite_populate_database();
    break;
    
    case 2:
    execute_sqlite_query1_experiment();
    break;

    case 3:
    execute_sqlite_query2_experiment();
    break;

    default:
    syslog(1, "SQLite-TEST only userData 1 & 2 implemented\n");
    break;
  }
}

//================================================================
// There are several hardcode and #defined objects used here
static sqlite3 * hcom_sqlite_register_vfs_and_open(const char *databaseName)
{
  sqlite3 *sqlite_db;
  int ret;
    
  syslog(1, "SQLite-TEST open database ENTERED\n");

  // Must register the vfs since we cannot use one of the built-in ones
  // To do this we ask the vfs for a pointer that represents itself.
  syslog(1, "SQLite-TEST test:Step #1-register vfs\n");

  // Using a modified version of the demo vfs
  sqlite3_vfs *pVfs = sqlite3_demovfs();
  if(pVfs == NULL)
  {
    syslog(1, "SQLite-TEST ERROR: sqlite3_demovfs\n");
    return NULL;
  }
  syslog(1, "SQLite-TEST test:Step #1-register vfs - success\n");
  syslog(1, "SQLite-TEST test:Step #2-find just registered self\n");

  // Then we register the vfs with sqlite
  ret = sqlite3_vfs_register(pVfs, 1);
  if(ret)
  {
    syslog(1, "SQLite-TEST ERROR: sqlite3_vfs_register\n");
    return NULL;
  }
  syslog(1, "SQLite-TEST test:Step #1-register vfs - success\n");
  syslog(1, "SQLite-TEST test:Step #2-find just registered self\n");

  // Next we attempt to find it...
  // This is really not needed because we already have what we
  // need, but for learning it seems a reasonable thing to do
  sqlite3_vfs *foundVfs = sqlite3_vfs_find(VIRTUAL_FILE_SYS_NAME);
  if(foundVfs == NULL)
  {
    syslog(1, "SQLite-TEST Cannot find vfs:%s)\n",
          VIRTUAL_FILE_SYS_NAME);
    return NULL;
  }
  syslog(1, "SQLite-TEST test:Step #2-find self registered - success\n");
  syslog(1, "SQLite-TEST test:Step #3-open database\n");

  //-------------------------------------------------------
  // Open database
  // https://sqlite.org/c3ref/open.html
  ret = sqlite3_open_v2(databaseName,
          &sqlite_db,
          SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
          VIRTUAL_FILE_SYS_NAME);
  if(ret)
  {
    syslog(1, "SQLite-TEST ERROR:Cannot open database:%s, error:%d (%s)\n",
          databaseName, sqlite3_extended_errcode(sqlite_db),
          sqlite3_errmsg(sqlite_db));
    return NULL;
  }

  syslog(1, "SQLite-TEST test:Step #3-open database-success\n");
  
  return sqlite_db;
}

//=========================================================
// Called from cli for testing
int execute_sqlite_populate_database()
{
  sqlite3 *sqlite_db;
  int ret;
  char *errMsg;
    
  syslog(1, "SQLite-TEST create and insert test:Started\n");
  sqlite_db = hcom_sqlite_register_vfs_and_open(DATABASE_TEST_NAME);
  if(sqlite_db == NULL)
  {
    syslog(1, "SQLite-TEST ERROR:Call to hcom_sqlite_register_vfs_and_open())\n",
          DATABASE_TEST_NAME);
  }

// // Turn off journal, at least for now
// #define TEST_SQL_TABLE_NO_JOURNAL "PRAGMA journal_mode=OFF"
//   ret = sqlite3_exec(sqlite_db, TEST_SQL_TABLE_NO_JOURNAL, NULL, NULL, &errMsg);
//   if( ret != SQLITE_OK )
//   {
//     syslog(1, "SQLite-TEST ERROR:Step #4-Turn off Journal\n");
//     return -1;
//   }
//   syslog(1, "SQLite-TEST test:Step #4-turn off journal - success\n");

  char *tableDropSql = "DROP TABLE IF EXISTS Names";

  syslog(1, "SQLite-TEST test:Step #4-Drop table if it exists [%s]\n", tableDropSql);
  ret = sqlite3_exec(sqlite_db, tableDropSql, NULL, NULL, &errMsg);
  if(ret)
  {
    syslog(1, "SQLite-TEST ERROR:Drop table-sqlite3_exec:'%s', errMsg:%s, error:%d (%s)\n",
          DATABASE_TEST_NAME, errMsg, sqlite3_extended_errcode(sqlite_db),
          sqlite3_errmsg(sqlite_db));
    return -1;
  }

   char *tableCreateSql = 
    "CREATE TABLE Names "
    "(nameid integer,"
    " first_name text not null,"
    " last_name text not null)";
 
  syslog(1, "SQLite-TEST test:Step #5-create database [%s]\n", tableCreateSql);

  ret = sqlite3_exec(sqlite_db, tableCreateSql, NULL, NULL, &errMsg);
  if(ret)
  {
    syslog(1, "SQLite-TEST ERROR:Create table-sqlite3_exec:'%s', errMsg:%s, error:%d (%s)\n",
          DATABASE_TEST_NAME, errMsg, sqlite3_extended_errcode(sqlite_db),
          sqlite3_errmsg(sqlite_db));
    return -1;
  }

  syslog(1, "SQLite-TEST test:Step #5-create database - success\n");
  //--------------------------------------------------
  syslog(1, "SQLite-TEST test:Step #6-open names file\n");

  #define FILE_OF_NAMES "/meadow0/TabDelimited200Names.txt"

  // Get first and last names from a tab-delimited file.
  FILE *pFile = fopen (FILE_OF_NAMES, "r");
  if((void *)pFile == NULL)
  {
    syslog(1, "SQLite-TEST ERROR:fopen:'%s', errno:%d\n",
          FILE_OF_NAMES, errno);
    return -1;
  }

  syslog(1, "SQLite-TEST test:Step #6-open names file - success\n");

#define BUFFER_SIZE 256
  int nameId = 0;
  char sFirstName[64];
  char sTab[4];
  char sLastName[64];
  char sSQL[256];

  // Begin transaction
  ret = sqlite3_exec(sqlite_db, "BEGIN TRANSACTION", NULL, NULL, &errMsg);
  if(ret < 0)
  {
    syslog(1, "SQLite-TEST ERROR:sqlite3_exec:'BEGIN TRANSACTION', errMsg:%s, error:%d (%s)\n",
          DATABASE_TEST_NAME, errMsg, sqlite3_extended_errcode(sqlite_db),
          sqlite3_errmsg(sqlite_db));
    return -1;
  }

  syslog(1, "SQLite-TEST test:Step #7 - fill database table with names\n");

  //--------------------------------------------------
  // Insert names into SQLlite database.
  while(true)
  {
    // Read a line containing first name, tab, lastname
    ret = fscanf(pFile, "%s%c%s", sFirstName, sTab, sLastName);
    if(ret != 3)
    {
      break;    // exit after last entry
    }
    nameId++;

    // Insert statement
    sprintf(sSQL, "INSERT INTO Names VALUES (%d, '%s','%s')", nameId, sFirstName, sLastName);
    // syslog(1, "--------------------------------------\n");
    // syslog(1, "SQLite-TEST test:Step #8 (%d)-Insert statement '%s'\n", nameId, sSQL);

    ret = sqlite3_exec(sqlite_db, sSQL, NULL, NULL, &errMsg);
    if(ret)
    {
      syslog(1, "SQLite-TEST ERROR:sqlite3_exec INSERT into '%s', errMsg:%s, error:%d (%s)\n",
            DATABASE_TEST_NAME, errMsg, sqlite3_extended_errcode(sqlite_db), sqlite3_errmsg(sqlite_db));
      return -1;
    }
  }

  ret = sqlite3_exec(sqlite_db, "END TRANSACTION", NULL, NULL, &errMsg);
  if(ret < 0)
  {
    syslog(1, "SQLite-TEST ERROR:sqlite3_exec:'END TRANSACTION', errMsg:%s, error:%d (%s)\n",
          DATABASE_TEST_NAME, errMsg, sqlite3_extended_errcode(sqlite_db),
          sqlite3_errmsg(sqlite_db));
    return -1;
  }
  
  if(ret < 0)
  {
    if(errno != 0)
    {
      syslog(1, "SQLite-TEST test:Step #8 ERROR:ret:%d, errno:%d\n", ret, errno);
      return -errno;
    }
  }

  sqlite3_close_v2(sqlite_db);

  syslog(1, "SQLite-TEST test:Step #8 SUCCESS wrote %d entries\n", nameId);
  return OK;
}

//===========================================================
int execute_sqlite_query1_experiment()
{
  sqlite3 *sqlite_db;
  int ret;
    
  syslog(1, "SQLite-TEST create and insert test:Started\n");
  sqlite_db = hcom_sqlite_register_vfs_and_open(DATABASE_TEST_NAME);
  if(sqlite_db == NULL)
  {
    syslog(1, "SQLite-TEST ERROR:Call to hcom_sqlite_register_vfs_and_open())\n",
          DATABASE_TEST_NAME);
  }

  syslog(1, "SQLite-TEST test:Step #3-query database\n");
  
  //--------------------------------------------------------
  // Create a sorted list
  sqlite3_stmt *stmt;
  const char *simpleQuerySql = 
    "SELECT * "
    "FROM Names "
    "ORDER BY last_name";

  ret = sqlite3_prepare_v2(sqlite_db, simpleQuerySql, -1, &stmt, NULL);
  if(ret)
  {
    syslog(1, "SQLite-TEST ERROR:sqlite3_prepare_v3:'%s', error:%d (%s)\n",
          DATABASE_TEST_NAME, sqlite3_extended_errcode(sqlite_db),
          sqlite3_errmsg(sqlite_db));
    return -1;
  }

  int counter = 1;

  // Step through each result
  while((ret = sqlite3_step(stmt)) == SQLITE_ROW)
  {
    // show the data
    int id = sqlite3_column_int (stmt, 0);
    const unsigned char *firstName = sqlite3_column_text (stmt, 1);
    const unsigned char *lastName = sqlite3_column_text (stmt, 2);

    // Just show via syslog
    syslog(1, "#%d-Row:%d, First:%s Name:%s\n", counter, id, firstName, lastName);
    counter++;
  }

  if(ret != SQLITE_DONE)
  {
    syslog(1, "SQLite-TEST ERROR:sqlite3_step:'%s', error:%d (%s)\n",
          DATABASE_TEST_NAME, sqlite3_extended_errcode(sqlite_db),
          sqlite3_errmsg(sqlite_db));
    return -1;
  }
  sqlite3_finalize(stmt);
  sqlite3_close_v2(sqlite_db);

  syslog(1, "SQLite-TEST test:Step #3-query database - success\n");
  return OK;
}

//===========================================================
int execute_sqlite_query2_experiment()
{
  syslog(1, "SQLite-TEST no test created\n");
  return OK;
}

//===========================================================
// These are required for sqlite
SQLITE_API int sqlite3_os_init(void)
{
  syslog(1, "MGR-ENTERED sqlite3_os_init\n");
  return SQLITE_OK;
}

SQLITE_API int sqlite3_os_end(void)
{
  syslog(1, "MGR-ENTERED sqlite3_os_end\n");
  return SQLITE_OK;
}

#endif  // #if defined(CONFIG_EXAMPLES_SQLITE_TESTS)

