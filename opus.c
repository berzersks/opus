#include "php_opus.h"
#include "zend_exceptions.h"

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_resample, 0, 3, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, pcm, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, src_rate, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, dst_rate, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, options, IS_ARRAY, 0, "[]")
ZEND_END_ARG_INFO()

PHP_FUNCTION(resample)
{
    zend_string *pcm_in;
    zend_long src_rate, dst_rate;
    zval *options = NULL;

    ZEND_PARSE_PARAMETERS_START(3, 4)
        Z_PARAM_STR(pcm_in)
        Z_PARAM_LONG(src_rate)
        Z_PARAM_LONG(dst_rate)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY(options)
    ZEND_PARSE_PARAMETERS_END();

    if (src_rate <= 0 || dst_rate <= 0) {
        zend_throw_error(NULL, "Invalid sample rates");
        RETURN_THROWS();
    }

    if (ZSTR_LEN(pcm_in) == 0) {
        RETURN_STRINGL("", 0);
    }

    // Default options
    int in_channels = 2;
    int out_channels = 0; // 0 means same as in_channels
    double gain = 1.0;
    zend_bool reverse = 0;
    zend_bool normalize = 0;
    char *format = "s16le";

    if (options && Z_TYPE_P(options) == IS_ARRAY) {
        zval *val;
        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "input_channels", sizeof("input_channels") - 1)) != NULL) {
            in_channels = (int)zval_get_long(val);
        }
        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "channels", sizeof("channels") - 1)) != NULL) {
            out_channels = (int)zval_get_long(val);
        }
        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "output_channels", sizeof("output_channels") - 1)) != NULL) {
            out_channels = (int)zval_get_long(val);
        }
        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "gain", sizeof("gain") - 1)) != NULL) {
            gain = zval_get_double(val);
        }
        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "reverse", sizeof("reverse") - 1)) != NULL) {
            reverse = zval_is_true(val);
        }
        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "normalize", sizeof("normalize") - 1)) != NULL) {
            normalize = zval_is_true(val);
        }
        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "format", sizeof("format") - 1)) != NULL) {
            format = Z_STRVAL_P(val);
        }
    }

    if (out_channels <= 0) out_channels = in_channels;
    if (in_channels <= 0) in_channels = 2;

    if (ZSTR_LEN(pcm_in) % (2 * in_channels) != 0) {
        zend_throw_error(NULL, "Invalid PCM data: length must be multiple of %d (2 bytes * %d channels)", 2 * in_channels, in_channels);
        RETURN_THROWS();
    }

    size_t in_samples = ZSTR_LEN(pcm_in) / (2 * in_channels);
    size_t out_samples = (size_t)(in_samples * ((double)dst_rate / (double)src_rate));
    
    // Safety margin for resampling
    size_t alloc_samples = (size_t)(out_samples * 1.05) + 128;
    opus_int16 *in_buf = (opus_int16 *)ZSTR_VAL(pcm_in);
    opus_int16 *out_buf = (opus_int16 *)emalloc(alloc_samples * out_channels * sizeof(opus_int16));

    size_t actual_out_samples = 0;

#ifdef HAVE_LIBSOXR
    size_t idone, odone;
    soxr_io_spec_t io = soxr_io_spec(SOXR_INT16_I, SOXR_INT16_I);
    soxr_error_t err = soxr_oneshot(src_rate, dst_rate, in_channels == out_channels ? in_channels : (in_channels > out_channels ? in_channels : out_channels),
                                    in_buf, in_samples, &idone,
                                    out_buf, alloc_samples, &odone,
                                    &io, NULL, NULL);
    
    if (err) {
        efree(out_buf);
        zend_throw_error(NULL, "Resampling failed: %s", err);
        RETURN_THROWS();
    }
    actual_out_samples = odone;
#else
    // Linear interpolation fallback (simplified, assumes same channel count for simplicity in fallback)
    double ratio = (double)dst_rate / (double)src_rate;
    actual_out_samples = (size_t)(in_samples * ratio);
    if (actual_out_samples > alloc_samples) actual_out_samples = alloc_samples;

    if (in_channels == out_channels) {
        for (size_t i = 0; i < actual_out_samples; i++) {
            double pos = i / ratio;
            size_t p = (size_t)pos;
            double frac = pos - p;
            for (int c = 0; c < in_channels; c++) {
                if (p < in_samples - 1) {
                    out_buf[i * in_channels + c] = (opus_int16)(in_buf[p * in_channels + c] + (in_buf[(p + 1) * in_channels + c] - in_buf[p * in_channels + c]) * frac);
                } else {
                    out_buf[i * in_channels + c] = in_buf[(in_samples - 1) * in_channels + c];
                }
            }
        }
    } else {
        // Handle channel conversion in fallback
        for (size_t i = 0; i < actual_out_samples; i++) {
            double pos = i / ratio;
            size_t p = (size_t)pos;
            double frac = pos - p;
            
            float mix[2] = {0, 0};
            if (in_channels == 1) {
                float val = (p < in_samples - 1) ? (in_buf[p] + (in_buf[p+1] - in_buf[p]) * frac) : in_buf[in_samples-1];
                mix[0] = mix[1] = val;
            } else {
                for (int c = 0; c < 2; c++) {
                    mix[c] = (p < in_samples - 1) ? (in_buf[p*in_channels + c] + (in_buf[(p+1)*in_channels + c] - in_buf[p*in_channels + c]) * frac) : in_buf[(in_samples-1)*in_channels + c];
                }
            }

            if (out_channels == 1) {
                out_buf[i] = (opus_int16)((mix[0] + mix[1]) * 0.5f);
            } else {
                out_buf[i * 2] = (opus_int16)mix[0];
                out_buf[i * 2 + 1] = (opus_int16)mix[1];
            }
        }
    }
#endif

    // Apply Gain
    if (gain != 1.0) {
        for (size_t i = 0; i < actual_out_samples * out_channels; i++) {
            float v = out_buf[i] * gain;
            if (v > 32767.0f) v = 32767.0f;
            if (v < -32768.0f) v = -32768.0f;
            out_buf[i] = (opus_int16)v;
        }
    }

    // Surprise 1: Reverse
    if (reverse) {
        for (size_t i = 0; i < actual_out_samples / 2; i++) {
            for (int c = 0; c < out_channels; c++) {
                opus_int16 tmp = out_buf[i * out_channels + c];
                out_buf[i * out_channels + c] = out_buf[(actual_out_samples - 1 - i) * out_channels + c];
                out_buf[(actual_out_samples - 1 - i) * out_channels + c] = tmp;
            }
        }
    }

    // Surprise 2: Normalization
    if (normalize) {
        opus_int16 max_val = 0;
        for (size_t i = 0; i < actual_out_samples * out_channels; i++) {
            opus_int16 abs_v = out_buf[i] < 0 ? -out_buf[i] : out_buf[i];
            if (abs_v > max_val) max_val = abs_v;
        }
        if (max_val > 0 && max_val < 32767) {
            float norm_gain = 32767.0f / (float)max_val;
            for (size_t i = 0; i < actual_out_samples * out_channels; i++) {
                out_buf[i] = (opus_int16)(out_buf[i] * norm_gain);
            }
        }
    }

    // Format conversion (L16 / s16be)
    if (strcasecmp(format, "s16be") == 0 || strcasecmp(format, "l16") == 0) {
        for (size_t i = 0; i < actual_out_samples * out_channels; i++) {
            uint16_t v = (uint16_t)out_buf[i];
            out_buf[i] = (opus_int16)((v << 8) | (v >> 8));
        }
    }

    zend_string *result = zend_string_init((char*)out_buf, actual_out_samples * out_channels * 2, 0);
    efree(out_buf);
    RETURN_STR(result);
}

static const zend_function_entry opus_functions[] = {
    PHP_FE(resample, arginfo_resample)
    PHP_FE_END
};

PHP_MINIT_FUNCTION(opus)
{
    register_opus_channel_class();
    return SUCCESS;
}

PHP_MINFO_FUNCTION(opus)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "Opus Audio Codec Extension", "Information");
    php_info_print_table_row(2, "Extension Version", PHP_OPUS_VERSION);
    php_info_print_table_row(2, "Status", "enabled");
    php_info_print_table_row(2, "libopus Version", opus_get_version_string());
    php_info_print_table_row(2, "Compiled", __DATE__ " " __TIME__);
    php_info_print_table_end();

    php_info_print_table_start();
    php_info_print_table_header(2, "Codec Capabilities", "Supported");
    php_info_print_table_row(2, "Sample Rates", "8000, 12000, 16000, 24000, 48000 Hz");
    php_info_print_table_row(2, "Audio Channels", "Mono, Stereo");
    php_info_print_table_row(2, "Applications", "VOIP, Audio, Restricted Low Delay");
    php_info_print_table_row(2, "Bitrate Control", "Configurable (6-510 kbps)");
    php_info_print_table_row(2, "Frame Sizes", "2.5, 5, 10, 20, 40, 60 ms");
    php_info_print_table_end();

    php_info_print_table_start();
    php_info_print_table_header(2, "Features", "Available");
    php_info_print_table_row(2, "Encoding", "Yes");
    php_info_print_table_row(2, "Decoding", "Yes");
    php_info_print_table_row(2, "DTX (Discontinuous Transmission)", "Yes");
    php_info_print_table_row(2, "FEC (Forward Error Correction)", "Yes");
    php_info_print_table_row(2, "VBR (Variable Bitrate)", "Yes");
    php_info_print_table_row(2, "Packet Loss Concealment", "Yes");
    php_info_print_table_end();
}

zend_module_entry opus_module_entry = {
    STANDARD_MODULE_HEADER,
    "opus",
    opus_functions,
    PHP_MINIT(opus),
    NULL,
    NULL,
    NULL,
    PHP_MINFO(opus),
    "1.1",
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_OPUS
ZEND_GET_MODULE(opus)
#endif
