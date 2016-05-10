plugin_LTLIBRARIES = libgstosxaudio.la

libgstosxaudio_la_SOURCES = gstosxringbuffer.c   \
                            gstosxaudioelement.c \
                            gstosxaudiosink.c    \
                            gstosxaudiosrc.c     \
                            gstosxaudio.c       \
                            AudioDevice.cpp  \
                            AudioDeviceList.cpp   \
                            AudioRingBuffer.cpp  \
                            AudioThruEngine.cpp  \
                            SoundflowerController.m \
                            gstosxsoundflowersrc.m


libgstosxaudio_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_CFLAGS) \
    -Wno-deprecated-declarations
libgstosxaudio_la_LIBADD = 			  \
    -lgstinterfaces-@GST_MAJORMINOR@  \
    -lgstaudio-@GST_MAJORMINOR@       \
    $(GST_PLUGINS_BASE_LIBS)          \
    $(GST_BASE_LIBS)                  \
    $(GST_LIBS)
libgstosxaudio_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) -Wl,-framework -Wl,CoreAudio -Wl,-framework -Wl,AudioUnit -Wl,-framework -Wl,CoreServices -Wl,-framework -Wl,IOKit -Wl,-framework -Wl,Cocoa
libgstosxaudio_la_LIBTOOLFLAGS = --tag=disable-static

AM_OBJCFLAGS=$(CFLAGS) $(GST_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) -Wno-aggregate-return
AM_CPPFLAGS=$(CFLAGS) $(GST_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) -Wno-aggregate-return

noinst_HEADERS = gstosxaudiosink.h	  \
                 gstosxaudioelement.h \
                 gstosxringbuffer.h	  \
                 gstosxaudiosrc.h  \
                 AudioDevice.h   \
                 AudioDeviceList.h    \
                 AudioRingBuffer.h    \
                 AudioThruEngine.h   \
                 gstosxsoundflowersrc.h  \
                 SoundflowerController.h




