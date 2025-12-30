PHP_ARG_ENABLE(opus, whether to enable opus support,
[  --enable-opus   Enable opus extension])

if test "$PHP_OPUS" != "no"; then
  PKG_CHECK_MODULES([OPUS], [opus], [], [
    AC_MSG_ERROR([libopus not found. Install libopus-dev])
  ])
  PHP_ADD_INCLUDE($OPUS_CFLAGS)
  PHP_ADD_LIBRARY_WITH_PATH(opus, $OPUS_LIBDIR, OPUS_SHARED_LIBADD)

  PHP_ADD_LIBRARY_WITH_PATH(soxr, $ext_srcdir/lib, OPUS_SHARED_LIBADD)
  PHP_ADD_LIBRARY(m, 1, OPUS_SHARED_LIBADD)
  PHP_ADD_LIBRARY(pthread, 1, OPUS_SHARED_LIBADD)


  PHP_NEW_EXTENSION(opus, opus.c opus_channel.c, $ext_shared)
fi
