obj-m := acpi_call.o

KVER ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KVER)/build
VERSION ?= $(shell cat VERSION)

default:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean

install:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules_install

load:
	-/sbin/rmmod acpi_call
	/sbin/insmod acpi_call.ko

dkms.conf: dkms.conf.in
	sed "s/@@VERSION@@/$(VERSION)/" $^ > $@

dkms-add: dkms.conf
	/usr/sbin/dkms add $(CURDIR)

dkms-build: dkms.conf
	/usr/sbin/dkms build acpi_call/$(VERSION)

dkms-install: dkms.conf
	/usr/sbin/dkms install acpi_call/$(VERSION)

dkms-remove: dkms.conf
	/usr/sbin/dkms remove acpi_call/$(VERSION) --all

modprobe-install:
	modprobe acpi_call

modprobe-remove:
	modprobe -r acpi_call

dev: modprobe-remove dkms-remove dkms-add dkms-build dkms-install modprobe-install
