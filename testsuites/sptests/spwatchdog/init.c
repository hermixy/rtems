/*  Init
 *
 *  This routine is the XXX.
 *
 *  Input parameters:
 *    argument - task argument
 *
 *  Output parameters:  NONE
 *
 *  COPYRIGHT (c) 2008.
 *  On-Line Applications Research Corporation (OAR).
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *  http://www.rtems.org/license/LICENSE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define CONFIGURE_INIT
#include "system.h"

#include <rtems/score/watchdogimpl.h>

const char rtems_test_name[] = "SPWATCHDOG";

static void test_watchdog_routine( Objects_Id id, void *arg )
{
  (void) id;
  (void) arg;

  rtems_test_assert( 0 );
}

static void test_watchdog_static_init( void )
{
  #if defined(RTEMS_USE_16_BIT_OBJECT)
    #define JUNK_ID 0x1234
  #else
    #define JUNK_ID 0x12345678
  #endif

  static Watchdog_Control a = WATCHDOG_INITIALIZER(
    test_watchdog_routine,
    JUNK_ID,
    (void *) 0xdeadbeef
  );
  Watchdog_Control b;

  memset( &b, 0, sizeof( b ) );
  _Watchdog_Initialize(
    &b,
    test_watchdog_routine,
    JUNK_ID,
    (void *) 0xdeadbeef
  );

  rtems_test_assert( memcmp( &a, &b, sizeof( a ) ) == 0 );
}

rtems_task Init(
  rtems_task_argument argument
)
{
  rtems_time_of_day  time;
  rtems_status_code  status;

  TEST_BEGIN();

  test_watchdog_static_init();

  build_time( &time, 12, 31, 1988, 9, 0, 0, 0 );

  status = rtems_clock_set( &time );
  directive_failed( status, "rtems_clock_set" );

  Task_name[ 1 ]  = rtems_build_name( 'T', 'A', '1', ' ' );
  Timer_name[ 1 ] = rtems_build_name( 'T', 'M', '1', ' ' );

  status = rtems_task_create(
    Task_name[ 1 ],
    1,
    RTEMS_MINIMUM_STACK_SIZE * 2,
    RTEMS_DEFAULT_MODES,
    RTEMS_DEFAULT_ATTRIBUTES,
    &Task_id[ 1 ]
  );
  directive_failed( status, "rtems_task_create of TA1" );

  status = rtems_task_start( Task_id[ 1 ], Task_1, 0 );
  directive_failed( status, "rtems_task_start of TA1" );

  puts( "INIT - rtems_timer_create - creating timer 1" );
  status = rtems_timer_create( Timer_name[ 1 ], &Timer_id[ 1 ] );
  directive_failed( status, "rtems_timer_create" );

  printf( "INIT - timer 1 has id (0x%" PRIxrtems_id ")\n", Timer_id[ 1 ] );

  status = rtems_task_delete( RTEMS_SELF );
  directive_failed( status, "rtems_task_delete of RTEMS_SELF" );


  rtems_test_exit( 0 );
}
