/* No filesystem. A bounded heap supports newlib's integer formatting internals. */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
extern char _heap_start,_heap_end;
void *_sbrk(ptrdiff_t increment){
    static char *next;
    if(!next)next=&_heap_start;
    intptr_t available=(intptr_t)&_heap_end-(intptr_t)next;
    if(increment<0 || increment>available){errno=ENOMEM;return (void*)-1;}
    char *previous=next;next+=increment;return previous;
}
int _close(int fd){(void)fd;errno=EBADF;return -1;}
int _lseek(int fd,int offset,int whence){(void)fd;(void)offset;(void)whence;errno=ESPIPE;return -1;}
int _read(int fd,char *buffer,int length){(void)fd;(void)buffer;(void)length;errno=EBADF;return -1;}
int _write(int fd,const char *buffer,int length){(void)fd;(void)buffer;(void)length;errno=EBADF;return -1;}
int _fstat(int fd,struct stat *st){(void)fd;(void)st;errno=EBADF;return -1;}
int _isatty(int fd){(void)fd;return 0;}
