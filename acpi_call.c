/* Copyright (c) 2010: Michal Kottman */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <acpi/acpi.h>

MODULE_LICENSE("GPL");

#define BUFFER_SIZE 256

extern struct proc_dir_entry *acpi_root_dir;

static char result_buffer[BUFFER_SIZE];

static u8 temporary_buffer[BUFFER_SIZE];

/** Appends the contents of an acpi_object to the result buffer
@param result   An acpi object holding result data
@returns        0 if the result could fully be saved, a higher value otherwise
*/
static int acpi_result_to_string(union acpi_object *result) {

#define BUFFER (result_buffer + strlen(result_buffer))
#define BYTES_AVAIL (BUFFER_SIZE - strlen(result_buffer))

    int not_written = 0;

    if (result->type == ACPI_TYPE_INTEGER) {
        not_written = snprintf(BUFFER, BYTES_AVAIL,
            "0x%x", (int)result->integer.value);
    } else if (result->type == ACPI_TYPE_STRING) {
        not_written = snprintf(BUFFER, BYTES_AVAIL,
            "\"%*s\"", result->string.length, result->string.pointer);
    } else if (result->type == ACPI_TYPE_BUFFER) {
        int i;
        int show_values = result->buffer.length;
        // do not store more than data if it does not fit. The first element is
        // just 4 chars, but there is also two bytes from the curly brackets
        if (show_values > BYTES_AVAIL / 6)
            show_values = BYTES_AVAIL / 6;

        sprintf(BUFFER, "{");
        for (i = 0; i < show_values; i++)
            sprintf(BUFFER,
                i == 0 ? "0x%02x" : ", 0x%02x", result->buffer.pointer[i]);

        if (result->buffer.length > BUFFER_SIZE / 6) {
            // if data was truncated, show a trailing comma
            sprintf(BUFFER, ",");
            not_written++;
        } else {
            sprintf(BUFFER, "}");
        }
    } else {
        not_written = snprintf(BUFFER, BYTES_AVAIL,
            "Object type 0x%x\n", result->type);
    }

    return not_written;
#undef BUFFER
#undef BYTES_AVAIL
}

/**
@param method   The full name of ACPI method to call
@param argc     The number of parameters
@param argv     A pre-allocated array of arguments of type acpi_object
*/
static void do_acpi_call(const char * method, int argc, union acpi_object *argv)
{
    acpi_status status;
    acpi_handle handle;
    struct acpi_object_list arg;
    struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };

    printk(KERN_INFO "acpi_call: Calling %s\n", method);

    // get the handle of the method, must be a fully qualified path
    status = acpi_get_handle(NULL, (acpi_string) method, &handle);

    if (ACPI_FAILURE(status))
    {
        snprintf(result_buffer, BUFFER_SIZE, "Error: %s", acpi_format_exception(status));
        printk(KERN_ERR "acpi_call: Cannot get handle: %s\n", result_buffer);
        return;
    }

    // prepare parameters
    arg.count = argc;
    arg.pointer = argv;

    // call the method
    status = acpi_evaluate_object(handle, NULL, &arg, &buffer);
    if (ACPI_FAILURE(status))
    {
        snprintf(result_buffer, BUFFER_SIZE, "Error: %s", acpi_format_exception(status));
        printk(KERN_ERR "acpi_call: Method call failed: %s\n", result_buffer);
        return;
    }

    // reset the result buffer
    *result_buffer = '\0';
    acpi_result_to_string(buffer.pointer);
    kfree(buffer.pointer);

    printk(KERN_INFO "acpi_call: Call successful: %s\n", result_buffer);
}

/** Decodes 2 hex characters to an u8 int
*/
u8 decodeHex(char *hex) {
    char buf[3] = { hex[0], hex[1], 0};
    return (u8) simple_strtoul(buf, NULL, 16);
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
    while (*s && *s != ' ')
        s++;
    // if no space is found, return 0 arguments
    if (*s == 0)
        return input;
    
    *args = (union acpi_object *) kmalloc(16 * sizeof(union acpi_object), GFP_KERNEL);

    while (*s) {
        if (*s == ' ') {
            if (*nargs == 0)
                *s = 0; // change first space to nul
            ++ *nargs;
            ++ s;
        } else {
            union acpi_object *arg = (*args) + (*nargs - 1);
            if (*s == '"') {
                // decode string
                arg->type = ACPI_TYPE_STRING;
                arg->string.pointer = ++s;
                arg->string.length = 0;
                while (*s && *s++ != '"')
                    arg->string.length ++;
                // skip the last "
                ++s;
            } else if (*s == 'b') {
                // decode buffer - bXXXX
                char *p = ++s;
                int len = 0, i;
                u8 *buf = NULL;
                
                while (*p && *p!=' ')
                    p++;
                
                len = p - s;
                if (len % 2 == 1) {
                    printk(KERN_ERR "acpi_call: buffer arg%d is not multiple of 8 bits\n", *nargs);
                    return NULL;
                }
                len /= 2;

                buf = (u8*) kmalloc(len, GFP_KERNEL);
                for (i=0; i<len; i++) {
                    buf[i] = decodeHex(s + i*2);
                }
                s = p;

                arg->type = ACPI_TYPE_BUFFER;
                arg->buffer.pointer = buf;
                arg->buffer.length = len;
            } else if (*s == '{') {
                // decode buffer - { b1, b2 ...}
                u8 *buf = temporary_buffer;
                arg->type = ACPI_TYPE_BUFFER;
                arg->buffer.pointer = buf;
                arg->buffer.length = 0;
                while (*s && *s++ != '}') {
                    if (buf >= temporary_buffer + sizeof(temporary_buffer)) {
                        printk(KERN_ERR "acpi_call: buffer arg%d is truncated because the buffer is full\n", *nargs);
                        // clear remaining arguments
                        while (*s && *s != '}')
                            ++s;
                        break;
                    }
                    else if (*s >= '0' && *s <= '9') {
                        // decode integer into buffer
                        arg->buffer.length ++;
                        if (s[0] == '0' && s[1] == 'x')
                            *buf++ = simple_strtol(s+2, 0, 16);
                        else
                            *buf++ = simple_strtol(s, 0, 10);
                    }
                    // skip until space or comma or '}'
                    while (*s && *s != ' ' && *s != ',' && *s != '}')
                        ++s;
                }
                // store the result in new allocated buffer
                buf = (u8*) kmalloc(arg->buffer.length, GFP_KERNEL);
                memcpy(buf, temporary_buffer, arg->buffer.length);
                arg->buffer.pointer = buf;
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
    char input[2 * BUFFER_SIZE] = { '\0' };
    union acpi_object *args;
    int nargs, i;
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
    if (method) {
        do_acpi_call(method, nargs, args);
        if (args) {
            for (i=0; i<nargs; i++)
                if (args[i].type == ACPI_TYPE_BUFFER)
                    kfree(args[i].buffer.pointer);
            kfree(args);
        }
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

    len = strlen(result_buffer);
    memcpy(page, result_buffer, len + 1);
    strcpy(result_buffer, "not called");

    return len;
}

/** module initialization function */
static int __init init_acpi_call(void)
{
    struct proc_dir_entry *acpi_entry = create_proc_entry("call", 0660, acpi_root_dir);

    strcpy(result_buffer, "not called");

    if (acpi_entry == NULL) {
      printk(KERN_ERR "acpi_call: Couldn't create proc entry\n");
      return -ENOMEM;
    }

    acpi_entry->write_proc = acpi_proc_write;
    acpi_entry->read_proc = acpi_proc_read;
    printk(KERN_INFO "acpi_call: Module loaded successfully\n");

    return 0;
}

static void __exit unload_acpi_call(void)
{
    remove_proc_entry("call", acpi_root_dir);
    printk(KERN_INFO "acpi_call: Module unloaded successfully\n");
}

module_init(init_acpi_call);
module_exit(unload_acpi_call);


