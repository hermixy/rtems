/**
 * @file
 *
 * @brief Set Time of Day Given a Timestamp
 *
 * @ingroup ScoreTOD
 */

/*  COPYRIGHT (c) 1989-2007.
 *  On-Line Applications Research Corporation (OAR).
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *  http://www.rtems.org/license/LICENSE.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <rtems/score/todimpl.h>
#include <rtems/score/threaddispatch.h>
#include <rtems/score/watchdogimpl.h>

void _TOD_Set_with_timestamp(
  const Timestamp_Control *tod_as_timestamp
)
{
  TOD_Control *tod = &_TOD;
  uint32_t nanoseconds = _Timestamp_Get_nanoseconds( tod_as_timestamp );
  Watchdog_Interval seconds_next = _Timestamp_Get_seconds( tod_as_timestamp );
  Watchdog_Interval seconds_now;
  ISR_lock_Context lock_context;
  Watchdog_Header *header;

  _Thread_Disable_dispatch();

  seconds_now = _TOD_Seconds_since_epoch();

  _TOD_Acquire( tod, &lock_context );
  tod->now = *tod_as_timestamp;
  _TOD_Release( tod, &lock_context );

  header = &_Watchdog_Seconds_header;

  if ( seconds_next < seconds_now )
    _Watchdog_Adjust_backward( header, seconds_now - seconds_next );
  else
    _Watchdog_Adjust_forward( header, seconds_next - seconds_now );

  tod->seconds_trigger = nanoseconds;
  tod->is_set = true;

  _Thread_Enable_dispatch();
}
