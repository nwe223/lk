LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_FLOAT_SRCS += \
	$(LOCAL_DIR)/gfx.c

include make/module.mk
