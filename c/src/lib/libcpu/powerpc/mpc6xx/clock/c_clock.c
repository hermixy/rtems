/**
 *  @brief Clock Tick Device Driver
 *
 *  This routine utilizes the Decrementer Register common to the PPC family.
 *
 *  The tick frequency is directly programmed to the configured number of
 *  microseconds per tick.
 */

/*
 *  COPYRIGHT (c) 1989-2014.
 *  On-Line Applications Research Corporation (OAR).
 *
 *  The license and distribution terms for this file may in
 *  the file LICENSE in this distribution or at
 *  http://www.rtems.org/license/LICENSE.
 *
 *  Modified to support the MPC750.
 *  Modifications Copyright (c) 1999 Eric Valette valette@crf.canon.fr
 */

#include <rtems.h>
#include <rtems/libio.h>
#include <rtems/clockdrv.h>
#include <stdlib.h>                     /* for atexit() */
#include <assert.h>
#include <libcpu/c_clock.h>
#include <libcpu/cpuIdent.h>
#include <libcpu/spr.h>
#include <rtems/bspIo.h>                /* for printk() */
#include <libcpu/powerpc-utility.h>

#include <bspopts.h>   /* for CLOCK_DRIVER_USE_FAST_IDLE */

SPR_RW(BOOKE_TCR)
SPR_RW(BOOKE_TSR)
SPR_RW(BOOKE_DECAR)

extern int BSP_connect_clock_handler (void);

/*
 *  Clock ticks since initialization
 */
volatile uint32_t   Clock_driver_ticks;

/*
 *  This is the value programmed into the count down timer.
 */
static uint32_t   Clock_Decrementer_value;

/*
 *  This is the value by which elapsed count down timer ticks are multiplied to
 *  give an elapsed duration in nanoseconds, left-shifted by 32 bits
 */
static uint64_t   Clock_Decrementer_reference;

void clockOff(void* unused)
{
  rtems_interrupt_level l;

  if ( ppc_cpu_is_bookE() ) {
    rtems_interrupt_disable(l);
    _write_BOOKE_TCR(_read_BOOKE_TCR() & ~BOOKE_TCR_DIE);
    rtems_interrupt_enable(l);
  } else {
    /*
     * Nothing to do as we cannot disable all interrupts and
     * the decrementer interrupt enable is MSR_EE
     */
  }
}

void clockOn(void* unused)
{
  rtems_interrupt_level l;

  PPC_Set_decrementer( Clock_Decrementer_value );

  if ( ppc_cpu_is_bookE() ) {
    _write_BOOKE_DECAR( Clock_Decrementer_value );

    rtems_interrupt_disable(l);

    /* clear pending/stale irq */
    _write_BOOKE_TSR( BOOKE_TSR_DIS );
    /* enable */
    _write_BOOKE_TCR( _read_BOOKE_TCR() | BOOKE_TCR_DIE );

    rtems_interrupt_enable(l);

  }
}

static void clockHandler(void)
{
  #if (CLOCK_DRIVER_USE_FAST_IDLE == 1)
    do {
      rtems_clock_tick();
    } while (
      _Thread_Heir == _Thread_Executing
        && _Thread_Executing->Start.entry_point
          == (Thread_Entry) rtems_configuration_get_idle_task()
    );

  #else
    rtems_clock_tick();
  #endif
}

static void (*clock_handler)(void);

/*
 *  Clock_isr
 *
 *  This is the clock tick interrupt handler.
 *
 *  Input parameters:
 *    vector - vector number
 *
 *  Output parameters:  NONE
 *
 *  Return values:      NONE
 *
 */
void clockIsr(void *unused)
{
  int decr;

  /*
   *  The driver has seen another tick.
   */
  do {
    register uint32_t flags;

    rtems_interrupt_disable(flags);
      __asm__ volatile (
	"mfdec %0; add %0, %0, %1; mtdec %0"
	: "=&r"(decr)
	: "r"(Clock_Decrementer_value)
      );
    rtems_interrupt_enable(flags);

    Clock_driver_ticks += 1;

    /*
     *  Real Time Clock counter/timer is set to automatically reload.
     */
    clock_handler();
  } while ( decr < 0 );
}

/*
 *  Clock_isr_bookE
 *
 *  This is the clock tick interrupt handler
 *  for bookE CPUs. For efficiency reasons we
 *  provide a separate handler rather than
 *  checking the CPU type each time.
 */
void clockIsrBookE(void *unused)
{
  /* Note: TSR bit has already been cleared in the exception handler */

  /*
   *  The driver has seen another tick.
   */

  Clock_driver_ticks += 1;

  /*
   *  Real Time Clock counter/timer is set to automatically reload.
   */
  clock_handler();
}

int clockIsOn(void* unused)
{
  uint32_t   msr_value;

  _CPU_MSR_GET( msr_value );

  if ( ppc_cpu_is_bookE() && ! (_read_BOOKE_TCR() & BOOKE_TCR_DIE) )
    msr_value = 0;

  if (msr_value & MSR_EE) return 1;

  return 0;
}


/*
 *  Clock_exit
 *
 *  This routine allows the clock driver to exit by masking the interrupt and
 *  disabling the clock's counter.
 */
void Clock_exit( void )
{
  (void) BSP_disconnect_clock_handler ();
}

static uint32_t Clock_driver_nanoseconds_since_last_tick(void)
{
  uint32_t clicks, tmp;

  PPC_Get_decrementer( clicks );

  /*
   * Multiply by 1000 here separately from below so we do not overflow
   * and get a negative value.
   */
  tmp = (Clock_Decrementer_value - clicks) * 1000;
  tmp /= (BSP_bus_frequency/BSP_time_base_divisor);

  return tmp * 1000;
}

static uint32_t Clock_driver_nanoseconds_since_last_tick_bookE(void)
{
  uint32_t clicks;
  uint64_t c;

  PPC_Get_decrementer( clicks );
  c = Clock_Decrementer_value - clicks;

  /*
   * Check whether a clock tick interrupt is pending and hence that the
   * decrementer's wrapped. If it has, we'll compensate by returning a time one
   * tick period longer.
   *
   * We have to check interrupt status after reading the decrementer. If we
   * don't, we may miss an interrupt and read a wrapped decrementer value
   * without compensating for it
   */
  if ( _read_BOOKE_TSR() & BOOKE_TSR_DIS )
  {
    /*
     * Re-read the decrementer: The tick interrupt may have been
     * generated and the decrementer wrapped during the time since we
     * last read it and the time we checked the interrupt status
     */
    PPC_Get_decrementer( clicks );
    c = (Clock_Decrementer_value - clicks) + Clock_Decrementer_value;
  }

  return (uint32_t)((c * Clock_Decrementer_reference) >> 32);
}

/*
 *  Clock_initialize
 *
 *  This routine initializes the clock driver.
 */
rtems_device_driver Clock_initialize(
  rtems_device_major_number major,
  rtems_device_minor_number minor,
  void *pargp
)
{
  rtems_interrupt_level l,tcr;

  Clock_Decrementer_value = (BSP_bus_frequency/BSP_time_base_divisor)*
            rtems_configuration_get_milliseconds_per_tick();

  Clock_Decrementer_reference = ((uint64_t)1000000U<<32)/
            (BSP_bus_frequency/BSP_time_base_divisor);

  /* set the decrementer now, prior to installing the handler
   * so no interrupts will happen in a while.
   */
  PPC_Set_decrementer( (unsigned)-1 );

  /* On a bookE CPU the decrementer works differently. It doesn't
   * count past zero but we can enable auto-reload :-)
   */
  if ( ppc_cpu_is_bookE() ) {

    rtems_interrupt_disable(l);

      tcr  = _read_BOOKE_TCR();
      tcr |= BOOKE_TCR_ARE;
      tcr &= ~BOOKE_TCR_DIE;
      _write_BOOKE_TCR(tcr);

    rtems_interrupt_enable(l);

    /*
     *  Set the nanoseconds since last tick handler
     */
    rtems_clock_set_nanoseconds_extension(
      Clock_driver_nanoseconds_since_last_tick_bookE
    );
  }
  else
  {
    /*
     *  Set the nanoseconds since last tick handler
     */
    rtems_clock_set_nanoseconds_extension(
      Clock_driver_nanoseconds_since_last_tick
    );
  }

  /*
   * If a decrementer exception was pending, it is cleared by
   * executing the default (nop) handler at this point;
   * The next exception will then be taken by our clock handler.
   * Clock handler installation initializes the decrementer to
   * the correct value.
   */
  clock_handler = clockHandler;
  if (!BSP_connect_clock_handler ()) {
    printk("Unable to initialize system clock\n");
    rtems_fatal_error_occurred(1);
  }

  return RTEMS_SUCCESSFUL;
} /* Clock_initialize */
