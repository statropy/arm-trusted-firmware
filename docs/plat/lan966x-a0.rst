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
   `link <https://bitbucket.microchip.com/scm/unge/sw-uboot.git>`__ branch "lan966x.spl.asic"


Build Procedure
~~~~~~~~~~~~~~~

-  Run ruby build script

   .. code:: shell

       ruby scripts/build.rb -p lan966x_a0

The build script will ensure the SDK and toolchain is installed, or it
will install the appropriate version required. U-Boot (BL33) will be
taken from the BSP.


Deploy TF-A Images
~~~~~~~~~~~~~~~~~~

TF-A binary bl2.bin, fip.bin and U-Boot is combined to form a flash
image called "lan966x_a0.img". This binary image should be flashed
into the device NOR, and the board strapping be set to "1" in order to
boot from NOR flash.

Flash image layout:

::

+-----------------+
|  BL2    (80k)   |
+-----------------+ <--- 80k
|  U-Boot Env 512k|
+-----------------+ <--- 592k
|  fip.bin        |
+-----------------+ <--- 1792k
|  OTP emulation  |
+-----------------+
