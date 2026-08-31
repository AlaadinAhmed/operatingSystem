#include "cpu/cpuid.h"
#include "elf.h"
#include "fs/pty.h"
#include "fs/vfs.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "process.h"
#include "scheduler.h"
#include "sys/syscalls.h"
#include <cstdint>
void init_syscall() {
    uint64_t efer = read_msr(IA32_EFER_MSR);
    write_msr(IA32_EFER_MSR, efer | 0x1);
    uint64_t star = ((uint64_t)0x08 << 32) | ((uint64_t)0x18 << 48);
    write_msr(IA32_STAR_MSR, star);
    write_msr(IA32_LSTAR_MSR, (uint64_t)asm_syscall_entry);
    write_msr(IA32_FMASK_MSR, 0x200);
}
static inline int64_t
user_syscall(uint64_t sys_num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
    int64_t           ret;
    register uint64_t r10_val asm("r10") = a4;
    register uint64_t r8_val asm("r8")   = a5;

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
    FileDescriptor *desc              = new FileDescriptor();
    desc->node                        = node;
    desc->offset                      = 0;
    desc->flags                       = flags;
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
extern "C" int64_t sys_ioctl(int fd, uint64_t request, void *args) {
    if (fd < 0 || fd >= MAX_OPEN_FILES)
        return -1;
    FileDescriptor *desc = current_task->filedescriptors[fd];
    if (!desc || !desc->node)
        return -1;
    PtyPair *pty = static_cast<PtyPair *>(desc->node->device_data);
    if (!pty || !pty->in_use)
        return -1;
    switch (request) {
    case TIOCPTN: {
        if (!args)
            return -1;
        *static_cast<int *>(args) = pty->id;
        return 0;
    }
    case TIOCSPTLCK: {
        if (!args)
            return -1;
        pty->locked = (*static_cast<int *>(args) != 0);
        return 0;
    }
    default:
        return -1;
    }
}
extern "C" int64_t sys_dup2(int oldfd, int newfd) {
    if (oldfd < 0 || oldfd >= MAX_OPEN_FILES || newfd < 0 || newfd >= MAX_OPEN_FILES)
        return -1;
    FileDescriptor *src = current_task->filedescriptors[oldfd];
    if (current_task->filedescriptors[newfd] != nullptr) {
        sys_close(newfd);
    }
    FileDescriptor *clone                = new FileDescriptor();
    clone->node                          = src->node;
    clone->offset                        = src->offset;
    clone->flags                         = src->flags;
    current_task->filedescriptors[newfd] = clone;
    return newfd;
}
extern "C" int64_t sys_execve(const char *path, char *const argv[], char *const envp[]) {
    vfs_node *exec_node = vfs_open(path, 0);
    if (!exec_node) {
        return -1;
    }
    PageTable *new_plm4_phys = create_process_pml4();
    uint64_t   entry_point   = 0;
    if (!load_elf_binary(exec_node, (uint64_t *)new_plm4_phys, &entry_point)) {
        return -1;
    }
    uint64_t user_stack_top;
    void    *stack_frame = pmm_alloc_page();
    vmm_map_page(new_plm4_phys, user_stack_top, (uint64_t)stack_frame,
                 VMM_PRESENT | VMM_USER | VMM_WRITE);
    PageTable *old_plm4         = current_task->pml4_physical;
    current_task->pml4_physical = new_plm4_phys;
    asm volatile("mov %0, %%cr3" ::"r"(new_plm4_phys) : "memory");
    vmm_destroy_user_space(old_plm4);
    current_task->context->rip    = entry_point;
    current_task->context->rsp    = user_stack_top;
    current_task->context->rflags = 0x202;

    return 0;
}
