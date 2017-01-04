#!/bin/bash

usage="Usage: ./graphic_card [option] \nOPTION: \n- off \n- on \n- show"

if [ $# -ne 1 ]; then
    echo -e $usage
else
    if lsmod | grep -q acpi_call; then
	echo 'acpi_call found'
    else
	echo 'acpi_call found, importing it'
	sudo insmod /home/sush/dev/c/acpi_call/acpi_call.ko
    fi
    if [ $1 = 'on' ]; then
	echo '\_SB.PCI0.P0P2.PEGP._ON' > /proc/acpi/call
	echo 'Discrete graphic card ENABLED !'
    elif [ $1 = 'off' ]; then
	echo '\_SB.PCI0.P0P2.PEGP._OFF' > /proc/acpi/call
	echo 'Discrete graphic card DISABLED !'
    elif [ $1 = 'show' ]; then
	grep rate /proc/acpi/battery/BAT1/state
    else
	echo -e $usage
    fi
fi
