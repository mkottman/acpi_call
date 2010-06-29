/* Copyright (c) 2010: Michal Kottman */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <acpi/acpi.h>

MODULE_LICENSE("GPL");

extern struct proc_dir_entry *acpi_root_dir;

static acpi_handle root_handle;

static int last_result;

static void do_acpi_call(const char * method)
{
    acpi_status status;
    acpi_handle handle;
    union acpi_object atpx_arg_elements[1];
    struct acpi_object_list atpx_arg;
    struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };

    printk(KERN_INFO "acpi_call: Calling %s\n", method);

    status = acpi_get_handle(root_handle, (acpi_string) method, &handle);
    last_result = 0;

    if (ACPI_FAILURE(status))
    {
        printk(KERN_ERR "acpi_call: Cannot get handle: %s\n", acpi_format_exception(status));
        return;
    }

    // for now, parameterless functions only
    atpx_arg.count = 0;
    atpx_arg.pointer = &atpx_arg_elements[0];

    // just to be sure
    atpx_arg_elements[0].type = ACPI_TYPE_INTEGER;
    atpx_arg_elements[0].integer.value = 0;

    status = acpi_evaluate_object(handle, NULL, &atpx_arg, &buffer);
    if (ACPI_FAILURE(status))
    {
        printk(KERN_ERR "acpi_call: ATPX method call failed: %s\n", acpi_format_exception(status));
        return;
    }
    kfree(buffer.pointer);

    last_result = 1;
    printk(KERN_INFO "acpi_call: Call successful\n");
}

static int acpi_proc_write( struct file *filp, const char __user *buff,
    unsigned long len, void *data )
{
    char input[256] = { '\0' };

    if (len > sizeof(input) - 1) {
        printk(KERN_ERR "acpi_call: Input too long! (%lu)\n", len);
        return -ENOSPC;
    }

    if (copy_from_user( input, buff, len )) {
        return -EFAULT;
    }
    input[len] = '\0';
    if (input[len-1] == '\n')
        input[len-1] = '\0';

    do_acpi_call(input);
    return len;
}

static int acpi_proc_read(char *page, char **start, off_t off,
    int count, int *eof, void *data)
{
    int len = 0;

    if (off > 0) {
        *eof = 1;
        return 0;
    }

    switch (last_result) {
    case -1: len = sprintf(page, "not called"); break;
    case 0: len = sprintf(page, "failed"); break;
    case 1: len = sprintf(page, "ok"); break;
    }
    last_result = -1;

    return len;
}

static int __init init_acpi_call(void)
{
    struct proc_dir_entry *acpi_entry = create_proc_entry("call", 0666, acpi_root_dir);

    last_result = -1;

    if (acpi_entry == NULL) {
      printk(KERN_ERR "acpi_call: Couldn't create proc entry\n");
      return -ENOMEM;
    }

    acpi_entry->write_proc = acpi_proc_write;
    acpi_entry->read_proc = acpi_proc_read;
    printk(KERN_INFO "acpi_call: Module loaded successfully\n");

    return 0;
}


static void unload_acpi_call(void)
{
    remove_proc_entry("call", acpi_root_dir);
}

module_init(init_acpi_call);
module_exit(unload_acpi_call);


