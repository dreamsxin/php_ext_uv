#ifndef _UV_TIMER_H
#define _UV_TIMER_H
#include "../php_ext_uv.h"
#include "fcall_info.h"
#include "uv_loop_resource.h"

ZEND_BEGIN_ARG_INFO(ARGINFO(UVTimer, __construct), 0)
    ZEND_ARG_OBJ_INFO(0, loop, UVLoop, 1)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(ARGINFO(UVTimer, start), 0)
    ZEND_ARG_INFO(0, timer_cb)
    ZEND_ARG_INFO(0, start)
    ZEND_ARG_INFO(0, repeat)
ZEND_END_ARG_INFO()

typedef struct uv_signal_ext_s{
    uv_timer_t uv_timer;
    int start;
    fcall_info_t callback;
    zval *object;
    zend_object zo;    
} uv_timer_ext_t;

static zend_object_value createUVTimerResource(zend_class_entry *class_type TSRMLS_DC);
static void timer_handle_callback(uv_timer_ext_t *timer_handle);

void freeUVTimerResource(void *object TSRMLS_DC);

PHP_METHOD(UVTimer, __construct);
PHP_METHOD(UVTimer, start);
PHP_METHOD(UVTimer, stop);

DECLARE_FUNCTION_ENTRY(UVTimer) = {
    PHP_ME(UVTimer, __construct, ARGINFO(UVTimer, __construct), ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)    
    PHP_ME(UVTimer, start, ARGINFO(UVTimer, start), ZEND_ACC_PUBLIC)
    PHP_ME(UVTimer, stop, NULL, ZEND_ACC_PUBLIC)    
    PHP_FE_END
};
#endif
