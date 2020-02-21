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
extern "C"
{
#include "ext/standard/md5.h"
}
//#define CMPP2                           1
#define PHP_SWOOLE_EXT_CMPP_VERSION     "4.4.13RC1"
#define PHP_SWOOLE_EXT_CMPP_VERSION_ID  1.0

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

#pragma pack (1)                                                                                 

typedef struct _CMPP2HEAD
{
    uint32_t Total_Length;
    uint32_t Command_Id;
    uint32_t Sequence_Id;

} cmpp2_head;

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
    uchar Msg_Fat;
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

#pragma pack ()



#ifdef __APPLE__
#include <libpq-fe.h>
#endif

#ifdef __linux__
#include <postgresql/libpq-fe.h>
#endif

enum query_type
{
    NORMAL_QUERY, META_DATA, PREPARE
};


#define PGSQL_ASSOC           1<<0
#define PGSQL_NUM             1<<1
#define PGSQL_BOTH            (PGSQL_ASSOC|PGSQL_NUM)

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

#endif
