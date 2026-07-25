/**
 * @file syscalls.c
 * @brief Explicit unsupported POSIX syscall stubs for the bare-metal PoC.
 *
 * The STM32CubeIDE nano C library may reference these symbols even when the
 * application does not use file I/O. Defining them here avoids libnosys
 * linker diagnostics while keeping file I/O intentionally unsupported.
 */

/**
 * @brief Reject a file close request because this firmware has no file system.
 * @param file_descriptor File descriptor supplied by the C library.
 * @return Always -1 because the operation is unsupported.
 */
int _close(int file_descriptor)
{
    (void)file_descriptor;
    return -1;
}

/**
 * @brief Reject a file seek request because this firmware has no file system.
 * @param file_descriptor File descriptor supplied by the C library.
 * @param offset Requested byte offset.
 * @param origin Requested seek origin.
 * @return Always -1 because the operation is unsupported.
 */
int _lseek(int file_descriptor, int offset, int origin)
{
    (void)file_descriptor;
    (void)offset;
    (void)origin;
    return -1;
}

/**
 * @brief Reject a file read request because transport input is handled directly.
 * @param file_descriptor File descriptor supplied by the C library.
 * @param buffer Destination buffer supplied by the C library.
 * @param length Requested byte count.
 * @return Always -1 because the operation is unsupported.
 */
int _read(int file_descriptor, char *buffer, int length)
{
    (void)file_descriptor;
    (void)buffer;
    (void)length;
    return -1;
}

/**
 * @brief Reject a file write request because transport output is handled directly.
 * @param file_descriptor File descriptor supplied by the C library.
 * @param buffer Source buffer supplied by the C library.
 * @param length Requested byte count.
 * @return Always -1 because the operation is unsupported.
 */
int _write(int file_descriptor, char *buffer, int length)
{
    (void)file_descriptor;
    (void)buffer;
    (void)length;
    return -1;
}
