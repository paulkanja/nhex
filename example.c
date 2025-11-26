#define NHEX_IMPLEMENTATION
#include "nhex.h"

int main() {
    nhinit();

    nhprintf("Hello, \033[0;1;31mnhex!");
    nhflush();
    while (nhgetc() != 'q') ;

    nhend();
    return 0;
}
