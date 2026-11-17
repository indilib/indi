Delimited TTY reader EOF regression

From the repository root:
  python3 test/tty_eof/run.py . build-testing/tty-eof

Requires Python 3, a Linux/POSIX C toolchain with GNU-compatible linker
--gc-sections support, and libm. No installed INDI library, equipment, emulator
server, or network connection is required.

The runner compiles the actual libs/indicore/indicom.c translation unit, with
minimal test-only generated configuration/version headers. Unrelated functions
are discarded at link time. The C regression uses local AF_UNIX socket pairs.

Six cases cover empty EOF, partial reply followed by EOF, and a valid delimited
reply for each of tty_read_section() and tty_nread_section(). EOF must return
TTY_READ_ERROR and report only bytes actually received; a valid reply must
return TTY_OK. The process exits nonzero if any case fails.

Before the fix: four EOF failures and two valid-reply passes.
After the fix: all six cases pass. Datagram-specific branches are not exercised
by these stream-socket tests.
