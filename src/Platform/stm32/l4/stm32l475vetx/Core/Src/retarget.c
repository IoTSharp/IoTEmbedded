#include "Board/Inc/bsp_board.h"

#include <errno.h>
#include <sys/stat.h>

#undef errno
extern int errno;

int _write(int file, char *ptr, int len) {
  (void)file;
  if (ptr == NULL || len <= 0) {
    return 0;
  }

  if (HAL_UART_Transmit(BSP_DEBUG_UART_HANDLE, (uint8_t *)ptr, (uint16_t)len, HAL_MAX_DELAY) == HAL_OK) {
    return len;
  }

  errno = EIO;
  return -1;
}

int _read(int file, char *ptr, int len) {
  (void)file;
  (void)ptr;
  (void)len;
  errno = ENOSYS;
  return -1;
}

int _close(int file) {
  (void)file;
  return -1;
}

int _fstat(int file, struct stat *st) {
  (void)file;
  if (st == NULL) {
    errno = EINVAL;
    return -1;
  }
  st->st_mode = S_IFCHR;
  return 0;
}

int _isatty(int file) {
  (void)file;
  return 1;
}

int _lseek(int file, int ptr, int dir) {
  (void)file;
  (void)ptr;
  (void)dir;
  return 0;
}
