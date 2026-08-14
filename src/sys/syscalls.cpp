#include "cpu/cpuid.h"
#include "fs/vfs.h"
#include "scheduler.h"
#include "sys/syscalls.h"
void init_syscall() {
    uint64_t efer = read_msr(IA32_EFER_MSR);
    write_msr(IA32_EFER_MSR, efer | 0x1);
    uint64_t star = ((uint64_t)0x08 << 32) | ((uint64_t)0x18 << 48);
    write_msr(IA32_STAR_MSR, star);
    write_msr(IA32_LSTAR_MSR, (uint64_t)asm_syscall_entry);
    write_msr(IA32_FMASK_MSR, 0x200);
}
static inline int64_t user_syscall(uint64_t sys_num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    int64_t ret;
    register uint64_t r10_val asm("r10") = a4;
    register uint64_t r8_val asm("r8") = a5;

    asm volatile("syscall"
                 : "=a"(ret)
                 : "a"(sys_num), "D"(a1), "S"(a2), "d"(a3), "r"(r10_val), "r"(r8_val)
                 : "rcx", "r11", "memory");
    return ret;
}
int64_t sys_open(const char *path, int flags) {
    vfs_node *node = vfs_open(path, flags);
    if (!node) {
        return -1;
    }
    int fd = -1;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (current_task->filedescriptors[i] == nullptr) {
            fd = i;
            break;
        }
    }
    if (fd == -1) {
        return -1;
    }
    FileDescriptor *desc = new FileDescriptor();
    desc->node = node;
    desc->offset = 0;
    desc->flags = flags;
    current_task->filedescriptors[fd] = desc;
    return fd;
}
extern "C" int64_t sys_read(int fd, uint8_t *buffer, std::size_t count) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        return -1;
    }
    FileDescriptor *desc = current_task->filedescriptors[fd];
    if (!desc || !desc->node || !desc->node->read) {
        return -1;
    }
    int64_t bytes_read = desc->node->read(desc->node, desc->offset, count, buffer);
    if (bytes_read > 0) {
        desc->offset += bytes_read;
    }
    return bytes_read;
}
extern "C" int64_t sys_write(int fd, uint8_t *buffer, std::size_t count) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        return -1;
    }
    FileDescriptor *desc = current_task->filedescriptors[fd];
    if (!desc || !desc->node || !desc->node->write) {
        return -1;
    }
    int64_t bytes_written = desc->node->write(desc->node, desc->offset, count, buffer);
    if (bytes_written > 0) {
        desc->offset += bytes_written;
    }
    return bytes_written;
}
extern "C" int64_t sys_close(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        return -1;
    }
    FileDescriptor *desc = current_task->filedescriptors[fd];
    if (!desc) {
        return -1;
    }
    if (desc->node && desc->node->close) {
        desc->node->close(desc->node);
    }
    delete desc;
    current_task->filedescriptors[fd] = nullptr;
    return 0;
}

extern "C" void sys_yield() {
    // Stub
}

extern "C" void sys_exit(int code) {
    (void)code;
    while (true) {
        asm volatile("hlt");
    }
}
