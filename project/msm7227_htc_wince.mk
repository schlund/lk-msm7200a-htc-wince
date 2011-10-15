# top level project rules for the msm7200a_htc_wince project
#
LOCAL_DIR := $(GET_LOCAL_DIR)

TARGET := msm7227_htc_wince

MODULES += app/aboot

DEFINES += WITH_DEBUG_DCC=0
DEFINES += WITH_DEBUG_UART=0
DEFINES += WITH_DEBUG_FBCON=1
