Microchip LAN966X EVB
=====================

The LAN9662 EVB is a 2-port gigabit TSN evaluation board.

The LAN9662 EVB has a single A7 core, running 600MHz.

Boot Sequence
-------------

Bootrom(SAMA-ROM) --> BL2 --> BL32 --> BL33(u-boot) --> Linux kernel

Note: The bl1 is not used since the LAN966X already has the non-ATF
boot ROM ("SAMA-ROM").

How to build
------------

Code Locations
~~~~~~~~~~~~~~

-  Trusted Firmware-A:
   `link <https://bitbucket.microchip.com/scm/unge/sw-arm-trusted-firmware.git>`__ branch "lan966x-v0"

-  U-Boot:
   `link <https://bitbucket.microchip.com/scm/unge/sw-uboot.git>`__ branch "lan966x.spl.asic-atf"


Build Procedure
~~~~~~~~~~~~~~~

-  Install LAN966X toolchain (and BSP)

-  Build u-boot, and get binary images: "spl/u-boot-spl.bin" (BL33) and "u-boot.img"

   .. code:: shell

       make lan966x_evb_defconfig
       CROSS_COMPILE=arm-none-eabi- make

-  Build TF-A

      Build bl2, bl32 and fip:

   .. code:: shell

       export CROSS_COMPILE=arm-none-eabi-
       make PLAT=lan966x_evb DEBUG=1 ARCH=aarch32 AARCH32_SP=sp_min BL33=path-to-spl-uboot all fip

Deploy TF-A Images
~~~~~~~~~~~~~~~~~~

TF-A binary bl2.bin, fip.bin and U-Boot is combined to form a flash
image called "flash.img". This binary image should be flashed into the
device NOR, and the board strapping be set to "1" in order to boot
from NOR flash.

As the FPGA image typically not has the bootrom in it, the BL1 must be
placed at SP:0x0 with a JTAG. Alternatively, it may also be made part
of the FPGA image.

Flash image layout:

::

+-----------------+
|  BL2    (80k)   |
+-----------------+ <--- 80k
|  U-Boot         |
+-----------------+ <--- 1536k (1.5Mb)
|  fip.bin        |
+-----------------+
