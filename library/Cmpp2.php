<?php
namespace Swoole\Coroutine;

class Cmpp2
{

    private $cmpp = NULL;
    private $setting = [];
    private $sendChannel = NULL;

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
                $this->sendChannel = new \Swoole\Coroutine\Channel(1);
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

    public function submit($mobile, $text, $ext, float $timeout = -1, int $udhi = -1)
    {
        if ($this->cmpp->errCode === CMPP_CONN_BROKEN) {
            $this->syncErr();
            $this->cmpp = null;
            return FALSE;
        }
        $smsContentLength = mb_strlen($text, 'UTF-8');
        if ($smsContentLength <= 70) { // 短短信
            $unicode_text = mb_convert_encoding($text, "UCS-2BE", "UTF-8");
            $ret = $this->cmpp->submit($mobile, $unicode_text, $ext);
            $cRet = $this->sendChannel->push($ret['packdata'], $timeout);
            if ($cRet === FALSE) {
                $this->errCode = $this->sendChannel->errCode;
                return FALSE;
            }
            $seqArr[] = $ret['sequence_id'];
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
                $ret = $this->cmpp->submit($mobile, $unicode_text, $ext, $udhi, $smsTotalNumber, $i + 1);
                $cRet = $this->sendChannel->push($ret['packdata'], $timeout);
                if ($cRet === FALSE) {
                    $this->errCode = $this->sendChannel->errCode;
                    return FALSE;
                }
                $seqArr[] = $ret['sequence_id'];
            }
        }
        return $seqArr;
    }

    public function recv(float $timeout = -1)
    {
     again:
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
            case CMPP2_TERMINATE_RESP:
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

    public function logout()
    {
        $this->cmpp->logout();
        $this->cmpp = NULL;
    }

}

class_alias("Swoole\\Coroutine\\Cmpp2", "Co\\Cmpp2", true);