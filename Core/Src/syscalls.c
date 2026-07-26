// Copyright (c) 2026 Ray Yang. All rights reserved.

/*
 * Minimal newlib syscall boundary.
 *
 * This firmware does not expose a POSIX file-descriptor layer.  The explicit
 * stubs below prevent GCC/newlib-nano from selecting libnosys warning stubs
 * during the final link.  Each unsupported operation fails deterministically.
 */

int _close(int file_descriptor)
{
    (void)file_descriptor;
    return -1;
}

int _lseek(int file_descriptor, int offset, int origin)
{
    (void)file_descriptor;
    (void)offset;
    (void)origin;
    return -1;
}

int _read(int file_descriptor, char *buffer, int length)
{
    (void)file_descriptor;
    (void)buffer;
    (void)length;
    return -1;
}

int _write(int file_descriptor, char *buffer, int length)
{
    (void)file_descriptor;
    (void)buffer;
    (void)length;
    return -1;
}
