AC_INIT([fmplayer], [0.1.0])
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AM_SILENT_RULES([yes])
AC_PROG_CC_C99

dnl AM_PATH_SDL2([2.0.5])
PKG_CHECK_MODULES([PORTAUDIO], [portaudio-2.0])
PKG_CHECK_MODULES([GTK3], [gtk+-3.0 cairo])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
