LOCAL_DIR := $(GET_LOCAL_DIR)

INCLUDES += \
			-I$(LOCAL_DIR)/include

OBJS += \
	$(LOCAL_DIR)/timer.o \
	$(LOCAL_DIR)/debug.o \
	$(LOCAL_DIR)/mddi.o \
	$(LOCAL_DIR)/hsusb.o \
	$(LOCAL_DIR)/nand.o

ifeq ($(PLATFORM),msm7200a)
		OBJS += $(LOCAL_DIR)/msm_i2c.o
endif

ifeq ($(PLATFORM),msm7227)
		OBJS += $(LOCAL_DIR)/msm_i2c.o \
		$(LOCAL_DIR)/proc_comm.o
endif

