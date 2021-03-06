#include "uv_tcp.h"

static zend_always_inline void initUVTcpFunctionCache(uv_tcp_ext_t *resource TSRMLS_DC){
}                    

static zend_always_inline void releaseUVTcpFunctionCache(uv_tcp_ext_t *resource TSRMLS_DC){
    freeFunctionCache(&resource->readCallback TSRMLS_CC);
    freeFunctionCache(&resource->writeCallback TSRMLS_CC);
    freeFunctionCache(&resource->errorCallback TSRMLS_CC);
    freeFunctionCache(&resource->connectCallback TSRMLS_CC);
    freeFunctionCache(&resource->shutdownCallback TSRMLS_CC);
}
                                        

void tcp_close_socket(uv_tcp_ext_t *handle){       
    if(handle->flag & UV_TCP_CLOSING_START){
         return;
    }
    handle->flag |= UV_TCP_CLOSING_START;
    uv_close((uv_handle_t *) handle, tcp_close_cb);
}

void setSelfReference(uv_tcp_ext_t *resource)
{
    if(resource->flag & UV_TCP_HANDLE_INTERNAL_REF){
        return;
    }
    Z_ADDREF_P(resource->object);
    resource->flag |= UV_TCP_HANDLE_INTERNAL_REF;
}

CLASS_ENTRY_FUNCTION_D(UVTcp){
    REGISTER_CLASS_WITH_OBJECT_NEW(UVTcp, createUVTcpResource);
    OBJECT_HANDLER(UVTcp).clone_obj = NULL;
    zend_declare_property_null(CLASS_ENTRY(UVTcp), ZEND_STRL("loop"), ZEND_ACC_PRIVATE TSRMLS_CC);
}

void releaseResource(uv_tcp_ext_t *resource){

    if(resource->sockPort != 0){
        resource->sockPort = 0;
        efree(resource->sockAddr);
        resource->sockAddr = NULL;
    }

    if(resource->peerPort != 0){
        resource->peerPort = 0;
        efree(resource->peerAddr);
        resource->peerAddr = NULL;
    }

    if(resource->flag & UV_TCP_READ_START){
        resource->flag &= ~UV_TCP_READ_START;
        uv_read_stop((uv_stream_t *) &resource->uv_tcp);
    }
    
    if(resource->flag & UV_TCP_HANDLE_START){
        resource->flag &= ~UV_TCP_HANDLE_START;
        uv_unref((uv_handle_t *) &resource->uv_tcp);
    }

    if(resource->flag & UV_TCP_HANDLE_INTERNAL_REF){
        resource->flag &= ~UV_TCP_HANDLE_INTERNAL_REF;
        zval_ptr_dtor(&resource->object);
    }

}

static void shutdown_cb(uv_shutdown_t* req, int status) {
    uv_tcp_ext_t *resource = (uv_tcp_ext_t *) req->handle;
    zval retval;
    zval *params[] = {resource->object, NULL};
    TSRMLS_FETCH();

    if(resource->shutdownCallback.func){
        MAKE_STD_ZVAL(params[1]);
        ZVAL_LONG(params[1], status);
    
        fci_call_function(&resource->shutdownCallback, &retval, 2, params TSRMLS_CC);
    
        zval_ptr_dtor(&params[1]);
        zval_dtor(&retval);
    }
}

static void tcp_close_cb(uv_handle_t* handle) {
    releaseResource((uv_tcp_ext_t *) handle);
}

static void write_cb(uv_write_t *wr, int status){
    zval retval;
    write_req_t *req = (write_req_t *) wr;
    uv_tcp_ext_t *resource = (uv_tcp_ext_t *) req->uv_write.handle;
    zval *params[] = {resource->object, NULL, NULL};
    TSRMLS_FETCH();

    if(resource->flag & UV_TCP_WRITE_CALLBACK_ENABLE && resource->writeCallback.func){
        MAKE_STD_ZVAL(params[1]);
        ZVAL_LONG(params[1], status);
        MAKE_STD_ZVAL(params[2]);
        ZVAL_LONG(params[2], req->buf.len);
    
        fci_call_function(&resource->writeCallback, &retval, 3, params TSRMLS_CC);
    
        zval_ptr_dtor(&params[1]);
        zval_ptr_dtor(&params[2]);
        zval_dtor(&retval);
    }
    efree(req->buf.base);
    efree(req);
}

static void alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    buf->base = emalloc(suggested_size);
    buf->len = suggested_size;
}

static void read_cb(uv_tcp_ext_t *resource, ssize_t nread, const uv_buf_t* buf) {
    zval retval;
    zval *params[] = {resource->object, NULL, NULL, NULL};
    TSRMLS_FETCH();
    
    if(nread > 0){
        if(resource->readCallback.func){
            MAKE_STD_ZVAL(params[1]);
            ZVAL_STRINGL(params[1], buf->base, nread, 1);        
            fci_call_function(&resource->readCallback, &retval, 2, params TSRMLS_CC);
            zval_ptr_dtor(&params[1]);
        }
    }
    else{    
        if(resource->errorCallback.func){
            MAKE_STD_ZVAL(params[1]);
            ZVAL_LONG(params[1], nread);        
            fci_call_function(&resource->errorCallback, &retval, 2, params TSRMLS_CC);
            zval_ptr_dtor(&params[1]);
        }
        tcp_close_socket((uv_tcp_ext_t *) &resource->uv_tcp);
    }
    efree(buf->base);
    zval_dtor(&retval);    
}

static void client_connection_cb(uv_connect_t* req, int status) {
    zval retval;
    uv_tcp_ext_t *resource = (uv_tcp_ext_t *) req->handle;
    zval *params[] = {resource->object, NULL};
    ZVAL_NULL(&retval);
    MAKE_STD_ZVAL(params[1]);
    ZVAL_LONG(params[1], status);
    TSRMLS_FETCH();

    if(uv_read_start((uv_stream_t *) resource, alloc_cb, (uv_read_cb) read_cb)){
        return;
    }
    resource->flag |= (UV_TCP_HANDLE_START|UV_TCP_READ_START);
    
    fci_call_function(&resource->connectCallback, &retval, 2, params TSRMLS_CC);
    zval_ptr_dtor(&params[1]);
    zval_dtor(&retval);
}

static void connection_cb(uv_tcp_ext_t *resource, int status) {
    zval retval;
    zval *params[] = {resource->object, NULL};
    ZVAL_NULL(&retval);
    MAKE_STD_ZVAL(params[1]);
    ZVAL_LONG(params[1], status);
    TSRMLS_FETCH();
    fci_call_function(&resource->connectCallback, &retval, 2, params TSRMLS_CC);
    zval_ptr_dtor(&params[1]);
    zval_dtor(&retval);
}


static zend_object_value createUVTcpResource(zend_class_entry *ce TSRMLS_DC) {
    zend_object_value retval;
    uv_tcp_ext_t *resource;
    resource = (uv_tcp_ext_t *) ecalloc(1, sizeof(uv_tcp_ext_t));

    zend_object_std_init(&resource->zo, ce TSRMLS_CC);
    object_properties_init(&resource->zo, ce);
    
    retval.handle = zend_objects_store_put(
        &resource->zo,
        (zend_objects_store_dtor_t) zend_objects_destroy_object,
        freeUVTcpResource,
        NULL TSRMLS_CC);

    retval.handlers = &OBJECT_HANDLER(UVTcp);
    
    initUVTcpFunctionCache(resource TSRMLS_CC);
    return retval;
}

void freeUVTcpResource(void *object TSRMLS_DC) {
    uv_tcp_ext_t *resource;
    resource = FETCH_RESOURCE(object, uv_tcp_ext_t);

    releaseResource(resource);
    releaseUVTcpFunctionCache(resource TSRMLS_CC);
    zend_object_std_dtor(&resource->zo TSRMLS_CC);
    efree(resource);
}

static inline void resolveSocket(uv_tcp_ext_t *resource){
    struct sockaddr addr;
    int addrlen;
    int ret;
    if(resource->sockPort == 0){
        addrlen = sizeof addr;
        if(ret = uv_tcp_getsockname(&resource->uv_tcp, &addr, &addrlen)){
            return;
        }
        resource->sockPort = sock_port(&addr);
        resource->sockAddr = sock_addr(&addr);
    }
}

static inline void resolvePeerSocket(uv_tcp_ext_t *resource){
    struct sockaddr addr;
    int addrlen;
    int ret;
    if(resource->peerPort == 0){
        addrlen = sizeof addr;
        if(ret = uv_tcp_getpeername(&resource->uv_tcp, &addr, &addrlen)){
            return;
        }
        resource->peerPort = sock_port(&addr);
        resource->peerAddr = sock_addr(&addr);
    }
}

PHP_METHOD(UVTcp, getSockname){
    zval *self = getThis();
    uv_tcp_ext_t *resource = FETCH_OBJECT_RESOURCE(self, uv_tcp_ext_t);
    resolveSocket(resource);
    if(resource->sockPort == 0){
        RETURN_FALSE;
    }
    RETURN_STRING(resource->sockAddr, 1);
}

PHP_METHOD(UVTcp, getSockport){
   zval *self = getThis();
   uv_tcp_ext_t *resource = FETCH_OBJECT_RESOURCE(self, uv_tcp_ext_t);
   resolveSocket(resource);
   if(resource->sockPort == 0){
       RETURN_FALSE;
   }
   RETURN_LONG(resource->sockPort);
}

PHP_METHOD(UVTcp, getPeername){
    zval *self = getThis();
    uv_tcp_ext_t *resource = FETCH_OBJECT_RESOURCE(self, uv_tcp_ext_t);
    resolvePeerSocket(resource);
    if(resource->peerPort == 0){
        RETURN_FALSE;
    }
    RETURN_STRING(resource->peerAddr, 1);
}

PHP_METHOD(UVTcp, getPeerport){
   zval *self = getThis();
   uv_tcp_ext_t *resource = FETCH_OBJECT_RESOURCE(self, uv_tcp_ext_t);
   resolvePeerSocket(resource);
   if(resource->peerPort == 0){
       RETURN_FALSE;
   }
   RETURN_LONG(resource->peerPort);
}

PHP_METHOD(UVTcp, accept){
    long ret, port;
    zval *self = getThis();
    const char *host;
    int host_len;
    char cstr_host[30];
    struct sockaddr_in addr;
    uv_tcp_ext_t *resource = FETCH_OBJECT_RESOURCE(self, uv_tcp_ext_t);
    uv_tcp_ext_t *client_resource;
    
    object_init_ex(return_value, CLASS_ENTRY(UVTcp));
    if(!make_accepted_uv_tcp_object(resource, return_value TSRMLS_CC)){
        RETURN_FALSE;
    }
    
    client_resource = FETCH_OBJECT_RESOURCE(return_value, uv_tcp_ext_t);
    if(uv_read_start((uv_stream_t *) client_resource, alloc_cb, (uv_read_cb) read_cb)){
        zval_dtor(return_value);
        RETURN_FALSE;
    }
    
}

PHP_METHOD(UVTcp, listen){
    long ret, port;
    zval *self = getThis();
    zval *onConnectCallback;
    const char *host;
    int host_len;
    char cstr_host[30];
    struct sockaddr_in addr;
    
    uv_tcp_ext_t *resource = FETCH_OBJECT_RESOURCE(self, uv_tcp_ext_t);
    
    if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "slz", &host, &host_len, &port, &onConnectCallback)) {
        return;
    }
    
    if(host_len == 0 || host_len >= 30){        
        RETURN_LONG(-1);
    }
    
    memcpy(cstr_host, host, host_len);
    cstr_host[host_len] = '\0';
    if((ret = uv_ip4_addr(cstr_host, port&0xffff, &addr)) != 0){
        RETURN_LONG(ret);
    }
    
    if((ret = uv_tcp_bind(&resource->uv_tcp, (const struct sockaddr*) &addr, 0)) != 0){
        RETURN_LONG(ret);
    }
    
    if((ret = uv_listen((uv_stream_t *) &resource->uv_tcp, SOMAXCONN, (uv_connection_cb) connection_cb)) != 0){
        RETURN_LONG(ret);
    }
    
    registerFunctionCache(&resource->connectCallback, onConnectCallback TSRMLS_CC);
    setSelfReference(resource);
    resource->flag |= UV_TCP_HANDLE_START;    
    RETURN_LONG(ret);
}

PHP_METHOD(UVTcp, setCallback){
    long ret = 0;
    zval *onReadCallback, *onWriteCallback, *onErrorCallback;
    zval *self = getThis();
    uv_tcp_ext_t *resource = FETCH_OBJECT_RESOURCE(self, uv_tcp_ext_t);

    if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zzz", &onReadCallback, &onWriteCallback, &onErrorCallback)) {
        return;
    }
    registerFunctionCache(&resource->readCallback, onReadCallback TSRMLS_CC);
    registerFunctionCache(&resource->writeCallback, onWriteCallback TSRMLS_CC);
    registerFunctionCache(&resource->errorCallback, onErrorCallback TSRMLS_CC);
    setSelfReference(resource);
    RETURN_LONG(ret);
}

PHP_METHOD(UVTcp, close){
    long ret;
    zval *self = getThis();
    uv_tcp_ext_t *resource = FETCH_OBJECT_RESOURCE(self, uv_tcp_ext_t);
    
    tcp_close_socket((uv_tcp_ext_t *) &resource->uv_tcp);
}

PHP_METHOD(UVTcp, __construct){
    zval *loop = NULL;
    zval *self = getThis();
    uv_tcp_ext_t *resource = FETCH_OBJECT_RESOURCE(self, uv_tcp_ext_t);
   
    if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z", &loop)) {
        return;
    }
    
    resource->object = self;
    
    if(NULL == loop || ZVAL_IS_NULL(loop)){
        uv_tcp_init(uv_default_loop(), (uv_tcp_t *) resource);
        return;
    }
    
    zend_update_property(CLASS_ENTRY(UVTcp), self, ZEND_STRL("loop"), loop TSRMLS_CC);
    uv_tcp_init(FETCH_UV_LOOP(), (uv_tcp_t *) resource);
}

PHP_METHOD(UVTcp, write){
    long ret;
    zval *self = getThis();
    char *buf;
    int buf_len;
    uv_tcp_ext_t *resource = FETCH_OBJECT_RESOURCE(self, uv_tcp_ext_t);

    if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &buf, &buf_len)) {
        return;
    }
    resource->flag |= UV_TCP_WRITE_CALLBACK_ENABLE;
    ret = tcp_write_raw((uv_stream_t *) &resource->uv_tcp, buf, buf_len);
    RETURN_LONG(ret);
}

int tcp_write_raw(uv_stream_t * handle, char *message, int size) {
    write_req_t *req;
    req = emalloc(sizeof(write_req_t));
    req->buf.base = emalloc(size);
    req->buf.len = size;
    memcpy(req->buf.base, message, size);
    return uv_write((uv_write_t *) req, handle, &req->buf, 1, write_cb);
}

PHP_METHOD(UVTcp, shutdown){
    long ret;
    zval *onShutdownCallback;
    zval *self = getThis();
    char *buf;
    int buf_len;
    uv_tcp_ext_t *resource = FETCH_OBJECT_RESOURCE(self, uv_tcp_ext_t);

    if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &onShutdownCallback)) {
        return;
    }
    
    if((ret = uv_shutdown(&resource->shutdown_req, (uv_stream_t *) &resource->uv_tcp, shutdown_cb)) == 0){
        registerFunctionCache(&resource->shutdownCallback, onShutdownCallback TSRMLS_CC);
        setSelfReference(resource);
    }
    
    RETURN_LONG(ret);
}

PHP_METHOD(UVTcp, connect){
    long ret, port;
    zval *self = getThis();
    zval *onConnectCallback;
    const char *host;
    int host_len;
    char cstr_host[30];
    struct sockaddr_in addr;
    
    uv_tcp_ext_t *resource = FETCH_OBJECT_RESOURCE(self, uv_tcp_ext_t);

    if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "slz", &host, &host_len, &port, &onConnectCallback)) {
        return;
    }
    
    if(host_len == 0 || host_len >= 30){        
        RETURN_LONG(-1);
    }

    memcpy(cstr_host, host, host_len);
    cstr_host[host_len] = '\0';
    if((ret = uv_ip4_addr(cstr_host, port&0xffff, &addr)) != 0){
        RETURN_LONG(ret);
    }
    
    if((ret = uv_tcp_connect(&resource->connect_req, &resource->uv_tcp, (const struct sockaddr *) &addr, client_connection_cb)) != 0){
        RETURN_LONG(ret);
    }
    
    registerFunctionCache(&resource->connectCallback, onConnectCallback TSRMLS_CC);
    setSelfReference(resource);
    resource->flag |= UV_TCP_HANDLE_START;    
    RETURN_LONG(ret);
}

zend_bool make_accepted_uv_tcp_object(uv_tcp_ext_t *server_resource, zval *client TSRMLS_DC){
    uv_tcp_ext_t *client_resource;
    client_resource = FETCH_OBJECT_RESOURCE(client, uv_tcp_ext_t);
    zval *loop = zend_read_property(CLASS_ENTRY(UVTcp), server_resource->object, ZEND_STRL("loop"), 0 TSRMLS_CC);
    zend_update_property(CLASS_ENTRY(UVTcp), client, ZEND_STRL("loop"), loop TSRMLS_CC);
    uv_tcp_init(ZVAL_IS_NULL(loop)?uv_default_loop():FETCH_UV_LOOP(), (uv_tcp_t *) client_resource);    
    
    if(uv_accept((uv_stream_t *) &server_resource->uv_tcp, (uv_stream_t *) &client_resource->uv_tcp)) {
        zval_dtor(client);
        return 0;
    }
    
    client_resource->flag |= (UV_TCP_HANDLE_START|UV_TCP_READ_START);
    client_resource->object = client;
    return 1;
}
