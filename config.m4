PHP_ARG_ENABLE(opus, whether to enable opus support,
[  --enable-opus   Enable opus extension], no)

if test "$PHP_OPUS" != "no"; then

  dnl === SOXR É OBRIGATÓRIO ===
  AC_CHECK_HEADER(soxr.h, [], [
    AC_MSG_ERROR([soxr.h not found. libsoxr is mandatory for opus extension])
  ])

  AC_CHECK_LIB(
    soxr,
    soxr_create,
    [],
    [AC_MSG_ERROR([libsoxr not found or missing soxr_create()])],
    [-lm -lpthread]
  )

  dnl === OPUS TAMBÉM ===
  AC_CHECK_HEADER(opus/opus.h, [], [
    AC_MSG_ERROR([opus/opus.h not found. libopus is required])
  ])

  AC_CHECK_LIB(
    opus,
    opus_encoder_create,
    [],
    [AC_MSG_ERROR([libopus not found or missing opus_encoder_create()])],
    [-lm]
  )

  dnl === DEFINES ===
  AC_DEFINE(HAVE_LIBSOXR, 1, [libsoxr support enabled])

  dnl === EXTENSION ===
  PHP_NEW_EXTENSION(opus, opus.c opus_channel.c, $ext_shared)

  dnl === LINKAGEM FINAL ===
  PHP_ADD_LIBRARY(soxr, 1, OPUS_SHARED_LIBADD)
  PHP_ADD_LIBRARY(opus, 1, OPUS_SHARED_LIBADD)
  PHP_ADD_LIBRARY(m, 1, OPUS_SHARED_LIBADD)
  PHP_ADD_LIBRARY(pthread, 1, OPUS_SHARED_LIBADD)

  PHP_SUBST(OPUS_SHARED_LIBADD)
fi
