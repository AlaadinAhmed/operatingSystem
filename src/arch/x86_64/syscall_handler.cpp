#include <cstddef>
#include <stdint.h>

extern "C" int64_t sys_write(int fd, const char *str, size_t count);
extern "C" void sys_yield();
extern "C" void sys_exit(int code);
extern "C" int64_t sys_open(const char *path, int flags);
extern "C" int64_t sys_read(int fd, uint8_t *buffer, size_t count);
extern "C" int64_t sys_close(int fd);

typedef int64_t (*syscall_fn)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

// Dispatcher Function
extern "C" int64_t c_syscall_handler(uint64_t sys_num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4,
                                     uint64_t arg5) {
    switch (sys_num) {
    case 0: // SYS_YIELD
        sys_yield();
        return 0;

    case 1: // SYS_WRITE
        return sys_write((int)arg1, (const char *)arg2, (size_t)arg3);

    case 2: // SYS_EXIT
        sys_exit((int)arg1);
        return 0;

    case 3: // SYS_OPEN
        return sys_open((const char *)arg1, (int)arg2);

    case 4: // SYS_READ
        return sys_read((int)arg1, (uint8_t *)arg2, (size_t)arg3);

    case 5: // SYS_CLOSE
        return sys_close((int)arg1);

    default:
        return -1; // ENOSYS: Unknown system call
    }
}
