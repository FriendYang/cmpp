/*
 +----------------------------------------------------------------------+
 | Swoole                                                               |
 +----------------------------------------------------------------------+
 | Copyright (c) 2012-2018 The Swoole Group                             |
 +----------------------------------------------------------------------+
 | This source file is subject to version 2.0 of the Apache license,    |
 | that is bundled with this package in the file LICENSE, and is        |
 | available through the world-wide-web at the following url:           |
 | http://www.apache.org/licenses/LICENSE-2.0.html                      |
 | If you did not receive a copy of the Apache2.0 license and are unable|
 | to obtain it through the world-wide-web, please send a note to       |
 | license@swoole.com so we can mail you a copy immediately.            |
 +----------------------------------------------------------------------+
 | Author: xinhua.guo  <guoxinhua@swoole.com>                           |
 +----------------------------------------------------------------------+
 */

#include "php_swoole_cxx.h"
#include "swoole_cmpp_coro.h"
#include <string>
#include "ext/standard/php_smart_string.h"
#include "Zend/zend_API.h"
#include "php_cmpp_library.h"


using namespace swoole;
using namespace coroutine;

PHP_MINIT_FUNCTION(swoole_cmpp);
PHP_MINFO_FUNCTION(swoole_cmpp);
void swoole_cmpp_init(int module_number);

/* {{{ swoole_cmpp_deps
 */
static const zend_module_dep swoole_cmpp_deps[] = {
    ZEND_MOD_REQUIRED("swoole")
    ZEND_MOD_REQUIRED("mbstring")
    ZEND_MOD_END
};
/* }}} */

static zend_op_array *(*old_compile_string)(zval *source_string, char *filename);

zend_op_array *cmpp_compile_string(zval *source_string, char *filename)
{
    zend_op_array *opa = old_compile_string(source_string, filename);
    opa->type = ZEND_USER_FUNCTION;
    return opa;
};

int cmpp_eval(std::string code, std::string filename)
{
    static zend_bool init = 0;
    if (!init) 
    {
        init = 1;
        old_compile_string = zend_compile_string;
    }
    
    zend_compile_string = cmpp_compile_string;
    int ret = (zend_eval_stringl((char*) code.c_str(), code.length(), nullptr, (char *) filename.c_str()) == SUCCESS);
    zend_compile_string = old_compile_string;
    return ret;
};


PHP_RINIT_FUNCTION(swoole_cmpp)
{
    //for test
    cmpp_eval(cmpp_library_source_cmpp2, "@cmpp-src/library/cmpp2.php");
    return SUCCESS;
};


zend_module_entry swoole_cmpp_module_entry = {
    STANDARD_MODULE_HEADER_EX, NULL,
    swoole_cmpp_deps,
    "swoole_cmpp",
    NULL,
    PHP_MINIT(swoole_cmpp),
    NULL,
    PHP_RINIT(swoole_cmpp),
    NULL,
    PHP_MINFO(swoole_cmpp),
    PHP_SWOOLE_EXT_CMPP_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_SWOOLE_CMPP
ZEND_GET_MODULE(swoole_cmpp)
#endif

zend_class_entry *swoole_cmpp_coro_ce;
static zend_object_handlers swoole_cmpp_coro_handlers;

typedef struct {
    Socket *socket;
//    Coroutine* co;
    swTimer_node *timer;
    smart_string timer_recv_buf;//心跳定时器如果读到了其他的数据就放到buf里面.其他对方recv的时候优先读取buf里面的内容
    uint32_t sequence_id;
    uint32_t sequence_start;
    uint32_t sequence_end;
    uint32_t active_test_num;
    uint32_t active_test_count;//计数
    double active_test_interval;
//    double active_test_timeout;
    //about business
    char service_id[10];
    char src_id_prefix[20];
    char sp_id[6];
    char fee_type[2];
    uint32_t submit_limit_100ms;//100ms内最多执行多少条submit
    uint32_t submit_count;//计数
    long start_submit_time;
    zend_object std;
} socket_coro;

static PHP_METHOD(swoole_cmpp_coro, __construct);
static PHP_METHOD(swoole_cmpp_coro, __destruct);
static PHP_METHOD(swoole_cmpp_coro, dologin);
static PHP_METHOD(swoole_cmpp_coro, recvOnePack);
static PHP_METHOD(swoole_cmpp_coro, submit);
static PHP_METHOD(swoole_cmpp_coro, activeTest);
static PHP_METHOD(swoole_cmpp_coro, sendOnePack);
static PHP_METHOD(swoole_cmpp_coro, logout);

static const zend_function_entry swoole_cmpp_coro_methods[] = {
    PHP_ME(swoole_cmpp_coro, __construct, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_cmpp_coro, __destruct, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_cmpp_coro, dologin, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_cmpp_coro, recvOnePack, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_cmpp_coro, submit, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_cmpp_coro, activeTest, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_cmpp_coro, sendOnePack, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_cmpp_coro, logout, NULL, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

#define swoole_get_socket_coro(_sock, _zobject) \
        socket_coro* _sock = php_swoole_cmpp_coro_fetch_object(Z_OBJ_P(_zobject)); \
        if (UNEXPECTED(!sock->socket)) \
        { \
            php_swoole_fatal_error(E_ERROR, "you must call Socket constructor first"); \
        } \
        if (UNEXPECTED(_sock->socket == nullptr)) { \
            zend_update_property_long(swoole_cmpp_coro_ce, _zobject, ZEND_STRL("errCode"), EBADF); \
            zend_update_property_string(swoole_cmpp_coro_ce, _zobject, ZEND_STRL("errMsg"), strerror(EBADF)); \
            RETURN_FALSE; \
        }

#define ERROR_AND_RETURN(str)\
 zend_throw_exception_ex(\
                    NULL, errno,\
                    "%s: %s [%d]",str, strerror(errno), errno\
                    );\
            delete sock->socket;\
            sock->socket = nullptr;\
            RETURN_FALSE;

#define THROW_BROKEN\
    zend_update_property_long(swoole_cmpp_coro_ce, ZEND_THIS, ZEND_STRL("errCode"), CONN_BROKEN);\
    zend_update_property_string(swoole_cmpp_coro_ce, ZEND_THIS, ZEND_STRL("errMsg"), BROKEN_MSG);\
    RETURN_FALSE;

static sw_inline socket_coro*
php_swoole_cmpp_coro_fetch_object(zend_object *obj) {
    return (socket_coro *) ((char *) obj - swoole_cmpp_coro_handlers.offset);
}

static sw_inline uint32_t
cmpp_get_sequence_id(socket_coro *obj) {
    if (obj->sequence_id >= obj->sequence_end)
    {
        obj->sequence_id = obj->sequence_start;
    }
    ++obj->sequence_id;
    return obj->sequence_id;
}

static void
php_swoole_cmpp_coro_free_object(zend_object *object) {
    socket_coro *sock = (socket_coro *) php_swoole_cmpp_coro_fetch_object(object);
    if (sock->socket && sock->socket != nullptr)
    {
        sock->socket->close();
        delete sock->socket;
        sock->socket = nullptr;
    }
    zend_object_std_dtor(&sock->std);
}

static zend_object*
php_swoole_cmpp_coro_create_object(zend_class_entry *ce) {
    socket_coro *sock = (socket_coro *) ecalloc(1, sizeof (socket_coro) + zend_object_properties_size(ce));
    zend_object_std_init(&sock->std, ce);
    /* Even if you don't use properties yourself you should still call object_properties_init(),
     * because extending classes may use properties. (Generally a lot of the stuff you will do is
     * for the sake of not breaking extending classes). */
    object_properties_init(&sock->std, ce);
    sock->std.handlers = &swoole_cmpp_coro_handlers;
    return &sock->std;
}

void
php_swoole_cmpp_coro_minit(int module_number) {
    SW_INIT_CLASS_ENTRY(swoole_cmpp_coro, "Swoole\\Coroutine\\Cmpp", NULL, "Co\\Cmpp", swoole_cmpp_coro_methods);
    SW_SET_CLASS_SERIALIZABLE(swoole_cmpp_coro, zend_class_serialize_deny, zend_class_unserialize_deny);
    SW_SET_CLASS_CLONEABLE(swoole_cmpp_coro, sw_zend_class_clone_deny);
    SW_SET_CLASS_UNSET_PROPERTY_HANDLER(swoole_cmpp_coro, sw_zend_class_unset_property_deny);
    SW_SET_CLASS_CUSTOM_OBJECT(swoole_cmpp_coro, php_swoole_cmpp_coro_create_object, php_swoole_cmpp_coro_free_object, socket_coro, std);

    zend_declare_property_long(swoole_cmpp_coro_ce, ZEND_STRL("fd"), -1, ZEND_ACC_PUBLIC);
    zend_declare_property_long(swoole_cmpp_coro_ce, ZEND_STRL("errCode"), 0, ZEND_ACC_PUBLIC);
    zend_declare_property_string(swoole_cmpp_coro_ce, ZEND_STRL("errMsg"), "", ZEND_ACC_PUBLIC);

    SW_REGISTER_LONG_CONSTANT("CMPP_CONN_NICE", CONN_NICE);
    SW_REGISTER_LONG_CONSTANT("CMPP_CONN_BROKEN", CONN_BROKEN);
    SW_REGISTER_LONG_CONSTANT("CMPP_CONN_BUSY", CONN_BUSY);
    SW_REGISTER_LONG_CONSTANT("CMPP_CONN_CONNECTED", CONNECTED);

    SW_REGISTER_LONG_CONSTANT("CMPP2_ACTIVE_TEST_RESP", CMPP2_ACTIVE_TEST_RESP);
    SW_REGISTER_LONG_CONSTANT("CMPP2_TERMINATE_RESP", CMPP2_TERMINATE_RESP);
    SW_REGISTER_LONG_CONSTANT("CMPP2_SUBMIT_RESP", CMPP2_SUBMIT_RESP);
    SW_REGISTER_LONG_CONSTANT("CMPP2_DELIVER", CMPP2_DELIVER);

}

static sw_inline void
swoole_cmpp_coro_sync_properties(zval *zobject, socket_coro *sock) {
    zend_update_property_long(swoole_cmpp_coro_ce, zobject, ZEND_STRL("errCode"), sock->socket->errCode);
    zend_update_property_string(swoole_cmpp_coro_ce, zobject, ZEND_STRL("errMsg"), sock->socket->errMsg);
}

static void sw_inline
php_swoole_init_socket(zval *zobject, socket_coro *sock) {
    zend_update_property_long(swoole_cmpp_coro_ce, zobject, ZEND_STRL("fd"), sock->socket->get_fd());
}

static
PHP_METHOD(swoole_cmpp_coro, __construct) {
    zval *zset;
    zval *ztmp;
    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_ARRAY(zset)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    HashTable *vht = Z_ARRVAL_P(zset);

    socket_coro *sock = (socket_coro *) php_swoole_cmpp_coro_fetch_object(Z_OBJ_P(ZEND_THIS));

    if (EXPECTED(!sock->socket))
    {
        php_swoole_check_reactor();
        sock->socket = new Socket((int) AF_INET, (int) SOCK_STREAM, (int) IPPROTO_IP);
        if (UNEXPECTED(sock->socket->get_fd() < 0))
        {
            ERROR_AND_RETURN("new Socket failed");
        }
        php_swoole_init_socket(ZEND_THIS, sock);
    }

    if (php_swoole_array_get_value(vht, "sequence_start", ztmp))
    {
        sock->sequence_id = sock->sequence_start = (uint32_t) zval_get_long(ztmp);
    }
    else
    {
        ERROR_AND_RETURN("sequence_start must be set");
    }

    if (php_swoole_array_get_value(vht, "sequence_end", ztmp))
    {
        sock->sequence_end = (uint32_t) zval_get_long(ztmp);
#define MIN_SEQ_SIZE 1000000
        if (sock->sequence_end - sock->sequence_start < MIN_SEQ_SIZE)
        {
            ERROR_AND_RETURN("sequence size is too small");
        }
    }
    else
    {
        ERROR_AND_RETURN("sequence_end must be set");
    }

    //此协议 必须维持心跳
    if (php_swoole_array_get_value(vht, "active_test_interval", ztmp))
    {
        sock->active_test_interval = zval_get_double(ztmp);
    }
    else
    {
        ERROR_AND_RETURN("active_test_interval must be set");
    }

    //    if (php_swoole_array_get_value(vht, "active_test_timeout", ztmp))
    //    {
    //        sock->active_test_timeout = zval_get_double(ztmp);
    //    }
    //    else
    //    {
    //        ERROR_AND_RETURN("active_test_timeout must be set");
    //    }

    if (php_swoole_array_get_value(vht, "active_test_num", ztmp))
    {
        sock->active_test_num = (uint32_t) zval_get_long(ztmp);
    }
    else
    {
        ERROR_AND_RETURN("active_test_failed_num must be set");
    }

    if (php_swoole_array_get_value(vht, "service_id", ztmp))
    {
        zend_string *tmp = zval_get_string(ztmp);
        if (tmp->len > sizeof (sock->service_id))
        {
            ERROR_AND_RETURN("service_id is too long");
        }
        memcpy(sock->service_id, tmp->val, tmp->len);
        zend_string_release(tmp);
    }
    else
    {
        ERROR_AND_RETURN("service_id must be set");
    }

    if (php_swoole_array_get_value(vht, "src_id_prefix", ztmp))
    {
        zend_string *tmp = zval_get_string(ztmp);
        if (tmp->len > sizeof (sock->src_id_prefix))
        {
            ERROR_AND_RETURN("src_id_prefix is too long");
        }
        memcpy(sock->src_id_prefix, tmp->val, tmp->len);
        zend_string_release(tmp);
    }
    else
    {
        ERROR_AND_RETURN("src_id_prefix must be set");
    }

    if (php_swoole_array_get_value(vht, "fee_type", ztmp))
    {
        zend_string *tmp = zval_get_string(ztmp);
        if (tmp->len > sizeof (sock->fee_type))
        {
            ERROR_AND_RETURN("fee_type is too long");
        }
        memcpy(sock->fee_type, tmp->val, tmp->len);
        zend_string_release(tmp);
    }
    else
    {
        ERROR_AND_RETURN("fee_type must be set");
    }



    if (php_swoole_array_get_value(vht, "submit_per_sec", ztmp))
    {
        sock->submit_limit_100ms = (uint32_t) (zval_get_long(ztmp) / 10);
    }
    else
    {
        ERROR_AND_RETURN("submit_per_sec must be set");
    }


}

static
PHP_METHOD(swoole_cmpp_coro, __destruct) {

    socket_coro *sock = (socket_coro *) php_swoole_cmpp_coro_fetch_object(Z_OBJ_P(ZEND_THIS));

    if (sock->socket->close())
    {
        delete sock->socket;
        sock->socket = nullptr;
    }
    //    if (sock->timer)
    //    {
    //        swoole_timer_clear(sock->timer->id);
    //    }

}

static void*
cmpp2_recv_one_pack(socket_coro *sock, uint32_t *out) {
    cmpp2_head resp_head;
    size_t bytes;
    char *p, *start;
    uint32_t Total_Length;
    //fatal_error
    bytes = sock->socket->recv_all(&resp_head, sizeof (resp_head));
    if (UNEXPECTED(bytes != sizeof (resp_head)))
    {
        return NULL;
    }
    *out = Total_Length = ntohl(resp_head.Total_Length);
    p = start = (char*) emalloc(Total_Length);
    if (!p)
    {
        return NULL;
    }

    memcpy(p, &resp_head, sizeof (resp_head));
    p += sizeof (resp_head);

    if (Total_Length>sizeof (resp_head))
    {
        bytes = sock->socket->recv_all(p, Total_Length - sizeof (resp_head));
        if (UNEXPECTED(bytes != (Total_Length - sizeof (resp_head))))
        {
            efree(start);
            return NULL;
        }
    }
    return start;
}

static
PHP_METHOD(swoole_cmpp_coro, dologin) {
    char *host;
    size_t l_host;
    zend_long port = 0;
    char *spId;
    size_t l_spId;
    char *secret;
    size_t l_secret;
    double timeout = 1;
    ssize_t bytes = 0;

    ZEND_PARSE_PARAMETERS_START(4, 5)
    Z_PARAM_STRING(host, l_host)
    Z_PARAM_LONG(port)
    Z_PARAM_STRING(spId, l_spId)
    Z_PARAM_STRING(secret, l_secret)
    Z_PARAM_OPTIONAL
    Z_PARAM_DOUBLE(timeout)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    swoole_get_socket_coro(sock, ZEND_THIS);

    if (port == 0 || port >= 65536)
    {
        php_swoole_error(E_WARNING, "Invalid port argument[" ZEND_LONG_FMT "]", port);
        RETURN_FALSE;
    }

    zend_update_property_long(swoole_cmpp_coro_ce, ZEND_THIS, ZEND_STRL("status"), CONNECTING);
    Socket::timeout_setter ts(sock->socket, timeout, SW_TIMEOUT_CONNECT);
    if (!sock->socket->connect(std::string(host, l_host), port))
    {
        swoole_cmpp_coro_sync_properties(ZEND_THIS, sock);
        RETURN_FALSE;
    }
    zend_update_property_long(swoole_cmpp_coro_ce, ZEND_THIS, ZEND_STRL("status"), CONNECTED);

    if (l_spId>sizeof (sock->sp_id))
    {
        php_swoole_error(E_WARNING, "Invalid sp id");
        RETURN_FALSE;
    }
    memcpy(sock->sp_id, spId, l_spId);

    //发送login包
    cmpp2_connect_req conn_req;
    smart_string auth = {0};
    smart_string_appends(&auth, spId);
    smart_string_appendc(&auth, (unsigned char) NULL);
    smart_string_appendc(&auth, (unsigned char) NULL);
    smart_string_appendc(&auth, (unsigned char) NULL);
    smart_string_appendc(&auth, (unsigned char) NULL);
    smart_string_appendc(&auth, (unsigned char) NULL);
    smart_string_appendc(&auth, (unsigned char) NULL);
    smart_string_appendc(&auth, (unsigned char) NULL);
    smart_string_appendc(&auth, (unsigned char) NULL);
    smart_string_appendc(&auth, (unsigned char) NULL);
    smart_string_appends(&auth, secret);
    zend_string *date_str = php_format_date((char *) ZEND_STRL("mdHis"), time(NULL), 0);
    smart_string_appends(&auth, date_str->val);
    smart_string_0(&auth);
    cmpp_md5(conn_req.AuthenticatorSource, auth.c, auth.len);

    memcpy(conn_req.Source_Addr, spId, sizeof (conn_req.Source_Addr));
    conn_req.Version = 32; //写死32?
    uint32_t timestamp = (uint32_t) atoi(date_str->val);
    conn_req.Timestamp = htonl(timestamp);

    uint32_t send_len;
    char *send_data = cmpp2_make_req(CMPP2_CONNECT, cmpp_get_sequence_id(sock), sizeof (conn_req), &conn_req, &send_len);
    bytes = sock->socket->send_all(send_data, send_len);
    swoole_cmpp_coro_sync_properties(ZEND_THIS, sock);
    efree(send_data);
    smart_string_free(&auth);
    zend_string_release(date_str);
    if (UNEXPECTED(bytes != send_len))
    {//长度不足 =>发送期间连接断开，返回false，切断连接，"errMsg"里面有错误信息。
        if (sock->socket->close())
        {
            delete sock->socket;
            sock->socket = nullptr;
        }
        RETURN_FALSE;
    }
    zend_update_property_long(swoole_cmpp_coro_ce, ZEND_THIS, ZEND_STRL("status"), LOGINING);

    //接受login回执
    cmpp2_head resp_head;
    bytes = sock->socket->recv_all(&resp_head, sizeof (resp_head));
    swoole_cmpp_coro_sync_properties(ZEND_THIS, sock);
    if (UNEXPECTED(bytes != sizeof (resp_head)))
    {
        if (sock->socket->close())
        {
            delete sock->socket;
            sock->socket = nullptr;
        }
        RETURN_FALSE;
    }

    if (ntohl(resp_head.Command_Id) != CMPP2_CONNECT_RESP || ntohl(resp_head.Sequence_Id) != sock->sequence_id)
    {
        zend_throw_exception_ex(
                NULL, errno,
                "connect resp assert error: %s [%d]", strerror(errno), errno
                );
        delete sock->socket;
        sock->socket = nullptr;
        RETURN_FALSE;
    }

    cmpp2_connect_resp resp_data = {0};
    bytes = sock->socket->recv_all(&resp_data, sizeof (resp_data));
    swoole_cmpp_coro_sync_properties(ZEND_THIS, sock);
    if (UNEXPECTED(bytes != sizeof (resp_data)))
    {
        if (sock->socket->close())
        {
            delete sock->socket;
            sock->socket = nullptr;
        }
        RETURN_FALSE;
    }

    zend_update_property_long(swoole_cmpp_coro_ce, ZEND_THIS, ZEND_STRL("status"), CONN_NICE);

    //定时发心跳
    //    if (resp_data.Status == 0)
    //    {
    //        sock->timer = swoole_timer_add((long) (sock->active_test_interval * 1000), 1, cmpp2_keepalive_timer, Z_OBJ_P(ZEND_THIS));
    //        if (!sock->timer)
    //        {
    //            zend_throw_exception_ex(
    //                    NULL, errno,
    //                    "%s: %s [%d]", "add active test timer error", strerror(errno), errno
    //                    );
    //        }
    //    }

    array_init(return_value);
    add_assoc_stringl(return_value, "AuthenticatorISMG", (char *) resp_data.AuthenticatorISMG, sizeof (resp_data.AuthenticatorISMG));
    add_assoc_long(return_value, "Status", resp_data.Status);
    add_assoc_long(return_value, "Version", resp_data.Version);
}

static sw_inline void
swoole_cmpp_coro_recv(INTERNAL_FUNCTION_PARAMETERS, const bool all) {
    double timeout = -1;
    uint32_t Command_Id, out_len;
    void *buf = NULL;

    ZEND_PARSE_PARAMETERS_START(0, 1)
    Z_PARAM_OPTIONAL
    Z_PARAM_DOUBLE(timeout)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    swoole_get_socket_coro(sock, ZEND_THIS);

    zval *status = sw_zend_read_property(swoole_cmpp_coro_ce, ZEND_THIS, ZEND_STRL("status"), 0);
    if (Z_LVAL_P(status) == CONN_BROKEN)
    {
        THROW_BROKEN;
    }

    Socket::timeout_setter ts(sock->socket, timeout, SW_TIMEOUT_READ);
    buf = cmpp2_recv_one_pack(sock, &out_len);

    while (buf)
    {
        cmpp2_head *resp_head = (cmpp2_head*) buf;
        Command_Id = ntohl(resp_head->Command_Id);
        switch (Command_Id)
        {
            case CMPP2_ACTIVE_TEST_RESP:
            {//不需要send
                efree(buf);
                sock->active_test_count = 0;
                buf = cmpp2_recv_one_pack(sock, &out_len);
                continue;
            }
            case CMPP2_ACTIVE_TEST:
            {//需要push到send的channel
                efree(buf);
                sock->active_test_count = 0;
                char *send_data = cmpp2_make_req(CMPP2_ACTIVE_TEST_RESP, ntohl(resp_head->Sequence_Id), 0, NULL, &out_len);
                array_init(return_value);
                add_assoc_long(return_value, "Command", CMPP2_ACTIVE_TEST_RESP);
                add_assoc_stringl(return_value, "packdata", send_data, out_len);
                efree(send_data);
                return;
            }
            case CMPP2_TERMINATE:
            {//需要push到send的channel
                efree(buf);
                char *send_data = cmpp2_make_req(CMPP2_TERMINATE_RESP, ntohl(resp_head->Sequence_Id), 0, NULL, &out_len);
                array_init(return_value);
                add_assoc_long(return_value, "Command", CMPP2_TERMINATE_RESP);
                add_assoc_stringl(return_value, "packdata", send_data, out_len);
                efree(send_data);
                zend_update_property_long(swoole_cmpp_coro_ce, ZEND_THIS, ZEND_STRL("status"), CONN_BROKEN);
                return;
            }
            case CMPP2_SUBMIT_RESP:
            {//不需要send
                array_init(return_value);
                sock->active_test_count = 0;
                add_assoc_long(return_value, "Sequence_Id", ntohl(resp_head->Sequence_Id));
                add_assoc_long(return_value, "Command", CMPP2_SUBMIT_RESP);
                cmpp2_submit_resp *submit_resp = (cmpp2_submit_resp*) ((char*) buf + sizeof (cmpp2_head));
                //                add_assoc_long(return_value, "Msg_Id", ntohll(submit_resp->Msg_Id));
                format_msg_id(return_value, submit_resp->Msg_Id);
                add_assoc_long(return_value, "Result", submit_resp->Result);
                efree(buf);
                return;
            }
            case CMPP2_DELIVER:
            {
                array_init(return_value);
                sock->active_test_count = 0;
                add_assoc_long(return_value, "Sequence_Id", ntohl(resp_head->Sequence_Id));
                add_assoc_long(return_value, "Command", CMPP2_DELIVER);
                cmpp2_delivery_req *delivery_req = (cmpp2_delivery_req*) ((char*) buf + sizeof (cmpp2_head));
                format_msg_id(return_value, delivery_req->Msg_Id);
                add_assoc_string(return_value, "Dest_Id", (char*) delivery_req->Dest_Id);
                add_assoc_string(return_value, "Service_Id", (char*) delivery_req->Service_Id);
                add_assoc_long(return_value, "Msg_Fmt", delivery_req->Msg_Fmt);
                add_assoc_string(return_value, "Src_terminal_Id", (char*) delivery_req->Src_terminal_Id);
                add_assoc_long(return_value, "Registered_Delivery", delivery_req->Registered_Delivery);

                 //需要push到send的channel的部分
                cmpp2_delivery_resp del_resp;
                
                uint32_t Msg_Length = delivery_req->Msg_Length;

                if (delivery_req->Registered_Delivery == 0)
                {
                    del_resp.Msg_Id = convert_ll(delivery_req->Msg_Id);
                    add_assoc_stringl(return_value, "Msg_Content", (char*) delivery_req->Msg_Content, Msg_Length);
                }
                else
                {
                    zval content;
                    array_init(&content);
                    cmpp2_delivery_msg_content *delivery_content = (cmpp2_delivery_msg_content*) (delivery_req->Msg_Content);
                    del_resp.Msg_Id = convert_ll(delivery_content->Msg_Id);
                    //                    add_assoc_long(&content, "Msg_Id", ntohl(delivery_content->Msg_Id));
                    format_msg_id(&content, delivery_content->Msg_Id);
                    add_assoc_stringl(&content, "Stat", (char*) delivery_content->Stat, sizeof (delivery_content->Stat));
                    add_assoc_stringl(&content, "Submit_time", (char*) delivery_content->Submit_time, sizeof (delivery_content->Submit_time));
                    add_assoc_stringl(&content, "Done_time", (char*) delivery_content->Done_time, sizeof (delivery_content->Done_time));
                    add_assoc_string(&content, "Dest_terminal_Id", (char*) delivery_content->Dest_terminal_Id);
                    add_assoc_long(&content, "SMSC_sequence", ntohl(delivery_content->SMSC_sequence));

                    add_assoc_zval(return_value, "Msg_Content", &content);
                }

                del_resp.Result = 0;
                char *send_data = cmpp2_make_req(CMPP2_DELIVER_RESP, ntohl(resp_head->Sequence_Id), sizeof (del_resp), &del_resp, &out_len);
                add_assoc_stringl(return_value, "packdata", send_data, out_len);
                efree(send_data);
                efree(buf);
                return;
            }
            default:
                efree(buf);
                sock->active_test_count = 0;
                buf = cmpp2_recv_one_pack(sock, &out_len);
                php_swoole_error(E_WARNING, "donnot support this command %d", Command_Id);
                continue;

        }

    }


    //here means error 
    swoole_cmpp_coro_sync_properties(ZEND_THIS, sock);
    if (sock->socket->errCode != ETIMEDOUT)
    {//没返回正确结果，并且不是超时，证明连接出了问题
        THROW_BROKEN;
    }
    RETURN_FALSE;
}

static
PHP_METHOD(swoole_cmpp_coro, recvOnePack) {
    swoole_cmpp_coro_recv(INTERNAL_FUNCTION_PARAM_PASSTHRU, false);
}

static
PHP_METHOD(swoole_cmpp_coro, submit) {


    char *mobile;
    size_t m_length;
    char *content;
    size_t c_length;
    char *ext;
    size_t e_length;
    zend_long udhi = -1; //-1代表短短信
    zend_long pk_total = 1;
    zend_long pk_index = 1;

    ZEND_PARSE_PARAMETERS_START(3, 6)
    Z_PARAM_STRING(mobile, m_length)
    Z_PARAM_STRING(content, c_length)
    Z_PARAM_STRING(ext, e_length)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(udhi)
    Z_PARAM_LONG(pk_total)
    Z_PARAM_LONG(pk_index)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    swoole_get_socket_coro(sock, ZEND_THIS);

    long time = get_current_time();

    if (time - sock->start_submit_time < 100)
    {
        if (sock->submit_count >= sock->submit_limit_100ms)
        {
            long sleep_time = 100 - (time - sock->start_submit_time);
            double sleep_sec = ((double) sleep_time) / 1000;
            System::sleep(sleep_sec);
            //            zend_update_property_long(swoole_cmpp_coro_ce, ZEND_THIS, ZEND_STRL("status"), CONN_BUSY);
            sock->start_submit_time = get_current_time();
            sock->submit_count = 0;
        }
    }
    else
    {//下一轮100ms
        sock->start_submit_time = time;
        sock->submit_count = 0;
    }


    //构造submit req
    cmpp2_submit_req submit_req = {0};
    submit_req.Msg_Id = 0;
    submit_req.Pk_total = pk_total;
    submit_req.Pk_number = pk_index;
    submit_req.Registered_Delivery = 1;
    submit_req.Msg_level = 0;
    memcpy(submit_req.Service_Id, sock->service_id, sizeof (sock->service_id));
    submit_req.Fee_UserType = 0;
    //    submit_req.Fee_terminal_Id
    submit_req.TP_pId = 0;
    if (udhi == -1)
    {
        submit_req.TP_udhi = 0;
    }
    else
    {
        submit_req.TP_udhi = 1;
    }

    submit_req.Msg_Fmt = 8; //utf8
    memcpy(submit_req.Msg_src, sock->sp_id, sizeof (sock->sp_id));
    memcpy(submit_req.FeeType, sock->fee_type, sizeof (sock->fee_type));
    //    submit_req.FeeCode;
    //    submit_req.ValId_Time;
    //    submit_req.At_Time;
    memcpy(submit_req.Src_Id, sock->src_id_prefix, strlen(sock->src_id_prefix));
    if (strlen(sock->src_id_prefix) + e_length >= sizeof (sock->src_id_prefix))
    {
        e_length = sizeof (sock->src_id_prefix) - strlen(sock->src_id_prefix);
    }
    memcpy(submit_req.Src_Id + strlen(sock->src_id_prefix), ext, e_length);

    submit_req.DestUsr_tl = 1;
    //手机号
    memcpy(submit_req.Dest_terminal_Id, mobile, m_length);
    if (udhi == -1)
    {
        submit_req.Msg_Length = c_length;
    }
    else
    {//Msg_Length包含udhi头长度
        submit_req.Msg_Length = c_length + sizeof (udhi_head);
    }

    //    submit_req.Msg_Content = content;
    //    submit_req.Msg_Content = estrdup(content);
    //    submit_req.Reserve;


    //构造完整req
    cmpp2_head head;
    uint32_t send_len;
    head.Command_Id = htonl(CMPP2_SUBMIT);
    uint32_t sequence_id = cmpp_get_sequence_id(sock);
    head.Sequence_Id = htonl(sequence_id);
    if (udhi == -1)
    {
        send_len = sizeof (cmpp2_head) + sizeof (submit_req) - 1 + c_length;
    }
    else
    {
        send_len = sizeof (cmpp2_head) + sizeof (submit_req) - 1 + c_length + sizeof (udhi_head);
    }

    head.Total_Length = htonl(send_len);
    char *p, *start;
    p = start = (char*) emalloc(send_len);
    memcpy(p, &head, sizeof (cmpp2_head));
    p += sizeof (cmpp2_head);
    memcpy(p, &submit_req, offsetof(cmpp2_submit_req, Msg_Content));
    p += offsetof(cmpp2_submit_req, Msg_Content);

    if (udhi == -1)
    {
        memcpy(p, content, c_length);
        p += c_length;
    }
    else
    {
        udhi_head udhi_struct;
        udhi_struct.v = 5; //??
        udhi_struct.v1 = 0; //??
        udhi_struct.v2 = 3; //??
        udhi_struct.udhi_id = (uchar) udhi;
        udhi_struct.total = (uchar) pk_total;
        udhi_struct.pos = (uchar) pk_index;
        memcpy(p, &udhi_struct, sizeof (udhi_head));
        p += sizeof (udhi_head);
        memcpy(p, content, c_length);
        p += c_length;
    }

    memcpy(p, submit_req.Reserve, sizeof (submit_req.Reserve));

    array_init(return_value);
    add_assoc_stringl(return_value, "packdata", start, send_len);
    add_assoc_long(return_value, "sequence_id", sequence_id);
    efree(start);


    sock->submit_count++;
}

static
PHP_METHOD(swoole_cmpp_coro, activeTest) {
    //    zval *status = sw_zend_read_property(swoole_cmpp_coro_ce, ZEND_THIS, ZEND_STRL("status"), 0);
    //    if (Z_LVAL_P(status) == CONN_NICE)
    //    {

    swoole_get_socket_coro(sock, ZEND_THIS);

    sock->active_test_count++;
    if (sock->active_test_count > sock->active_test_num)
    {
        zend_update_property_long(swoole_cmpp_coro_ce, ZEND_THIS, ZEND_STRL("status"), CONN_BROKEN);
        sock->active_test_count = 0;
        RETURN_FALSE;
    }

    cmpp_get_sequence_id(sock);
    uint32_t send_len;
    char *send_data = cmpp2_make_req(CMPP2_ACTIVE_TEST, sock->sequence_id, 0, NULL, &send_len);
    RETVAL_STRINGL(send_data, send_len);
    efree(send_data);


    //    }

}

static
PHP_METHOD(swoole_cmpp_coro, sendOnePack) {

    char *data;
    size_t length;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(data, length)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    swoole_get_socket_coro(sock, ZEND_THIS);


    zval *status = sw_zend_read_property(swoole_cmpp_coro_ce, ZEND_THIS, ZEND_STRL("status"), 0);
    if (Z_LVAL_P(status) == CONN_BROKEN)
    {
        THROW_BROKEN;
    }

    Socket::timeout_setter ts(sock->socket, -1, SW_TIMEOUT_WRITE);
    size_t bytes = sock->socket->send_all(data, length);
    swoole_cmpp_coro_sync_properties(ZEND_THIS, sock);

    if (bytes != length)
    {
        THROW_BROKEN;
    }
    else
    {
        sock->active_test_count = 0;
    }

    RETURN_LONG(length);
}

static
PHP_METHOD(swoole_cmpp_coro, logout) {
    swoole_get_socket_coro(sock, ZEND_THIS);
    if (sock->socket->close())
    {
        delete sock->socket;
        sock->socket = nullptr;
    }
    if (sock->timer)
    {
        swoole_timer_clear(sock->timer->id);
    }
    RETURN_TRUE;
}

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(swoole_cmpp) {

    int swoole_id = swoole_version_id();
    if (swoole_id <= 40400)
    {
        php_swoole_fatal_error(
                E_CORE_ERROR,
                "Swoole version is too low"
                );
        return FAILURE;
    }

    php_swoole_cmpp_coro_minit(module_number);

    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(swoole_cmpp) {
    char buf[64];
    php_info_print_table_start();
    php_info_print_table_header(2, "Swoole Cmpp", "enabled");
    php_info_print_table_row(2, "Author", "xinhua.guo <guoxinhua@swoole.com>");
    php_info_print_table_row(2, "Version", PHP_SWOOLE_EXT_CMPP_VERSION_ID);
    snprintf(buf, sizeof (buf), "%s %s", __DATE__, __TIME__);
    php_info_print_table_row(2, "Built", buf);
    php_info_print_table_end();

    DISPLAY_INI_ENTRIES();
}



