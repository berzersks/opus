#include "php_opus.h"
#include "soxr.h"
#include "zend_smart_string.h"

/* Fallback para versões do soxr sem SOXR_LOW_LATENCY */
#ifndef SOXR_LOW_LATENCY
#define SOXR_LOW_LATENCY 0
#endif

#include <math.h>

/* ========= Helper: Get best frame size ========= */
static inline int opus_get_best_frame_size(int sample_rate) {
    // Retorna 20ms (padrão recomendado) em samples
    return (int)(sample_rate * 0.020);
}



zend_class_entry *opus_channel_ce;
zend_object_handlers opus_channel_object_handlers;

typedef struct _opus_channel_object {
    opus_channel_t *intern;
    zend_object std;
} opus_channel_object;

static inline opus_channel_object *opus_channel_from_obj(zend_object *obj) {
    return (opus_channel_object *)((char *)(obj) - XtOffsetOf(opus_channel_object, std));
}

#define Z_OPUS_CHANNEL_P(zv) opus_channel_from_obj(Z_OBJ_P(zv))

/* ========= Prototypes ========= */
PHP_METHOD(opusChannel, __construct);
PHP_METHOD(opusChannel, encode);
PHP_METHOD(opusChannel, decode);
PHP_METHOD(opusChannel, destroy);
PHP_METHOD(opusChannel, setBitrate);
PHP_METHOD(opusChannel, setVBR);
PHP_METHOD(opusChannel, setComplexity);
PHP_METHOD(opusChannel, setDTX);
PHP_METHOD(opusChannel, setSignalVoice);
PHP_METHOD(opusChannel, reset);
PHP_METHOD(opusChannel, resample);
PHP_METHOD(opusChannel, enhanceVoiceClarity);
PHP_METHOD(opusChannel, spatialStereoEnhance);
PHP_METHOD(opusChannel, monoToStereo);
PHP_METHOD(opusChannel, stereoToMono);
PHP_METHOD(opusChannel, hasLibsoxr);
PHP_METHOD(opusChannel, getInfo);

/* ========= Argumentos ========= */
ZEND_BEGIN_ARG_INFO_EX(arginfo_opus_construct, 0, 0, 0)
    ZEND_ARG_TYPE_INFO(0, sample_rate, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, channels, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_opus_encode, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, pcm_data, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, pcm_rate, IS_LONG, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_opus_decode, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, encoded_data, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, pcm_rate_out, IS_LONG, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_opus_resample, 0, 3, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, pcm_data, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, src_rate, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, dst_rate, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_opus_long, 0, 0, 1)
    ZEND_ARG_TYPE_INFO(0, value, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_opus_bool, 0, 0, 1)
    ZEND_ARG_TYPE_INFO(0, enable, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_opus_reset, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_opus_destroy, 0, 0, IS_VOID, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_opus_enhance_voice, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, pcm_data, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, intensity, IS_DOUBLE, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_opus_spatial_stereo, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, pcm_data, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, width, IS_DOUBLE, 1)
    ZEND_ARG_TYPE_INFO(0, depth, IS_DOUBLE, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_opus_mono_to_stereo, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, pcm_data, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_opus_stereo_to_mono, 0, 1, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, pcm_data, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_opus_has_libsoxr, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_opus_get_info, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

/* ========= Object Lifecycle ========= */
static zend_object *opus_channel_create_object(zend_class_entry *ce) {
    opus_channel_object *obj = ecalloc(1, sizeof(opus_channel_object) + zend_object_properties_size(ce));

    zend_object_std_init(&obj->std, ce);
    object_properties_init(&obj->std, ce);
    obj->std.handlers = &opus_channel_object_handlers;

    obj->intern = (opus_channel_t *)ecalloc(1, sizeof(opus_channel_t));
    obj->intern->encoder = NULL;
    obj->intern->decoder = NULL;
#ifdef HAVE_LIBSOXR
    obj->intern->soxr_state = NULL;
    obj->intern->soxr_src_rate = 0.0;
    obj->intern->soxr_dst_rate = 0.0;
#endif

    return &obj->std;
}

void opus_channel_free_storage(zend_object *object) {
    opus_channel_object *obj = opus_channel_from_obj(object);

    if (obj->intern) {
        if (obj->intern->encoder) {
            opus_encoder_destroy(obj->intern->encoder);
            obj->intern->encoder = NULL;
        }
        if (obj->intern->decoder) {
            opus_decoder_destroy(obj->intern->decoder);
            obj->intern->decoder = NULL;
        }
#ifdef HAVE_LIBSOXR
        if (obj->intern->soxr_state) {
            soxr_delete(obj->intern->soxr_state);
            obj->intern->soxr_state = NULL;
        }
#endif
        efree(obj->intern);
        obj->intern = NULL;
    }

    zend_object_std_dtor(object);
}

/* ========= Métodos ========= */

PHP_METHOD(opusChannel, __construct)
{
    zend_long sample_rate = 48000, channels = 1;
    int err;

    ZEND_PARSE_PARAMETERS_START(0, 2)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(sample_rate)
        Z_PARAM_LONG(channels)
    ZEND_PARSE_PARAMETERS_END();

    // Validate parameters
    if (sample_rate != 8000 && sample_rate != 12000 && sample_rate != 16000 &&
        sample_rate != 24000 && sample_rate != 48000) {
        zend_throw_error(NULL, "Invalid sample_rate: must be 8000, 12000, 16000, 24000, or 48000");
        RETURN_THROWS();
    }

    if (channels != 1 && channels != 2) {
        zend_throw_error(NULL, "Invalid channels: must be 1 or 2");
        RETURN_THROWS();
    }

    opus_channel_object *obj = Z_OPUS_CHANNEL_P(ZEND_THIS);
    opus_channel_t *intern = obj->intern;

    // Prevent double initialization
    if (intern->encoder != NULL || intern->decoder != NULL) {
        zend_throw_error(NULL, "OpusChannel already initialized");
        RETURN_THROWS();
    }

    intern->sample_rate = (int)sample_rate;
    intern->channels = (int)channels;

    // Initialize state variables
    intern->hp_prev = 0.0f;
    intern->lp_prev = 0.0f;
    intern->delay_pos = 0;
    intern->ap_state_l = 0.0f;
    intern->ap_state_r = 0.0f;
    intern->reverb_l = 0.0f;
    intern->reverb_r = 0.0f;
    memset(intern->delay_buffer, 0, sizeof(intern->delay_buffer));

    intern->encoder = opus_encoder_create(sample_rate, channels, OPUS_APPLICATION_VOIP, &err);
    if (err != OPUS_OK) {
        zend_throw_error(NULL, "Opus encoder init failed: %s", opus_strerror(err));
        RETURN_THROWS();
    }

    opus_encoder_ctl(intern->encoder, OPUS_SET_BITRATE(32000));
    opus_encoder_ctl(intern->encoder, OPUS_SET_VBR(0));
    opus_encoder_ctl(intern->encoder, OPUS_SET_COMPLEXITY(5));
    opus_encoder_ctl(intern->encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    opus_encoder_ctl(intern->encoder, OPUS_SET_DTX(1));

    intern->decoder = opus_decoder_create(sample_rate, channels, &err);
    if (err != OPUS_OK) {
        opus_encoder_destroy(intern->encoder);
        intern->encoder = NULL;
        zend_throw_error(NULL, "Opus decoder init failed: %s", opus_strerror(err));
        RETURN_THROWS();
    }
}

/* ========= Configurações ========= */
PHP_METHOD(opusChannel, setBitrate)
{
    zend_long bitrate;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(bitrate)
    ZEND_PARSE_PARAMETERS_END();

    opus_channel_object *obj = Z_OPUS_CHANNEL_P(ZEND_THIS);
    if (!obj->intern || !obj->intern->encoder) {
        zend_throw_error(NULL, "OpusChannel not initialized");
        RETURN_THROWS();
    }

    if (bitrate < 500 || bitrate > 512000) {
        zend_throw_error(NULL, "Invalid bitrate: must be between 500 and 512000");
        RETURN_THROWS();
    }

    opus_encoder_ctl(obj->intern->encoder, OPUS_SET_BITRATE(bitrate));
}

PHP_METHOD(opusChannel, setVBR)
{
    zend_bool enable;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_BOOL(enable)
    ZEND_PARSE_PARAMETERS_END();

    opus_channel_object *obj = Z_OPUS_CHANNEL_P(ZEND_THIS);
    if (!obj->intern || !obj->intern->encoder) {
        zend_throw_error(NULL, "OpusChannel not initialized");
        RETURN_THROWS();
    }

    opus_encoder_ctl(obj->intern->encoder, OPUS_SET_VBR(enable));
}

PHP_METHOD(opusChannel, setComplexity)
{
    zend_long level;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(level)
    ZEND_PARSE_PARAMETERS_END();

    opus_channel_object *obj = Z_OPUS_CHANNEL_P(ZEND_THIS);
    if (!obj->intern || !obj->intern->encoder) {
        zend_throw_error(NULL, "OpusChannel not initialized");
        RETURN_THROWS();
    }

    if (level < 0 || level > 10) {
        zend_throw_error(NULL, "Invalid complexity: must be between 0 and 10");
        RETURN_THROWS();
    }

    opus_encoder_ctl(obj->intern->encoder, OPUS_SET_COMPLEXITY(level));
}

PHP_METHOD(opusChannel, setDTX)
{
    zend_bool enable;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_BOOL(enable)
    ZEND_PARSE_PARAMETERS_END();

    opus_channel_object *obj = Z_OPUS_CHANNEL_P(ZEND_THIS);
    if (!obj->intern || !obj->intern->encoder) {
        zend_throw_error(NULL, "OpusChannel not initialized");
        RETURN_THROWS();
    }

    opus_encoder_ctl(obj->intern->encoder, OPUS_SET_DTX(enable));
}

PHP_METHOD(opusChannel, setSignalVoice)
{
    zend_bool enable;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_BOOL(enable)
    ZEND_PARSE_PARAMETERS_END();

    opus_channel_object *obj = Z_OPUS_CHANNEL_P(ZEND_THIS);
    if (!obj->intern || !obj->intern->encoder) {
        zend_throw_error(NULL, "OpusChannel not initialized");
        RETURN_THROWS();
    }

    opus_encoder_ctl(obj->intern->encoder, OPUS_SET_SIGNAL(enable ? OPUS_SIGNAL_VOICE : OPUS_SIGNAL_MUSIC));
}

/* ========= Reset ========= */
PHP_METHOD(opusChannel, reset)
{
    opus_channel_object *obj = Z_OPUS_CHANNEL_P(ZEND_THIS);
    if (!obj->intern) {
        zend_throw_error(NULL, "OpusChannel not initialized");
        RETURN_THROWS();
    }

    if (obj->intern->encoder) {
        opus_encoder_ctl(obj->intern->encoder, OPUS_RESET_STATE);
    }
    if (obj->intern->decoder) {
        opus_decoder_ctl(obj->intern->decoder, OPUS_RESET_STATE);
    }
#ifdef HAVE_LIBSOXR
    if (obj->intern->soxr_state) {
        soxr_delete(obj->intern->soxr_state);
        obj->intern->soxr_state = NULL;
        obj->intern->soxr_src_rate = 0;
        obj->intern->soxr_dst_rate = 0;
    }
#endif

    // Reset state variables
    obj->intern->hp_prev = 0.0f;
    obj->intern->lp_prev = 0.0f;
    obj->intern->delay_pos = 0;
    obj->intern->ap_state_l = 0.0f;
    obj->intern->ap_state_r = 0.0f;
    obj->intern->reverb_l = 0.0f;
    obj->intern->reverb_r = 0.0f;
    memset(obj->intern->delay_buffer, 0, sizeof(obj->intern->delay_buffer));
}

/* ========= Encode / Decode ========= */
PHP_METHOD(opusChannel, encode)
{
    zend_string *pcm_in;
    zend_long pcm_rate = 0;
    int nbBytes;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STR(pcm_in)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(pcm_rate)
    ZEND_PARSE_PARAMETERS_END();

    opus_channel_object *obj = Z_OPUS_CHANNEL_P(ZEND_THIS);
    if (!obj->intern || !obj->intern->encoder) {
        zend_throw_error(NULL, "OpusChannel not initialized");
        RETURN_THROWS();
    }

    if (ZSTR_LEN(pcm_in) == 0) {
        RETURN_STRINGL("", 0);
    }

    if (ZSTR_LEN(pcm_in) % (2 * obj->intern->channels) != 0) {
        zend_throw_error(NULL, "Invalid PCM data size: must be multiple of %d bytes", 2 * obj->intern->channels);
        RETURN_THROWS();
    }

    const opus_int16 *input_pcm = (const opus_int16*)ZSTR_VAL(pcm_in);
    int total_samples = ZSTR_LEN(pcm_in) / (2 * obj->intern->channels);

    // Handle resampling if needed
    opus_int16 *work_pcm = NULL;
    int work_samples = total_samples;

    if (pcm_rate > 0 && pcm_rate != obj->intern->sample_rate) {
#ifdef HAVE_LIBSOXR
        size_t idone, odone;
        work_samples = (int)((double)total_samples * obj->intern->sample_rate / pcm_rate) + 128;
        work_pcm = (opus_int16*)emalloc(work_samples * obj->intern->channels * sizeof(opus_int16));

        soxr_io_spec_t io = soxr_io_spec(SOXR_INT16_I, SOXR_INT16_I);
        soxr_quality_spec_t q_spec = soxr_quality_spec(SOXR_HQ, 0);

        soxr_error_t err = soxr_oneshot(pcm_rate, obj->intern->sample_rate, obj->intern->channels,
                                        input_pcm, total_samples, &idone,
                                        work_pcm, work_samples, &odone,
                                        &io, &q_spec, NULL);

        if (err) {
            efree(work_pcm);
            zend_throw_error(NULL, "Resampling failed: %s", err);
            RETURN_THROWS();
        }
        work_samples = (int)odone;
        input_pcm = work_pcm;
#else
        zend_throw_error(NULL, "Resampling not available (libsoxr required)");
        RETURN_THROWS();
#endif
    }

    // Opus requires specific frame sizes: 2.5, 5, 10, 20, 40, 60ms
    // Calculate valid frame sizes for this sample rate
    int valid_frames[] = {
        (int)(obj->intern->sample_rate * 0.0025),  // 2.5ms
        (int)(obj->intern->sample_rate * 0.005),   // 5ms
        (int)(obj->intern->sample_rate * 0.010),   // 10ms
        (int)(obj->intern->sample_rate * 0.020),   // 20ms
        (int)(obj->intern->sample_rate * 0.040),   // 40ms
        (int)(obj->intern->sample_rate * 0.060)    // 60ms
    };

    // Find the smallest valid frame size that fits our data
    int frame_size = valid_frames[3]; // default to 20ms
    for (int i = 0; i < 6; i++) {
        if (work_samples <= valid_frames[i]) {
            frame_size = valid_frames[i];
            break;
        }
    }

    // If data is larger than 60ms, use 60ms and encode multiple times
    if (work_samples > valid_frames[5]) {
        frame_size = valid_frames[5]; // 60ms
    }

    // Allocate padded buffer if needed
    opus_int16 *encode_buffer = NULL;
    const opus_int16 *encode_ptr = input_pcm;

    if (work_samples < frame_size) {
        // Pad with zeros
        encode_buffer = (opus_int16*)emalloc(frame_size * obj->intern->channels * sizeof(opus_int16));
        memcpy(encode_buffer, input_pcm, work_samples * obj->intern->channels * sizeof(opus_int16));
        memset(&encode_buffer[work_samples * obj->intern->channels], 0,
               (frame_size - work_samples) * obj->intern->channels * sizeof(opus_int16));
        encode_ptr = encode_buffer;
    } else if (work_samples > frame_size) {
        // Need to encode multiple frames
        smart_string result = {0};
        smart_string_alloc(&result, (work_samples / frame_size + 1) * 4000, 0);

        // Add magic marker for multi-frame format: "OPmf" (4 bytes)
        smart_string_appendl(&result, "OPmf", 4);

        unsigned char *frame_opus_buf = (unsigned char*)emalloc(4000);
        int offset = 0;

        while (offset < work_samples) {
            int samples_left = work_samples - offset;
            int samples_to_encode = samples_left >= frame_size ? frame_size : samples_left;

            // Pad last frame if needed
            if (samples_to_encode < frame_size) {
                encode_buffer = (opus_int16*)emalloc(frame_size * obj->intern->channels * sizeof(opus_int16));
                memcpy(encode_buffer, &input_pcm[offset * obj->intern->channels],
                       samples_to_encode * obj->intern->channels * sizeof(opus_int16));
                memset(&encode_buffer[samples_to_encode * obj->intern->channels], 0,
                       (frame_size - samples_to_encode) * obj->intern->channels * sizeof(opus_int16));
                encode_ptr = encode_buffer;
            } else {
                encode_ptr = &input_pcm[offset * obj->intern->channels];
            }

            nbBytes = opus_encode(obj->intern->encoder, encode_ptr, frame_size, frame_opus_buf, 4000);

            if (encode_buffer) {
                efree(encode_buffer);
                encode_buffer = NULL;
            }

            if (nbBytes < 0) {
                efree(frame_opus_buf);
                if (work_pcm) efree(work_pcm);
                smart_string_free(&result);
                zend_throw_error(NULL, "Opus encode failed: %s", opus_strerror(nbBytes));
                RETURN_THROWS();
            }

            // Store frame size + data
            uint16_t frame_len = (uint16_t)nbBytes;
            smart_string_appendl(&result, (char*)&frame_len, 2);
            smart_string_appendl(&result, (char*)frame_opus_buf, nbBytes);

            offset += frame_size;
        }

        efree(frame_opus_buf);
        if (work_pcm) efree(work_pcm);

        smart_string_0(&result);
        zend_string *ret = zend_string_init(result.c, result.len, 0);
        smart_string_free(&result);
        RETURN_STR(ret);
    }

    // Single frame encoding
    unsigned char *opus_buf = (unsigned char*)emalloc(4000);
    nbBytes = opus_encode(obj->intern->encoder, encode_ptr, frame_size, opus_buf, 4000);

    if (encode_buffer) efree(encode_buffer);
    if (work_pcm) efree(work_pcm);

    if (nbBytes < 0) {
        efree(opus_buf);
        zend_throw_error(NULL, "Opus encode failed: %s", opus_strerror(nbBytes));
        RETURN_THROWS();
    }

    zend_string *ret = zend_string_init((char*)opus_buf, nbBytes, 0);
    efree(opus_buf);
    RETURN_STR(ret);
}

PHP_METHOD(opusChannel, decode)
{
    zend_string *opus_in;
    zend_long pcm_rate_out = 0;
    int ret;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STR(opus_in)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(pcm_rate_out)
    ZEND_PARSE_PARAMETERS_END();

    opus_channel_object *obj = Z_OPUS_CHANNEL_P(ZEND_THIS);
    if (!obj->intern || !obj->intern->decoder) {
        zend_throw_error(NULL, "OpusChannel not initialized");
        RETURN_THROWS();
    }

    if (ZSTR_LEN(opus_in) == 0) {
        RETURN_STRINGL("", 0);
    }

    const unsigned char *data = (const unsigned char*)ZSTR_VAL(opus_in);
    size_t data_len = ZSTR_LEN(opus_in);

    // Maximum frame size is 5760 samples (120ms at 48kHz)
    int max_frame_size = 5760;

    // Check if this is multi-frame format (magic marker "OPmf")
    int is_multi_frame = 0;
    if (data_len >= 4 && data[0] == 'O' && data[1] == 'P' && data[2] == 'm' && data[3] == 'f') {
        is_multi_frame = 1;
        data += 4;  // Skip magic marker
        data_len -= 4;
    }

    if (is_multi_frame) {
        // Multi-frame format: decode each frame
        smart_string result = {0};
        smart_string_alloc(&result, data_len * 60, 0);

        opus_int16 *frame_pcm = (opus_int16*)emalloc(max_frame_size * obj->intern->channels * sizeof(opus_int16));
        size_t pos = 0;

        while (pos + 2 <= data_len) {
            uint16_t frame_len = data[pos] | (data[pos + 1] << 8);
            pos += 2;

            if (pos + frame_len > data_len) {
                efree(frame_pcm);
                smart_string_free(&result);
                zend_throw_error(NULL, "Invalid multi-frame Opus data");
                RETURN_THROWS();
            }

            ret = opus_decode(obj->intern->decoder, &data[pos], frame_len, frame_pcm, max_frame_size, 0);

            if (ret < 0) {
                efree(frame_pcm);
                smart_string_free(&result);
                zend_throw_error(NULL, "Opus decode failed: %s", opus_strerror(ret));
                RETURN_THROWS();
            }

            smart_string_appendl(&result, (char*)frame_pcm, ret * obj->intern->channels * sizeof(opus_int16));
            pos += frame_len;
        }

        efree(frame_pcm);

        // Handle resampling if needed
        if (pcm_rate_out > 0 && pcm_rate_out != obj->intern->sample_rate) {
#ifdef HAVE_LIBSOXR
            int total_samples = result.len / (2 * obj->intern->channels);
            size_t out_samples = (size_t)((double)total_samples * pcm_rate_out / obj->intern->sample_rate) + 128;
            opus_int16 *resampled = (opus_int16*)emalloc(out_samples * obj->intern->channels * sizeof(opus_int16));

            size_t idone, odone;
            soxr_io_spec_t io = soxr_io_spec(SOXR_INT16_I, SOXR_INT16_I);
            soxr_quality_spec_t q_spec = soxr_quality_spec(SOXR_HQ, 0);

            soxr_error_t err = soxr_oneshot(obj->intern->sample_rate, pcm_rate_out, obj->intern->channels,
                                            (opus_int16*)result.c, total_samples, &idone,
                                            resampled, out_samples, &odone,
                                            &io, &q_spec, NULL);

            if (err) {
                efree(resampled);
                smart_string_free(&result);
                zend_throw_error(NULL, "Resampling failed: %s", err);
                RETURN_THROWS();
            }

            smart_string_free(&result);
            zend_string *ret_str = zend_string_init((char*)resampled, odone * obj->intern->channels * 2, 0);
            efree(resampled);
            RETURN_STR(ret_str);
#else
            smart_string_free(&result);
            zend_throw_error(NULL, "Resampling not available (libsoxr required)");
            RETURN_THROWS();
#endif
        }

        smart_string_0(&result);
        zend_string *ret_str = zend_string_init(result.c, result.len, 0);
        smart_string_free(&result);
        RETURN_STR(ret_str);
    }

    // Single frame format
    opus_int16 *pcm_out = (opus_int16*)emalloc(max_frame_size * obj->intern->channels * sizeof(opus_int16));

    ret = opus_decode(obj->intern->decoder, data, (int)data_len, pcm_out, max_frame_size, 0);

    if (ret < 0) {
        efree(pcm_out);
        zend_throw_error(NULL, "Opus decode failed: %s", opus_strerror(ret));
        RETURN_THROWS();
    }

    size_t pcm_bytes = ret * obj->intern->channels * sizeof(opus_int16);

    // Handle resampling if needed
    if (pcm_rate_out > 0 && pcm_rate_out != obj->intern->sample_rate) {
#ifdef HAVE_LIBSOXR
        size_t out_samples = (size_t)((double)ret * pcm_rate_out / obj->intern->sample_rate) + 128;
        opus_int16 *resampled = (opus_int16*)emalloc(out_samples * obj->intern->channels * sizeof(opus_int16));

        size_t idone, odone;
        soxr_io_spec_t io = soxr_io_spec(SOXR_INT16_I, SOXR_INT16_I);
        soxr_quality_spec_t q_spec = soxr_quality_spec(SOXR_HQ, 0);

        soxr_error_t err = soxr_oneshot(obj->intern->sample_rate, pcm_rate_out, obj->intern->channels,
                                        pcm_out, ret, &idone,
                                        resampled, out_samples, &odone,
                                        &io, &q_spec, NULL);

        if (err) {
            efree(resampled);
            efree(pcm_out);
            zend_throw_error(NULL, "Resampling failed: %s", err);
            RETURN_THROWS();
        }

        efree(pcm_out);
        zend_string *ret_str = zend_string_init((char*)resampled, odone * obj->intern->channels * 2, 0);
        efree(resampled);
        RETURN_STR(ret_str);
#else
        efree(pcm_out);
        zend_throw_error(NULL, "Resampling not available (libsoxr required)");
        RETURN_THROWS();
#endif
    }

    zend_string *ret_str = zend_string_init((char*)pcm_out, pcm_bytes, 0);
    efree(pcm_out);
    RETURN_STR(ret_str);
}
PHP_METHOD(opusChannel, resample)
{
    zend_string *pcm_in;
    zend_long src_rate, dst_rate;

    ZEND_PARSE_PARAMETERS_START(3, 3)
        Z_PARAM_STR(pcm_in)
        Z_PARAM_LONG(src_rate)
        Z_PARAM_LONG(dst_rate)
    ZEND_PARSE_PARAMETERS_END();

    opus_channel_object *obj = Z_OPUS_CHANNEL_P(ZEND_THIS);
    if (!obj->intern) {
        zend_throw_error(NULL, "OpusChannel not initialized");
        RETURN_THROWS();
    }

    // Validate parameters
    if (src_rate <= 0 || dst_rate <= 0) {
        zend_throw_error(NULL, "Invalid sample rates");
        RETURN_THROWS();
    }

    if (ZSTR_LEN(pcm_in) == 0) {
        RETURN_STRINGL("", 0);
    }

    if (ZSTR_LEN(pcm_in) % 2 != 0) {
        zend_throw_error(NULL, "Invalid PCM data: must be multiple of 2 bytes");
        RETURN_THROWS();
    }

    const opus_int16 *in = (const opus_int16*)ZSTR_VAL(pcm_in);
    size_t in_samples = ZSTR_LEN(pcm_in) / (2 * obj->intern->channels);

    // Calculate output size with safety margin
    size_t estimated_out = (size_t)(in_samples * ((double)dst_rate / (double)src_rate) * 1.1) + 64;
    if (estimated_out > 8192 * 6) estimated_out = 8192 * 6;

    opus_int16 *outbuf = (opus_int16 *)emalloc(estimated_out * 2);
    memset(outbuf, 0, estimated_out * 2);
    size_t odone = 0;

#ifdef HAVE_LIBSOXR
    // Use per-instance soxr state instead of static
    if (!obj->intern->soxr_state ||
        obj->intern->soxr_src_rate != (double)src_rate ||
        obj->intern->soxr_dst_rate != (double)dst_rate) {

        if (obj->intern->soxr_state) {
            soxr_delete(obj->intern->soxr_state);
            obj->intern->soxr_state = NULL;
        }

        soxr_error_t err;
        soxr_io_spec_t io = soxr_io_spec(SOXR_INT16_I, SOXR_INT16_I);
        soxr_quality_spec_t q = soxr_quality_spec(SOXR_LQ, 0);
        q.phase_response = 50; /* otimizado para VoIP / baixa latência perceptual */
        soxr_runtime_spec_t r = soxr_runtime_spec(0);

        obj->intern->soxr_state = soxr_create(
            (double)src_rate,
            (double)dst_rate,
            obj->intern->channels,
            &err, &io, &q, &r
        );
        if (err) {
            efree(outbuf);
            zend_throw_error(NULL, "soxr_create failed: %s", err);
            RETURN_THROWS();
        }

        obj->intern->soxr_src_rate = src_rate;
        obj->intern->soxr_dst_rate = dst_rate;
    }

    soxr_error_t perr = soxr_process(obj->intern->soxr_state, in, in_samples, NULL,
                                      outbuf, estimated_out, &odone);
    if (perr) {
        efree(outbuf);
        zend_throw_error(NULL, "soxr_process failed: %s", perr);
        RETURN_THROWS();
    }

    // Flush residual samples
    size_t extra = 0;
    if (odone < estimated_out) {
        soxr_process(obj->intern->soxr_state, NULL, 0, NULL,
                     outbuf + odone, estimated_out - odone, &extra);
        odone += extra;
    }

#else
    // Linear interpolation fallback
    double ratio = (double)dst_rate / (double)src_rate;
    size_t out_samples = (size_t)(in_samples * ratio);
    if (out_samples > estimated_out) out_samples = estimated_out;

    for (size_t i = 0; i < out_samples; i++) {
        double pos = i / ratio;
        size_t p = (size_t)pos;
        if (p >= in_samples - 1) {
            outbuf[i] = in[in_samples - 1];
        } else {
            size_t p1 = p + 1;
            double frac = pos - p;
            outbuf[i] = (opus_int16)(in[p] + (in[p1] - in[p]) * frac);
        }
    }
    odone = out_samples;
#endif

    if (odone == 0) {
        efree(outbuf);
        RETURN_STRINGL("", 0);
    }

    zend_string *result = zend_string_init((char*)outbuf, odone * 2, 0);
    efree(outbuf);
    RETURN_STR(result);
}


/* ========= Método 1: Enhanced Voice Clarity (Clarificador de Voz) ========= */
PHP_METHOD(opusChannel, enhanceVoiceClarity)
{
    zend_string *pcm_in;
    double intensity = 1.0;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STR(pcm_in)
        Z_PARAM_OPTIONAL
        Z_PARAM_DOUBLE(intensity)
    ZEND_PARSE_PARAMETERS_END();

    opus_channel_object *obj = Z_OPUS_CHANNEL_P(ZEND_THIS);
    if (!obj->intern) {
        zend_throw_error(NULL, "OpusChannel not initialized");
        RETURN_THROWS();
    }

    if (intensity < 0.0) intensity = 0.0;
    if (intensity > 2.0) intensity = 2.0;

    if (ZSTR_LEN(pcm_in) == 0) {
        RETURN_STRINGL("", 0);
    }

    if (ZSTR_LEN(pcm_in) % 2 != 0) {
        zend_throw_error(NULL, "Invalid PCM data: must be multiple of 2 bytes");
        RETURN_THROWS();
    }

    const opus_int16 *in = (const opus_int16*)ZSTR_VAL(pcm_in);
    size_t num_samples = ZSTR_LEN(pcm_in) / 2;
    opus_int16 *out = emalloc(num_samples * 2);

    // Parâmetros mais suaves para evitar volume muito baixo
    float gate_threshold = -50.0f + (intensity * 5.0f);
    float comp_ratio = 1.5f + (intensity * 0.5f);
    float gain_boost = 2.0f + (intensity * 1.0f); // Ganho maior

    // Use per-instance state instead of static
    float hp_prev = obj->intern->hp_prev;
    float lp_prev = obj->intern->lp_prev;
    const float hp_alpha = 0.95f; // Menos agressivo
    const float lp_alpha = 0.25f; // Mais passagem

    float envelope = 0.0f;
    const float attack = 0.01f; // Mais lento
    const float release = 0.1f; // Mais lento

    float comp_env = 0.0f;
    const float comp_threshold = 0.5f; // Threshold maior

    // Zero out output buffer for safety
    memset(out, 0, num_samples * 2);

    for (size_t i = 0; i < num_samples; i++) {
        float sample = (float)in[i] / 32768.0f;

        // 1. Filtro High-Pass (remove DC offset)
        float hp_out = sample - hp_prev;
        hp_prev = hp_prev + (hp_alpha * (sample - hp_prev));

        // 2. Filtro Low-Pass (suaviza)
        lp_prev = lp_prev + (lp_alpha * (hp_out - lp_prev));
        float filtered = lp_prev;

        // 3. Gate de Ruído mais suave
        float abs_sample = filtered > 0 ? filtered : -filtered;
        if (abs_sample > envelope) {
            envelope = envelope + (attack * (abs_sample - envelope));
        } else {
            envelope = envelope + (release * (abs_sample - envelope));
        }

        float gate_db = 20.0f * log10f(envelope + 0.00001f);
        float gate_factor = 1.0f;
        if (gate_db < gate_threshold) {
            // Transição suave ao invés de corte abrupto
            float gate_range = 20.0f;
            gate_factor = (gate_db - (gate_threshold - gate_range)) / gate_range;
            if (gate_factor < 0.0f) gate_factor = 0.0f;
            if (gate_factor > 1.0f) gate_factor = 1.0f;
        }
        filtered *= gate_factor;

        // 4. Compressor mais suave
        float current_abs = filtered > 0 ? filtered : -filtered;
        comp_env = comp_env * 0.99f + current_abs * 0.01f;
        float comp_gain = 1.0f;
        if (comp_env > comp_threshold) {
            comp_gain = comp_threshold + ((comp_env - comp_threshold) / comp_ratio);
            if (comp_env > 0.0001f) {
                comp_gain = comp_gain / comp_env;
            }
        }
        filtered *= comp_gain;

        // 5. Ganho final e saturação suave
        float output = filtered * gain_boost;

        // Saturação suave (soft clipping)
        if (output > 0.95f) output = 0.95f + 0.05f * tanhf((output - 0.95f) * 10.0f);
        if (output < -0.95f) output = -0.95f + 0.05f * tanhf((output + 0.95f) * 10.0f);

        // Converter de volta para int16
        out[i] = (opus_int16)(output * 32767.0f);
    }

    // Save state for next call
    obj->intern->hp_prev = hp_prev;
    obj->intern->lp_prev = lp_prev;

    zend_string *result = zend_string_init((char*)out, num_samples * 2, 0);
    efree(out);
    RETURN_STR(result);
}

/* ========= Método 2: Spatial Stereo Enhance (Expansor Espacial) ========= */
PHP_METHOD(opusChannel, spatialStereoEnhance)
{
    zend_string *pcm_in;
    double width = 1.0;
    double depth = 0.5;

    ZEND_PARSE_PARAMETERS_START(1, 3)
        Z_PARAM_STR(pcm_in)
        Z_PARAM_OPTIONAL
        Z_PARAM_DOUBLE(width)
        Z_PARAM_DOUBLE(depth)
    ZEND_PARSE_PARAMETERS_END();

    opus_channel_object *obj = Z_OPUS_CHANNEL_P(ZEND_THIS);
    if (!obj->intern) {
        zend_throw_error(NULL, "OpusChannel not initialized");
        RETURN_THROWS();
    }

    if (width < 0.0) width = 0.0;
    if (width > 2.0) width = 2.0;
    if (depth < 0.0) depth = 0.0;
    if (depth > 1.0) depth = 1.0;

    if (ZSTR_LEN(pcm_in) == 0) {
        RETURN_STRINGL("", 0);
    }

    if (ZSTR_LEN(pcm_in) % 2 != 0) {
        zend_throw_error(NULL, "Invalid PCM data: must be multiple of 2 bytes");
        RETURN_THROWS();
    }

    const opus_int16 *in = (const opus_int16*)ZSTR_VAL(pcm_in);
    size_t num_samples = ZSTR_LEN(pcm_in) / 2;
    int channels = obj->intern->channels;

    // Saída sempre em estéreo
    size_t num_frames = num_samples / channels;
    opus_int16 *out = (opus_int16 *)emalloc(num_frames * 2 * 2);
    memset(out, 0, num_frames * 2 * 2);

    // Use per-instance state instead of static
    const size_t delay_samples = (size_t)(depth * 20.0);
    const float ap_coeff = 0.7f;

    for (size_t i = 0; i < num_frames; i++) {
        float left, right;

        // Converte entrada para estéreo se for mono
        if (channels == 1) {
            float mono = (float)in[i] / 32768.0f;
            left = mono;
            right = mono;
        } else {
            left = (float)in[i * 2] / 32768.0f;
            right = (float)in[i * 2 + 1] / 32768.0f;
        }

        // Mid-Side Processing
        float mid = (left + right) * 0.5f;
        float side = (left - right) * 0.5f;

        // Expande a imagem estéreo
        side *= width;

        // All-Pass Filter
        float ap_in = side;
        float ap_out = ap_coeff * ap_in + obj->intern->ap_state_r;
        obj->intern->ap_state_r = ap_in - ap_coeff * ap_out;

        // Delay diferencial (efeito Haas)
        obj->intern->delay_buffer[obj->intern->delay_pos] = (opus_int16)(side * 32767.0f);
        size_t delayed_pos = (obj->intern->delay_pos + 4096 - delay_samples) % 4096;
        float delayed = (float)obj->intern->delay_buffer[delayed_pos] / 32768.0f;

        obj->intern->delay_pos = (obj->intern->delay_pos + 1) % 4096;

        // Reconstrói Left/Right
        float enhanced_side = side * (1.0f - depth) + (ap_out + delayed * 0.3f) * depth;

        float out_left = mid + enhanced_side;
        float out_right = mid - enhanced_side;

        // Pseudo-reverb
        obj->intern->reverb_l = obj->intern->reverb_l * 0.7f + out_left * 0.3f * depth;
        obj->intern->reverb_r = obj->intern->reverb_r * 0.7f + out_right * 0.3f * depth;

        out_left += obj->intern->reverb_l * 0.15f;
        out_right += obj->intern->reverb_r * 0.15f;

        // Limitador suave
        if (out_left > 1.0f) out_left = 1.0f;
        if (out_left < -1.0f) out_left = -1.0f;
        if (out_right > 1.0f) out_right = 1.0f;
        if (out_right < -1.0f) out_right = -1.0f;

        // Saída stereo
        out[i * 2] = (opus_int16)(out_left * 32767.0f);
        out[i * 2 + 1] = (opus_int16)(out_right * 32767.0f);
    }

    zend_string *result = zend_string_init((char*)out, num_frames * 2 * 2, 0);
    efree(out);
    RETURN_STR(result);
}

/* ========= Método 3: Mono to Stereo (Conversão Mono → Estéreo) ========= */
PHP_METHOD(opusChannel, monoToStereo)
{
    zend_string *pcm_in;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(pcm_in)
    ZEND_PARSE_PARAMETERS_END();

    opus_channel_object *obj = Z_OPUS_CHANNEL_P(ZEND_THIS);
    if (!obj->intern) {
        zend_throw_error(NULL, "OpusChannel not initialized");
        RETURN_THROWS();
    }

    if (ZSTR_LEN(pcm_in) == 0) {
        RETURN_STRINGL("", 0);
    }

    if (ZSTR_LEN(pcm_in) % 2 != 0) {
        zend_throw_error(NULL, "Invalid PCM data: must be multiple of 2 bytes");
        RETURN_THROWS();
    }

    const opus_int16 *in = (const opus_int16*)ZSTR_VAL(pcm_in);
    size_t num_samples = ZSTR_LEN(pcm_in) / 2;

    // Aloca buffer de saída estéreo (2 canais)
    opus_int16 *out = emalloc(num_samples * 2 * 2);

    // Duplica cada amostra mono para L e R
    for (size_t i = 0; i < num_samples; i++) {
        out[i * 2] = in[i];     // Left
        out[i * 2 + 1] = in[i]; // Right
    }

    zend_string *result = zend_string_init((char*)out, num_samples * 2 * 2, 0);
    efree(out);
    RETURN_STR(result);
}

/* ========= Método 4: Stereo to Mono (Conversão Estéreo → Mono) ========= */
PHP_METHOD(opusChannel, stereoToMono)
{
    zend_string *pcm_in;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(pcm_in)
    ZEND_PARSE_PARAMETERS_END();

    opus_channel_object *obj = Z_OPUS_CHANNEL_P(ZEND_THIS);
    if (!obj->intern) {
        zend_throw_error(NULL, "OpusChannel not initialized");
        RETURN_THROWS();
    }

    if (ZSTR_LEN(pcm_in) == 0) {
        RETURN_STRINGL("", 0);
    }

    if (ZSTR_LEN(pcm_in) % 4 != 0) {
        zend_throw_error(NULL, "Invalid stereo PCM data: must be multiple of 4 bytes");
        RETURN_THROWS();
    }

    const opus_int16 *in = (const opus_int16*)ZSTR_VAL(pcm_in);
    size_t num_frames = ZSTR_LEN(pcm_in) / 4; // 2 samples (L+R) * 2 bytes

    // Aloca buffer de saída mono
    opus_int16 *out = emalloc(num_frames * 2);

    // Mistura L e R com média aritmética
    for (size_t i = 0; i < num_frames; i++) {
        int32_t left = in[i * 2];
        int32_t right = in[i * 2 + 1];
        out[i] = (opus_int16)((left + right) / 2);
    }

    zend_string *result = zend_string_init((char*)out, num_frames * 2, 0);
    efree(out);
    RETURN_STR(result);
}

/* ========= Método 5: hasLibsoxr (Verifica se libsoxr está disponível) ========= */
PHP_METHOD(opusChannel, hasLibsoxr)
{
    ZEND_PARSE_PARAMETERS_NONE();

#ifdef HAVE_LIBSOXR
    RETURN_TRUE;
#else
    RETURN_FALSE;
#endif
}

/* ========= Método 6: getInfo (Retorna informações da instância) ========= */
PHP_METHOD(opusChannel, getInfo)
{
    ZEND_PARSE_PARAMETERS_NONE();

    opus_channel_object *obj = Z_OPUS_CHANNEL_P(ZEND_THIS);
    if (!obj->intern) {
        zend_throw_error(NULL, "OpusChannel not initialized");
        RETURN_THROWS();
    }

    array_init(return_value);

    // Informações básicas
    add_assoc_string(return_value, "extension_version", PHP_OPUS_VERSION);
    add_assoc_string(return_value, "libopus_version", opus_get_version_string());
    add_assoc_long(return_value, "sample_rate", obj->intern->sample_rate);
    add_assoc_long(return_value, "channels", obj->intern->channels);

    // Estado dos codecs
    add_assoc_bool(return_value, "encoder_initialized", obj->intern->encoder != NULL);
    add_assoc_bool(return_value, "decoder_initialized", obj->intern->decoder != NULL);

    // Informações do encoder (se disponível)
    if (obj->intern->encoder) {
        opus_int32 bitrate, vbr, complexity, dtx, signal;

        opus_encoder_ctl(obj->intern->encoder, OPUS_GET_BITRATE(&bitrate));
        opus_encoder_ctl(obj->intern->encoder, OPUS_GET_VBR(&vbr));
        opus_encoder_ctl(obj->intern->encoder, OPUS_GET_COMPLEXITY(&complexity));
        opus_encoder_ctl(obj->intern->encoder, OPUS_GET_DTX(&dtx));
        opus_encoder_ctl(obj->intern->encoder, OPUS_GET_SIGNAL(&signal));

        zval encoder_info;
        array_init(&encoder_info);
        add_assoc_long(&encoder_info, "bitrate", bitrate);
        add_assoc_bool(&encoder_info, "vbr", vbr);
        add_assoc_long(&encoder_info, "complexity", complexity);
        add_assoc_bool(&encoder_info, "dtx", dtx);
        add_assoc_string(&encoder_info, "signal", signal == OPUS_SIGNAL_VOICE ? "voice" : "music");

        add_assoc_zval(return_value, "encoder", &encoder_info);
    }

    // Informações do resampler
#ifdef HAVE_LIBSOXR
    add_assoc_bool(return_value, "libsoxr_available", 1);
    add_assoc_bool(return_value, "soxr_active", obj->intern->soxr_state != NULL);

    if (obj->intern->soxr_state) {
        zval soxr_info;
        array_init(&soxr_info);
        add_assoc_double(&soxr_info, "src_rate", obj->intern->soxr_src_rate);
        add_assoc_double(&soxr_info, "dst_rate", obj->intern->soxr_dst_rate);
        add_assoc_zval(return_value, "soxr", &soxr_info);
    }
#else
    add_assoc_bool(return_value, "libsoxr_available", 0);
    add_assoc_string(return_value, "resampler", "linear_fallback");
#endif

    // Hora de compilação
    add_assoc_string(return_value, "compiled", __DATE__ " " __TIME__);
}

/* ========= Destroy ========= */
PHP_METHOD(opusChannel, destroy)
{
    opus_channel_object *obj = Z_OPUS_CHANNEL_P(ZEND_THIS);

    if (obj->intern) {
        if (obj->intern->encoder) {
            opus_encoder_destroy(obj->intern->encoder);
            obj->intern->encoder = NULL;
        }
        if (obj->intern->decoder) {
            opus_decoder_destroy(obj->intern->decoder);
            obj->intern->decoder = NULL;
        }
#ifdef HAVE_LIBSOXR
        if (obj->intern->soxr_state) {
            soxr_delete(obj->intern->soxr_state);
            obj->intern->soxr_state = NULL;
        }
#endif
        efree(obj->intern);
        obj->intern = NULL;
    }
}

/* ========= Registro ========= */
static const zend_function_entry opus_channel_methods[] = {
    PHP_ME(opusChannel, __construct,          arginfo_opus_construct,      ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(opusChannel, encode,               arginfo_opus_encode,         ZEND_ACC_PUBLIC)
    PHP_ME(opusChannel, decode,               arginfo_opus_decode,         ZEND_ACC_PUBLIC)
    PHP_ME(opusChannel, resample,             arginfo_opus_resample,       ZEND_ACC_PUBLIC)
    PHP_ME(opusChannel, setBitrate,           arginfo_opus_long,           ZEND_ACC_PUBLIC)
    PHP_ME(opusChannel, setVBR,               arginfo_opus_bool,           ZEND_ACC_PUBLIC)
    PHP_ME(opusChannel, setComplexity,        arginfo_opus_long,           ZEND_ACC_PUBLIC)
    PHP_ME(opusChannel, setDTX,               arginfo_opus_bool,           ZEND_ACC_PUBLIC)
    PHP_ME(opusChannel, setSignalVoice,       arginfo_opus_bool,           ZEND_ACC_PUBLIC)
    PHP_ME(opusChannel, reset,                arginfo_opus_reset,          ZEND_ACC_PUBLIC)
    PHP_ME(opusChannel, enhanceVoiceClarity,  arginfo_opus_enhance_voice,  ZEND_ACC_PUBLIC)
    PHP_ME(opusChannel, spatialStereoEnhance, arginfo_opus_spatial_stereo, ZEND_ACC_PUBLIC)
    PHP_ME(opusChannel, monoToStereo,         arginfo_opus_mono_to_stereo, ZEND_ACC_PUBLIC)
    PHP_ME(opusChannel, stereoToMono,         arginfo_opus_stereo_to_mono, ZEND_ACC_PUBLIC)
    PHP_ME(opusChannel, hasLibsoxr,           arginfo_opus_has_libsoxr,    ZEND_ACC_PUBLIC)
    PHP_ME(opusChannel, getInfo,              arginfo_opus_get_info,       ZEND_ACC_PUBLIC)
    PHP_ME(opusChannel, destroy,              arginfo_opus_destroy,        ZEND_ACC_PUBLIC)
    PHP_FE_END
};

void register_opus_channel_class()
{
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "opusChannel", opus_channel_methods);
    opus_channel_ce = zend_register_internal_class(&ce);

    // Set custom object handlers with destructor
    opus_channel_ce->create_object = opus_channel_create_object;

    memcpy(&opus_channel_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    opus_channel_object_handlers.offset = XtOffsetOf(opus_channel_object, std);
    opus_channel_object_handlers.free_obj = opus_channel_free_storage;
}
