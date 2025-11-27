#include <sys/stat.h>
#include <stdint.h>
#include <errno.h>

extern int _write(int file, char *ptr, int len);

/* stubs to make stdio.h happy */
int _close(int file) {
    return -1;
}

int _fstat(int file, struct stat *st) {
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file) {
    return 1;
}

int _lseek(int file, int ptr, int dir) {
    return 0;
}

int _read(int file, char *ptr, int len) {
    errno = EINVAL;
    return -1;
}

/* MALLOC WILL FAIL! */
void * _sbrk(void) {
	return (void *)-1;
}
