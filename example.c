#define NHEX_IMPLEMENTATION
#include "nhex.h"

int main() {
    nhinit();

    char quit_ch = 'q';
    nhprint("Hello, \033[0;1;31mnhex!\033[0m\n");
    nhprintf("\nPress '%c' to quit", quit_ch);
    nhflush();
    char inpc;
    while ((inpc = nhgetc()) >= 0) {
        if (inpc == quit_ch) { break; }
    }

    nhend();
    return 0;
}
