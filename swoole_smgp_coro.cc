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

void swoole_smgp_init(int module_number);


zend_class_entry *swoole_smgp_coro_ce;
static zend_object_handlers swoole_smgp_coro_handlers;


static PHP_METHOD(swoole_smgp_coro, __construct);
static PHP_METHOD(swoole_smgp_coro, __destruct);
static PHP_METHOD(swoole_smgp_coro, dologin);
static PHP_METHOD(swoole_smgp_coro, sendOnePack);
static PHP_METHOD(swoole_smgp_coro, recvOnePack);
static PHP_METHOD(swoole_smgp_coro, activeTest);
static
PHP_METHOD(swoole_smgp_coro, submit);
static
PHP_METHOD(swoole_smgp_coro, logout);


static const zend_function_entry swoole_smgp_coro_methods[] = {
    PHP_ME(swoole_smgp_coro, __construct, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_smgp_coro, __destruct, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_smgp_coro, dologin, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_smgp_coro, submit, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_smgp_coro, sendOnePack, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_smgp_coro, recvOnePack, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_smgp_coro, activeTest, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_smgp_coro, logout, NULL, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

void
php_swoole_smgp_coro_minit(int module_number) {
    SW_INIT_CLASS_ENTRY(swoole_smgp_coro, "Swoole\\Coroutine\\SmgpProtocal", NULL, "Co\\SmgpProtocal", swoole_smgp_coro_methods);
    SW_SET_CLASS_SERIALIZABLE(swoole_smgp_coro, zend_class_serialize_deny, zend_class_unserialize_deny);
    SW_SET_CLASS_CLONEABLE(swoole_smgp_coro, sw_zend_class_clone_deny);
    SW_SET_CLASS_UNSET_PROPERTY_HANDLER(swoole_smgp_coro, sw_zend_class_unset_property_deny);
    SW_SET_CLASS_CUSTOM_OBJECT(swoole_smgp_coro, php_swoole_cmpp_coro_create_object, php_swoole_cmpp_coro_free_object, socket_coro, std);

    zend_declare_property_long(swoole_smgp_coro_ce, ZEND_STRL("fd"), -1, ZEND_ACC_PUBLIC);
    zend_declare_property_long(swoole_smgp_coro_ce, ZEND_STRL("errCode"), 0, ZEND_ACC_PUBLIC);
    zend_declare_property_string(swoole_smgp_coro_ce, ZEND_STRL("errMsg"), "", ZEND_ACC_PUBLIC);


    SW_REGISTER_LONG_CONSTANT("SMGP_ACTIVE_TEST_RESP", SMGP_ACTIVE_TEST_RESP);
    SW_REGISTER_LONG_CONSTANT("SMGP_TERMINATE_RESP", SMGP_TERMINATE_RESP);
    SW_REGISTER_LONG_CONSTANT("SMGP_SUBMIT_RESP", SMGP_SUBMIT_RESP);
    SW_REGISTER_LONG_CONSTANT("SMGP_DELIVER_RESP", SMGP_DELIVER_RESP);
    SW_REGISTER_LONG_CONSTANT("SMGP_DELIVER", SMGP_DELIVER);

}

static
PHP_METHOD(swoole_smgp_coro, __construct) {
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
PHP_METHOD(swoole_smgp_coro, __destruct) {

    socket_coro *sock = (socket_coro *) php_swoole_cmpp_coro_fetch_object(Z_OBJ_P(ZEND_THIS));

    if (sock->socket && sock->socket != nullptr)
    {
        sock->socket->close();
        delete sock->socket;
        sock->socket = nullptr;
    }

}

static
PHP_METHOD(swoole_smgp_coro, close) {

    socket_coro *sock = (socket_coro *) php_swoole_cmpp_coro_fetch_object(Z_OBJ_P(ZEND_THIS));

    if (sock->socket && sock->socket != nullptr)
    {
        sock->socket->close();
        delete sock->socket;
        sock->socket = nullptr;
    }
}

static
PHP_METHOD(swoole_smgp_coro, dologin) {
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

    //发送login包
    smgp_login login_req = {0};
    memcpy(login_req.ClientID, spId, l_spId);
    smart_string auth = {0};
    smart_string_appends(&auth, spId);
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
    cmpp_md5(login_req.AuthenticatorClient, auth.c, auth.len);

    login_req.LoginMode = 2; // 客户端用来登录服务器端的登录类型, 2表示收发短信
    uint32_t timestamp = (uint32_t) atoi(date_str->val);
    login_req.TimeStamp = htonl(timestamp);
    login_req.ClientVersion = 0x30; //??

    uint32_t send_len;
    char *send_data = common_make_req(SMGP_CONNECT, common_get_sequence_id(sock), sizeof (login_req), &login_req, &send_len);
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

    //接受login回执
    smgp_head resp_head;
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

    if (ntohl(resp_head.RequestID) != SMGP_CONNECT_RESP || ntohl(resp_head.SequenceID) != sock->sequence_id)
    {
        zend_throw_exception_ex(
                NULL, errno,
                "connect resp assert error: %s [%d]", strerror(errno), errno
                );
        delete sock->socket;
        sock->socket = nullptr;
        RETURN_FALSE;
    }

    smgp_login_resp resp_data = {0};
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
    add_assoc_stringl(return_value, "AuthenticatorServer", (char *) resp_data.AuthenticatorServer, sizeof (resp_data.AuthenticatorServer));
    add_assoc_long(return_value, "Status", ntohl(resp_data.Status));
    add_assoc_long(return_value, "ServerVersion", resp_data.ServerVersion);
}

static
PHP_METHOD(swoole_smgp_coro, recvOnePack) {

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

    cmpp_head smgp_resp;
    Socket::timeout_setter ts(sock->socket, timeout, SW_TIMEOUT_READ);
    buf = cmpp_recv_one_pack(sock, &out_len, smgp_resp);

    while (buf)
    {
        smgp_head *resp_head = (smgp_head*) buf;
        Command_Id = ntohl(resp_head->RequestID);
        switch (Command_Id)
        {
            case SMGP_ACTIVE_TEST_RESP:
            {//收到心跳回执，不需要send
                efree(buf);
                sock->active_test_count = 0;
                buf = cmpp_recv_one_pack(sock, &out_len, smgp_resp);
                continue;
            }
            case SMGP_ACTIVE_TEST:
            {//收到心跳，需要push到send的channel
                efree(buf);
                sock->active_test_count = 0;
                char *send_data = common_make_req(SMGP_ACTIVE_TEST_RESP, ntohl(resp_head->RequestID), 0, NULL, &out_len);
                array_init(return_value);
                add_assoc_long(return_value, "RequestID", SMGP_ACTIVE_TEST_RESP);
                add_assoc_stringl(return_value, "packdata", send_data, out_len);
                efree(send_data);
                return;
            }
            case SMGP_TERMINATE:
            {//收到断开连接，需要push到send的channel
                efree(buf);
                char *send_data = common_make_req(SMGP_TERMINATE_RESP, ntohl(resp_head->RequestID), 0, NULL, &out_len);
                array_init(return_value);
                add_assoc_long(return_value, "RequestID", SMGP_TERMINATE_RESP);
                add_assoc_stringl(return_value, "packdata", send_data, out_len);
                efree(send_data);
                sock->is_broken = 1;
                return;
            }
            case SMGP_TERMINATE_RESP:
            {//不需要send  主动logout的回复包
                efree(buf);
                sock->is_broken = 1;
                RETURN_FALSE;
            }
            case SMGP_SUBMIT_RESP:
            {//收到submit的回执，不需要send
                array_init(return_value);
                sock->active_test_count = 0;
                add_assoc_long(return_value, "Sequence_Id", ntohl(resp_head->SequenceID));
                add_assoc_long(return_value, "RequestID", SMGP_SUBMIT_RESP);
                smgp_submit_resp *submit_resp = (smgp_submit_resp*) ((char*) buf + sizeof (smgp_head));
                std::string tmp = BinToHex((char*) submit_resp->MsgID, sizeof (submit_resp->MsgID), 0);
                add_assoc_stringl(return_value, "MsgID", (char*) tmp.c_str(), tmp.length());
                add_assoc_long(return_value, "Status", ntohl(submit_resp->Status));
                efree(buf);
                return;
            }
            case SMGP_DELIVER:
            {//收到delivery投递
                array_init(return_value);
                sock->active_test_count = 0;
                //返回值
                smgp_delivery *delivery_req = (smgp_delivery*) ((char*) resp_head + sizeof (smgp_head));
                add_assoc_long(return_value, "SequenceID", ntohl(resp_head->SequenceID));
                add_assoc_long(return_value, "RequestID", SMGP_DELIVER);
                std::string tmp = BinToHex((char*) delivery_req->MsgID, sizeof (delivery_req->MsgID), 0);
                add_assoc_stringl(return_value, "MsgID", (char*) tmp.c_str(), tmp.length());
                add_assoc_long(return_value, "IsReport", delivery_req->IsReport);
                add_assoc_long(return_value, "MsgFormat", delivery_req->MsgFormat);
                add_assoc_string(return_value, "RecvTime", (char*) delivery_req->RecvTime);
                add_assoc_string(return_value, "SrcTermID", (char*) delivery_req->SrcTermID);
                add_assoc_string(return_value, "DestTermID", (char*) delivery_req->DestTermID);
                add_assoc_long(return_value, "MsgLength", delivery_req->MsgLength);
                char *content = (char*) emalloc(delivery_req->MsgLength);
                memcpy(content, (char *) &delivery_req->MsgContent, delivery_req->MsgLength);
                add_assoc_stringl(return_value, "MsgContent", content, delivery_req->MsgLength);
                //构造resp
                smgp_delivery_resp del_resp = {0};
                del_resp.Status = 0;
                memcpy(&del_resp.MsgID, delivery_req->MsgID, sizeof (delivery_req->MsgID));
                char *send_data = common_make_req(SMGP_DELIVER_RESP, ntohl(resp_head->SequenceID), sizeof (del_resp), &del_resp, &out_len);
                add_assoc_stringl(return_value, "packdata", send_data, out_len);
                //释放内存
                efree(send_data);
                efree(content);
                efree(buf);
                return;
            }
            default:
                efree(buf);
                sock->active_test_count = 0;
                buf = cmpp_recv_one_pack(sock, &out_len, smgp_resp);
                php_swoole_error(E_WARNING, "donnot support this command %d", Command_Id);
                continue;

        }

    }

    //here means error 
    swoole_cmpp_coro_sync_properties(ZEND_THIS, sock);
    //    if (sock->socket->errCode != ETIMEDOUT)
    //    {//没返回正确结果，并且不是超时，证明连接出了问题
    //        //        SET_BROKEN;
    //    }
    RETURN_FALSE;
}

static
PHP_METHOD(swoole_smgp_coro, submit) {


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
    smgp_submit submit_req = {0};
    submit_req.MsgType = 6; //6＝MT 消息（SP 发给终端)
    submit_req.NeedReport = 1;
    submit_req.Priority = 3;
    memcpy(submit_req.ServiceID, sock->service_id, sizeof (sock->service_id));
    /**
     * SMGP: 对计费用户采取的收费类型。
     * 00=免费,此时 FixedFee 和 FeeCode 无效;
     * 01=按条计信息费,此时 FeeCode 表示每条费用,FixedFee 无效;
     * 02=按包月收取信息费,此时 FeeCode 无效,FixedFee 表示包月费用;
     * 03=按封顶收取信息费,若按条收费的费用总和达到或超过封顶费后,则
     * 按照封顶费用收取费用;若按条收费的费用总和没有达到封顶费用,则按照每条
     * 费用总和收取费用。FeeCode 表示每条费用,FixedFee 表示封顶费用。
     * 其它保留
     */
    memcpy(submit_req.FeeType, sock->fee_type, sizeof (sock->fee_type));

    //    submit_req.FeeCode = '';
    //    submit_req.FixedFee = '';
    submit_req.MsgFormat = 8; //utf8
    //    submit_req.ValidTime = '';
    //    submit_req.AtTime = '';
    //用户那里显示的  谁发的
    memcpy(submit_req.SrcTermID, sock->src_id_prefix, strlen(sock->src_id_prefix));
    if (strlen(sock->src_id_prefix) + e_length >= sizeof (sock->src_id_prefix))
    {
        e_length = sizeof (sock->src_id_prefix) - strlen(sock->src_id_prefix);
    }
    memcpy(submit_req.SrcTermID + strlen(sock->src_id_prefix), ext, e_length);
    //    submit_req.ChargeTermID = "";
    submit_req.DestTermIDCount = 1;
    //手机号
    memcpy(submit_req.DestTermID, mobile, m_length);

    if (udhi == -1)
    {
        submit_req.Msg_Length = c_length;
    }
    else
    {//Msg_Length包含udhi头长度
        submit_req.Msg_Length = c_length + sizeof (udhi_head);
    }


    //构造完整req
    smgp_head head;
    uint32_t send_len;
    uint32_t sequence_id = common_get_sequence_id(sock);
    head.RequestID = htonl(SMGP_SUBMIT);
    head.SequenceID = htonl(sequence_id);
    if (udhi == -1)
    {
        send_len = sizeof (smgp_head) + sizeof (submit_req) - 1 + c_length; //-1 减去Msg_Content一字节占位空间
    }
    else
    {
        send_len = sizeof (smgp_head) + sizeof (submit_req) - 1 + c_length + sizeof (udhi_head) + sizeof (TVL);
    }

    head.PacketLength = htonl(send_len);
    char *p, *start;
    p = start = (char*) ecalloc(1, send_len);
    memcpy(p, &head, sizeof (smgp_head));
    p += sizeof (smgp_head);
    memcpy(p, &submit_req, offsetof(smgp_submit, Msg_Content));
    p += offsetof(smgp_submit, Msg_Content);

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
    p += sizeof (submit_req.Reserve);

    if (udhi != -1)
    {
        TVL tvl = {0};
        tvl.Tag = htons(0x002);
        tvl.Length = htons(1);
        tvl.value = 0x01; //目前写死一字节长度
        memcpy(p, &tvl, sizeof (tvl));
    }

    array_init(return_value);
    add_assoc_stringl(return_value, "packdata", start, send_len);
    add_assoc_long(return_value, "sequence_id", sequence_id);
    efree(start);

    sock->submit_count++;
}

static
PHP_METHOD(swoole_smgp_coro, activeTest) {

    swoole_get_socket_coro(sock, ZEND_THIS);

    sock->active_test_count++;
    if (sock->active_test_count > sock->active_test_num)
    {
        sock->is_broken = 1;
        sock->active_test_count = 0;
        RETURN_FALSE;
    }

    common_get_sequence_id(sock);
    uint32_t send_len;
    char *send_data = common_make_req(SMGP_ACTIVE_TEST, sock->sequence_id, 0, NULL, &send_len);
    RETVAL_STRINGL(send_data, send_len);
    efree(send_data);

}

static
PHP_METHOD(swoole_smgp_coro, sendOnePack) {

    common_send_one_pack(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}

static
PHP_METHOD(swoole_smgp_coro, logout) {
    swoole_get_socket_coro(sock, ZEND_THIS);

    common_get_sequence_id(sock);
    uint32_t send_len;
    char *send_data = common_make_req(SMGP_TERMINATE, sock->sequence_id, 0, NULL, &send_len);
    RETVAL_STRINGL(send_data, send_len);

    efree(send_data);
}
