#include <acpi/acpi.h>
#include <linux/proc_fs.h>

MODULE_LICENSE("GPL");

extern struct proc_dir_entry *acpi_root_dir;

static acpi_handle root_handle;

static int do_acpi_call(const char * method, int len)
{
    acpi_status status;
    acpi_handle handle;
    union acpi_object atpx_arg_elements[1];
    struct acpi_object_list atpx_arg;
    struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };

    printk(KERN_INFO "acpi_call: Calling %s\n", method);

    status = acpi_get_handle(root_handle, (acpi_string) method, &handle);

    if (ACPI_FAILURE(status))
    {
        printk(KERN_ERR "acpi_call: Cannot get handle: %s\n", acpi_format_exception(status));
        return -ENOSYS;
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
        return -ENOSYS;
    }
    kfree(buffer.pointer);

    printk(KERN_INFO "acpi_call: Call successful\n");
    return len;
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

    return do_acpi_call(input, len);
}


static int __init init_acpi_call(void)
{
    struct proc_dir_entry *acpi_entry = create_proc_entry("call", 0222, acpi_root_dir);
    
    if (acpi_entry == NULL) {
      printk(KERN_ERR "acpi_call: Couldn't create proc entry\n");
      return -ENOMEM;
    }
    
    acpi_entry->write_proc = acpi_proc_write;
    printk(KERN_INFO "acpi_call: Module loaded successfully\n");

    return 0;
}


static void unload_acpi_call(void)
{
    remove_proc_entry("call", acpi_root_dir);
}

module_init(init_acpi_call);
module_exit(unload_acpi_call);


