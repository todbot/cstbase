CST Base station firmware
=========================

For Microchip PIC16F1454 or PIC16F1455:
 - http://www.microchip.com/wwwproducts/Devices.aspx?dDocName=en556969

Uses Microchip's USB Framework, aka "MCHPFSUSB Library",
recently renamed "MLA" - Microchip Library for Applications

This firmware is built & tested with the following:
- USB Stack - 2013-06-15 "Legacy MLA" -   http://www.microchip.com/pagehandler/en-us/devtools/mla/
- XC8 compiler v1.12 in "Free" mode
- MPLAB X v2.00

Note: to compile, must have the Microchip Application Library frameworks
symlinked in the same directory level as the "cstbase-hid" directory.
e.g. if you installed the framework in ~/microchip_solutions_v2013-06-15/Microchip
then in this directory do. 

    ln -s ~/microchip_solutions_v2013-06-15/Microchip Microchip

The symlink "Microchip" is already placed in the repo as an example.

The orignal non-USB firmware is in "CST-Base_Station_1454.X".
It is for comparison purposes only.


The firmware with USB support is in "cstbase-hid".
It tries to be a faithful recreation of the above firmware, but
some changes had to be made (mostly in the ISR) to make sure the
USB interrupt was not starved.



