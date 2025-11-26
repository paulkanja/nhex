#ifndef _h_NHEX_
#define _h_NHEX_

#include <stdbool.h>
#include <stdio.h>

void nhclear();
void nhend();
int  nhflush();
int  nhgetc();
bool nhinit();
int  nhprint(const char *str);
int  nhprintf(const char *format, ...);

#endif // _h_NHEX_


#ifdef NHEX_IMPLEMENTATION

// TODO: implement Windows version
#ifndef _WIN32

#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define _DEFAULT_NHEX_BUFFER_SIZE (1024)

static struct {
    int initialized;
    char *buffer;
    size_t buffer_count, buffer_size;
    struct termios ctx_term;
    struct termios origin_term;
} _ctx;

static void _nhcls();
static void _nhcontf(int sig);
static bool _nhreserve(size_t count);
static void _nhsigf(int sig);
static void _nhtstpf(int sig);

void nhclear() {
    _ctx.buffer_count = 0;
    _ctx.buffer[0] = '\0';
    // TODO: properly shrink the buffer
    _nhcls();
}

void nhend() {
    if (!_ctx.initialized) { return; }
    free(_ctx.buffer);
    _ctx.initialized = false;
    // TODO: restore signal handlers
    printf("\033[0m\033[?25h\033[?1049l");
    fflush(stdout);
    // TODO: handle potential 'tcsetattr' error
    tcsetattr(STDIN_FILENO, TCSANOW, &_ctx.origin_term);
}

int nhflush() {
    _ctx.ctx_term.c_oflag |= OPOST;
    // TODO: handle potential 'tcsetattr' errors
    tcsetattr(STDIN_FILENO, TCSANOW, &_ctx.ctx_term);
    if (!_ctx.initialized) { return EOF; }
    if (_ctx.buffer_count == 0) { return 0; }
    if (fwrite(_ctx.buffer, 1, _ctx.buffer_count, stdout) <= 0) {
        return EOF;
    }
    printf("\033[0m");
    int out = fflush(stdout);
    _ctx.ctx_term.c_oflag &= ~OPOST;
    // TODO: handle potential 'tcsetattr' errors
    tcsetattr(STDIN_FILENO, TCSANOW, &_ctx.ctx_term);
    return out;
}

int nhgetc() {
    char input[4];
    int n = (int)read(STDIN_FILENO, input, 4);
    if (n < 0) { return n; }
    int c = 0;
    for (int i = 0; i < n; ++i) {
        c <<= 8;
        c |= input[i];
    }
    return c;
}

bool nhinit() {
    if (_ctx.initialized) { return false; }
    if (tcgetattr(STDIN_FILENO, &_ctx.origin_term) == -1){
        return false;
    }
    _ctx.ctx_term = _ctx.origin_term;
    _ctx.ctx_term.c_iflag &= ~(
        IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON
    );
    _ctx.ctx_term.c_oflag &= ~OPOST;
    _ctx.ctx_term.c_cflag &= ~(CSIZE | PARENB);
    _ctx.ctx_term.c_cflag |= CS8;
    _ctx.ctx_term.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN);
    _ctx.ctx_term.c_cc[VMIN] = 1;
    _ctx.ctx_term.c_cc[VTIME] = 0;
    // TODO: handle potential 'tcsetattr' error
    tcsetattr(STDIN_FILENO, TCSANOW, &_ctx.ctx_term);
    // TODO: save signal handlers
    signal(SIGABRT, &_nhsigf);
    signal(SIGINT,  &_nhsigf);
    signal(SIGKILL, &_nhsigf);
    signal(SIGQUIT, &_nhsigf);
    signal(SIGSEGV, &_nhsigf);
    signal(SIGTSTP, &_nhtstpf);
    signal(SIGCONT, &_nhcontf);
    char *buffer = (char *)malloc(_DEFAULT_NHEX_BUFFER_SIZE);
    if (!buffer) {
        _ctx.initialized = true;
        nhend();
        return false;
    }
    _ctx.buffer = buffer;
    _ctx.buffer_size = _DEFAULT_NHEX_BUFFER_SIZE;
    _ctx.buffer_count = 0;
    _ctx.buffer[0] = '\0';
    _ctx.initialized = true;
    _nhcls();
    return true;
}

int nhprint(const char *str) {
    size_t count = strlen(str);
    if (count == 0) { return 0; }
    if (!_nhreserve(count)) { return -1; }
    memmove(_ctx.buffer + _ctx.buffer_count, str, count);
    _ctx.buffer_count += count;
    _ctx.buffer[_ctx.buffer_count] = '\0';
    return count;
}

int nhprintf(const char *format, ...) {
    if (!_ctx.initialized) { return -1; }
    va_list args;
    va_start(args, format);
    int count = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (count <= 0) { return count; }
    if (!_nhreserve(count)) { return -1; }
    va_start(args, format);
    count = vsnprintf(
        _ctx.buffer + _ctx.buffer_count,
        count + 1, format, args
    );
    va_end(args);
    if (count > 0) {
        _ctx.buffer_count += count;
        _ctx.buffer[_ctx.buffer_count] = '\0';
        return count;
    }
    return -1;
}

static void _nhcls() {
    printf("\033[?1049h\033[2J\033[H");
    fflush(stdout);
}

static void _nhcontf(int sig) {
    signal(SIGTSTP, _nhtstpf);
    // TODO: handle potential 'tcsetattr' 'tcgetattr' errors
    tcgetattr(STDIN_FILENO, &_ctx.origin_term);
    tcsetattr(STDIN_FILENO, TCSANOW, &_ctx.ctx_term);
    _nhcls();
    nhflush();
    fflush(stdout);
    signal(sig, SIG_DFL);
    raise(sig);
    signal(sig, _nhcontf);
}

static bool _nhreserve(size_t count) {
    size_t desired = _ctx.buffer_count + count;
    // TODO: research better oerflow detection
    if (desired < _ctx.buffer_count || desired < count) {
        return false;
    }
    size_t size = _ctx.buffer_size;
    if (desired <= size) { return true; }
    while (size < desired) {
        // TODO: research better oerflow detection
        if ((size<<1) < size) {
            size = ~(size_t)0;
            break;
        }
        size <<= 1;
    }
    char *buffer = (char *)realloc(_ctx.buffer, size);
    if (buffer == NULL) {
        return false;
    }
    _ctx.buffer = buffer;
    _ctx.buffer_size = size;
    return true;
}

static void _nhsigf(int sig) {
    nhend();
    signal(sig, SIG_DFL);
    raise(sig);
}

static void _nhtstpf(int sig) {
    printf("\033[0m\033[?25h\033[?1049l");
    fflush(stdout);
    // TODO: handle potential 'tcsetattr' 'tcgetattr' errors
    tcgetattr(STDIN_FILENO, &_ctx.ctx_term);
    tcsetattr(STDIN_FILENO, TCSANOW, &_ctx.origin_term);
    signal(sig, SIG_DFL);
    raise(sig);
}

#else

void nhclear() {
    fprintf(stderr, "ERROR: nhex is not implemented for Windows\n");
}

void nhend() {
    fprintf(stderr, "ERROR: nhex is not implemented for Windows\n");
}

int nhflush() {
    fprintf(stderr, "ERROR: nhex is not implemented for Windows\n");
    return EOF;
}

int nhgetc() {
    fprintf(stderr, "ERROR: nhex is not implemented for Windows\n");
    return EOF;
}

bool nhinit() {
    fprintf(stderr, "ERROR: nhex is not implemented for Windows\n");
    return false;
}

int nhprintf(const char *format, ...) {
    fprintf(stderr, "ERROR: nhex is not implemented for Windows\n");
    return -1;
}

#endif // _WIN32

#endif // NHEX_IMPLEMENTATION
