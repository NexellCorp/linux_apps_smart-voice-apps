LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES:= \
	util.c				\
	audioplay.cpp		\
	audiostream.cpp		\
	smart_voice.cpp

LOCAL_C_INCLUDES += \
	frameworks/native/include		\
	system/core/include				\
	$(LOCAL_PATH)/../lib/			\
	$(LOCAL_PATH)/../lib/android/

LOCAL_SHARED_LIBRARIES := \
	libcutils		\
	libutils

LOCAL_SHARED_LIBRARIES += \
	libtinyalsa

LOCAL_STATIC_LIBRARIES +=

LOCAL_LDFLAGS += \
	-L$(LOCAL_PATH)/../lib	\
	-lagcpdm_and \
	-lresample_and

LOCAL_CFLAGS += -std=c++11

LOCAL_MODULE:= smart_voice_and
LOCAL_MODULE_PATH := $(LOCAL_PATH)

include $(BUILD_EXECUTABLE)
