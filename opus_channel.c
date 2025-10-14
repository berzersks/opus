#include "php_spech_opus.h"

zend_class_entry *opus_channel_ce;

PHP_METHOD(opusChannel, __construct)
{
    zend_long sample_rate = 16000, channels = 1;
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
    if (err != OPUS_OK) zend_throw_error(NULL, "Opus encoder init failed: %s", opus_strerror(err));

    intern->decoder = opus_decoder_create(sample_rate, channels, &err);
    if (err != OPUS_OK) zend_throw_error(NULL, "Opus decoder init failed: %s", opus_strerror(err));

    zend_update_property_ptr(opus_channel_ce, Z_OBJ_P(ZEND_THIS), "ctx", intern);
}

PHP_METHOD(opusChannel, encode)
{
    zval *zctx;
    opus_channel_t *ctx;
    zend_string *pcm_in;
    unsigned char out[4000];
    int nbBytes;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(pcm_in)
    ZEND_PARSE_PARAMETERS_END();

    zctx = zend_read_property(opus_channel_ce, Z_OBJ_P(ZEND_THIS), "ctx", sizeof("ctx") - 1, 1, NULL);
    ctx = (opus_channel_t*)Z_PTR_P(zctx);

    nbBytes = opus_encode(ctx->encoder, (const opus_int16*)ZSTR_VAL(pcm_in),
                          ZSTR_LEN(pcm_in) / (2 * ctx->channels),
                          out, sizeof(out));

    if (nbBytes < 0)
        RETURN_FALSE;

    RETURN_STRINGL((char*)out, nbBytes);
}

PHP_METHOD(opusChannel, decode)
{
    zval *zctx;
    opus_channel_t *ctx;
    zend_string *opus_in;
    opus_int16 pcm_out[4096];
    int frame_size, ret;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(opus_in)
    ZEND_PARSE_PARAMETERS_END();

    zctx = zend_read_property(opus_channel_ce, Z_OBJ_P(ZEND_THIS), "ctx", sizeof("ctx") - 1, 1, NULL);
    ctx = (opus_channel_t*)Z_PTR_P(zctx);

    frame_size = ctx->sample_rate / 50;
    ret = opus_decode(ctx->decoder, (const unsigned char*)ZSTR_VAL(opus_in), ZSTR_LEN(opus_in),
                      pcm_out, frame_size, 0);

    if (ret < 0)
        RETURN_FALSE;

    RETURN_STRINGL((char*)pcm_out, ret * ctx->channels * 2);
}

PHP_METHOD(opusChannel, destroy)
{
    zval *zctx = zend_read_property(opus_channel_ce, Z_OBJ_P(ZEND_THIS), "ctx", sizeof("ctx") - 1, 1, NULL);
    opus_channel_t *ctx = (opus_channel_t*)Z_PTR_P(zctx);
    if (!ctx) RETURN_FALSE;

    if (ctx->encoder) opus_encoder_destroy(ctx->encoder);
    if (ctx->decoder) opus_decoder_destroy(ctx->decoder);
    efree(ctx);
    zend_update_property_null(opus_channel_ce, Z_OBJ_P(ZEND_THIS), "ctx", sizeof("ctx") - 1);
}

static const zend_function_entry opus_channel_methods[] = {
    PHP_ME(opusChannel, __construct, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
    PHP_ME(opusChannel, encode, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(opusChannel, decode, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(opusChannel, destroy, NULL, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

void register_opus_channel_class()
{
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "opusChannel", opus_channel_methods);
    opus_channel_ce = zend_register_internal_class(&ce);
    zend_declare_property_null(opus_channel_ce, "ctx", sizeof("ctx") - 1, ZEND_ACC_PROTECTED);
}
