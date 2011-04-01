LOCAL_DIR := $(GET_LOCAL_DIR)

ARCH := arm
ARM_CPU := arm1136j-s
CPU := generic

#MMC_SLOT := 1
#DEFINES += MMC_SLOT=$(MMC_SLOT)

DEFINES += WITH_CPU_EARLY_INIT=0 WITH_CPU_WARM_BOOT=0

INCLUDES += -I$(LOCAL_DIR)/include

OBJS += \
	$(LOCAL_DIR)/platform.o \
	$(LOCAL_DIR)/interrupts.o \
	$(LOCAL_DIR)/gpio.o \
	$(LOCAL_DIR)/acpuclock.o \
	$(LOCAL_DIR)/clock.o

LINKER_SCRIPT += $(BUILDDIR)/system-onesegment.ld

include platform/msm_shared/rules.mk

