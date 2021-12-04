// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include "obpf.h"

char LICENSE[] SEC("license") = "Dual BSD/GPL";
const volatile unsigned long long min_duration_ns = 0;

struct
{
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 256 * 1024);
} rb SEC(".maps");

struct
{
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 8192);
    __type(key, tid_t);
    __type(value, u64);
} dir_entries SEC(".maps");

const int            MAX_D_NAME_LEN = 128;
const volatile char* PATTERN;
const char           DT_DIR = 4;
//const char           DT_REG = 8;

SEC("tracepoint/syscalls/sys_enter_getdents64")
int handle_enter_getdents64(struct trace_event_raw_sys_enter *ctx)
{
    tid_t tid = bpf_get_current_pid_tgid();

    struct linux_dirent64 *d_entry = (struct linux_dirent64 *) ctx->args[1]; 
    if (d_entry == NULL)
    {
        return 0;
    }

    bpf_map_update_elem(&dir_entries, &tid, &d_entry, BPF_ANY);

    return 0;
}

SEC("tracepoint/syscalls/sys_exit_getdents64")
int handle_exit_getdents64(struct trace_event_raw_sys_exit *ctx)
{
    struct task_struct *task;
    struct event       *e;

    task = (struct task_struct *)bpf_get_current_task();
    tid_t tid = bpf_get_current_pid_tgid();

    long unsigned int * dir_addr  = bpf_map_lookup_elem(&dir_entries, &tid);
    if (dir_addr == NULL)
    {
        return 0;
    }

    bpf_map_delete_elem(&dir_entries, &tid);

    /* Loop over directory entries */
    struct linux_dirent64 * d_entry;
    unsigned short int      d_reclen;
    unsigned short int      d_name_len; 
    long                    offset = 0;

    long unsigned int d_entry_base_addr = *dir_addr;
    long              ret               = ctx->ret;

    //MAX_D_NAME_LEN == 128 - const isn't working with allocation here...
    char d_name[128];  
    int  count = 16;
    char d_type;

    int i=0;
    while (i < 256)   // limitation for now, only examine the first 256 entries
    {
        bpf_printk("Loop %d: offset: %d, total len: %d", i, offset, ret);

        if (offset >= ret)
        {
            break;
        }

        d_entry = (struct linux_dirent64 *) (d_entry_base_addr + offset);

        // read d_reclen
        bpf_probe_read_user(&d_reclen, sizeof(d_reclen), &d_entry->d_reclen);

        // skip if it's a directory entry
        bpf_probe_read_user(&d_type, sizeof(d_type), &d_entry->d_type);
        if (d_type == DT_DIR)
        {
            offset += d_reclen;
            i++;
            continue;
        }

        //read d_name
        d_name_len = d_reclen - 2 - (offsetof(struct linux_dirent64, d_name));
        long success = bpf_probe_read_user(&d_name, MAX_D_NAME_LEN, d_entry->d_name);
        if ( success != 0 )
        {
            offset += d_reclen;
            i++;
            continue;
        }

        bpf_printk("d_reclen: %d, d_name_len: %d, %s", d_reclen, d_name_len, d_name);

        /* match and overwrite */
        if ( d_name_len > 6 )
        {	
            bpf_printk("** sys_enter_getdents64 ** OVERWRITING"); 		
            
            char replace[] = "xxxxxx";
           
            // overwrite the user space d_name buffer
            long success = bpf_probe_write_user((char *) &d_entry->d_name, (char *) replace, sizeof(char) * 6);
            bpf_printk("** RESULT %d", success);  
        }

        offset += d_reclen;
        i++;
    }
	
    return 0;
}
