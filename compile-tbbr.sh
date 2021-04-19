export CROSS_COMPILE=arm-none-eabi-
set -e
ARGS=$*
AUTH="TRUSTED_BOARD_BOOT=1 ARM_ROTPK_LOCATION=regs GENERATE_COT=1 CREATE_KEYS=1 ROT_KEY=keys/rotprivk_rsa.pem MBEDTLS_DIR=mbedtls"
# Note: $UBOOT_SPL must point to LAN966x SPL binary
B="BL33=$UBOOT_SPL"
P="DEBUG=1 LOG_LEVEL=40 PLAT=lan966x ARM_ARCH_MAJOR=7 ARCH=aarch32 AARCH32_SP=sp_min"
make $P $A $B $AUTH ${ARGS:=all fip}
ls -l build/lan966x/debug/*.bin
