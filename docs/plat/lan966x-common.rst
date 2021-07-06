Microchip LAN966X common
=========================

This document is depicting the common features of the lan966x-evb and the lan966x-sunrise firmware.



Boot Sequence
-------------

Bootrom(BL1) --> BL2 --> BL32 --> BL33(u-boot) --> Linux kernel



Memory Layout
-------------

Maserati A0 addresses:

MEDIA     Type     Sub-Type    Area         Offset/name   Max Size
========  =======  ==========  ===========  ============  =================
QSPI/NOR  SPL      Code/text   n/a          0x0           0x00014000 (80KB)
QSPI/NOR  UBoot    Code/text   n/a          0x0014000     0x000EC000 (944KB)
QSPI/NOR  UBoot    Env         n/a          0x0100000     0x00040000 (256KB)
QSPI/NOR  UBoot    Env-bak     n/a          0x0140000     0x00040000 (256KB)

eMMC      SPL      Code/text   Boot P1      Page 0x0000   0x00014000 (80KB)
eMMC      UBoot    Code/text   User         Page 0x0000   0x000EC000 (944KB)
eMMC      UBoot    Env         User         Page 0x0800   0x00040000 (256KB)
eMMC      UBoot    Env-bak     User         Page 0x0A00   0x00040000 (256KB)

SD-Card   SPL      Code/text   Partition 1  boot.bin      0x00014000 (80KB)
SD-Card   UBoot    Code/text   Partition 1  u-boot.img    0x000EC000 (944KB)
SD-Card   UBoot    Env         Partition 1  uboot.env     0x00040000 (256KB) 



Maserati B0 addresses:
 
MEDIA     Type      Sub-Type    Area         Offset/name             Max Size
========  ========  ==========  ===========  ======================  =========
eMMC      MBR/GPT   Part table  User         0x0      (Page 0x0000)  32KB
eMMC      Config    Data        User         0x8000   (Page 0x40)    2KB
eMMC      Reserved  N/A         User         0x8800   (Page 0x44)    30KB

eMMC      UBoot     Env         User         0x10000  (Page 0x80)    256KB
eMMC      UBoot     Env-bak     User         0x50000  (Page 0x280)   256KB

eMMC      TF-A      FIP         User         0x90000  (Page 0x480)   1472KB

eMMC      UBoot     Code/text   User         0x200000 (Page 0x1000)  2MB
eMMC      Falcon    DT          User         0x400000 (Page 0x2000)  1MB
eMMC      Falcon    Kernel      User         0x500000 (Page 0x2800)  27MB

eMMC      ext4      Image-A     User         0x40000000              1024MB
eMMC      ext4      Image-B     User         0x80000000              1024MB
eMMC      ext4      Config      User         0xC0000000               742MB




