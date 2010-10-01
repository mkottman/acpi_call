/* Copyright (c) 2010: Michal Kottman */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <acpi/acpi.h>

MODULE_LICENSE("GPL");

extern struct proc_dir_entry *acpi_root_dir;

static char error_buffer[256];

static int last_result;

/**
@param method   The full name of ACPI method to call
@param argc     The number of parameters
@param argv     A pre-allocated array of arguments of type acpi_object
*/
static void do_acpi_call(const char * method, int argc, union acpi_object *argv)
{
    acpi_status status;
    acpi_handle handle;
    struct acpi_object_list atpx_arg;
    struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };

    printk(KERN_INFO "acpi_call: Calling %s\n", method);

    // get the handle of the method, must be a fully qualified path
    status = acpi_get_handle(NULL, (acpi_string) method, &handle);
    last_result = 0;

    if (ACPI_FAILURE(status))
    {
        strcpy(error_buffer, acpi_format_exception(status));
        printk(KERN_ERR "acpi_call: Cannot get handle: %s\n", error_buffer);
        return;
    }

    // prepare parameters
    atpx_arg.count = argc;
    atpx_arg.pointer = argv;

    // call the method
    status = acpi_evaluate_object(handle, NULL, &atpx_arg, &buffer);
    if (ACPI_FAILURE(status))
    {
        strcpy(error_buffer, acpi_format_exception(status));
        printk(KERN_ERR "acpi_call: ATPX method call failed: %s\n", error_buffer);
        return;
    }
    kfree(buffer.pointer);

    last_result = 1;
    error_buffer[0] = 0;

    printk(KERN_INFO "acpi_call: Call successful\n");
}

/** Parses method name and arguments
@param input Input string to be parsed. Modified in the process.
@param nargs Set to number of arguments parsed (output)
@param args 
*/
static char *parse_acpi_args(char *input, int *nargs, union acpi_object **args)
{
    char *s = input;

    *nargs = 0;
    *args = NULL;

    // the method name is separated from the arguments by a space
    while (*s && *s != ' ') s++;
    // if no space is found, return 0 arguments
    if (*s == 0)
        return input;

    *args = (union acpi_object *) kmalloc(16 * sizeof(union acpi_object), GFP_KERNEL);

    while (*s) {
        if (*s == ' ') {
            ++ *nargs;
            ++ s;
        } else {
            union acpi_object *arg = (*args) + (*nargs - 1);
            if (*s == '"') {
                // decode string
                arg->type = ACPI_TYPE_STRING;
                arg->string.pointer = ++s;
                arg->string.length = 0;
                while (*s && *s != '"') {
                    arg->string.length ++;
                    ++s;
                }
                // skip the last "
                ++s;
            } else {
                // decode integer, N or 0xN
                arg->type = ACPI_TYPE_INTEGER;
                if (s[0] == '0' && s[1] == 'x') {
                    arg->integer.value = simple_strtol(s+2, 0, 16);
                } else {
                    arg->integer.value = simple_strtol(s, 0, 10);
                }
                while (*s && *s != ' ') {
                    ++s;
                }
            }
        }
    }

    return input;
}

/** procfs write callback. Called when writing into /proc/acpi/call.
*/
static int acpi_proc_write( struct file *filp, const char __user *buff,
    unsigned long len, void *data )
{
    char input[512] = { '\0' };
    union acpi_object *args;
    int nargs;
    char *method;

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

    method = parse_acpi_args(input, &nargs, &args);
    do_acpi_call(method, nargs, args);
    if (args) {
        kfree(args);
    }

    return len;
}

/** procfs 'call' read callback. Called when reading the content of /proc/acpi/call.
Returns the last call status:
- "not called" when no call was previously issued
- "failed" if the call failed
- "ok" if the call succeeded
*/
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
    case 0: len = sprintf(page, "failed: %s", error_buffer); break;
    case 1: len = sprintf(page, "ok"); break;
    }
    last_result = -1;

    return len;
}

/** module initialization function */
static int __init init_acpi_call(void)
{
    struct proc_dir_entry *acpi_entry = create_proc_entry("call", 0666, acpi_root_dir);

    last_result = -1;
    error_buffer[0] = 0;

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


