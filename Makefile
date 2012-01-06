#! /usr/bin/make -f

package=acpi_call

obj-m := acpi_call.o

KVERSION := $(shell uname -r)
KDIR := /lib/modules/$(KVERSION)/build
PWD := $(shell pwd)
INSTALLDIR?=${DESTDIR}/lib/modules/${KVERSION}/kernel/extra/


default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

load:
	-/sbin/rmmod acpi_call
	/sbin/insmod acpi_call.ko

install: ${package}.ko
	install -d ${INSTALLDIR}
	install $< ${INSTALLDIR}

