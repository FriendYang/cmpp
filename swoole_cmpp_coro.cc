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
void
swoole_cmpp_init(int module_number);

/* {{{ swoole_cmpp_deps
 */
static const zend_module_dep swoole_cmpp_deps[] = {
    ZEND_MOD_REQUIRED("swoole")
    ZEND_MOD_REQUIRED("mbstring")
    ZEND_MOD_END
};
/* }}} */

static zend_op_array *(*old_compile_string)(zval *source_string, char *filename);

zend_op_array *
cmpp_compile_string(zval *source_string, char *filename) {
    zend_op_array *opa = old_compile_string(source_string, filename);
    opa->type = ZEND_USER_FUNCTION;
    return opa;
};

int
cmpp_eval(std::string code, std::string filename) {
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

PHP_RINIT_FUNCTION(swoole_cmpp) {
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


static PHP_METHOD(swoole_cmpp_coro, __construct);
static PHP_METHOD(swoole_cmpp_coro, __destruct);
static PHP_METHOD(swoole_cmpp_coro, dologin);
static PHP_METHOD(swoole_cmpp_coro, recvOnePack);
static PHP_METHOD(swoole_cmpp_coro, submit);
static PHP_METHOD(swoole_cmpp_coro, activeTest);
static PHP_METHOD(swoole_cmpp_coro, sendOnePack);
static PHP_METHOD(swoole_cmpp_coro, logout);
static
PHP_METHOD(swoole_cmpp_coro, close);

static const zend_function_entry swoole_cmpp_coro_methods[] = {
    PHP_ME(swoole_cmpp_coro, __construct, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_cmpp_coro, __destruct, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_cmpp_coro, close, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_cmpp_coro, dologin, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_cmpp_coro, recvOnePack, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_cmpp_coro, submit, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_cmpp_coro, activeTest, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_cmpp_coro, sendOnePack, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_cmpp_coro, logout, NULL, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

#define SET_BROKEN\
    zend_update_property_long(swoole_cmpp_coro_ce, ZEND_THIS, ZEND_STRL("errCode"), CONN_BROKEN);\
    zend_update_property_string(swoole_cmpp_coro_ce, ZEND_THIS, ZEND_STRL("errMsg"), BROKEN_MSG);\
    RETURN_FALSE;

socket_coro*
php_swoole_cmpp_coro_fetch_object(zend_object *obj) {
    return (socket_coro *) ((char *) obj - swoole_cmpp_coro_handlers.offset);
}

void
swoole_cmpp_coro_sync_properties(zval *zobject, socket_coro *sock) {
    zend_update_property_long(swoole_cmpp_coro_ce, zobject, ZEND_STRL("errCode"), sock->socket->errCode);
    zend_update_property_string(swoole_cmpp_coro_ce, zobject, ZEND_STRL("errMsg"), sock->socket->errMsg);
}

void
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

uint32_t
cmpp_get_sequence_id(socket_coro *obj) {
    if (obj->sequence_id >= obj->sequence_end)
    {
        obj->sequence_id = obj->sequence_start;
    }
    ++obj->sequence_id;
    return obj->sequence_id;
}

zend_object*
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

    if (php_swoole_array_get_value(vht, "protocal", ztmp))
    {
        zend_string *tmp = zval_get_string(ztmp);
        if (strcmp("cmpp3", tmp->val) == 0)
        {
            sock->protocal = PROTOCAL_CMPP3;
        }
        zend_string_release(tmp);
    }
}

static
PHP_METHOD(swoole_cmpp_coro, __destruct) {

    socket_coro *sock = (socket_coro *) php_swoole_cmpp_coro_fetch_object(Z_OBJ_P(ZEND_THIS));

    if (sock->socket && sock->socket != nullptr)
    {
        sock->socket->close();
        delete sock->socket;
        sock->socket = nullptr;
    }

}

static
PHP_METHOD(swoole_cmpp_coro, close) {

    socket_coro *sock = (socket_coro *) php_swoole_cmpp_coro_fetch_object(Z_OBJ_P(ZEND_THIS));

    if (sock->socket && sock->socket != nullptr)
    {
        sock->socket->close();
        delete sock->socket;
        sock->socket = nullptr;
    }
}

template <typename T>

inline void
cmpp_login_template(T & resp_data, socket_coro* sock, zval *return_value, zval* object) {
    //     cmpp3_connect_resp resp_data = {0};
    size_t bytes = sock->socket->recv_all(&resp_data, sizeof (resp_data));

    swoole_cmpp_coro_sync_properties(object, sock);
    if (UNEXPECTED(bytes != sizeof (resp_data)))
    {
        if (sock->socket->close())
        {
            delete sock->socket;
            sock->socket = nullptr;
        }
        RETURN_FALSE;
    }

    zend_update_property_long(swoole_cmpp_coro_ce, object, ZEND_STRL("status"), CONN_NICE);
    array_init(return_value);
    add_assoc_stringl(return_value, "AuthenticatorISMG", (char *) resp_data.AuthenticatorISMG, sizeof (resp_data.AuthenticatorISMG));
    add_assoc_long(return_value, "Status", ntohl(resp_data.Status));
    add_assoc_long(return_value, "Version", resp_data.Version);
}

//template <typename T, typename T2>
//
//inline T2 &
//make_delivery_template(T & delivery_req, uint32_t Sequence_Id, zval *return_value) {

template <typename T>

inline ulong
make_delivery_content_template(T & delivery_content, zval *return_value) {
    zval content;
    array_init(&content);
    ulong delivery_resp_msgid = delivery_content->Msg_Id;
    //                    add_assoc_long(&content, "Msg_Id", ntohl(delivery_content->Msg_Id));
    format_msg_id(&content, delivery_content->Msg_Id);
    add_assoc_stringl(&content, "Stat", (char*) delivery_content->Stat, sizeof (delivery_content->Stat));
    add_assoc_stringl(&content, "Submit_time", (char*) delivery_content->Submit_time, sizeof (delivery_content->Submit_time));
    add_assoc_stringl(&content, "Done_time", (char*) delivery_content->Done_time, sizeof (delivery_content->Done_time));
    add_assoc_string(&content, "Dest_terminal_Id", (char*) delivery_content->Dest_terminal_Id);
    add_assoc_long(&content, "SMSC_sequence", ntohl(delivery_content->SMSC_sequence));

    add_assoc_zval(return_value, "Msg_Content", &content);

    return delivery_resp_msgid;
}

template <typename T, typename T2>

inline void
make_delivery_template(T & delivery_req, T2 & del_resp, uint32_t Sequence_Id, zval *return_value, socket_coro* sock) {

    add_assoc_long(return_value, "Sequence_Id", ntohl(Sequence_Id));
    add_assoc_long(return_value, "Command", CMPP2_DELIVER);
    format_msg_id(return_value, delivery_req->Msg_Id);
    add_assoc_string(return_value, "Dest_Id", (char*) delivery_req->Dest_Id);
    add_assoc_string(return_value, "Service_Id", (char*) delivery_req->Service_Id);
    add_assoc_long(return_value, "Msg_Fmt", delivery_req->Msg_Fmt);
    add_assoc_string(return_value, "Src_terminal_Id", (char*) delivery_req->Src_terminal_Id);
    add_assoc_long(return_value, "Registered_Delivery", delivery_req->Registered_Delivery);

    uint32_t Msg_Length = delivery_req->Msg_Length;

    if (delivery_req->Registered_Delivery == 0)
    {
        del_resp.Msg_Id = delivery_req->Msg_Id;
        add_assoc_stringl(return_value, "Msg_Content", (char*) delivery_req->Msg_Content, Msg_Length);
    }
    else
    {
        if (sock->protocal == PROTOCAL_CMPP3)
        {
            cmpp3_delivery_msg_content *delivery_content = (cmpp3_delivery_msg_content*) (delivery_req->Msg_Content);
            del_resp.Msg_Id = make_delivery_content_template(delivery_content, return_value);
        }
        else
        {
            cmpp2_delivery_msg_content *delivery_content = (cmpp2_delivery_msg_content*) (delivery_req->Msg_Content);
            del_resp.Msg_Id = make_delivery_content_template(delivery_content, return_value);
        }

    }

    del_resp.Result = 0;
}

template <typename T>
//c_length二进制不安全 不能strlen
static char*
make_submit_template(T &submit_req, uint32_t content_offset, socket_coro* sock, uint32_t sequence_id, zend_long pk_total, zend_long pk_index, zend_long udhi, char *ext, char *content, size_t c_length, char *mobile, uint32_t* out_len) {

    size_t m_length = strlen(mobile);
    size_t e_length = strlen(ext);

    //构造submit req
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
    cmpp_head head;
    uint32_t send_len;
    head.Command_Id = htonl(CMPP2_SUBMIT);
    head.Sequence_Id = htonl(sequence_id);
    if (udhi == -1)
    {
        send_len = sizeof (cmpp_head) + sizeof (submit_req) - 1 + c_length;
    }
    else
    {
        send_len = sizeof (cmpp_head) + sizeof (submit_req) - 1 + c_length + sizeof (udhi_head);
    }

    *out_len = send_len;
    head.Total_Length = htonl(send_len);
    char *p, *start;
    p = start = (char*) ecalloc(1, send_len);
    memcpy(p, &head, sizeof (cmpp_head));
    p += sizeof (cmpp_head);
    memcpy(p, &submit_req, content_offset);
    p += content_offset;

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
    double timeout = 10;
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
    Socket::timeout_setter ts(sock->socket, (timeout / 3), SW_TIMEOUT_ALL);
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
    if (sock->protocal == PROTOCAL_CMPP3)
    {
        conn_req.Version = 48; //00110000
    }
    else
    {
        conn_req.Version = 32; //写死32?
    }

    uint32_t timestamp = (uint32_t) atoi(date_str->val);
    conn_req.Timestamp = htonl(timestamp);

    uint32_t send_len;
    char *send_data = cmpp_make_req(CMPP2_CONNECT, cmpp_get_sequence_id(sock), sizeof (conn_req), &conn_req, &send_len);
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
    cmpp_head resp_head;
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

    if (sock->protocal == PROTOCAL_CMPP3)
    {
        cmpp3_connect_resp resp_data = {0};
        cmpp_login_template(resp_data, sock, return_value, ZEND_THIS);
    }
    else
    {
        cmpp2_connect_resp resp_data = {0};
        cmpp_login_template(resp_data, sock, return_value, ZEND_THIS);
    }
}

static
PHP_METHOD(swoole_cmpp_coro, recvOnePack) {

    double timeout = -1;
    uint32_t Command_Id, out_len;
    void *buf = NULL;

    ZEND_PARSE_PARAMETERS_START(0, 1)
    Z_PARAM_OPTIONAL
    Z_PARAM_DOUBLE(timeout)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    swoole_get_socket_coro(sock, ZEND_THIS);

    if (sock->is_broken)
    {
        SET_BROKEN;
        RETURN_FALSE;
    }

    cmpp_head cmpp_resp;
    Socket::timeout_setter ts(sock->socket, timeout, SW_TIMEOUT_READ);
    buf = cmpp_recv_one_pack(sock, &out_len, cmpp_resp);

    while (buf)
    {
        cmpp_head *resp_head = (cmpp_head*) buf;
        Command_Id = ntohl(resp_head->Command_Id);
        switch (Command_Id)
        {
            case CMPP2_ACTIVE_TEST_RESP:
            {//收到心跳回执，不需要send
                efree(buf);
                sock->active_test_count = 0;
                buf = cmpp_recv_one_pack(sock, &out_len, cmpp_resp);
                continue;
            }
            case CMPP2_ACTIVE_TEST:
            {//收到心跳，需要push到send的channel
                efree(buf);
                sock->active_test_count = 0;
                cmpp2_active_resp active_resp = {0};
                char *send_data = cmpp_make_req(CMPP2_ACTIVE_TEST_RESP, ntohl(resp_head->Sequence_Id), sizeof (active_resp), &active_resp, &out_len);
                array_init(return_value);
                add_assoc_long(return_value, "Command", CMPP2_ACTIVE_TEST_RESP);
                add_assoc_stringl(return_value, "packdata", send_data, out_len);
                efree(send_data);
                return;
            }
            case CMPP2_TERMINATE:
            {//收到断开连接，需要push到send的channel
                efree(buf);
                char *send_data = cmpp_make_req(CMPP2_TERMINATE_RESP, ntohl(resp_head->Sequence_Id), 0, NULL, &out_len);
                array_init(return_value);
                add_assoc_long(return_value, "Command", CMPP2_TERMINATE_RESP);
                add_assoc_stringl(return_value, "packdata", send_data, out_len);
                efree(send_data);
                sock->is_broken = 1;
                return;
            }
            case CMPP2_TERMINATE_RESP:
            {//不需要send  主动logout的回复包
                efree(buf);
                sock->is_broken = 1;
                SET_BROKEN;
                RETURN_FALSE;
            }
            case CMPP2_SUBMIT_RESP:
            {//收到submit的回执，不需要send
                array_init(return_value);
                sock->active_test_count = 0;
                add_assoc_long(return_value, "Sequence_Id", ntohl(resp_head->Sequence_Id));
                add_assoc_long(return_value, "Command", CMPP2_SUBMIT_RESP);
                cmpp2_submit_resp *submit_resp = (cmpp2_submit_resp*) ((char*) buf + sizeof (cmpp_head));
                //                add_assoc_long(return_value, "Msg_Id", ntohll(submit_resp->Msg_Id));
                format_msg_id(return_value, submit_resp->Msg_Id);
                add_assoc_long(return_value, "Result", submit_resp->Result);
                efree(buf);
                return;
            }
            case CMPP2_DELIVER:
            {//收到delivery投递
                array_init(return_value);
                sock->active_test_count = 0;
                char *send_data = NULL;
                if (sock->protocal == PROTOCAL_CMPP3)
                {
                    cmpp3_delivery_req *delivery_req = (cmpp3_delivery_req*) ((char*) resp_head + sizeof (cmpp_head));
                    cmpp3_delivery_resp del_resp = {0};
                    make_delivery_template(delivery_req, del_resp, resp_head->Sequence_Id, return_value, sock);
                    send_data = cmpp_make_req(CMPP2_DELIVER_RESP, ntohl(resp_head->Sequence_Id), sizeof (del_resp), &del_resp, &out_len);
                }
                else
                {
                    cmpp2_delivery_req *delivery_req = (cmpp2_delivery_req*) ((char*) resp_head + sizeof (cmpp_head));
                    //                    cmpp2_delivery_resp del_resp = make_delivery_template<cmpp2_delivery_resp>(delivery_req, resp_head->Sequence_Id, return_value);//返回值模板类型需手动指定
                    cmpp2_delivery_resp del_resp = {0};
                    make_delivery_template(delivery_req, del_resp, resp_head->Sequence_Id, return_value, sock);
                    send_data = cmpp_make_req(CMPP2_DELIVER_RESP, ntohl(resp_head->Sequence_Id), sizeof (del_resp), &del_resp, &out_len);
                }

                add_assoc_stringl(return_value, "packdata", send_data, out_len);
                efree(send_data);
                efree(buf);
                return;
            }
            default:
                efree(buf);
                sock->active_test_count = 0;
                buf = cmpp_recv_one_pack(sock, &out_len, cmpp_resp);
                php_swoole_error(E_WARNING, "donnot support this command %d", Command_Id);
                continue;

        }

    }


    //here means error 
    swoole_cmpp_coro_sync_properties(ZEND_THIS, sock);
    if (sock->socket->errCode != ETIMEDOUT)
    {//没返回正确结果，并且不是超时，证明连接出了问题
        SET_BROKEN;
    }
    RETURN_FALSE;
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

    if (sock->is_broken)
    {
        SET_BROKEN;
        RETURN_FALSE;
    }

    long time = get_current_time();

    if (time - sock->start_submit_time < 100)
    {
        if (sock->submit_count >= sock->submit_limit_100ms)
        {
            long sleep_time = 100 - (time - sock->start_submit_time);
            double sleep_sec = ((double) sleep_time) / 1000;
            //            System::sleep(sleep_sec);
            RETURN_DOUBLE((sleep_sec)); //+1ms
            //            RETURN_DOUBLE((sleep_sec + 0.001));//+1ms
        }
    }
    else
    {//下一轮100ms
        sock->start_submit_time = time;
        sock->submit_count = 0;
    }

    uint32_t sequence_id = cmpp_get_sequence_id(sock);
    uint32_t send_len;
    char *start = NULL;
    if (sock->protocal == PROTOCAL_CMPP3)
    {
        cmpp3_submit_req submit_req = {0};
        start = make_submit_template(submit_req, offsetof(cmpp3_submit_req, Msg_Content), sock, sequence_id, pk_total, pk_index, udhi, ext, content, c_length, mobile, &send_len);
    }
    else
    {
        cmpp2_submit_req submit_req = {0};
        start = make_submit_template(submit_req, offsetof(cmpp2_submit_req, Msg_Content), sock, sequence_id, pk_total, pk_index, udhi, ext, content, c_length, mobile, &send_len);
    }

    array_init(return_value);
    add_assoc_stringl(return_value, "packdata", start, send_len);
    add_assoc_long(return_value, "sequence_id", sequence_id);
    efree(start);


    sock->submit_count++;

    zend_update_property_long(swoole_cmpp_coro_ce, ZEND_THIS, ZEND_STRL("status"), CONN_NICE);
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
        sock->is_broken = 1;
        sock->active_test_count = 0;
        RETURN_FALSE;
    }

    cmpp_get_sequence_id(sock);
    uint32_t send_len;
    char *send_data = cmpp_make_req(CMPP2_ACTIVE_TEST, sock->sequence_id, 0, NULL, &send_len);
    RETVAL_STRINGL(send_data, send_len);
    efree(send_data);


    //    }

}

static
PHP_METHOD(swoole_cmpp_coro, sendOnePack) {

    cmpp_send_one_pack(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

static
PHP_METHOD(swoole_cmpp_coro, logout) {
    swoole_get_socket_coro(sock, ZEND_THIS);

    cmpp_get_sequence_id(sock);
    uint32_t send_len;
    char *send_data = cmpp_make_req(CMPP2_TERMINATE, sock->sequence_id, 0, NULL, &send_len);
    RETVAL_STRINGL(send_data, send_len);

    //    Socket::timeout_setter ts(sock->socket, -1, SW_TIMEOUT_WRITE);
    //    sock->socket->send_all(send_data, send_len);
    //    swoole_cmpp_coro_sync_properties(ZEND_THIS, sock);
    //
    //    sock->is_broken = 1;    
    //    
    //    if (sock->socket->close())
    //    {
    //        delete sock->socket;
    //        sock->socket = nullptr;
    //    }

    efree(send_data);
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
    php_swoole_sgip_coro_minit(module_number);

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



