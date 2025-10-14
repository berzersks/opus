#include "php_spech_opus.h"

PHP_MINIT_FUNCTION(spech_opus)
{
    register_opus_channel_class();
    return SUCCESS;
}

PHP_MINFO_FUNCTION(spech_opus)
{
    php_info_print_table_start();
    php_info_print_table_row(2, "spech_opus support", "enabled");
    php_info_print_table_row(2, "libopus", opus_get_version_string());
    php_info_print_table_end();
}

zend_module_entry spech_opus_module_entry = {
    STANDARD_MODULE_HEADER,
    "spech_opus",
    NULL,
    PHP_MINIT(spech_opus),
    NULL,
    NULL,
    NULL,
    PHP_MINFO(spech_opus),
    "1.0",
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_SPECH_OPUS
ZEND_GET_MODULE(spech_opus)
#endif
