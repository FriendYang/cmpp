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
        if (is_null($this->cmpp)) {
            $this->cmpp = new \Swoole\Coroutine\Cmpp($set);
        }
    }

    public function login($ip, $port, $uname, $pwd, float $timeout = -1)
    {
        $ret = $this->cmpp->login($ip, $port, $uname, $pwd, $timeout);
        if (is_array($ret) && $ret['Status'] == 0) {
            $this->sendChannel = new \Swoole\Coroutine\Channel(1);
            //心跳协程
            \Swoole\Coroutine::create(function () {
                while (1) {
                    \Swoole\Coroutine::sleep($this->setting['active_test_interval']);
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
                    $this->cmpp->sendOnePack($pack);
                }
            });
            true;
        } else {
            throw new Exception($this->cmpp->errMsg);
        }
    }

    public function submit($mobile, $text, $ext, float $timeout = -1): int
    {
        $ret = $this->cmpp->submit($mobile, $text, $ext, $timeout);
        $this->sendChannel->push($ret['packdata']);
        return $ret['sequence_id'];
    }

    public function recv(float $timeout = -1): array
    {
        $ret = $this->cmpp->recvOnePack($timeout);
        switch ($ret['Command']) {
            case CMPP2_ACTIVE_TEST_RESP:
            case CMPP2_TERMINATE_RESP:
                $this->sendChannel->push($ret["packdata"]);
                return $this->recv($timeout);
            case CMPP2_DELIVER:
                $this->sendChannel->push($ret["packdata"]);
                unset($ret["packdata"]);
                return $ret;
            case CMPP2_SUBMIT_RESP:
                return $ret;
            default :
                throw new Exception("error command " . $ret['Command']);
        }
    }

    public function logout()
    {
        $this->cmpp->logout();
    }

}

class_alias("Swoole\\Coroutine\\Cmpp2", "Co\\Cmpp2", true);
