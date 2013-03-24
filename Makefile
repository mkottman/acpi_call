obj-m := acpi_call.o

KVER ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KVER)/build

default:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean

install:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules_install

load:
	-/sbin/rmmod acpi_call
	/sbin/insmod acpi_call.ko

dkms-add:
	/usr/sbin/dkms add $(CURDIR)

dkms-build:
	/usr/sbin/dkms build acpi_call/1.0

dkms-remove:
	/usr/sbin/dkms remove acpi_call/1.0 --all

