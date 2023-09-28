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
   `link <https://bitbucket.microchip.com/scm/unge/sw-uboot.git>`__ branch "lan966x.spl.asic"


Build Procedure
~~~~~~~~~~~~~~~

-  Run ruby build script

   .. code:: shell

       ruby scripts/build.rb -p lan966x_sr

The build script will ensure the SDK and toolchain is installed, or it
will install the appropriate version required. U-Boot (BL33) will be
taken from the BSP.


Deploy TF-A Images
~~~~~~~~~~~~~~~~~~

TF-A binary bl2.bin, fip.bin and U-Boot is combined to form a flash
image called "lan966x_sr.img". This binary image should be flashed
into the device NOR, and the board strapping be set to "1" in order to
boot from NOR flash.

As the FPGA image typically not has the bootrom in it, the BL1 must be
placed at SP:0x0 with a JTAG. Alternatively, it may also be made part
of the FPGA image.

Flash image layout:

::

+-----------------+
|  unused (80k)   |
+-----------------+ <--- 80k
|  U-Boot Env 512k|
+-----------------+ <--- 592k
|  fip.bin        |
+-----------------+ <--- 1792k
|  OTP emulation  |
+-----------------+
