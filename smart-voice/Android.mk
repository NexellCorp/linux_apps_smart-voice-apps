LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

ANDROID_VERSION_STR := $(subst ., ,$(PLATFORM_VERSION))
ANDROID_VERSION_MAJOR := $(firstword $(ANDROID_VERSION_STR))

LOCAL_SRC_FILES:= \
	util.c				\
	audioplay.cpp		\
	audiostream.cpp		\
	smart_voice.cpp

LOCAL_C_INCLUDES += \
	frameworks/native/include		\
	system/core/include

ifeq "7" "$(ANDROID_VERSION_MAJOR)"
LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../../../library/libagcpdm	\
	$(LOCAL_PATH)/../../../library/libresample

LOCAL_LDFLAGS += \
	-L$(LOCAL_PATH)/../../../library/libagcpdm	\
	-lagcpdm \

LOCAL_STATIC_LIBRARIES += \
	libresample

LOCAL_CFLAGS += -DNOUGAT
else
LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../lib/			\
	$(LOCAL_PATH)/../lib/android/

LOCAL_CFLAGS += -std=c++11

LOCAL_LDFLAGS += \
	-L$(LOCAL_PATH)/../lib/android	\
	-lagcpdm \
	-L$(LOCAL_PATH)/../lib/android	\
	-lresample

include external/stlport/libstlport.mk
LOCAL_SHARED_LIBRARIES += libstlport
APP_STL := stlport_static
APP_STL := gnustl_static
endif

LOCAL_SHARED_LIBRARIES := \
	libcutils		\
	libutils

LOCAL_SHARED_LIBRARIES += \
	libtinyalsa

LOCAL_MODULE:= smart_voice_and
#LOCAL_MODULE_PATH := $(LOCAL_PATH)

include $(BUILD_EXECUTABLE)
