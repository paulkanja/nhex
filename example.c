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
