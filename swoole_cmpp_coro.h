/*
  +----------------------------------------------------------------------+
  | Swoole                                                               |
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
#ifndef SWOOLE_CMPP_H_
#define SWOOLE_CMPP_H_

#include "ext/swoole/config.h"
#include "ext/swoole/php_swoole_cxx.h"
#include "config.h"

using namespace swoole;
using namespace coroutine;

extern "C"
{
#include "ext/standard/md5.h"
}
//#define CMPP2                           1
#define PHP_SWOOLE_EXT_CMPP_VERSION     "4.4.13RC1"
#define PHP_SWOOLE_EXT_CMPP_VERSION_ID  1.0

#define htonll(x)   ((((uint64_t)htonl(x&0xFFFFFFFF)) << 32) + htonl(x >> 32))
#define ntohll(x)   ((((uint64_t)ntohl(x&0xFFFFFFFF)) << 32) + ntohl(x >> 32))

//
//#define PHP_SWOOLE_EXT_CMPP_VERSION     "4.4.13RC1"
//#define PHP_SWOOLE_EXT_CMPP_VERSION_ID  40413
//
//#if PHP_SWOOLE_EXT_CMPP_VERSION_ID != SWOOLE_VERSION_ID
//#error "Ext version does not match the Swoole version"
//#endif

//主机字节序
#define CMPP2_CONNECT  0x00000001 
#define CMPP2_CONNECT_RESP   0x80000001 
#define CMPP2_TERMINATE   0x00000002 
#define CMPP2_TERMINATE_RESP   0x80000002 
#define CMPP2_SUBMIT   0x00000004 
#define CMPP2_SUBMIT_RESP   0x80000004 
#define CMPP2_DELIVER   0x00000005 
#define CMPP2_DELIVER_RESP   0x80000005 
//#define CMPP2_QUERY   0x00000006 
//#define CMPP2_QUERY_RESP   0x80000006 
//#define CMPP2_CANCEL   0x00000007 
//#define CMPP2_CANCEL_RESP   0x80000007 
#define CMPP2_ACTIVE_TEST   0x00000008 
#define CMPP2_ACTIVE_TEST_RESP   0x80000008 


#define CONNECTING        0
#define CONNECTED         1
#define LOGINING          2
#define CONN_NICE         3//空闲
#define CONN_BUSY         4//频率太高
#define CONN_TESTING      6//active test的时候最好不要收发数据
//#define CONN_BUSY         7//频率太高
#define CONN_BROKEN       8
#define BROKEN_MSG        "the connection is broken"

#define PROTOCAL_CMPP2        0
#define PROTOCAL_CMPP3        1

typedef struct
{
    Socket *socket;
    zend_bool is_broken;
    //    Coroutine* co;
    swTimer_node *timer;
    uint32_t sequence_id;
    uint32_t sequence_start;
    uint32_t sequence_end;
    uint32_t active_test_num;
    uint32_t active_test_count; //计数
    uchar protocal;
    double active_test_interval;
    //    double active_test_timeout;
    //about business
    char service_id[10];
    char src_id_prefix[20];
    char sp_id[6];
    char fee_type[2];
    uint32_t submit_limit_100ms; //100ms内最多执行多少条submit
    uint32_t submit_count; //计数
    long start_submit_time;
    zend_object std;
} socket_coro;



#pragma pack (1)                                                                                 

typedef struct _CMPP2HEAD
{
    uint32_t Total_Length;
    uint32_t Command_Id;
    uint32_t Sequence_Id;

} cmpp2_head;

typedef struct _CMPP2ACTIVE_RESP
{
    uchar Reserved;
} cmpp2_active_resp;

typedef struct _CMPP2CONNECT_REQ
{
    unsigned char Source_Addr[6];
    unsigned char AuthenticatorSource[16];
    uchar Version;
    uint32_t Timestamp;
} cmpp2_connect_req;

typedef struct _CMPP2CONNECT_RESP
{
    unsigned char Status;
    unsigned char AuthenticatorISMG[16];
    uchar Version;
} cmpp2_connect_resp;

typedef struct _CMPP2SUBMIT_REQ
{
    ulong Msg_Id;
    uchar Pk_total;
    uchar Pk_number;
    uchar Registered_Delivery;
    uchar Msg_level;
    uchar Service_Id[10];
    uchar Fee_UserType;
    uchar Fee_terminal_Id[21];
    uchar TP_pId;
    uchar TP_udhi;
    uchar Msg_Fmt;
    uchar Msg_src[6];
    uchar FeeType[2];
    uchar FeeCode[6];
    uchar ValId_Time[17];
    uchar At_Time[17];
    uchar Src_Id[21]; //**
    uchar DestUsr_tl;
    uchar Dest_terminal_Id[21]; //**
    uchar Msg_Length;
    uchar* Msg_Content; //**
    uchar Reserve[8];

} cmpp2_submit_req;

typedef struct _CMPP2SUBMIT_RESP
{
    ulong Msg_Id;
    uchar Result;
} cmpp2_submit_resp;

typedef struct _CMPP2DELIVER_REQ
{
    ulong Msg_Id;
    uchar Dest_Id[21];
    uchar Service_Id[10];
    uchar TP_pid;
    uchar TP_udhi;
    uchar Msg_Fmt;
    uchar Src_terminal_Id[21];
    uchar Registered_Delivery;
    uchar Msg_Length;
    uchar Msg_Content[255];
    uchar Reserved[8];
} cmpp2_delivery_req;

typedef struct _CMPP2DELIVER_MSG_CONTENT
{
    ulong Msg_Id;
    uchar Stat[7];
    uchar Submit_time[10];
    uchar Done_time[10];
    uchar Dest_terminal_Id[21];
    uint32_t SMSC_sequence;
} cmpp2_delivery_msg_content;

typedef struct _CMPP2DELIVER_RESP
{
    ulong Msg_Id;
    uchar Result;
} cmpp2_delivery_resp;

typedef struct _CMPP2UDHI_HEAD
{
    uchar v;
    uchar v1;
    uchar v2;
    uchar udhi_id;
    uchar total;
    uchar pos;
} udhi_head;

//*********************************CMPP3********************************

typedef struct _CMPP3SUBMIT_REQ
{
    ulong Msg_Id;
    uchar Pk_total;
    uchar Pk_number;
    uchar Registered_Delivery;
    uchar Msg_level;
    uchar Service_Id[10];
    uchar Fee_UserType;
    uchar Fee_terminal_Id[32];
    uchar Fee_terminal_type;
    uchar TP_pId;
    uchar TP_udhi;
    uchar Msg_Fmt;
    uchar Msg_src[6];
    uchar FeeType[2];
    uchar FeeCode[6];
    uchar ValId_Time[17];
    uchar At_Time[17];
    uchar Src_Id[21]; //**
    uchar DestUsr_tl;
    uchar Dest_terminal_Id[32]; //**
    uchar Dest_terminal_type;
    uchar Msg_Length;
    uchar* Msg_Content; //**
    uchar LinkID[20];

} cmpp3_submit_req;

typedef struct _CMPP3DELIVER_REQ
{
    ulong Msg_Id;
    uchar Dest_Id[21];
    uchar Service_Id[10];
    uchar TP_pid;
    uchar TP_udhi;
    uchar Msg_Fmt;
    uchar Src_terminal_Id[32];
    uchar Src_terminal_type;
    uchar Registered_Delivery;
    uchar Msg_Length;
    uchar Msg_Content[255];
    uchar LinkID[20];
} cmpp3_delivery_req;

typedef struct _CMPP3DELIVER_MSG_CONTENT
{
    ulong Msg_Id;
    uchar Stat[7];
    uchar Submit_time[10];
    uchar Done_time[10];
    uchar Dest_terminal_Id[21];
    uint32_t SMSC_sequence;
} cmpp3_delivery_msg_content;

typedef struct _CMPP3DELIVER_RESP
{
    ulong Msg_Id;
    uint32_t Result;
} cmpp3_delivery_resp;

typedef struct _CMPP3SUBMIT_RESP
{
    ulong Msg_Id;
    uint32_t Result;
} cmpp3_submit_resp;


typedef struct _CMPP3CONNECT_RESP
{
    uint32_t Status;
    uchar AuthenticatorISMG[16];
    uchar Version;
} cmpp3_connect_resp;

#pragma pack ()

static zend_always_inline void cmpp_md5(unsigned char *digest, const char *src, size_t len)
{

    PHP_MD5_CTX ctx;
    PHP_MD5Init(&ctx);
    PHP_MD5Update(&ctx, src, len);
    PHP_MD5Final(digest, &ctx);
    //    make_digest_ex(des, digest, 16);
}

static zend_always_inline cmpp2_head cmpp2_make_head(uint32_t cmd, uint32_t sequence_Id, uint32_t body_len)
{

    cmpp2_head head;
    head.Command_Id = cmd;
    head.Sequence_Id = sequence_Id;
    head.Total_Length = sizeof (cmpp2_head) + body_len;
    return head;
}

static zend_always_inline char* cmpp2_make_req(uint32_t cmd, uint32_t sequence_Id, uint32_t body_len, void *body, uint32_t *out)
{

    cmpp2_head head;
    head.Command_Id = htonl(cmd);
    head.Sequence_Id = htonl(sequence_Id);
    *out = sizeof (cmpp2_head) + body_len;
    head.Total_Length = htonl(*out);

    char *ret = (char*) emalloc(head.Total_Length);
    memcpy(ret, &head, sizeof (cmpp2_head));
    if (body)
    {
        memcpy(ret + sizeof (cmpp2_head), body, body_len);
    }
    return ret;
}

static zend_always_inline long get_current_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static zend_always_inline ulong convert_ll(ulong msg_id)
{
    ulong tmp_long = ntohll(msg_id);
    return tmp_long;
}

static zend_always_inline void format_msg_id(zval *ret_value, ulong msg_id)
{
    char tmp[128] = {0};
    ulong tmp_long = ntohll(msg_id);
    sprintf(tmp, "%lx", tmp_long);
    add_assoc_string(ret_value, "Msg_Id", tmp);
}

static char* make_cmpp2_submit(socket_coro* sock, uint32_t sequence_id, zend_long pk_total, zend_long pk_index, zend_long udhi, char *ext, char *content, char *mobile, uint32_t* out_len)
{

    size_t m_length = strlen(mobile);
    size_t e_length = strlen(ext);
    size_t c_length = strlen(content);

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
    } else
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
    } else
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
    head.Sequence_Id = htonl(sequence_id);
    if (udhi == -1)
    {
        send_len = sizeof (cmpp2_head) + sizeof (submit_req) - 1 + c_length;
    } else
    {
        send_len = sizeof (cmpp2_head) + sizeof (submit_req) - 1 + c_length + sizeof (udhi_head);
    }

    *out_len = send_len;
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
    } else
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

static char* make_cmpp3_submit(socket_coro* sock, uint32_t sequence_id, zend_long pk_total, zend_long pk_index, zend_long udhi, char *ext, char *content, char *mobile, uint32_t* out_len)
{

    size_t m_length = strlen(mobile);
    size_t e_length = strlen(ext);
    size_t c_length = strlen(content);

    //构造submit req
    cmpp3_submit_req submit_req = {0};
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
    } else
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
    submit_req.Dest_terminal_type = 0;
    //手机号
    memcpy(submit_req.Dest_terminal_Id, mobile, m_length);
    if (udhi == -1)
    {
        submit_req.Msg_Length = c_length;
    } else
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
    head.Sequence_Id = htonl(sequence_id);
    if (udhi == -1)
    {
        send_len = sizeof (cmpp2_head) + sizeof (submit_req) - 1 + c_length;
    } else
    {
        send_len = sizeof (cmpp2_head) + sizeof (submit_req) - 1 + c_length + sizeof (udhi_head);
    }

    *out_len = send_len;
    head.Total_Length = htonl(send_len);
    char *p, *start;
    p = start = (char*) emalloc(send_len);
    memcpy(p, &head, sizeof (cmpp2_head));
    p += sizeof (cmpp2_head);
    memcpy(p, &submit_req, offsetof(cmpp3_submit_req, Msg_Content));
    p += offsetof(cmpp3_submit_req, Msg_Content);

    if (udhi == -1)
    {
        memcpy(p, content, c_length);
        p += c_length;
    } else
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

    memcpy(p, submit_req.LinkID, sizeof (submit_req.LinkID));
    return start;

}

static cmpp2_delivery_resp make_cmpp2_delivery(zval *return_value, cmpp2_head *resp_head)
{
    
    add_assoc_long(return_value, "Sequence_Id", ntohl(resp_head->Sequence_Id));
    add_assoc_long(return_value, "Command", CMPP2_DELIVER);
    cmpp2_delivery_req *delivery_req = (cmpp2_delivery_req*) ((char*) resp_head + sizeof (cmpp2_head));
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
        del_resp.Msg_Id = delivery_req->Msg_Id;
        add_assoc_stringl(return_value, "Msg_Content", (char*) delivery_req->Msg_Content, Msg_Length);
    } else
    {
        zval content;
        array_init(&content);
        cmpp2_delivery_msg_content *delivery_content = (cmpp2_delivery_msg_content*) (delivery_req->Msg_Content);
        del_resp.Msg_Id = delivery_content->Msg_Id;
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
    return del_resp;
}


static cmpp3_delivery_resp make_cmpp3_delivery(zval *return_value, cmpp2_head *resp_head)
{
    
    add_assoc_long(return_value, "Sequence_Id", ntohl(resp_head->Sequence_Id));
    add_assoc_long(return_value, "Command", CMPP2_DELIVER);
    cmpp3_delivery_req *delivery_req = (cmpp3_delivery_req*) ((char*) resp_head + sizeof (cmpp2_head));
    format_msg_id(return_value, delivery_req->Msg_Id);
    add_assoc_string(return_value, "Dest_Id", (char*) delivery_req->Dest_Id);
    add_assoc_string(return_value, "Service_Id", (char*) delivery_req->Service_Id);
    add_assoc_long(return_value, "Msg_Fmt", delivery_req->Msg_Fmt);
    add_assoc_string(return_value, "Src_terminal_Id", (char*) delivery_req->Src_terminal_Id);
    add_assoc_long(return_value, "Registered_Delivery", delivery_req->Registered_Delivery);

    //需要push到send的channel的部分
    cmpp3_delivery_resp del_resp;

    uint32_t Msg_Length = delivery_req->Msg_Length;

    if (delivery_req->Registered_Delivery == 0)
    {
        del_resp.Msg_Id = delivery_req->Msg_Id;
        add_assoc_stringl(return_value, "Msg_Content", (char*) delivery_req->Msg_Content, Msg_Length);
    } else
    {
        zval content;
        array_init(&content);
        cmpp2_delivery_msg_content *delivery_content = (cmpp2_delivery_msg_content*) (delivery_req->Msg_Content);
        del_resp.Msg_Id = delivery_content->Msg_Id;
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
    return del_resp;
}



#endif
