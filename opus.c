#include "php_opus.h"

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
    NULL,
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
