# Fixtures of the hardening check

Real linker output, never headers written by a test: `test_hardening_check.py` reads these files and expects what the platform's own tools reported for them. Each is named `*.bin` so nothing takes it for a program to run, and none is run.

The program, `probe.c`, built 2026-11-16:

```c
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) { char buf[16]; strcpy(buf, argc > 1 ? argv[1] : "x"); puts(buf); return 0; }
```

`delay.c`, for the delay-loaded import:

```c
#include <windows.h>
int main(int argc, char **argv) { (void)argv; if (argc > 5) MessageBeep(0); return 0; }
```

`tiny.c`, a static program with no C library and so no dynamic string table:

```c
void _start(void) { __asm__ volatile ("mov $60, %eax\n xor %edi, %edi\n syscall"); }
```

| File | Built with | Command |
| ---- | ---------- | ------- |
| `pe-hardened.exe.bin` | MSVC 19.52.37029, x64, Windows 11 | see below |
| `pe-plain.exe.bin` | MSVC 19.52.37029, x64, Windows 11 | see below |
| `pe32-hardened.exe.bin` | MSVC 19.52.37029, x86, Windows 11 | see below |
| `pe-delay.exe.bin` | MSVC 19.52.37029, x64, Windows 11 (`delay.c`) | see below |
| `pe32-delay.exe.bin` | MSVC 19.52.37029, x86, Windows 11 (`delay.c`) | see below |
| `pe-asan.exe.bin` | clang-cl 22.1.3, Visual Studio 18, x64 | see below |
| `elf-hardened.bin` | GCC 15.2.0, x86-64, Ubuntu 26.04 in WSL | see below |
| `elf-plain.bin` | GCC 15.2.0, x86-64, Ubuntu 26.04 in WSL | see below |
| `elf-asan.bin` | GCC 15.2.0, x86-64, Ubuntu 26.04 in WSL | see below |
| `elf-ubsan-static.bin.gz` | GCC 15.2.0, x86-64, Ubuntu 26.04 in WSL, then `gzip -9` | see below |
| `elf-static.bin` | GCC 15.2.0, x86-64, Ubuntu 26.04 in WSL (`tiny.c`, no C library) | see below |

```text
cl /nologo /O1 /MD /guard:cf /GS probe.c /Fe:pe-hardened.exe /link /guard:cf /CETCOMPAT /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT
cl /nologo /O1 /MD /GS- probe.c /Fe:pe-plain.exe /link /CETCOMPAT:NO /DYNAMICBASE:NO /HIGHENTROPYVA:NO /NXCOMPAT:NO /FIXED
cl /nologo /O1 /MD /guard:cf probe.c /Fe:pe32-hardened.exe /link /guard:cf /DYNAMICBASE /NXCOMPAT        (x86 tools)
cl /nologo /O1 /MD delay.c /Fe:pe-delay.exe /link user32.lib delayimp.lib /DELAYLOAD:user32.dll
cl /nologo /O1 /MD delay.c /Fe:pe32-delay.exe /link user32.lib delayimp.lib /DELAYLOAD:user32.dll        (x86 tools)
clang-cl /nologo /O1 /MD -fsanitize=address probe.c /Fe:pe-asan.exe
gcc -O1 -fPIE -pie -fstack-protector-all -Wl,-z,relro,-z,now,-z,noexecstack probe.c -o elf-hardened
gcc -O1 -no-pie -fno-PIE -fno-stack-protector -Wl,-z,norelro,-z,lazy,-z,execstack probe.c -o elf-plain
gcc -O1 -fsanitize=address probe.c -o elf-asan
gcc -O1 -fsanitize=undefined -static-libubsan probe.c -o elf-ubsan-static
gcc -O1 -static -nostdlib -fno-stack-protector tiny.c -o elf-static
strip elf-hardened elf-plain elf-asan elf-ubsan-static elf-static
```

What `dumpbin /headers /loadconfig /imports` (Visual Studio 18) reported:

| File | Dynamic base | High entropy | NX | Control Flow Guard | CET | Guard CF functions | Sanitizer import |
| ---- | :-: | :-: | :-: | :-: | :-: | :-: | ---- |
| `pe-hardened.exe.bin` | yes | yes | yes | yes | yes | 10 | none |
| `pe-plain.exe.bin` | no | no | no | no | no | 0 | none |
| `pe-asan.exe.bin` | yes | yes | yes | no | no | 0 | `clang_rt.asan_dynamic-x86_64.dll` |
| `pe32-hardened.exe.bin` | yes | not in a 32-bit image | yes | yes | no | 9 | none |
| `pe-delay.exe.bin` | yes | yes | yes | no | no | 0 | none; `USER32.dll` is delay-loaded |
| `pe32-delay.exe.bin` | yes | not in a 32-bit image | yes | no | no | 0 | none; `USER32.dll` delay-loaded |

What `readelf -hW`, `readelf -lW`, `readelf -dW` and `readelf --dyn-syms -W` (GNU binutils, Ubuntu 26.04) reported:

| File | Type | GNU_RELRO | BIND_NOW | GNU_STACK | `__stack_chk_fail` | Needed |
| ---- | ---- | :-: | :-: | :-: | :-: | ---- |
| `elf-hardened.bin` | DYN, PIE, with INTERP | yes | yes (`FLAGS` and `FLAGS_1`) | RW | yes | `libc.so.6` |
| `elf-plain.bin` | EXEC | no | no | RWE | no | `libc.so.6` |
| `elf-asan.bin` | DYN, PIE, with INTERP | yes | yes (`FLAGS` and `FLAGS_1`) | RW | yes | `libasan.so.8`, `libc.so.6` |
| `elf-ubsan-static.bin.gz` | DYN, PIE, with INTERP | yes | yes | RW | yes | `libm.so.6`, `libgcc_s.so.1`, `libc.so.6` |
| `elf-static.bin` | EXEC, no INTERP, no DYNAMIC | no | no | RW | no dynamic string table | none |

`pe32-delay.exe.bin`'s image base is 0x400000, which its test uses to rewrite the delay import into the older format that holds addresses.

The runtime linked into `elf-ubsan-static` leaves no needed library: what names it is the runtime's own option variable, `UBSAN_OPTIONS`, as a NUL-terminated string in the file (`grep -c UBSAN_OPTIONS` finds it; none of the other fixtures holds a sanitizer option variable).
