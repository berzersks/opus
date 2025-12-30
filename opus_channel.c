#include "php_opus.h"
#include <math.h>

#ifdef HAVE_LIBSOXR
#include "soxr.h"
#endif

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

/* ========= Object Lifecycle ========= */
static zend_object *opus_channel_create_object(zend_class_entry *ce) {
    opus_channel_object *obj = ecalloc(1, sizeof(opus_channel_object) + zend_object_properties_size(ce));

    zend_object_std_init(&obj->std, ce);
    object_properties_init(&obj->std, ce);
    obj->std.handlers = &opus_channel_object_handlers;

    obj->intern = NULL;

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
    opus_channel_t *intern;
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

    // Prevent double initialization
    if (obj->intern != NULL) {
        zend_throw_error(NULL, "OpusChannel already initialized");
        RETURN_THROWS();
    }

    intern = ecalloc(1, sizeof(opus_channel_t));
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
        efree(intern);
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
        efree(intern);
        zend_throw_error(NULL, "Opus decoder init failed: %s", opus_strerror(err));
        RETURN_THROWS();
    }

#ifdef HAVE_LIBSOXR
    intern->soxr_state = NULL;
    intern->soxr_src_rate = 0;
    intern->soxr_dst_rate = 0;
#endif

    obj->intern = intern;
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
    zend_long pcm_rate = 48000;
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

    // Validate input size
    if (ZSTR_LEN(pcm_in) == 0) {
        zend_throw_error(NULL, "Empty PCM data");
        RETURN_THROWS();
    }

    if (ZSTR_LEN(pcm_in) % (2 * obj->intern->channels) != 0) {
        zend_throw_error(NULL, "Invalid PCM data size: must be multiple of %d bytes", 2 * obj->intern->channels);
        RETURN_THROWS();
    }

    const opus_int16 *in = (const opus_int16*)ZSTR_VAL(pcm_in);
    int in_samples = ZSTR_LEN(pcm_in) / (2 * obj->intern->channels);

    // Validate frame size: Opus supports 2.5, 5, 10, 20, 40, 60, 80, 100, 120ms frames
    // Calculate valid frame sizes for this sample rate
    // Frame duration in milliseconds: 2.5, 5, 10, 20, 40, 60, 80, 100, 120
    double frame_durations[] = {0.0025, 0.005, 0.010, 0.020, 0.040, 0.060, 0.080, 0.100, 0.120};

    int is_valid = 0;
    for (int i = 0; i < 9; i++) {
        int expected_samples = (int)(obj->intern->sample_rate * frame_durations[i]);
        if (in_samples == expected_samples) {
            is_valid = 1;
            break;
        }
    }

    if (!is_valid) {
        zend_throw_error(NULL, "Invalid frame size: %d samples. Must be 2.5, 5, 10, 20, 40, 60, 80, 100, or 120ms worth of samples at %dHz",
                        in_samples, obj->intern->sample_rate);
        RETURN_THROWS();
    }

    // Allocate output buffer (max opus frame size is ~4000 bytes)
    unsigned char *out = emalloc(4000);

    nbBytes = opus_encode(obj->intern->encoder, in, in_samples, out, 4000);
    if (nbBytes < 0) {
        efree(out);
        zend_throw_error(NULL, "Opus encode failed: %s", opus_strerror(nbBytes));
        RETURN_THROWS();
    }

    zend_string *result = zend_string_init((char*)out, nbBytes, 0);
    efree(out);
    RETURN_STR(result);
}

PHP_METHOD(opusChannel, decode)
{
    zend_string *opus_in;
    zend_long pcm_rate_out = 48000;
    int frame_size, ret;

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

    // Validate input
    if (ZSTR_LEN(opus_in) == 0) {
        zend_throw_error(NULL, "Empty Opus data");
        RETURN_THROWS();
    }

    if (ZSTR_LEN(opus_in) > 4000) {
        zend_throw_error(NULL, "Opus data too large (max 4000 bytes)");
        RETURN_THROWS();
    }

    // Maximum frame size is 5760 samples (120ms at 48kHz)
    frame_size = 5760;
    opus_int16 *pcm_out = emalloc(frame_size * obj->intern->channels * 2);

    ret = opus_decode(obj->intern->decoder, (const unsigned char*)ZSTR_VAL(opus_in),
                      ZSTR_LEN(opus_in), pcm_out, frame_size, 0);
    if (ret < 0) {
        efree(pcm_out);
        zend_throw_error(NULL, "Opus decode failed: %s", opus_strerror(ret));
        RETURN_THROWS();
    }

    zend_string *result = zend_string_init((char*)pcm_out, ret * obj->intern->channels * 2, 0);
    efree(pcm_out);
    RETURN_STR(result);
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
    size_t in_samples = ZSTR_LEN(pcm_in) / 2;

    // Calculate output size with safety margin
    size_t estimated_out = (size_t)(in_samples * ((double)dst_rate / (double)src_rate) * 1.1) + 64;
    if (estimated_out > 8192 * 6) estimated_out = 8192 * 6;

    opus_int16 *outbuf = emalloc(estimated_out * 2);
    size_t odone = 0;

#ifdef HAVE_LIBSOXR
    // Use per-instance soxr state instead of static
    if (!obj->intern->soxr_state ||
        obj->intern->soxr_src_rate != src_rate ||
        obj->intern->soxr_dst_rate != dst_rate) {

        if (obj->intern->soxr_state) {
            soxr_delete(obj->intern->soxr_state);
            obj->intern->soxr_state = NULL;
        }

        soxr_error_t err;
        soxr_io_spec_t io = soxr_io_spec(SOXR_INT16_I, SOXR_INT16_I);
        soxr_quality_spec_t q = soxr_quality_spec(SOXR_MQ, SOXR_LOW_LATENCY);
        soxr_runtime_spec_t r = soxr_runtime_spec(0);

        obj->intern->soxr_state = soxr_create((double)src_rate, (double)dst_rate, 1, &err, &io, &q, &r);
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

    // Parâmetros adaptativos baseados na intensidade
    float gate_threshold = -40.0f + (intensity * 10.0f);
    float comp_ratio = 2.0f + (intensity * 1.5f);
    float gain_boost = 1.0f + (intensity * 0.4f);

    // Use per-instance state instead of static
    float hp_prev = obj->intern->hp_prev;
    float lp_prev = obj->intern->lp_prev;
    const float hp_alpha = 0.98f;
    const float lp_alpha = 0.15f;

    float envelope = 0.0f;
    const float attack = 0.001f;
    const float release = 0.05f;

    float comp_env = 0.0f;
    const float comp_threshold = 0.3f;

    for (size_t i = 0; i < num_samples; i++) {
        float sample = (float)in[i] / 32768.0f;

        // 1. Filtro High-Pass
        float hp_out = sample - hp_prev;
        hp_prev = hp_prev + (hp_alpha * (sample - hp_prev));

        // 2. Filtro Low-Pass
        lp_prev = lp_prev + (lp_alpha * (hp_out - lp_prev));
        float filtered = lp_prev;

        // 3. Gate de Ruído Adaptativo
        float abs_sample = filtered > 0 ? filtered : -filtered;
        if (abs_sample > envelope) {
            envelope = envelope + (attack * (abs_sample - envelope));
        } else {
            envelope = envelope + (release * (abs_sample - envelope));
        }

        float gate_db = 20.0f * log10f(envelope + 0.0001f);
        float gate_factor = 1.0f;
        if (gate_db < gate_threshold) {
            gate_factor = 0.1f;
        }
        filtered *= gate_factor;

        // 4. Compressor Dinâmico
        comp_env = comp_env * 0.999f + abs_sample * 0.001f;
        float comp_gain = 1.0f;
        if (comp_env > comp_threshold) {
            comp_gain = comp_threshold + ((comp_env - comp_threshold) / comp_ratio);
            comp_gain = comp_gain / comp_env;
        }
        filtered *= comp_gain;

        // 5. Ganho final e saturação suave
        float output = filtered * gain_boost;

        // Saturação suave (soft clipping)
        if (output > 0.9f) output = 0.9f + 0.1f * tanhf((output - 0.9f) * 10.0f);
        if (output < -0.9f) output = -0.9f + 0.1f * tanhf((output + 0.9f) * 10.0f);

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
    opus_int16 *out = emalloc(num_frames * 2 * 2);

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
