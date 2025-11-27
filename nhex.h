#ifndef _h_NHEX_
#define _h_NHEX_

#include <stdbool.h>
#include <stdio.h>

typedef long long nhc_t;

enum nhtflag {
    NHT_ECHO     =  1,
    NHT_NOECHO   =  2,
    NHT_RAW      =  4,
    NHT_NORAW    =  8,
    NHT_CBREAK   = 16,
    NHT_NOCBREAK = 32,
};

// TODO: use cross-platform keymaps
#define NHKEY_TAB           (0x09)
#define NHKEY_ESC           (0x1B)
#define NHKEY_UP        (0x1B5B41)
#define NHKEY_DOWN      (0x1B5B42)
#define NHKEY_RIGHT     (0x1B5B43)
#define NHKEY_LEFT      (0x1B5B44)
#define NHKEY_CLEAR     (0x1B5B45)
#define NHKEY_END       (0x1B5B46)
#define NHKEY_HOME      (0x1B5B48)
#define NHKEY_BTAB      (0x1B5B60)
#define NHKEY_INS     (0x1B5B327E)
#define NHKEY_DEL     (0x1B5B337E)
#define NHKEY_PGUP    (0x1B5B357E)
#define NHKEY_PGDOWN  (0x1B5B367E)

void  nhclear();
bool  nhcpos(int *row_ptr, int *col_ptr);
int   nhcols();
void  nhend();
int   nhflush();
nhc_t nhgetc();
bool  nhgettflags(int *flags_ptr);
bool  nhmv(int row, int col);
int   nhmvnf(int row, int col);
bool  nhinit();
int   nhprint(const char *str);
int   nhprintf(const char *format, ...);
int   nhrows();
bool  nhsettflags(int flags);
bool  nhwsize(int *rows_ptr, int *cols_ptr);

#endif // _h_NHEX_



#ifdef NHEX_IMPLEMENTATION

#ifndef _WIN32

#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define _DEFAULT_NHEX_BUFFER_SIZE (1024)

static struct {
    int initialized;
    char *buffer;
    size_t buffer_count, buffer_size;
    struct termios ctx_term;
    struct winsize ctx_ws;
    struct termios origin_term;
} _ctx;

static void _nhcls();
static void _nhcontf(int sig);
static bool _nhreserve(size_t count);
static void _nhsigf(int sig);
static void _nhtstpf(int sig);

void nhclear() {
    if (!_ctx.initialized) { return; }
    _ctx.buffer_count = 0;
    _ctx.buffer[0] = '\0';
    // TODO: properly shrink the buffer
    _nhcls();
}

// TODO: investigate methods of getting pos without nhflush
bool nhcpos(int *row_ptr, int *col_ptr) {
    if (!row_ptr && !col_ptr) { return true; }
    int row = -1;
    int col = -1;
    struct termios fterm = _ctx.ctx_term;
    fterm.c_lflag &= ~(ECHO | ECHONL);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &fterm) == -1) { return false; }
    nhflush();
    printf("\033[6n");
    fflush(stdout);
    char buffer[32];
    if (read(STDIN_FILENO, buffer, 32) == -1) {
        tcsetattr(STDIN_FILENO, TCSANOW, &_ctx.ctx_term);
        return false;
    }
    if (tcsetattr(STDIN_FILENO, TCSANOW, &_ctx.ctx_term) == -1) {
        return false;
    }
    if (sscanf(buffer + 2, "%d;%d", &row, &col) == EOF) { return false; }
    if (row_ptr) { *row_ptr = row; }
    if (col_ptr) { *col_ptr = col; }
    return true;
}

int nhcols() {
    int cols;
    nhwsize(&cols, NULL);
    return cols;
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
    if (!_ctx.initialized) { return EOF; }
    if (_ctx.buffer_count == 0) { return 0; }
    struct termios fterm = _ctx.ctx_term;
    fterm.c_oflag |= OPOST;
    // TODO: handle potential 'tcsetattr' errors
    tcsetattr(STDIN_FILENO, TCSANOW, &fterm);
    _nhcls();
    if (fwrite(_ctx.buffer, 1, _ctx.buffer_count, stdout) <= 0) {
        return EOF;
    }
    printf("\033[0m");
    int out = fflush(stdout);
    // TODO: handle potential 'tcsetattr' errors
    tcsetattr(STDIN_FILENO, TCSANOW, &_ctx.ctx_term);
    return out;
}

nhc_t nhgetc() {
    if (!_ctx.initialized) { return -1; }
    char input[8];
    int n = (int)read(STDIN_FILENO, input, 8);
    if (n < 0) { return n; }
    nhc_t c = 0LL;
    for (int i = 0; i < n; ++i) {
        c <<= 8;
        c |= input[i];
    }
    return c;
}

bool nhgettflags(int *flags_ptr) {
    *flags_ptr = 0;
    if (!_ctx.initialized) { return false; }
    if (tcgetattr(STDIN_FILENO, &_ctx.ctx_term) == -1) { return false; }
    int flags = 0;
    if (
        _ctx.ctx_term.c_lflag & ECHO &&
        _ctx.ctx_term.c_lflag & ECHONL
    ) { flags |= NHT_ECHO; } else { flags |= NHT_NOECHO; }
    if (!(
        _ctx.ctx_term.c_lflag & ISIG
    )) { flags |= NHT_CBREAK; } else { flags |= NHT_NOCBREAK; }
    if (!(
        _ctx.ctx_term.c_oflag & OPOST ||
        _ctx.ctx_term.c_iflag & (BRKINT | ICRNL | IXON) ||
        _ctx.ctx_term.c_lflag & (ICANON | IEXTEN) ||
        !(_ctx.ctx_term.c_cflag & CS8)
    )) { flags |= NHT_RAW; } else { flags |= NHT_NORAW; }
    *flags_ptr = flags;
    return true;
}

bool nhinit() {
    if (_ctx.initialized) { return false; }
    if (tcgetattr(STDIN_FILENO, &_ctx.origin_term) == -1) { return false; }
    _ctx.ctx_term = _ctx.origin_term;
    _ctx.ctx_term.c_iflag &= ~(
        IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON
    );
    _ctx.ctx_term.c_oflag &= ~OPOST;
    _ctx.ctx_term.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN);
    _ctx.ctx_term.c_cflag &= ~(CSIZE | PARENB);
    _ctx.ctx_term.c_cflag |= CS8;
    _ctx.ctx_term.c_cc[VMIN] = 1;
    _ctx.ctx_term.c_cc[VTIME] = 0;
    // TODO: handle potential 'tcsetattr' error
    if (tcsetattr(STDIN_FILENO, TCSANOW, &_ctx.ctx_term) == -1) {
        return false;
    }
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
        // TODO: handle potential 'tcsetattr' error
        tcsetattr(STDIN_FILENO, TCSANOW, &_ctx.ctx_term);
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

bool nhmv(int row, int col) {
    if (nhmvnf(row, col) < 0) { return false; }
    return nhflush() != EOF;
}

int nhmvnf(int row, int col) {
    if (row > 0 && col > 0) {
        return nhprintf("\033[%d;%dH", row, col);
    }
    if (col > 0) {
        return nhprintf("\033[%dG", col);
    }
    if (row > 0) {
        int _col;
        if (!nhcpos(NULL, &_col)) { return -1; }
        return nhprintf("\033[s\033[%d;%dH\033[u", row, _col);
    }
    return 0;
}

int nhprint(const char *str) {
    if (!_ctx.initialized) { return -1; }
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

int nhrows() {
    int rows;
    nhwsize(NULL, &rows);
    return rows;
}

bool nhsettflags(int flags) {
    if (!_ctx.initialized) { return false; }
    if (flags & NHT_NOECHO) {
        _ctx.ctx_term.c_lflag &= ~(ECHO & ECHONL);
    } else if (flags & NHT_ECHO) {
        _ctx.ctx_term.c_lflag |=   ECHO | ECHONL;
    }
    if (flags & NHT_NOCBREAK) {
        _ctx.ctx_term.c_lflag |=  ISIG;
    } else if (flags & NHT_CBREAK) {
        _ctx.ctx_term.c_lflag &= ~ISIG;
    }
    if (flags & NHT_NORAW) {
        _ctx.ctx_term.c_oflag |= OPOST;
        _ctx.ctx_term.c_iflag |= BRKINT | ICRNL | IXON;
        _ctx.ctx_term.c_lflag |= ICANON | IEXTEN;
        _ctx.ctx_term.c_cflag |= CS7;
    } else if (flags & NHT_RAW) {
        _ctx.ctx_term.c_oflag &= ~OPOST;
        _ctx.ctx_term.c_iflag &= ~(BRKINT | ICRNL | IXON);
        _ctx.ctx_term.c_lflag &= ~(ICANON | IEXTEN);
        _ctx.ctx_term.c_cflag &= ~(CSIZE | PARENB);
        _ctx.ctx_term.c_cflag |= CS8;
    }
    return tcsetattr(STDIN_FILENO, TCSANOW, &_ctx.ctx_term) != -1;
}

bool nhwsize(int *rows_ptr, int *cols_ptr) {
    if (!rows_ptr && !cols_ptr) { return true; }
    if (rows_ptr) { *rows_ptr = -1; }
    if (cols_ptr) { *cols_ptr = -1; }
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &_ctx.ctx_ws) == -1) { return false; }
    if (rows_ptr != NULL) { *rows_ptr = _ctx.ctx_ws.ws_row; }
    if (cols_ptr != NULL) { *cols_ptr = _ctx.ctx_ws.ws_col; }
    return true;
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

// TODO: implement Windows version

#endif // _WIN32

#endif // NHEX_IMPLEMENTATION
