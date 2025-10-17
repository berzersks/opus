#include "php_opus.h"

zend_class_entry *opus_channel_ce;

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

/* ========= Helpers ========= */
/* 8 kHz → 48 kHz */
static void pcm_upsample_8k_to_48k(const opus_int16 *in, int in_samples, opus_int16 *out)
{
    for (int i = 0; i < in_samples - 1; i++) {
        opus_int16 a = in[i];
        opus_int16 b = in[i + 1];
        for (int j = 0; j < 6; j++) {
            float r = (float)j / 6.0f;
            out[i * 6 + j] = (opus_int16)((1.0f - r) * a + r * b);
        }
    }
    for (int j = 0; j < 6; j++)
        out[(in_samples - 1) * 6 + j] = in[in_samples - 1];
}

/* 48 kHz → 8 kHz */
static void pcm_downsample_48k_to_8k(const opus_int16 *in, int in_samples, opus_int16 *out)
{
    int out_samples = in_samples / 6;
    for (int i = 0; i < out_samples; i++) {
        int sum = 0;
        for (int j = 0; j < 6; j++)
            sum += in[i * 6 + j];
        out[i] = (opus_int16)(sum / 6);
    }
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

    intern = ecalloc(1, sizeof(opus_channel_t));
    intern->sample_rate = (int)sample_rate;
    intern->channels = (int)channels;

    intern->encoder = opus_encoder_create(sample_rate, channels, OPUS_APPLICATION_VOIP, &err);
    if (err != OPUS_OK) {
        efree(intern);
        zend_throw_error(NULL, "Opus encoder init failed: %s", opus_strerror(err));
        RETURN_FALSE;
    }

    /* Configuração padrão otimizada para voz */
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
        RETURN_FALSE;
    }

    zval zctx;
    ZVAL_PTR(&zctx, intern);
    zend_update_property(opus_channel_ce, Z_OBJ_P(ZEND_THIS), ZEND_STRL("ctx"), &zctx);
}

/* ========= Controles ========= */
PHP_METHOD(opusChannel, setBitrate)
{
    zend_long bitrate;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(bitrate)
    ZEND_PARSE_PARAMETERS_END();

    zval *zctx = zend_read_property(opus_channel_ce, Z_OBJ_P(ZEND_THIS), ZEND_STRL("ctx"), 1, NULL);
    opus_channel_t *ctx = (opus_channel_t*)Z_PTR_P(zctx);
    opus_encoder_ctl(ctx->encoder, OPUS_SET_BITRATE(bitrate));
}

PHP_METHOD(opusChannel, setVBR)
{
    zend_bool enable;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_BOOL(enable)
    ZEND_PARSE_PARAMETERS_END();

    zval *zctx = zend_read_property(opus_channel_ce, Z_OBJ_P(ZEND_THIS), ZEND_STRL("ctx"), 1, NULL);
    opus_channel_t *ctx = (opus_channel_t*)Z_PTR_P(zctx);
    opus_encoder_ctl(ctx->encoder, OPUS_SET_VBR(enable));
}

PHP_METHOD(opusChannel, setComplexity)
{
    zend_long level;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(level)
    ZEND_PARSE_PARAMETERS_END();

    zval *zctx = zend_read_property(opus_channel_ce, Z_OBJ_P(ZEND_THIS), ZEND_STRL("ctx"), 1, NULL);
    opus_channel_t *ctx = (opus_channel_t*)Z_PTR_P(zctx);
    opus_encoder_ctl(ctx->encoder, OPUS_SET_COMPLEXITY(level));
}

PHP_METHOD(opusChannel, setDTX)
{
    zend_bool enable;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_BOOL(enable)
    ZEND_PARSE_PARAMETERS_END();

    zval *zctx = zend_read_property(opus_channel_ce, Z_OBJ_P(ZEND_THIS), ZEND_STRL("ctx"), 1, NULL);
    opus_channel_t *ctx = (opus_channel_t*)Z_PTR_P(zctx);
    opus_encoder_ctl(ctx->encoder, OPUS_SET_DTX(enable));
}

PHP_METHOD(opusChannel, setSignalVoice)
{
    zend_bool enable;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_BOOL(enable)
    ZEND_PARSE_PARAMETERS_END();

    zval *zctx = zend_read_property(opus_channel_ce, Z_OBJ_P(ZEND_THIS), ZEND_STRL("ctx"), 1, NULL);
    opus_channel_t *ctx = (opus_channel_t*)Z_PTR_P(zctx);
    opus_encoder_ctl(ctx->encoder, OPUS_SET_SIGNAL(enable ? OPUS_SIGNAL_VOICE : OPUS_SIGNAL_MUSIC));
}

PHP_METHOD(opusChannel, reset)
{
    zval *zctx = zend_read_property(opus_channel_ce, Z_OBJ_P(ZEND_THIS), ZEND_STRL("ctx"), 1, NULL);
    opus_channel_t *ctx = (opus_channel_t*)Z_PTR_P(zctx);
    if (!ctx) RETURN_FALSE;
    opus_encoder_ctl(ctx->encoder, OPUS_RESET_STATE);
    opus_decoder_ctl(ctx->decoder, OPUS_RESET_STATE);
    RETURN_TRUE;
}

/* ========= Encode / Decode ========= */
PHP_METHOD(opusChannel, encode)
{
    zval *zctx;
    opus_channel_t *ctx;
    zend_string *pcm_in;
    zend_long pcm_rate = 48000;
    unsigned char out[4000];
    int nbBytes;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STR(pcm_in)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(pcm_rate)
    ZEND_PARSE_PARAMETERS_END();

    zctx = zend_read_property(opus_channel_ce, Z_OBJ_P(ZEND_THIS), ZEND_STRL("ctx"), 1, NULL);
    ctx = (opus_channel_t*)Z_PTR_P(zctx);

    const opus_int16 *in = (const opus_int16*)ZSTR_VAL(pcm_in);
    int in_samples = ZSTR_LEN(pcm_in) / (2 * ctx->channels);
    opus_int16 tmp48[960 * 2];
    const opus_int16 *pcm = in;

    if (pcm_rate == 8000) {
        pcm_upsample_8k_to_48k(in, in_samples, tmp48);
        pcm = tmp48;
        in_samples *= 6;
    }

    nbBytes = opus_encode(ctx->encoder, pcm, in_samples, out, sizeof(out));
    if (nbBytes < 0)
        RETURN_FALSE;

    RETURN_STRINGL((char*)out, nbBytes);
}

PHP_METHOD(opusChannel, decode)
{
    zval *zctx;
    opus_channel_t *ctx;
    zend_string *opus_in;
    zend_long pcm_rate_out = 48000;
    opus_int16 pcm48[4096];
    opus_int16 pcm8[4096];
    int frame_size, ret;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STR(opus_in)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(pcm_rate_out)
    ZEND_PARSE_PARAMETERS_END();

    zctx = zend_read_property(opus_channel_ce, Z_OBJ_P(ZEND_THIS), ZEND_STRL("ctx"), 1, NULL);
    ctx = (opus_channel_t*)Z_PTR_P(zctx);

    frame_size = ctx->sample_rate / 50;
    ret = opus_decode(ctx->decoder, (const unsigned char*)ZSTR_VAL(opus_in), ZSTR_LEN(opus_in),
                      pcm48, frame_size, 0);
    if (ret < 0)
        RETURN_FALSE;

    if (pcm_rate_out == 8000) {
        pcm_downsample_48k_to_8k(pcm48, ret, pcm8);
        RETURN_STRINGL((char*)pcm8, (ret / 6) * ctx->channels * 2);
    }

    RETURN_STRINGL((char*)pcm48, ret * ctx->channels * 2);
}

/* ========= Destroy ========= */
PHP_METHOD(opusChannel, destroy)
{
    zval *zctx = zend_read_property(opus_channel_ce, Z_OBJ_P(ZEND_THIS), ZEND_STRL("ctx"), 1, NULL);
    opus_channel_t *ctx = (opus_channel_t*)Z_PTR_P(zctx);
    if (!ctx)
        RETURN_FALSE;

    if (ctx->encoder) opus_encoder_destroy(ctx->encoder);
    if (ctx->decoder) opus_decoder_destroy(ctx->decoder);
    efree(ctx);
    zend_update_property_null(opus_channel_ce, Z_OBJ_P(ZEND_THIS), ZEND_STRL("ctx"));
    RETURN_TRUE;
}

/* ========= Registro ========= */
static const zend_function_entry opus_channel_methods[] = {
    PHP_ME(opusChannel, __construct,     arginfo_opus_construct, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(opusChannel, encode,          arginfo_opus_encode,    ZEND_ACC_PUBLIC)
    PHP_ME(opusChannel, decode,          arginfo_opus_decode,    ZEND_ACC_PUBLIC)
    PHP_ME(opusChannel, setBitrate,      arginfo_opus_long,      ZEND_ACC_PUBLIC)
    PHP_ME(opusChannel, setVBR,          arginfo_opus_bool,      ZEND_ACC_PUBLIC)
    PHP_ME(opusChannel, setComplexity,   arginfo_opus_long,      ZEND_ACC_PUBLIC)
    PHP_ME(opusChannel, setDTX,          arginfo_opus_bool,      ZEND_ACC_PUBLIC)
    PHP_ME(opusChannel, setSignalVoice,  arginfo_opus_bool,      ZEND_ACC_PUBLIC)
    PHP_ME(opusChannel, reset,           arginfo_opus_reset,     ZEND_ACC_PUBLIC)
    PHP_ME(opusChannel, destroy,         arginfo_opus_destroy,   ZEND_ACC_PUBLIC)
    PHP_FE_END
};


void register_opus_channel_class()
{
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "opusChannel", opus_channel_methods);
    opus_channel_ce = zend_register_internal_class(&ce);
    zend_declare_property_null(opus_channel_ce, ZEND_STRL("ctx"), ZEND_ACC_PROTECTED);
}
