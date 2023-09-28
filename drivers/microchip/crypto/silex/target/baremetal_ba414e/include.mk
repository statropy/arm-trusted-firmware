SRCS+=target/baremetal_ba414e/baremetal_ba414e.c target/baremetal_ba414e/baremetal_wait.c target/hw/ba414/pkhardware_ba414e.c target/hw/ba414/pkhardware_ba414e_run.c target/hw/ba414/ba414_status.c target/hw/ba414/ec_curves.c target/hw/ba414/cmddefs_ecc.c target/hw/ba414/cmddefs_modmath.c target/hw/ba414/cmddefs_sm9.c target/hw/ba414/cmddefs_ecjpake.c target/hw/ba414/cmddefs_srp.c
ALLBUILDDIRS+=$(BUILDDIR)/target/hw/ba414/
CPPFLAGS+=-Itarget/baremetal_ba414e/
LDFLAGS+=
