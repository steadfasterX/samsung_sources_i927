LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES :=  \
    aescrypt.c      \
    aeskey.c        \
    aestab.c

LOCAL_MODULE := libaes
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)
