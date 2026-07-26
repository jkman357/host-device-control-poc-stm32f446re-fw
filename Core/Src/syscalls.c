// Copyright (c) 2026 Ray Yang. All rights reserved.
//
// File:
//     syscalls.c
//
// Purpose:
//     Provides deterministic unsupported newlib syscall stubs.
//
// Responsibilities:
//     - Prevents newlib-nano from linking warning-producing libnosys stubs.
//     - Rejects unsupported file-descriptor input and output operations.
//
// Notes:
//     The firmware does not provide a POSIX file system or console through these functions.

#define SYSCALL_RESULT_ERROR (-1)

/*
 * Function:
 *     _close
 *
 * Purpose:
 *     Rejects an unsupported newlib file-descriptor close request.
 *
 * Input Parameters:
 *     file_descriptor:
 *         File descriptor supplied by newlib.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     SYSCALL_RESULT_ERROR:
 *         The operation is unsupported.
 *
 * Notes:
 *     The symbol name is required by newlib and is exempt from module-prefix naming.
 */
int _close(int file_descriptor)
{
    (void)file_descriptor;
    return SYSCALL_RESULT_ERROR;
}

/*
 * Function:
 *     _lseek
 *
 * Purpose:
 *     Rejects an unsupported newlib file-position request.
 *
 * Input Parameters:
 *     file_descriptor:
 *         File descriptor supplied by newlib.
 *     offset:
 *         Requested file offset.
 *     origin:
 *         Requested seek origin.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     SYSCALL_RESULT_ERROR:
 *         The operation is unsupported.
 *
 * Notes:
 *     The symbol name is required by newlib and is exempt from module-prefix naming.
 */
int _lseek(int file_descriptor, int offset, int origin)
{
    (void)file_descriptor;
    (void)offset;
    (void)origin;
    return SYSCALL_RESULT_ERROR;
}

/*
 * Function:
 *     _read
 *
 * Purpose:
 *     Rejects an unsupported newlib file-descriptor read request.
 *
 * Input Parameters:
 *     file_descriptor:
 *         File descriptor supplied by newlib.
 *     buffer:
 *         Caller-provided destination pointer; it is not accessed.
 *     length:
 *         Requested byte count.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     SYSCALL_RESULT_ERROR:
 *         The operation is unsupported.
 *
 * Notes:
 *     The symbol name is required by newlib and is exempt from module-prefix naming.
 */
int _read(int file_descriptor, char *buffer, int length)
{
    (void)file_descriptor;
    (void)buffer;
    (void)length;
    return SYSCALL_RESULT_ERROR;
}

/*
 * Function:
 *     _write
 *
 * Purpose:
 *     Rejects an unsupported newlib file-descriptor write request.
 *
 * Input Parameters:
 *     file_descriptor:
 *         File descriptor supplied by newlib.
 *     buffer:
 *         Caller-provided source pointer; it is not accessed.
 *     length:
 *         Requested byte count.
 *
 * Output Parameters:
 *     None.
 *
 * Return Value:
 *     SYSCALL_RESULT_ERROR:
 *         The operation is unsupported.
 *
 * Notes:
 *     The symbol name is required by newlib and is exempt from module-prefix naming.
 */
int _write(int file_descriptor, char *buffer, int length)
{
    (void)file_descriptor;
    (void)buffer;
    (void)length;
    return SYSCALL_RESULT_ERROR;
}
