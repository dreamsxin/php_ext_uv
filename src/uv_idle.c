#include "uv_idle.h"

CLASS_ENTRY_FUNCTION_D(UVIdle){
    REGISTER_CLASS_WITH_OBJECT_NEW(UVIdle, createUVIdleResource);
    OBJECT_HANDLER(UVIdle).offset = XtOffsetOf(uv_idle_ext_t, zo);
    OBJECT_HANDLER(UVIdle).clone_obj = NULL;
    OBJECT_HANDLER(UVIdle).free_obj = freeUVIdleResource;
    zend_declare_property_null(CLASS_ENTRY(UVIdle), ZEND_STRL("loop"), ZEND_ACC_PRIVATE);
}

static void idle_handle_callback(uv_idle_ext_t *idle_handle){
    zval retval;
    fci_call_function(&idle_handle->callback, &retval, 1, &idle_handle->object);
    zval_dtor(&retval);
}

static zend_object *createUVIdleResource(zend_class_entry *ce) {
    uv_idle_ext_t *resource;
    resource = ALLOC_RESOURCE(uv_idle_ext_t);

    zend_object_std_init(&resource->zo, ce);
    object_properties_init(&resource->zo, ce);
    
    resource->zo.handlers = &OBJECT_HANDLER(UVIdle);
    ZVAL_NULL(&resource->callback.func);
    return &resource->zo;
}

void freeUVIdleResource(zend_object *object) {
    uv_idle_ext_t *resource;
    resource = FETCH_RESOURCE(object, uv_idle_ext_t);
    if(resource->start){
        uv_idle_stop((uv_idle_t *) resource);
    }
    
    uv_unref((uv_handle_t *) resource);
    freeFunctionCache(&resource->callback);
    zend_object_std_dtor(&resource->zo);
    efree(resource);
}

PHP_METHOD(UVIdle, __construct){
    zval *loop = NULL;
    zval *self = getThis();
    zend_function *fptr;
    uv_idle_ext_t *resource = FETCH_OBJECT_RESOURCE(self, uv_idle_ext_t);
    if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "|z", &loop)) {
        return;
    }

    if(NULL == loop || ZVAL_IS_NULL(loop)){
        uv_idle_init(uv_default_loop(), (uv_idle_t *) resource);
        return;
    }

    zend_update_property(CLASS_ENTRY(UVIdle), self, ZEND_STRL("loop"), loop);
    uv_idle_init(FETCH_UV_LOOP(), (uv_idle_t *) resource);
}

PHP_METHOD(UVIdle, start){
    zval *idle_cb;
    long ret;
    zval *self = getThis();
    uv_idle_ext_t *resource = FETCH_OBJECT_RESOURCE(self, uv_idle_ext_t);
    
    if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "z", &idle_cb)) {
        return;
    }
    
    ret = uv_idle_start((uv_idle_t *) resource, (uv_idle_cb) idle_handle_callback);
    if(ret == 0){
        registerFunctionCache(&resource->callback, idle_cb);
        resource->start = 1;
        ZVAL_ZVAL(&resource->object, self, 1, 0);
        Z_ADDREF(resource->object);
    }
    RETURN_LONG(ret);
}

PHP_METHOD(UVIdle, stop){
    long ret;
    zval *self = getThis();
    uv_idle_ext_t *resource = FETCH_OBJECT_RESOURCE(self, uv_idle_ext_t);
    
    if(!resource->start){
        RETURN_LONG(-1);
    }
    
    ret = uv_idle_stop((uv_idle_t *) resource);
    if(ret == 0){
        resource->start = 0;
        zval_dtor(&resource->object);
    }
    RETURN_LONG(ret);
}
