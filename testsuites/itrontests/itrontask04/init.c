 /*  Init
 *
 *  This routine is the initialization task for this test program.
 *  It is called from init_exec and has the responsibility for creating
 *  and starting the tasks that make up the test.  If the time of day
 *  clock is required for the test, it should also be set to a known
 *  value by this function.
 *
 *  Input parameters:  NONE
 *
 *  Output parameters:  NONE
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *  http://www.OARcorp.com/rtems/license.html.
 *
 *  $Id$
 */

#define TEST_INIT
#include "system.h"
#include <stdio.h>

void ITRON_Init( void )
{
  ER                status;
  T_CTSK            pk_ctsk;

  puts( "\n\n*** ITRON TEST 6 ***" );

  status = chg_pri( 0, 20 );
  directive_failed( status, "chg_pri to 20" );

  pk_ctsk.exinf    = NULL;
  pk_ctsk.tskatr   = TA_HLNG;
  pk_ctsk.stksz    = RTEMS_MINIMUM_STACK_SIZE; 
  pk_ctsk.itskpri  = 2; 

  pk_ctsk.task     = Task_1;
  status = cre_tsk( TA1_ID, &pk_ctsk );
  directive_failed( status, "cre_tsk of TA1" );

  pk_ctsk.task     = Task_2;
  status = cre_tsk( TA2_ID, &pk_ctsk );
  directive_failed( status, "cre_tsk of TA2" );

  pk_ctsk.itskpri  = 1; 
  pk_ctsk.task     = Task_3;
  status = cre_tsk( TA3_ID, &pk_ctsk );
  directive_failed( status, "cre_tsk of TA3" );

  puts("INIT - dis_dsp while starting tasks");
  status = dis_dsp( );
  directive_failed( status, "dis_dsp from ITRON_Init" );  
  status  = sta_tsk( TA1_ID, 0 );
  directive_failed( status, "sta_tsk of TA1" );
  status  = sta_tsk( TA2_ID, 0 );
  directive_failed( status, "sta_tsk of TA2" );
  status  = sta_tsk( TA3_ID, 0 );
  directive_failed( status, "sta_tsk of TA3" );

  puts( "INIT - suspending TA2 3 times" );
  status = sus_tsk( TA2_ID  );
  directive_failed( status, "sus_tsk of TA2" );
  status = sus_tsk( TA2_ID  );
  directive_failed( status, "sus_tsk of TA2" );
  status = sus_tsk( TA2_ID  );
  directive_failed( status, "sus_tsk of TA2" );

  puts("INIT - ena_dsp while starting tasks");
  status = ena_dsp( );

  puts( "INIT - suspending TA1 3 times" );
  status = sus_tsk( TA1_ID  );
  directive_failed( status, "sus_tsk of TA2" );
  status = sus_tsk( TA1_ID  );
  directive_failed( status, "sus_tsk of TA2" );
  status = sus_tsk( TA1_ID  );
  directive_failed( status, "sus_tsk of TA2" );

  puts("INIT - exd_tsk");
  exd_tsk();
  directive_failed( 0, "exd_tsk" );
}

