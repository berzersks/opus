PHP_ARG_ENABLE(spech_opus, whether to enable spech_opus support,
[  --enable-spech_opus   Enable spech_opus extension])

if test "$PHP_SPECH_OPUS" != "no"; then
  PKG_CHECK_MODULES([OPUS], [opus], [], [
    AC_MSG_ERROR([libopus not found. Install libopus-dev])
  ])
  PHP_ADD_INCLUDE($OPUS_CFLAGS)
  PHP_ADD_LIBRARY_WITH_PATH(opus, $OPUS_LIBDIR, SPECH_OPUS_SHARED_LIBADD)
  PHP_NEW_EXTENSION(spech_opus, spech_opus.c opus_channel.c, $ext_shared)
fi
