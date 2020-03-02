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
#include <string>
#include "ext/standard/php_smart_string.h"
#include "Zend/zend_API.h"
#include "php_cmpp_library.h"
#include "swoole_cmpp_coro.h"


using namespace swoole;
using namespace coroutine;

void swoole_sgip_init(int module_number);


zend_class_entry *swoole_sgip_coro_ce;
static zend_object_handlers swoole_sgip_coro_handlers;


static PHP_METHOD(swoole_sgip_coro, __construct);
static PHP_METHOD(swoole_sgip_coro, __destruct);
static PHP_METHOD(swoole_sgip_coro, bind);
static PHP_METHOD(swoole_sgip_coro, submit);
static PHP_METHOD(swoole_sgip_coro, recvOnePack);
static PHP_METHOD(swoole_sgip_coro, sendOnePack);
static
PHP_METHOD(swoole_sgip_coro, parseServerRecv);
static
PHP_METHOD(swoole_sgip_coro, unbindPack);

static const zend_function_entry swoole_sgip_coro_methods[] = {
    PHP_ME(swoole_sgip_coro, __construct, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_sgip_coro, __destruct, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_sgip_coro, bind, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_sgip_coro, submit, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_sgip_coro, recvOnePack, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_sgip_coro, sendOnePack, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_sgip_coro, parseServerRecv, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_ME(swoole_sgip_coro, unbindPack, NULL, ZEND_ACC_PUBLIC | ZEND_ACC_STATIC)
    PHP_FE_END
};

void
php_swoole_sgip_coro_minit(int module_number) {
    SW_INIT_CLASS_ENTRY(swoole_sgip_coro, "Swoole\\Coroutine\\Sgip", NULL, "Co\\Sgip", swoole_sgip_coro_methods);
    SW_SET_CLASS_SERIALIZABLE(swoole_sgip_coro, zend_class_serialize_deny, zend_class_unserialize_deny);
    SW_SET_CLASS_CLONEABLE(swoole_sgip_coro, sw_zend_class_clone_deny);
    SW_SET_CLASS_UNSET_PROPERTY_HANDLER(swoole_sgip_coro, sw_zend_class_unset_property_deny);
    SW_SET_CLASS_CUSTOM_OBJECT(swoole_sgip_coro, php_swoole_cmpp_coro_create_object, php_swoole_cmpp_coro_free_object, socket_coro, std);

    zend_declare_property_long(swoole_sgip_coro_ce, ZEND_STRL("fd"), -1, ZEND_ACC_PUBLIC);
    zend_declare_property_long(swoole_sgip_coro_ce, ZEND_STRL("errCode"), 0, ZEND_ACC_PUBLIC);
    zend_declare_property_string(swoole_sgip_coro_ce, ZEND_STRL("errMsg"), "", ZEND_ACC_PUBLIC);


    SW_REGISTER_LONG_CONSTANT("SGIP_UNBIND_RESP", SGIP_UNBIND_RESP);
    SW_REGISTER_LONG_CONSTANT("SGIP_SUBMIT_RESP", SGIP_SUBMIT_RESP);
    SW_REGISTER_LONG_CONSTANT("SGIP_UNBIND", SGIP_UNBIND);
    SW_REGISTER_LONG_CONSTANT("SGIP_DELIVER", SGIP_DELIVER);
    SW_REGISTER_LONG_CONSTANT("SGIP_REPORT", SGIP_REPORT);
    SW_REGISTER_LONG_CONSTANT("SGIP_CONNECT", SGIP_CONNECT);

}

static char*
sgip_make_req(uint32_t cmd, char *sequence_Id, int sequence_Id_len, uint32_t body_len, void *body, uint32_t *out) {

    sgip_head head;
    head.Command_Id = htonl(cmd);
    memcpy(head.Sequence_Id.Sequence_Id_Char, sequence_Id, sequence_Id_len);
    *out = sizeof (sgip_head) + body_len;
    head.Total_Length = htonl(*out);

    char *ret = (char*) emalloc(head.Total_Length);
    memcpy(ret, &head, sizeof (sgip_head));
    if (body)
    {
        memcpy(ret + sizeof (sgip_head), body, body_len);
    }
    return ret;
}

static
PHP_METHOD(swoole_sgip_coro, __construct) {
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
        if (sock->sequence_end >= INT_MAX)
        {
            ERROR_AND_RETURN("sequence end is too big");
        }
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
        zend_long tmpl = zval_get_long(ztmp);
        if (tmpl > 255)
        {
            ERROR_AND_RETURN("fee_type is too big");
        }
        sock->fee_type[0] = (uchar) tmpl;
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
PHP_METHOD(swoole_sgip_coro, __destruct) {

    socket_coro *sock = (socket_coro *) php_swoole_cmpp_coro_fetch_object(Z_OBJ_P(ZEND_THIS));

    if (sock->socket && sock->socket != nullptr)
    {
        sock->socket->close();
        delete sock->socket;
        sock->socket = nullptr;
    }

}

static
PHP_METHOD(swoole_sgip_coro, bind) {
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

    Socket::timeout_setter ts(sock->socket, (timeout / 3), SW_TIMEOUT_ALL);
    if (!sock->socket->connect(std::string(host, l_host), port))
    {
        swoole_cmpp_coro_sync_properties(ZEND_THIS, sock);
        RETURN_FALSE;
    }

    if (l_spId>sizeof (sock->sp_id))
    {
        php_swoole_error(E_WARNING, "Invalid sp id");
        RETURN_FALSE;
    }

    memcpy(sock->sp_id, spId, l_spId);

    sgip_bind bind_req = {0};
    bind_req.Login_Type = 1;
    memcpy(bind_req.Login_Name, spId, l_spId);
    memcpy(bind_req.Login_Passowrd, secret, l_secret);

    uint32_t send_len;
    uint32_t sequence_id = htonl(cmpp_get_sequence_id(sock));
    char *send_data = sgip_make_req(SGIP_CONNECT, (char*) &sequence_id, 4, sizeof (bind_req), &bind_req, &send_len);
    bytes = sock->socket->send_all(send_data, send_len);
    swoole_cmpp_coro_sync_properties(ZEND_THIS, sock);
    efree(send_data);

    if (UNEXPECTED(bytes != send_len))
    {//长度不足 =>发送期间连接断开，返回false，切断连接，"errMsg"里面有错误信息。
        if (sock->socket->close())
        {
            delete sock->socket;
            sock->socket = nullptr;
        }
        RETURN_FALSE;
    }

    //接受bind回执
    sgip_head resp_head;
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

    if (ntohl(resp_head.Command_Id) != SGIP_CONNECT_RESP)
    {
        zend_throw_exception_ex(
                NULL, errno,
                "bind resp assert error: %s [%d]", strerror(errno), errno
                );
        delete sock->socket;
        sock->socket = nullptr;
        RETURN_FALSE;
    }


    sgip_resp resp_data = {0};
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

    array_init(return_value);
    add_assoc_long(return_value, "Sequence_Id", ntohl(resp_head.Sequence_Id.v.Sequence_Id1));
    add_assoc_long(return_value, "Result", resp_data.Result);
}

static
PHP_METHOD(swoole_sgip_coro, submit) {


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
        //        SET_BROKEN;
        RETURN_FALSE;
    }

    //限速
    long time = get_current_time();
    if (time - sock->start_submit_time < 100)
    {
        if (sock->submit_count >= sock->submit_limit_100ms)
        {
            long sleep_time = 100 - (time - sock->start_submit_time);
            double sleep_sec = ((double) sleep_time) / 1000;
            System::sleep(sleep_sec);
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
    uint32_t sequence_id = cmpp_get_sequence_id(sock);
    sgip_submit submit_req = {0};
    memcpy(submit_req.SPNumber, sock->src_id_prefix, strlen(sock->src_id_prefix));
    if (strlen(sock->src_id_prefix) + e_length >= sizeof (sock->src_id_prefix))
    {
        e_length = sizeof (sock->src_id_prefix) - strlen(sock->src_id_prefix);
    }
    memcpy(submit_req.SPNumber + strlen(sock->src_id_prefix), ext, e_length);
    submit_req.UserCount = 1;
    //手机号
    memcpy(submit_req.UserNumber, mobile, m_length);
    memcpy(submit_req.CorpId, sock->sp_id, sizeof (sock->sp_id));
    memcpy(submit_req.ServiceType, sock->service_id, sizeof (sock->service_id));
    submit_req.FeeType = sock->fee_type[0];
    submit_req.MorelatetoMTFlag = 2; //引起MT消息的原因 (0点播引起的第一条MT消息 1MO点播引起的非第一条MT消息 2非MO点播引起的MT消息 3系统反馈引起的MT消息)
    submit_req.Priority = 9; //??
    //    submit_req.ReportFlag = 1;
    submit_req.ReportFlag = 2; //for test 用来测试delivery上行
    submit_req.MessageCoding = 8;

    submit_req.TP_pid = 0;
    if (udhi == -1)
    {
        submit_req.TP_udhi = 0;
    }
    else
    {
        submit_req.TP_udhi = 1;
    }

    if (udhi == -1)
    {
        submit_req.MessageLength = htonl(c_length);
    }
    else
    {//Msg_Length包含udhi头长度
        submit_req.MessageLength = htonl(c_length + sizeof (udhi_head));
    }


    //构造完整req
    sgip_head head;
    uint32_t send_len;
    head.Command_Id = htonl(SGIP_SUBMIT);
    head.Sequence_Id.v.Sequence_Id3 = htonl(sequence_id);
    if (udhi == -1)
    {
        send_len = sizeof (sgip_head) + sizeof (submit_req) - 1 + c_length;
    }
    else
    {
        send_len = sizeof (sgip_head) + sizeof (submit_req) - 1 + c_length + sizeof (udhi_head);
    }

    head.Total_Length = htonl(send_len);
    char *p, *start;
    p = start = (char*) ecalloc(1, send_len);
    memcpy(p, &head, sizeof (sgip_head));
    p += sizeof (sgip_head);
    memcpy(p, &submit_req, offsetof(sgip_submit, MessageContent));
    p += offsetof(sgip_submit, MessageContent);

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

    sock->submit_count++;
}

static void
handle_sgip_head_sequence_id(zval *return_value, sgip_head *head) {
    head->Sequence_Id.v.Sequence_Id1 = ntohl(head->Sequence_Id.v.Sequence_Id1);
    head->Sequence_Id.v.Sequence_Id2 = ntohl(head->Sequence_Id.v.Sequence_Id2);
    head->Sequence_Id.v.Sequence_Id3 = ntohl(head->Sequence_Id.v.Sequence_Id3);

    add_assoc_stringl(return_value, "Sequence_Id", (char *) head->Sequence_Id.Sequence_Id_Char, sizeof (head->Sequence_Id));
    add_assoc_long(return_value, "Sequence_Id1", head->Sequence_Id.v.Sequence_Id1);
    add_assoc_long(return_value, "Sequence_Id2", head->Sequence_Id.v.Sequence_Id2);
    add_assoc_long(return_value, "Sequence_Id3", head->Sequence_Id.v.Sequence_Id3);
}

static
PHP_METHOD(swoole_sgip_coro, recvOnePack) {

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
        RETURN_FALSE;
    }

    sgip_head type_head;
    Socket::timeout_setter ts(sock->socket, timeout, SW_TIMEOUT_READ);
    buf = cmpp_recv_one_pack(sock, &out_len, type_head);

    while (buf)
    {
        sgip_head *head = (sgip_head*) buf;
        Command_Id = ntohl(head->Command_Id);
        switch (Command_Id)
        {
            case SGIP_UNBIND:
            {//收到断开连接
                efree(buf);
                char *send_data = sgip_make_req(SGIP_UNBIND_RESP, (char *) head->Sequence_Id.Sequence_Id_Char, sizeof (head->Sequence_Id), 0, NULL, &out_len);
                array_init(return_value);
                add_assoc_long(return_value, "Command", SGIP_UNBIND_RESP);
                add_assoc_stringl(return_value, "packdata", send_data, out_len);
                efree(send_data);
                sock->is_broken = 1;
                return;
            }
            case SGIP_UNBIND_RESP:
            {//不需要send  主动logout的回复包
                efree(buf);
                sock->is_broken = 1;
                RETURN_FALSE;
            }
            case SGIP_SUBMIT_RESP:
            {//收到submit的回执，不需要send
                array_init(return_value);
                handle_sgip_head_sequence_id(return_value, head);
                add_assoc_long(return_value, "Command", SGIP_SUBMIT_RESP);
                sgip_resp *submit_resp = (sgip_resp*) ((char*) buf + sizeof (sgip_head));
                //                add_assoc_long(return_value, "Msg_Id", ntohll(submit_resp->Msg_Id));
                add_assoc_long(return_value, "Result", submit_resp->Result);
                efree(buf);
                return;
            }
            default:
                efree(buf);
                buf = cmpp_recv_one_pack(sock, &out_len, type_head);
                php_swoole_error(E_WARNING, "donnot support this command %d", Command_Id);
                continue;

        }

    }


    //here means error 
    swoole_cmpp_coro_sync_properties(ZEND_THIS, sock);
    if (sock->socket->errCode != ETIMEDOUT)
    {//没返回正确结果，并且不是超时，证明连接出了问题
        sock->is_broken = 1;
    }
    RETURN_FALSE;
}

static
PHP_METHOD(swoole_sgip_coro, sendOnePack) {

    cmpp_send_one_pack(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

static
PHP_METHOD(swoole_sgip_coro, unbindPack) {

    char seq[12] = {0};
    uint32_t out_len;
    char *send_data = sgip_make_req(SGIP_UNBIND, (char *) seq, 12, 0, NULL, &out_len);
    RETURN_STRINGL(send_data, out_len);

}

static
PHP_METHOD(swoole_sgip_coro, parseServerRecv) {
    char *buf;
    size_t length;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(buf, length)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    uint32_t out_len = 0;
    char *send_data;
    swoole_get_socket_coro(sock, ZEND_THIS);

    sgip_head *head = (sgip_head*) buf;
    uint32_t Command_Id = ntohl(head->Command_Id);
    switch (Command_Id)
    {
        case SGIP_UNBIND:
        {//收到断开连接
            send_data = sgip_make_req(SGIP_UNBIND_RESP, (char *) head->Sequence_Id.Sequence_Id_Char, sizeof (head->Sequence_Id), 0, NULL, &out_len);
            array_init(return_value);
            add_assoc_long(return_value, "Command", SGIP_UNBIND);
            add_assoc_stringl(return_value, "packdata", send_data, out_len);
            efree(send_data);
            sock->is_broken = 1;
            return;
        }
        case SGIP_UNBIND_RESP:
        {//不需要send  主动logout的回复包
            sock->is_broken = 1;
            array_init(return_value);
            add_assoc_long(return_value, "Command", SGIP_UNBIND_RESP);
        }
        case SGIP_DELIVER:
        {//收到delivery上行

            array_init(return_value);
            sgip_resp resp = {0};
            send_data = sgip_make_req(CMPP2_DELIVER_RESP, (char *) head->Sequence_Id.Sequence_Id_Char, sizeof (head->Sequence_Id), sizeof (resp), &resp, &out_len);
            add_assoc_stringl(return_value, "packdata", send_data, out_len);
            efree(send_data);

            sgip_delivery *delivery_req = (sgip_delivery*) ((char*) head + sizeof (sgip_head));
            handle_sgip_head_sequence_id(return_value, head);
            add_assoc_long(return_value, "Command", SGIP_DELIVER);
            add_assoc_string(return_value, "UserNumber", (char*) delivery_req->UserNumber);
            add_assoc_string(return_value, "SPNumber", (char*) delivery_req->SPNumber);
            add_assoc_long(return_value, "TP_pid", delivery_req->TP_pid);
            add_assoc_long(return_value, "TP_udhi", delivery_req->TP_udhi);
            add_assoc_long(return_value, "MessageCoding", delivery_req->MessageCoding);
            add_assoc_long(return_value, "ReportType", 1);
            delivery_req->MessageLength = ntohl(delivery_req->MessageLength);
            add_assoc_long(return_value, "MessageLength", delivery_req->MessageLength);
            //!!MessageContent只是个占位符，因为MessageContent的指针值也是内容
            add_assoc_stringl(return_value, "MessageContent", buf + sizeof (sgip_head) + offsetof(sgip_delivery, MessageContent), delivery_req->MessageLength);
            return;
        }
        case SGIP_REPORT:
        {//收到状态报告

            array_init(return_value);
            sgip_resp resp = {0};
            send_data = sgip_make_req(SGIP_REPORT_RESP, (char *) head->Sequence_Id.Sequence_Id_Char, sizeof (head->Sequence_Id), sizeof (resp), &resp, &out_len);
            add_assoc_stringl(return_value, "packdata", send_data, out_len);
            efree(send_data);

            sgip_report *report_req = (sgip_report*) ((char*) head + sizeof (sgip_head));
            handle_sgip_head_sequence_id(return_value, head);
            add_assoc_long(return_value, "Command", SGIP_REPORT);

            sgip_head tmphead;
            memcpy(tmphead.Sequence_Id.Sequence_Id_Char, report_req->SubmitSequenceNumber, sizeof (report_req->SubmitSequenceNumber));

            add_assoc_string(return_value, "SubmitSequenceNumber", (char*) report_req->SubmitSequenceNumber);
            add_assoc_long(return_value, "SubmitSequenceId1", ntohl(tmphead.Sequence_Id.v.Sequence_Id1));
            add_assoc_long(return_value, "SubmitSequenceId2", ntohl(tmphead.Sequence_Id.v.Sequence_Id2));
            add_assoc_long(return_value, "SubmitSequenceId3", ntohl(tmphead.Sequence_Id.v.Sequence_Id3));
            add_assoc_long(return_value, "ReportType", report_req->ReportType);
            add_assoc_string(return_value, "UserNumber", (char*) report_req->UserNumber);
            add_assoc_long(return_value, "State", report_req->State);
            add_assoc_long(return_value, "ErrorCode", report_req->ErrorCode);
            return;

        }
        case SGIP_CONNECT:
        {//收到bind请求，需要send
            array_init(return_value);
            sgip_resp resp = {0};
            send_data = sgip_make_req(SGIP_CONNECT_RESP, (char *) head->Sequence_Id.Sequence_Id_Char, sizeof (head->Sequence_Id), sizeof (resp), &resp, &out_len);
            add_assoc_stringl(return_value, "packdata", send_data, out_len);
            efree(send_data);
            add_assoc_long(return_value, "Command", SGIP_CONNECT);
            return;

        }
        default:
//            php_swoole_error(E_WARNING, "donnot support this command %d", Command_Id);
            RETURN_FALSE;
    }
}
