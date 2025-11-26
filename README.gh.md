# nhex

> [!WARNING]
> This library is not thoroughly tested. Use at your own discretion. For a production-ready library, checkout `ncurses` and similar or offshoot libraries.

> [!WARNING]
> This library is not implemented for Windows.

A simple single-header C library for terminal alternate buffer management and utilities.

### Why does this library exist?

This is a simple library made for a side project of mine. In need of a simple lightweight terminal library, I just handrolled the few lines of code necessary.

## License

This work is marked <a href="https://creativecommons.org/publicdomain/zero/1.0/">CC0 1.0 Universal</a>
<br>
<img src="https://mirrors.creativecommons.org/presskit/icons/cc.svg" alt="" width="24"><img src="https://mirrors.creativecommons.org/presskit/icons/zero.svg" alt="" width="24">

## Example

```c
#define NHEX_IMPLEMENTATION
#include "nhex.h"

int main() {
    nhinit();

    char quit_ch = 'q';
    nhprint("Hello, \033[0;1;31mnhex!\033[0m\n");
    nhprintf("\nPress '%c' to quit", quit_ch);
    nhflush();
    while (nhgetc() != quit_ch) ;

    nhend();
    return 0;
}
```

## Usage

Just download the file nhex.h or copy its contents and include it. It works for C99 and later versions of C.

## Dependencies

The library depends only on `libc`, using the POSIX header files `unistd.h` and `termios.h`.
