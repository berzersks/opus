#ifndef PHP_OPUS_H
#define PHP_OPUS_H

#include "php.h"
#include <opus/opus.h>

#ifdef HAVE_LIBSOXR
#include "soxr.h"
#endif

extern zend_module_entry opus_module_entry;
#define phpext_opus_ptr &opus_module_entry





typedef struct _opus_channel_t {
    OpusEncoder *encoder;
    OpusDecoder *decoder;
    int sample_rate;
    int channels;

#ifdef HAVE_LIBSOXR
    soxr_t soxr_state;
    double soxr_src_rate;
    double soxr_dst_rate;
#endif

    // State for enhanceVoiceClarity (per-instance instead of static)
    float hp_prev;
    float lp_prev;

    // State for spatialStereoEnhance (per-instance instead of static)
    opus_int16 delay_buffer[4096];
    size_t delay_pos;
    float ap_state_l;
    float ap_state_r;
    float reverb_l;
    float reverb_r;
} opus_channel_t;




PHP_METHOD(opusChannel, __construct);
PHP_METHOD(opusChannel, encode);
PHP_METHOD(opusChannel, decode);
PHP_METHOD(opusChannel, resample);
PHP_METHOD(opusChannel, destroy);
PHP_METHOD(opusChannel, enhanceVoiceClarity);
PHP_METHOD(opusChannel, spatialStereoEnhance);

void register_opus_channel_class(void);
void opus_channel_free_storage(zend_object *object);

#endif /* PHP_OPUS_H */
