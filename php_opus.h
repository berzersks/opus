#ifndef PHP_OPUS_H
#define PHP_OPUS_H

#include "php.h"
#include <opus/opus.h>

extern zend_module_entry opus_module_entry;
#define phpext_opus_ptr &opus_module_entry

typedef struct _opus_channel_t {
    OpusEncoder *encoder;
    OpusDecoder *decoder;
    int sample_rate;
    int channels;
} opus_channel_t;

PHP_METHOD(opusChannel, __construct);
PHP_METHOD(opusChannel, encode);
PHP_METHOD(opusChannel, decode);
PHP_METHOD(opusChannel, destroy);

#endif
