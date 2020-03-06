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


#define SGIP_CONNECT  0x1 
#define SGIP_CONNECT_RESP   0x80000001 
#define SGIP_UNBIND   0x2 
#define SGIP_UNBIND_RESP   0x80000002 
#define SGIP_SUBMIT   0x3 
#define SGIP_SUBMIT_RESP   0x80000003 
#define SGIP_DELIVER   0x4 
#define SGIP_DELIVER_RESP   0x80000004 
#define SGIP_REPORT   0x5
#define SGIP_REPORT_RESP   0x80000005


#define SMGP_CONNECT  0x00000001
#define SMGP_CONNECT_RESP 0x80000001
#define SMGP_TERMINATE 0x00000006
#define SMGP_TERMINATE_RESP 0x80000006
#define SMGP_SUBMIT  0x00000002
#define SMGP_SUBMIT_RESP 0x80000002
#define SMGP_DELIVER 0x00000003
#define SMGP_DELIVER_RESP 0x80000003
#define SMGP_ACTIVE_TEST 0x00000004
#define SMGP_ACTIVE_TEST_RESP 0x80000004



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


#define ERROR_AND_RETURN(str)\
 zend_throw_exception_ex(\
                    NULL, errno,\
                    "%s: %s [%d]",str, strerror(errno), errno\
                    );\
            delete sock->socket;\
            sock->socket = nullptr;\
            RETURN_FALSE;


#define swoole_get_socket_coro(_sock, _zobject) \
        socket_coro* _sock = php_swoole_cmpp_coro_fetch_object(Z_OBJ_P(_zobject)); \
        if (UNEXPECTED(!sock->socket)) \
        { \
            php_swoole_fatal_error(E_ERROR, "you must call Socket constructor first"); \
        } 

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
    uchar fee_type[2];
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

} cmpp_head;

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
    uchar Msg_Content; //**
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
    uchar Msg_Content; //**
    //    uchar LinkID[20];
    uchar Reserve[20]; //hack for union name with template function

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
    uchar Dest_terminal_Id[32];
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




//*****************************SGIP*******************************//

typedef struct _SGIPHEAD
{
    uint32_t Total_Length;
    uint32_t Command_Id;

    union
    {

        struct
        {
            uint32_t Sequence_Id1;
            uint32_t Sequence_Id2;
            uint32_t Sequence_Id3;
        } v;
        uchar Sequence_Id_Char[12];
    } Sequence_Id;
} sgip_head;

//sgip resp是通用格式

typedef struct _SGIP_RESP
{
    uchar Result;
    uchar Reserve[8];
} sgip_resp;

typedef struct _SGIPBIND
{
    uchar Login_Type;
    uchar Login_Name[16];
    uchar Login_Passowrd[16];
    uchar Reserve[8];
} sgip_bind;

typedef struct _SGIPSUBMIT
{
    uchar SPNumber[21]; //相当于srcId
    uchar ChargeNumber[21];
    uchar UserCount;
    uchar UserNumber[21]; //想当于Dest_terminal_Id
    uchar CorpId[5];
    uchar ServiceType[10];
    uchar FeeType;
    uchar FeeValue[6];
    uchar GivenValue[6];
    uchar AgentFlag;
    uchar MorelatetoMTFlag;
    uchar Priority;
    uchar ExpireTime[16];
    uchar ScheduleTime[16];
    uchar ReportFlag;
    uchar TP_pid;
    uchar TP_udhi;
    uchar MessageCoding;
    uchar MessageType;
    uint32_t MessageLength;
    uchar MessageContent;
    uchar Reserve[8];
} sgip_submit;

typedef struct _SGIPDELIVER
{
    uchar UserNumber[21];
    uchar SPNumber[21];
    uchar TP_pid;
    uchar TP_udhi;
    uchar MessageCoding;
    uint32_t MessageLength;
    char* MessageContent;
    uchar Reserved[8];
} sgip_delivery;

typedef struct _SGIPREPORT
{
    uchar SubmitSequenceNumber[12];
    uchar ReportType;
    uchar UserNumber[21];
    uchar State;
    uchar ErrorCode;
    uchar Reserved[8];
} sgip_report;




//*****************************SMGP*******************************//

typedef struct _SMGPHEAD
{
    uint32_t PacketLength;
    uint32_t RequestID;
    uint32_t SequenceID;
} smgp_head;

typedef struct _SMGPLOGIN
{
    uchar ClientID[8];
    uchar AuthenticatorClient[16];
    uchar LoginMode;
    uint32_t TimeStamp;
    uchar ClientVersion;
} smgp_login;

typedef struct _SMGPLOGIN_RESP
{
    uint32_t Status;
    uchar AuthenticatorServer[16];
    uchar ServerVersion;
} smgp_login_resp;

typedef struct _SMGPSUBMIT
{
    uchar MsgType;
    uchar NeedReport;
    uchar Priority;
    uchar ServiceID[10];
    uchar FeeType[2];
    uchar FeeCode[6];
    uchar FixedFee[6];
    uchar MsgFormat;
    uchar ValidTime[17];
    uchar AtTime[17];
    uchar SrcTermID[21]; //**
    uchar ChargeTermID[21]; //**
    uchar DestTermIDCount;
    uchar DestTermID[21];
    uchar Msg_Length;
    uchar Msg_Content; //**
    uchar Reserve[8];
} smgp_submit;

typedef struct TLV_S
{
    uint16_t Tag;
    uint16_t Length;
    uchar value;
} TVL;

typedef struct _SMGPSUBMIT_RESP
{
    uchar MsgID[10];
    uint32_t Status;
} smgp_submit_resp;


typedef struct _SMGPDELIVER
{
    uchar MsgID[10];
    uchar IsReport;
    uchar MsgFormat;
    uchar RecvTime[14];
    uchar SrcTermID[21];
    uchar DestTermID[21];
    uchar MsgLength;
    uchar MsgContent;
    uchar Reserve[8];
} smgp_delivery;

typedef struct _SMGPDELIVER_RESP
{
    uchar MsgID[10];
    uint32_t Status;
} smgp_delivery_resp;


#pragma pack ()



socket_coro* php_swoole_cmpp_coro_fetch_object(zend_object *obj);
void swoole_cmpp_coro_sync_properties(zval *zobject, socket_coro *sock);
void php_swoole_cmpp_coro_free_object(zend_object *object);
uint32_t common_get_sequence_id(socket_coro *obj);
zend_object* php_swoole_cmpp_coro_create_object(zend_class_entry *ce);
void php_swoole_sgip_coro_minit(int module_number);
void php_swoole_smgp_coro_minit(int module_number);

static zend_always_inline void cmpp_md5(unsigned char *digest, const char *src, size_t len)
{

    PHP_MD5_CTX ctx;
    PHP_MD5Init(&ctx);
    PHP_MD5Update(&ctx, src, len);
    PHP_MD5Final(digest, &ctx);
    //    make_digest_ex(des, digest, 16);
}

static zend_always_inline char* common_make_req(uint32_t cmd, uint32_t sequence_Id, uint32_t body_len, void *body, uint32_t *out)
{

    cmpp_head head;
    head.Command_Id = htonl(cmd);
    head.Sequence_Id = htonl(sequence_Id);
    *out = sizeof (cmpp_head) + body_len;
    head.Total_Length = htonl(*out);

    char *ret = (char*) emalloc(head.Total_Length);
    memcpy(ret, &head, sizeof (cmpp_head));
    if (body)
    {
        memcpy(ret + sizeof (cmpp_head), body, body_len);
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

template <typename T>
void*
cmpp_recv_one_pack(socket_coro *sock, uint32_t *out, T &resp_head)
{
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

static zend_always_inline void common_send_one_pack(INTERNAL_FUNCTION_PARAMETERS)
{
    char *data;
    size_t length;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(data, length)
    ZEND_PARSE_PARAMETERS_END_EX(RETURN_FALSE);

    swoole_get_socket_coro(sock, ZEND_THIS);

    Socket::timeout_setter ts(sock->socket, -1, SW_TIMEOUT_WRITE);
    size_t bytes = sock->socket->send_all(data, length);
    swoole_cmpp_coro_sync_properties(ZEND_THIS, sock);

    if (bytes != length)
    {
        sock->is_broken = 1;
    }

    RETURN_LONG(length);
}

/*
 * 将一串char转换成16进制的字符串
 * 由于一个char一个字节，可以表示2个16进制，所以输入size 输出size*2
 */
static inline std::string BinToHex(const std::string &strBin, size_t size, bool bIsUpper/* = false*/)
{
    std::string strHex;
    strHex.resize(size * 2);
    for (size_t i = 0; i < size; i++)
    {
        uint8_t cTemp = strBin[i];
        for (size_t j = 0; j < 2; j++)
        {
            uint8_t cCur = (cTemp & 0x0f);
            if (cCur < 10)
            {
                cCur += '0';
            } else
            {
                cCur += ((bIsUpper ? 'A' : 'a') - 10);
            }
            strHex[2 * i + 1 - j] = cCur;
            cTemp >>= 4;
        }
    }

    return strHex;
}

#endif
