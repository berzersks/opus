#include "php_opus.h"
#include "zend_exceptions.h"





#ifdef HAVE_LIBSOXR
#include <soxr.h>
#endif

typedef struct {
#ifdef HAVE_LIBSOXR
    soxr_t soxr;
    long src_rate;
    long dst_rate;
    int  channels;
    int  recipe;     // SOXR_HQ, SOXR_VHQ, ...
    unsigned flags;  // quality flags
    double passband_end;   // 0..1 (optional override)
    double stopband_begin; // > passband_end (optional override)
    double phase_response; // 0..100 (optional override)
    double precision;      // optional override
    int in_use;
#endif
} soxr_slot_t;

#ifdef HAVE_LIBSOXR
#define SOXR_CACHE_SLOTS 8
static soxr_slot_t g_soxr_cache[SOXR_CACHE_SLOTS];

static int soxr_parse_quality_recipe(const char *q) {
    if (!q) return SOXR_HQ;
    if (!strcasecmp(q, "vhq")) return SOXR_VHQ;
    if (!strcasecmp(q, "hq"))  return SOXR_HQ;
    if (!strcasecmp(q, "mq"))  return SOXR_MQ;
    if (!strcasecmp(q, "lq"))  return SOXR_LQ;
    if (!strcasecmp(q, "qq"))  return SOXR_QQ;
    return SOXR_HQ;
}

static soxr_t soxr_get_cached(
    long src_rate, long dst_rate, int channels,
    int recipe, unsigned flags,
    double passband_end, double stopband_begin,
    double phase_response, double precision
) {
    // 1) tenta achar igual
    for (int i = 0; i < SOXR_CACHE_SLOTS; i++) {
        soxr_slot_t *s = &g_soxr_cache[i];
        if (!s->in_use) continue;
        if (s->src_rate == src_rate && s->dst_rate == dst_rate &&
            s->channels == channels && s->recipe == recipe && s->flags == (int)flags &&
            s->passband_end == passband_end && s->stopband_begin == stopband_begin &&
            s->phase_response == phase_response && s->precision == precision &&
            s->soxr) {
            return s->soxr;
        }
    }

    // 2) acha slot vazio ou reaproveita o 0
    int idx = -1;
    for (int i = 0; i < SOXR_CACHE_SLOTS; i++) {
        if (!g_soxr_cache[i].in_use) { idx = i; break; }
    }
    if (idx < 0) idx = 0;

    soxr_slot_t *s = &g_soxr_cache[idx];
    if (s->in_use && s->soxr) {
        soxr_delete(s->soxr);
        s->soxr = NULL;
    }

    soxr_error_t err = NULL;
    soxr_io_spec_t io = soxr_io_spec(SOXR_INT16_I, SOXR_INT16_I);

    soxr_quality_spec_t q = soxr_quality_spec(recipe, flags);

    // overrides finos (quando >0)
    if (precision > 0)       q.precision = precision;
    if (phase_response >= 0) q.phase_response = phase_response;
    if (passband_end > 0)    q.passband_end = passband_end;
    if (stopband_begin > 0)  q.stopband_begin = stopband_begin;

    // Cria stateful
    soxr_t r = soxr_create((double)src_rate, (double)dst_rate, (unsigned)channels,
                          &err, &io, &q, NULL);
    if (!r) return NULL;

    s->soxr = r;
    s->src_rate = src_rate;
    s->dst_rate = dst_rate;
    s->channels = channels;
    s->recipe = recipe;
    s->flags = flags;
    s->passband_end = passband_end;
    s->stopband_begin = stopband_begin;
    s->phase_response = phase_response;
    s->precision = precision;
    s->in_use = 1;

    return r;
}

static inline uint16_t bswap16_u(uint16_t x) { return (uint16_t)((x<<8) | (x>>8)); }

static void byteswap_s16_inplace(opus_int16 *buf, size_t samples_total) {
    uint16_t *p = (uint16_t*)buf;
    for (size_t i = 0; i < samples_total; i++) p[i] = bswap16_u(p[i]);
}
#endif


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

    // Defaults
    int in_channels = 1;          // VoIP default sensato
    int out_channels = 0;         // 0 = igual ao in
    double gain = 1.0;
    zend_bool reverse = 0;
    zend_bool normalize = 0;

    const char *input_format  = "s16le";
    const char *output_format = "s16le";

#ifdef HAVE_LIBSOXR
    const char *quality = "hq";
    unsigned q_flags = 0;
    double passband_end   = -1;   // -1 = não mexe
    double stopband_begin = -1;
    double phase_response = -1;
    double precision      = -1;
    zend_bool flush = 0;
    zend_bool reset_state = 0;
#endif

    if (options && Z_TYPE_P(options) == IS_ARRAY) {
        zval *val;

        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "input_channels", sizeof("input_channels")-1)))
            in_channels = (int)zval_get_long(val);

        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "channels", sizeof("channels")-1)))
            out_channels = (int)zval_get_long(val);

        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "output_channels", sizeof("output_channels")-1)))
            out_channels = (int)zval_get_long(val);

        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "gain", sizeof("gain")-1)))
            gain = zval_get_double(val);

        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "reverse", sizeof("reverse")-1)))
            reverse = zval_is_true(val);

        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "normalize", sizeof("normalize")-1)))
            normalize = zval_is_true(val);

        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "input_format", sizeof("input_format")-1)) && Z_TYPE_P(val) == IS_STRING)
            input_format = Z_STRVAL_P(val);

        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "format", sizeof("format")-1)) && Z_TYPE_P(val) == IS_STRING)
            output_format = Z_STRVAL_P(val);

        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "output_format", sizeof("output_format")-1)) && Z_TYPE_P(val) == IS_STRING)
            output_format = Z_STRVAL_P(val);

#ifdef HAVE_LIBSOXR
        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "quality", sizeof("quality")-1)) && Z_TYPE_P(val) == IS_STRING)
            quality = Z_STRVAL_P(val);

        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "q_flags", sizeof("q_flags")-1)))
            q_flags = (unsigned)zval_get_long(val);

        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "passband_end", sizeof("passband_end")-1)))
            passband_end = zval_get_double(val);

        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "stopband_begin", sizeof("stopband_begin")-1)))
            stopband_begin = zval_get_double(val);

        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "phase_response", sizeof("phase_response")-1)))
            phase_response = zval_get_double(val);

        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "precision", sizeof("precision")-1)))
            precision = zval_get_double(val);

        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "flush", sizeof("flush")-1)))
            flush = zval_is_true(val);

        if ((val = zend_hash_str_find(Z_ARRVAL_P(options), "reset", sizeof("reset")-1)))
            reset_state = zval_is_true(val);
#endif
    }

    if (in_channels <= 0) in_channels = 1;
    if (out_channels <= 0) out_channels = in_channels;

    if (in_channels != 1 && in_channels != 2) {
        zend_throw_error(NULL, "input_channels must be 1 or 2");
        RETURN_THROWS();
    }
    if (out_channels != 1 && out_channels != 2) {
        zend_throw_error(NULL, "output_channels must be 1 or 2");
        RETURN_THROWS();
    }

    if (ZSTR_LEN(pcm_in) % (2 * in_channels) != 0) {
        zend_throw_error(NULL, "Invalid PCM data: length must be multiple of %d bytes", 2 * in_channels);
        RETURN_THROWS();
    }

    size_t in_frames = ZSTR_LEN(pcm_in) / (2 * (size_t)in_channels); // frames = amostras por canal

    // Copia/ajusta input se precisar (BE -> LE)
    opus_int16 *in_buf = (opus_int16*)ZSTR_VAL(pcm_in);
    opus_int16 *in_work = in_buf;
    zend_bool in_allocated = 0;

#ifdef HAVE_LIBSOXR
    if (!strcasecmp(input_format, "s16be") || !strcasecmp(input_format, "l16")) {
        in_work = (opus_int16*)emalloc(in_frames * (size_t)in_channels * sizeof(opus_int16));
        memcpy(in_work, in_buf, in_frames * (size_t)in_channels * sizeof(opus_int16));
        byteswap_s16_inplace(in_work, in_frames * (size_t)in_channels);
        in_allocated = 1;
    }
#endif

    // Converte canais ANTES do resample quando for 2->1 (melhor + mais barato)
    int work_channels = in_channels;
    opus_int16 *chan_work = in_work;
    zend_bool chan_allocated = 0;

    if (in_channels == 2 && out_channels == 1) {
        chan_work = (opus_int16*)emalloc(in_frames * sizeof(opus_int16));
        for (size_t i = 0; i < in_frames; i++) {
            int32_t L = in_work[i*2];
            int32_t R = in_work[i*2 + 1];
            int32_t m = (L + R) / 2;
            if (m > 32767) m = 32767;
            if (m < -32768) m = -32768;
            chan_work[i] = (opus_int16)m;
        }
        work_channels = 1;
        chan_allocated = 1;
    }

    // Agora resample stateful
#ifdef HAVE_LIBSOXR
    int recipe = soxr_parse_quality_recipe(quality);

    // se user passou -1, não mexe
    double pb = (passband_end   > 0 ? passband_end   : 0);
    double sb = (stopband_begin > 0 ? stopband_begin : 0);
    double ph = (phase_response >= 0 ? phase_response : -1);
    double pr = (precision      > 0 ? precision : -1);

    soxr_t soxr = soxr_get_cached(src_rate, dst_rate, work_channels, recipe, q_flags, pb, sb, ph, pr);
    if (!soxr) {
        if (chan_allocated) efree(chan_work);
        if (in_allocated) efree(in_work);
        zend_throw_error(NULL, "soxr_create failed");
        RETURN_THROWS();
    }

    if (reset_state) {
        // zera o estado interno (sem destruir)
        soxr_clear(soxr);
    }

    double ratio = (double)dst_rate / (double)src_rate;

    // estimativa: frames*ratio + delay + folga
    double dly = soxr_delay(soxr);
    size_t out_frames_cap = (size_t)ceil((double)in_frames * ratio + dly + 64.0);
    if (out_frames_cap < 64) out_frames_cap = 64;

    // Aloca pro pior caso (upmix depois pode dobrar)
    int max_ch = (out_channels > work_channels) ? out_channels : work_channels;
    opus_int16 *out_buf = (opus_int16*)emalloc(out_frames_cap * (size_t)max_ch * sizeof(opus_int16));

    size_t idone = 0, odone = 0;
    soxr_error_t err = soxr_process(soxr,
                                    chan_work, in_frames, &idone,
                                    out_buf, out_frames_cap, &odone);
    if (err) {
        efree(out_buf);
        if (chan_allocated) efree(chan_work);
        if (in_allocated) efree(in_work);
        zend_throw_error(NULL, "Resampling failed: %s", err);
        RETURN_THROWS();
    }

    // Flush opcional: drena o delay interno
    if (flush) {
        size_t odone2 = 0, idone2 = 0;
        // tenta drenar numa segunda passada (pequena)
        err = soxr_process(soxr, NULL, 0, &idone2,
                           out_buf + (odone * (size_t)work_channels),
                           out_frames_cap - odone, &odone2);
        if (!err) odone += odone2;
    }

    size_t out_frames = odone;

    // Upmix depois (1->2)
    if (work_channels == 1 && out_channels == 2) {
        for (size_t i = out_frames; i > 0; i--) {
            size_t k = i - 1;
            opus_int16 v = out_buf[k];
            out_buf[k*2]     = v;
            out_buf[k*2 + 1] = v;
        }
    }

    // total de samples = frames * out_channels
    size_t out_samples_total = out_frames * (size_t)out_channels;

    // Gain (clip)
    if (gain != 1.0) {
        for (size_t i = 0; i < out_samples_total; i++) {
            double v = (double)out_buf[i] * gain;
            if (v > 32767.0) v = 32767.0;
            if (v < -32768.0) v = -32768.0;
            out_buf[i] = (opus_int16)v;
        }
    }

    // Reverse por frame
    if (reverse && out_frames > 1) {
        for (size_t i = 0; i < out_frames / 2; i++) {
            size_t j = out_frames - 1 - i;
            for (int c = 0; c < out_channels; c++) {
                opus_int16 tmp = out_buf[i*(size_t)out_channels + c];
                out_buf[i*(size_t)out_channels + c] = out_buf[j*(size_t)out_channels + c];
                out_buf[j*(size_t)out_channels + c] = tmp;
            }
        }
    }

    // Normalize (cuidado com -32768)
    if (normalize) {
        int32_t maxv = 0;
        for (size_t i = 0; i < out_samples_total; i++) {
            int32_t v = out_buf[i];
            if (v < 0) v = -v;
            if (v > maxv) maxv = v;
        }
        if (maxv > 0 && maxv < 32767) {
            double ng = 32767.0 / (double)maxv;
            for (size_t i = 0; i < out_samples_total; i++) {
                double v = (double)out_buf[i] * ng;
                if (v > 32767.0) v = 32767.0;
                if (v < -32768.0) v = -32768.0;
                out_buf[i] = (opus_int16)v;
            }
        }
    }

    // Output endian
    if (!strcasecmp(output_format, "s16be") || !strcasecmp(output_format, "l16")) {
        byteswap_s16_inplace(out_buf, out_samples_total);
    }

    zend_string *result = zend_string_init((char*)out_buf, out_samples_total * 2, 0);

    efree(out_buf);
    if (chan_allocated) efree(chan_work);
    if (in_allocated) efree(in_work);

    RETURN_STR(result);

#else
    // fallback simples: (se você quer qualidade, habilite o soxr)
    zend_throw_error(NULL, "resample: libsoxr not available");
    RETURN_THROWS();
#endif
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
    "1.1.1",
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_OPUS
ZEND_GET_MODULE(opus)
#endif
