// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     startup_stm32f446xx.h
//
// Purpose:
//     Defines the public startup and exception-handler integration contract.
//
// Public Contract:
//     - Declares the reset and default handlers used by the vector table.
//     - Declares weak core exception handlers.
//     - Keeps vector-table storage private to startup_stm32f446xx.c.

#ifndef STARTUP_STM32F446XX_H
#define STARTUP_STM32F446XX_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Function:
 *     startup_reset_handler
 *
 * Purpose:
 *     Initializes runtime memory and transfers control to main.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Execution Context: processor reset. Blocking is not applicable before main starts.
 */
void startup_reset_handler(void);

/*
 * Function:
 *     startup_default_handler
 *
 * Purpose:
 *     Stops execution for an unimplemented exception or interrupt.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Execution Context: exception or ISR. The function intentionally does not return.
 */
void startup_default_handler(void);

/*
 * Function:
 *     startup_nmi_handler
 *
 * Purpose:
 *     Handles the Non-Maskable Interrupt when no specialized handler is installed.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Execution Context: exception. This weak symbol aliases startup_default_handler unless overridden.
 */
void startup_nmi_handler(void);

/*
 * Function:
 *     startup_hard_fault_handler
 *
 * Purpose:
 *     Handles a HardFault when no specialized handler is installed.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Execution Context: exception. This weak symbol aliases startup_default_handler unless overridden.
 */
void startup_hard_fault_handler(void);

/*
 * Function:
 *     startup_memory_management_fault_handler
 *
 * Purpose:
 *     Handles a memory-management fault when no specialized handler is installed.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Execution Context: exception. This weak symbol aliases startup_default_handler unless overridden.
 */
void startup_memory_management_fault_handler(void);

/*
 * Function:
 *     startup_bus_fault_handler
 *
 * Purpose:
 *     Handles a bus fault when no specialized handler is installed.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Execution Context: exception. This weak symbol aliases startup_default_handler unless overridden.
 */
void startup_bus_fault_handler(void);

/*
 * Function:
 *     startup_usage_fault_handler
 *
 * Purpose:
 *     Handles a usage fault when no specialized handler is installed.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Execution Context: exception. This weak symbol aliases startup_default_handler unless overridden.
 */
void startup_usage_fault_handler(void);

/*
 * Function:
 *     startup_supervisor_call_handler
 *
 * Purpose:
 *     Handles a supervisor call when no specialized handler is installed.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Execution Context: exception. This weak symbol aliases startup_default_handler unless overridden.
 */
void startup_supervisor_call_handler(void);

/*
 * Function:
 *     startup_debug_monitor_handler
 *
 * Purpose:
 *     Handles a debug monitor exception when no specialized handler is installed.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Execution Context: exception. This weak symbol aliases startup_default_handler unless overridden.
 */
void startup_debug_monitor_handler(void);

/*
 * Function:
 *     startup_pend_sv_handler
 *
 * Purpose:
 *     Handles PendSV when no specialized handler is installed.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Execution Context: exception. This weak symbol aliases startup_default_handler unless overridden.
 */
void startup_pend_sv_handler(void);

/*
 * Function:
 *     startup_system_tick_handler
 *
 * Purpose:
 *     Handles SysTick when no specialized handler is installed.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None.
 *
 * Notes:
 *     Execution Context: exception. This weak symbol aliases startup_default_handler unless overridden.
 */
void startup_system_tick_handler(void);

#ifdef __cplusplus
}
#endif

#endif // STARTUP_STM32F446XX_H
