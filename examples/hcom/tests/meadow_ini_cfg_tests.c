/****************************************************************************
 * \apps\examples\hcom\tests\meadow_ini_cfg_tests.c
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

#include "../hcom_common.h"
#include <meadow/hcom_upd_shared.h>
#include <meadow/hcom_shared_common.h>

// #if HCOM_INCLUDE_INI_CFG_TESTS_IN_BUILD > 0
#if 1

/****************************************************************************
 * Configuration file key value pair tests
 ****************************************************************************/

#define HCOM_TEST_INI_CFG_TEST_BUFFER_LEN 32

static void hcom_tests_ini_cfg_basic(uint32_t userData);
static void hcom_tests_ini_cfg_name_list(uint32_t userData);
static void hcom_tests_ini_cfg_show_return(char *returnValueBuf, int ret);

//================================================================
void hcom_tests_ini_cfg_execute_selected(uint32_t userData)
{
  if(userData == 1)
    return hcom_tests_ini_cfg_basic(userData);
  
  if(userData > 49 && userData < 100)
    return hcom_tests_ini_cfg_name_list(userData);

  syslog(2, "Unknown ini cfg test #%d\n", userData);
}

//================================================================
// Tests using the data provided by the original author
void hcom_tests_ini_cfg_basic(uint32_t userData)
{
  int ret;

  char *fileName = "/meadow0/OrigTestFile.cfg";
  char *sectionName = "user";
  char *keyName = "pi";

  char returnValueBuf[HCOM_TEST_INI_CFG_TEST_BUFFER_LEN];
  int returnBufLen = HCOM_TEST_INI_CFG_TEST_BUFFER_LEN;

  syslog(2, "\nini cfg test #%d\n", userData);

  ret = hcom_via_nx_ini_cfg_get_value(fileName, sectionName,
        keyName, returnValueBuf, returnBufLen);
  
  hcom_tests_ini_cfg_show_return(returnValueBuf, ret);
}

//================================================================
void hcom_tests_ini_cfg_name_list(uint32_t userData)
{
  int ret;
  char *fileName;
  char *sectionName;
  char *keyName;

  syslog(2, "\nini cfg test #%d\n", userData);
  switch(userData)
  {
    case 50:    // Expect 'LAWSON'
      fileName = "/meadow0/namelist.cfg";
      sectionName = "gentlemen";
      keyName = "ANTONIO";  // Last name in list
      break;

    case 51:    // Expect 'SMITH'
      fileName = "/meadow0/namelist.cfg";
      sectionName = "ladies";
      keyName = "MARY";  // First name in list
      break;

    case 52:    // Expect the device name 
      fileName = "/meadow0/meadow.cfg";
      sectionName = "startup";
      keyName = "DeviceName";
      break;

    case 53:    // Expect error
      fileName = "/meadow0/meadow.cfg";
      sectionName = "startup";
      keyName = "devicename";
      break;
      
    case 54:    // Expect success
      fileName = "/meadow0/meadow.cfg";
      sectionName = NULL;
      keyName = "monorun";
      break;

    case 55:    // Expect success
      fileName = NULL;
      sectionName = NULL;
      keyName = "monorun";
      break;

    case 56:    // Expect success
      fileName = "";
      sectionName = "";
      keyName = "monorun";
      break;

    case 57:    // Expect error no key provided
      fileName = NULL;
      sectionName = NULL;
      //keyName = "monorun";
      break;

    case 58:    // Expect failure these may be garbage 
      // fileName = NULL;
      // sectionName = "";
      keyName = "monorun";
      break;

    default:
      return;
  }

  syslog(2, "%s@%d-Test Query - FileName:%s, Section:%s, Key:%s\n",
            __FILE__, __LINE__, fileName, sectionName, keyName);

  char returnValueBuf[HCOM_TEST_INI_CFG_TEST_BUFFER_LEN];
  int returnBufLen = HCOM_TEST_INI_CFG_TEST_BUFFER_LEN;
  ret = hcom_via_nx_ini_cfg_get_value(fileName, sectionName,
        keyName, returnValueBuf, returnBufLen);
  
  hcom_tests_ini_cfg_show_return(returnValueBuf, ret);
}

//================================================================
void hcom_tests_ini_cfg_show_return(char *returnValueBuf, int ret)
{
  if(ret == OK)
  {
    syslog(2, "Result-SUCCESS ini cfg test, ret:%d\n", ret);
    return;
  }

  // Source generated error message
  if(ret < 0 || ret > 0)
  {
    // No key found isn't really an error
    if(ret == MEADOW_CONFIG_ERROR_NO_KEY_FOUND)
      syslog(LOG_WARNING, "'%s', ret:%d", returnValueBuf, ret);
    else
      syslog(LOG_ERR, "'%s', ret:%d", returnValueBuf, ret);
  }
}

// Three test file are used their contents are:
// meadow.cfg
/*
; Test config file for Meadow F7 Micro

[startup]           ; Meadow startup configuration parameters
UART1=trace        ; uart=app is the default
TraceLevel=2		    ; Only 1, 2 or 3 are valid. All others = default
MonoRun=no			    ; Should mono run?

DeviceName=Peter's F7 MIcro ; This is the new device name

*/

// namelist.cfg
/*
; Test config file with 200 names

[ladies]
MARY=SMITH
PATRICIA=JOHNSON
LINDA=WILLIAMS
BARBARA=JONES
ELIZABETH=BROWN
JENNIFER=DAVIS
MARIA=MILLER
SUSAN=WILSON
MARGARET=MOORE
DOROTHY=TAYLOR
LISA=ANDERSON
NANCY=THOMAS
KAREN=JACKSON
BETTY=WHITE
HELEN=HARRIS
SANDRA=MARTIN
DONNA=THOMPSON
CAROL=GARCIA
RUTH=MARTINEZ
SHARON=ROBINSON
MICHELLE=CLARK
LAURA=RODRIGUEZ
SARAH=LEWIS
KIMBERLY=LEE
DEBORAH=WALKER
JESSICA=HALL
SHIRLEY=ALLEN
CYNTHIA=YOUNG
ANGELA=HERNANDEZ
MELISSA=KING
BRENDA=WRIGHT
AMY=LOPEZ
ANNA=HILL
REBECCA=SCOTT
VIRGINIA=GREEN
KATHLEEN=ADAMS
PAMELA=BAKER
MARTHA=GONZALEZ
DEBRA=NELSON
AMANDA=CARTER
STEPHANIE=MITCHELL
CAROLYN=PEREZ
CHRISTINE=ROBERTS
MARIE=TURNER
JANET=PHILLIPS
CATHERINE=CAMPBELL
FRANCES=PARKER
ANN=EVANS
JOYCE=EDWARDS
DIANE=COLLINS
ALICE=STEWART
JULIE=SANCHEZ
HEATHER=MORRIS
TERESA=ROGERS
DORIS=REED
GLORIA=COOK
EVELYN=MORGAN
JEAN=BELL
CHERYL=MURPHY
MILDRED=BAILEY
KATHERINE=RIVERA
JOAN=COOPER
ASHLEY=RICHARDSON
JUDITH=COX
ROSE=HOWARD
JANICE=WARD
KELLY=TORRES
NICOLE=PETERSON
JUDY=GRAY
CHRISTINA=RAMIREZ
KATHY=JAMES
THERESA=WATSON
BEVERLY=BROOKS
DENISE=KELLY
TAMMY=SANDERS
IRENE=PRICE
JANE=BENNETT
LORI=WOOD
RACHEL=BARNES
MARILYN=ROSS
ANDREA=HENDERSON
KATHRYN=COLEMAN
LOUISE=JENKINS
SARA=PERRY
ANNE=POWELL
JACQUELINE=LONG
WANDA=PATTERSON
BONNIE=HUGHES
JULIA=FLORES
RUBY=WASHINGTON
LOIS=BUTLER
TINA=SIMMONS
PHYLLIS=FOSTER
NORMA=GONZALES
PAULA=BRYANT
DIANA=ALEXANDER
ANNIE=RUSSELL
LILLIAN=GRIFFIN
EMILY=DIAZ

[gentlemen]
ROBIN=HAYES
JAMES=MYERS
JOHN=FORD
ROBERT=HAMILTON
MICHAEL=GRAHAM
WILLIAM=SULLIVAN
DAVID=WALLACE
RICHARD=WOODS
CHARLES=COLE
JOSEPH=WEST
THOMAS=JORDAN
CHRISTOPHER=OWENS
DANIEL=REYNOLDS
PAUL=FISHER
MARK=ELLIS
DONALD=HARRISON
GEORGE=GIBSON
KENNETH=MCDONALD
STEVEN=CRUZ
EDWARD=MARSHALL
BRIAN=ORTIZ
RONALD=GOMEZ
ANTHONY=MURRAY
KEVIN=FREEMAN
JASON=WELLS
MATTHEW=WEBB
GARY=SIMPSON
TIMOTHY=STEVENS
JOSE=TUCKER
LARRY=PORTER
JEFFREY=HUNTER
FRANK=HICKS
SCOTT=CRAWFORD
ERIC=HENRY
STEPHEN=BOYD
ANDREW=MASON
RAYMOND=MORALES
GREGORY=KENNEDY
JOSHUA=WARREN
JERRY=DIXON
DENNIS=RAMOS
WALTER=REYES
PATRICK=BURNS
PETER=GORDON
HAROLD=SHAW
DOUGLAS=HOLMES
HENRY=RICE
CARL=ROBERTSON
ARTHUR=HUNT
RYAN=BLACK
ROGER=DANIELS
JOE=PALMER
JUAN=MILLS
JACK=NICHOLS
ALBERT=GRANT
JONATHAN=KNIGHT
JUSTIN=FERGUSON
TERRY=ROSE
GERALD=STONE
KEITH=HAWKINS
SAMUEL=DUNN
WILLIE=PERKINS
RALPH=HUDSON
LAWRENCE=SPENCER
NICHOLAS=GARDNER
ROY=STEPHENS
BENJAMIN=PAYNE
BRUCE=PIERCE
BRANDON=BERRY
ADAM=MATTHEWS
HARRY=ARNOLD
FRED=WAGNER
WAYNE=WILLIS
BILLY=RAY
STEVE=WATKINS
LOUIS=OLSON
JEREMY=CARROLL
AARON=DUNCAN
RANDY=SNYDER
HOWARD=HART
EUGENE=CUNNINGHAM
CARLOS=BRADLEY
RUSSELL=LANE
BOBBY=ANDREWS
VICTOR=RUIZ
MARTIN=HARPER
ERNEST=FOX
PHILLIP=RILEY
TODD=ARMSTRONG
JESSE=CARPENTER
CRAIG=WEAVER
ALAN=GREENE
SHAWN=LAWRENCE
CLARENCE=ELLIOTT
SEAN=CHAVEZ
PHILIP=SIMS
CHRIS=AUSTIN
JOHNNY=PETERS
EARL=KELLEY
JIMMY=FRANKLIN
ANTONIO=LAWSON

*/

// OrigTestFile.cfg
/*
; Test config file for ini_example.c and INIReaderTest.cpp

[protocol]             ; Protocol configuration
version=6              ; IPv6

[user]
name = Bob Smith       ; Spaces around '=' are stripped
email = bob@smith.com  ; And comments (like this) ignored
active = true          ; Test a boolean
pi = 3.14159           ; Test a floating point number

*/
#endif