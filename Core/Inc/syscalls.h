// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     syscalls.h
//
// Purpose:
//     Defines the newlib-nano syscall integration contract for the bare-metal PoC.
//
// Public Contract:
//     - Declares the externally mandated newlib syscall symbols.
//     - Defines all file and stream operations as unsupported.
//     - Does not provide heap allocation or I/O redirection.

#ifndef SYSCALLS_H
#define SYSCALLS_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Function:
 *     _close
 *
 * Purpose:
 *     Rejects a file close request because no file system is available.
 *
 * Input Parameters:
 *     file_descriptor:
 *         Supplies the newlib file descriptor.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     SYSCALL_RESULT_UNSUPPORTED:
 *         The operation is unsupported.
 *
 * Notes:
 *     The symbol name is mandated by newlib and is covered by Deviation Record DR-NAME-001.
 */
int _close(int file_descriptor);

/*
 * Function:
 *     _lseek
 *
 * Purpose:
 *     Rejects a file seek request because no file system is available.
 *
 * Input Parameters:
 *     file_descriptor:
 *         Supplies the newlib file descriptor.
 *     offset:
 *         Supplies the requested byte offset.
 *     origin:
 *         Supplies the requested seek origin.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     SYSCALL_RESULT_UNSUPPORTED:
 *         The operation is unsupported.
 *
 * Notes:
 *     The symbol name is mandated by newlib and is covered by Deviation Record DR-NAME-001.
 */
int _lseek(int file_descriptor, int offset, int origin);

/*
 * Function:
 *     _read
 *
 * Purpose:
 *     Rejects a standard file read request.
 *
 * Input Parameters:
 *     file_descriptor:
 *         Supplies the newlib file descriptor.
 *     buffer:
 *         Points to caller-owned destination storage.
 *     length:
 *         Supplies the requested byte count.
 *
 * Output Parameters:
 *     buffer:
 *         Remains unchanged because the operation is unsupported.
 *
 * Return Value:
 *     SYSCALL_RESULT_UNSUPPORTED:
 *         The operation is unsupported.
 *
 * Notes:
 *     The symbol name is mandated by newlib and is covered by Deviation Record DR-NAME-001.
 */
int _read(int file_descriptor, char *buffer, int length);

/*
 * Function:
 *     _write
 *
 * Purpose:
 *     Rejects a standard file write request.
 *
 * Input Parameters:
 *     file_descriptor:
 *         Supplies the newlib file descriptor.
 *     buffer:
 *         Points to caller-owned source data that is not retained or modified.
 *     length:
 *         Supplies the requested byte count.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     SYSCALL_RESULT_UNSUPPORTED:
 *         The operation is unsupported.
 *
 * Notes:
 *     The symbol name is mandated by newlib and is covered by Deviation Record DR-NAME-001.
 */
int _write(int file_descriptor, char *buffer, int length);

#ifdef __cplusplus
}
#endif

#endif // SYSCALLS_H
