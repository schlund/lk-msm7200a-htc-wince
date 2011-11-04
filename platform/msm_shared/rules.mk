LOCAL_DIR := $(GET_LOCAL_DIR)

INCLUDES += \
			-I$(LOCAL_DIR)/include

OBJS += \
	$(LOCAL_DIR)/timer.o \
	$(LOCAL_DIR)/debug.o \
	$(LOCAL_DIR)/mddi.o \
	$(LOCAL_DIR)/hsusb.o

ifeq ($(MSM_NAND_WINCE),1)
	OBJS += $(LOCAL_DIR)/nand_wince.o
else
	OBJS += $(LOCAL_DIR)/nand.o
endif

ifeq ($(PLATFORM),msm7200a)
		OBJS += $(LOCAL_DIR)/msm_i2c.o
endif

ifeq ($(PLATFORM),msm7227)
		OBJS += $(LOCAL_DIR)/msm_i2c.o \
		$(LOCAL_DIR)/proc_comm.o
endif

