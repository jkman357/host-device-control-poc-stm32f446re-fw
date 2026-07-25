// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     main.h
//
// Purpose:
//     Declares the standard C firmware entry point.
//
// Public Contract:
//     - Exposes only the entry-point declaration required by startup integration.

#ifndef MAIN_H
#define MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Function:
 *     main
 *
 * Purpose:
 *     Initializes the firmware and runs the application dispatcher.
 *
 * Input Parameters:
 *     None.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     None during normal operation because the dispatcher loop does not return.
 *
 * Notes:
 *     The name main is required by the freestanding C startup contract.
 */
int main(void);

#ifdef __cplusplus
}
#endif

#endif // MAIN_H
