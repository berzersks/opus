PHP_ARG_ENABLE(opus, whether to enable opus support,
[  --enable-opus   Enable opus extension], no)

if test "$PHP_OPUS" != "no"; then

  AC_CHECK_HEADER([soxr.h], [], [
    AC_MSG_ERROR([soxr.h not found. libsoxr is mandatory for opus extension])
  ])

  AC_CHECK_HEADER([opus/opus.h], [], [
    AC_MSG_ERROR([opus/opus.h not found. libopus is required])
  ])

  PHP_CHECK_LIBRARY(
    soxr,
    soxr_create,
    [
      AC_DEFINE(HAVE_LIBSOXR, 1, [libsoxr support enabled])
      PHP_ADD_LIBRARY(soxr, 1, OPUS_SHARED_LIBADD)
      PHP_ADD_LIBRARY(m, 1, OPUS_SHARED_LIBADD)
      PHP_ADD_LIBRARY(pthread, 1, OPUS_SHARED_LIBADD)
    ],
    [
      AC_MSG_ERROR([libsoxr not found or missing soxr_create()])
    ],
    [$LDFLAGS -lm -lpthread]
  )

  PHP_CHECK_LIBRARY(
    opus,
    opus_encoder_create,
    [
      AC_DEFINE(HAVE_LIBOPUS, 1, [libopus support enabled])
      PHP_ADD_LIBRARY(opus, 1, OPUS_SHARED_LIBADD)
      PHP_ADD_LIBRARY(m, 1, OPUS_SHARED_LIBADD)
    ],
    [
      AC_MSG_ERROR([libopus not found or missing opus_encoder_create()])
    ],
    [$LDFLAGS -lm]
  )

  PHP_NEW_EXTENSION(opus, opus.c opus_channel.c, $ext_shared)
  PHP_SUBST(OPUS_SHARED_LIBADD)
fi
