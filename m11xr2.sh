#!/bin/sh
# Based on m11xr2hack by George Shearer

if ! lsmod | grep -q acpi_call; then
    echo "Error: acpi_call module not loaded"
    exit
fi

acpi_call () {
    echo "$*" > /proc/acpi/call
    cat /proc/acpi/call
}


case "$1" in
off)
    echo NVOP $(acpi_call "\_SB.PCI0.P0P2.PEGP.NVOP 0 0x100 0x1A {255,255,255,255}")
    echo _PS3 $(acpi_call "\_SB.PCI0.P0P2.PEGP._PS3")
;;
on)
    echo _PS0 $(acpi_call "\_SB.PCI0.P0P2.PEGP._PS0")
;;
*)
    echo "Usage: $0 [on|off]"
;;
esac

