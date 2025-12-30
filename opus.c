#include "php_opus.h"

PHP_MINIT_FUNCTION(opus)
{
    register_opus_channel_class();
    return SUCCESS;
}

PHP_MINFO_FUNCTION(opus)
{
    php_info_print_table_start();
    php_info_print_table_row(2, "opus support", "enabled");
    php_info_print_table_row(2, "libopus", opus_get_version_string());
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
    "1.0",
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_OPUS
ZEND_GET_MODULE(opus)
#endif
