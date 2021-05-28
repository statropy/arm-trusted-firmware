Microchip Sunrise LAN966X
=========================

The "sunrise" board is a general-purpose FPGA development board, which
can support a FPGA version of LAN966X, with somewhat reduced speed.

The LAN966X has a single A7 core.

Boot Sequence
-------------

Bootrom(BL1) --> BL2 --> BL32 --> BL33(u-boot) --> Linux kernel

How to build
------------

Code Locations
~~~~~~~~~~~~~~

-  Trusted Firmware-A:
   `link <https://bitbucket.microchip.com/scm/unge/sw-arm-trusted-firmware.git>`__ branch "lan966x-v0"

-  U-Boot:
   `link <https://bitbucket.microchip.com/scm/unge/sw-uboot.git>`__ branch "lan966x.spl-atf"


Build Procedure
~~~~~~~~~~~~~~~

-  Install LAN966X toolchain (and BSP)

-  Build u-boot, and get binary images: "spl/u-boot-spl.bin" (BL33) and "u-boot.img"

   .. code:: shell

       make lan966x_defconfig
       CROSS_COMPILE=arm-none-eabi- make

-  Build TF-A

      Build bl1, bl2, bl32 and fip:

   .. code:: shell

       export CROSS_COMPILE=arm-none-eabi-
       make PLAT=lan966x_sr DEBUG=1 ARCH=aarch32 AARCH32_SP=sp_min BL33=path-to-spl-uboot all fip

Deploy TF-A Images
~~~~~~~~~~~~~~~~~~

TF-A binary fip.bin and U-Boot is combined to form a flash image
called "flash.img". This binary image should be flashed into the
device NOR, and the board strapping be set to "1" in order to boot
from NOR flash.

As the FPGA image typically not has the bootrom in it, the BL1 must be
placed at SP:0x0 with a JTAG. Alternatively, it may also be made part
of the FPGA image.

Flash image layout:

::

+-----------------+
|  unused (80k)   |
+-----------------+ <--- 80k
|  U-Boot         |
+-----------------+ <--- 1536k (1.5Mb)
|  fip.bin        |
+-----------------+
