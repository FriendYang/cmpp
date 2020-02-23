<?php

namespace Swoole\Coroutine;

class Cmpp2
{

    private $cmpp = NULL;
    private $setting = [];
    private $sendChannel = NULL;
    private $recvCid = -1;
    private $submitting = 0;
    private $recvWating = 0;
    private $inbatch = 0;

    public function __construct($set)
    {
        $this->setting = $set;
    }

    public function login($ip, $port, $uname, $pwd, float $timeout = -1)
    {
        if (is_null($this->cmpp)) {
            $this->cmpp = new \Swoole\Coroutine\Cmpp($this->setting);
            $ret = $this->cmpp->dologin($ip, $port, $uname, $pwd, $timeout);
            if (is_array($ret) && $ret['Status'] == 0) {
                $this->sendChannel = new \Swoole\Coroutine\Channel(3);
                //心跳协程
                \Swoole\Coroutine::create(function () {
                    while (1) {
                        \Swoole\Coroutine::sleep($this->setting['active_test_interval']);
                        if (is_null($this->cmpp)) {
                            break; //结束协程
                        }
                        $pingData = $this->cmpp->activeTest();
                        if ($pingData) {
                            $this->sendChannel->push($pingData);
                        }
                    }
                });
                //专门的发送协程
                \Swoole\Coroutine::create(function () {
                    while (1) {
                        $pack = $this->sendChannel->pop();
                        if (is_null($this->cmpp)) {
                            break; //结束协程
                        }
                        $this->cmpp->sendOnePack($pack);
                    }
                });
                return $ret;
            } else {
                $this->errMsg = $this->cmpp->errMsg;
                $this->errCode = $this->cmpp->errCode;
                $this->cmpp = null;
                return $ret;
            }
        } else {
            $this->errMsg = "the connection is connected";
            $this->errCode = CMPP_CONN_CONNECTED;
            return FALSE;
        }
    }

    private function realSubmit($mobile, $unicode_text, $ext, float $timeout = -1, int $udhi = -1, int $smsTotalNumber = -1, int $i = -1)
    {
        again:
        $ret = $this->cmpp->submit($mobile, $unicode_text, $ext, $udhi, $smsTotalNumber, $i);
        if ($ret === FALSE) {
            $this->syncErr();
            $this->cmpp = null;
            return FALSE;
        }
        if (is_double($ret)) {
            if ($this->inbatch) {//证明submitArr触发的realSubmit
                $this->submitting = 0;
                if ($this->recvWating) {
                    \Swoole\Coroutine::resume($this->recvCid);
                }
                \Swoole\Coroutine::sleep($ret);
                $this->submitting = 1;
            } else {
                \Swoole\Coroutine::sleep($ret);
            }
            goto again;
        }

//        $this->submitChannel->push(1, $timeout);
        $cRet = $this->sendChannel->push($ret['packdata'], $timeout);
        if ($cRet === FALSE) {
            $this->errCode = $this->sendChannel->errCode;
            return FALSE;
        }
        return $ret['sequence_id'];
    }

    /*
     * array(
     *   [0] => array(
     *    'mobile'=>'1581142343',
     *    'text'=>'测试短信',
     *    'ext'=>'扩展码',
     *    'udhi'=>'如果是长短信 这里需要填udhi的序列号',
     *    )
     * )
     */
    /*
     * submit优先于recv里面逻辑
     * 如果撞上了recv yield，恢复方式有2种，1是submit超过限速sleep时候唤醒，2是submitArr函数结束。
     */

    public function submitArr($arr, $timeout = -1)
    {
        $this->submitting = $this->inbatch = 1;
        $seqArr = [];
        foreach ($arr as $msg) {
            $tmp = $this->submit($msg['mobile'], $msg['text'], $msg['ext'], $timeout, $msg['udhi']);
            if ($tmp === false) {
                $tmp = [FALSE];
            }
            $seqArr = array_merge($tmp, $seqArr);
        }
        $this->submitting = $this->inbatch = 0;
        if ($this->recvWating) {
            \Swoole\Coroutine::resume($this->recvCid);
        }
        return $seqArr;
    }

    public function submit($mobile, $text, $ext, float $timeout = -1, int $udhi = -1)
    {
        $smsContentLength = mb_strlen($text, 'UTF-8');
        if ($smsContentLength <= 70) { // 短短信
            $unicode_text = mb_convert_encoding($text, "UCS-2BE", "UTF-8");
            $ret = $this->realSubmit($mobile, $unicode_text, $ext, $timeout);
            if ($ret === FALSE) {
                return FALSE;
            }
            $seqArr[] = $ret;
        } else { //长短信时
            if ($udhi == -1 || $udhi >= 255) {
                $this->errMsg = "udhi param error ($udhi)";
                return FALSE;
            }
            $seqArr = [];
            $smsTotalNumber = ceil($smsContentLength / 67);
            for ($i = 0; $i < $smsTotalNumber; $i++) {
                $content = mb_substr($text, $i * 67, 67, 'UTF-8');
                $unicode_text = mb_convert_encoding($content, "UCS-2BE", 'UTF-8');
                $ret = $this->realSubmit($mobile, $unicode_text, $ext, $timeout, $udhi, $smsTotalNumber, $i + 1);
                if ($ret === FALSE) {
                    return FALSE;
                }
                $seqArr[] = $ret;
            }
        }
        return $seqArr;
    }

    public function recv(float $timeout = -1)
    {
        if ($this->recvCid == -1) {
            $this->recvCid = \Swoole\Coroutine::getCid();
        }
        again:

        if ($this->submitting === 1) {
            $this->recvWating = 1;
            \Swoole\Coroutine::suspend();
            $this->recvWating = 0;
        }
        $ret = $this->cmpp->recvOnePack($timeout);
        if ($ret === false) {
            $this->syncErr();
            if ($this->errCode === CMPP_CONN_BROKEN) {
                $this->cmpp = null;
            }
            return FALSE;
        }
        switch ($ret['Command']) {
            case CMPP2_ACTIVE_TEST_RESP:
            case CMPP2_TERMINATE_RESP://对端主动断开给回复的包
                $this->sendChannel->push($ret["packdata"]);
                $ret = $this->cmpp->recvOnePack($timeout);
                goto again;
            case CMPP2_DELIVER:
                $this->sendChannel->push($ret["packdata"]);
                unset($ret["packdata"]);
                return $ret;
            case CMPP2_SUBMIT_RESP:
                return $ret;
            default :
                throw new \Exception("error command " . $ret['Command']);
        }
    }

    private function syncErr()
    {
        $this->errCode = $this->cmpp->errCode;
        $this->errMsg = $this->cmpp->errMsg;
    }

    /*
     * 发送term包 收到回复的term resp后 recv返回false 断开连接
     */

    public function logout()
    {
        $packdata = $this->cmpp->logout();
        $this->sendChannel->push($packdata);
    }

}

class_alias("Swoole\\Coroutine\\Cmpp2", "Co\\Cmpp2", true);
