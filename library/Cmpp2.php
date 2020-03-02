<?php

namespace Swoole\Coroutine;

//短信接口基类
abstract class CmppAbstract
{

    public $errCode = 0;
    public $errMsg = "";
    public $ext = NULL;
    public $setting = [];
    public $sendChannel = NULL;

    abstract function login($ip, $port, $uname, $pwd, float $timeout = -1);

    abstract function recv();

    abstract function logout();

    abstract function realSubmit($mobile, $unicode_text, $ext, float $timeout = -1, int $udhi = -1, int $smsTotalNumber = 1, int $i = 1);

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

    public function syncErr()
    {
        $this->errCode = $this->ext->errCode;
        $this->errMsg = $this->ext->errMsg;
    }

}

class Cmpp2 extends CmppAbstract
{//cmpp容器类
//    private $recvCid = -1;
//    private $submitting = 0;
//    private $recvWating = 0;
//    private $inbatch = 0;

    public function __construct($set)
    {
        $this->setting = $set;
    }

    private function connBroken()
    {//连接断开 销毁实例 结束协程
        $this->ext->close();
        $this->ext = NULL;
    }

    public function login($ip, $port, $uname, $pwd, float $timeout = -1)
    {
        if (is_null($this->ext)) {
            $this->ext = new \Swoole\Coroutine\Cmpp($this->setting);
            $ret = $this->ext->dologin($ip, $port, $uname, $pwd, $timeout);
            if (is_array($ret) && $ret['Status'] == 0) {
                $this->sendChannel = new \Swoole\Coroutine\Channel(3);
                //心跳协程
                \Swoole\Coroutine::create(function () {
                    while (1) {
                        \Swoole\Coroutine::sleep($this->setting['active_test_interval']);
                        if (is_null($this->ext)) {
                            break; //结束协程
                        }
                        $pingData = $this->ext->activeTest();
                        if ($pingData === FALSE) {
                            return $this->ext->close();
                        }
                        if ($pingData) {
                            $this->sendChannel->push($pingData);
                        }
                    }
                });
                //专门的发送协程
                \Swoole\Coroutine::create(function () {
                    while (1) {
                        $pack = $this->sendChannel->pop();
                        if (is_null($this->ext)) {
                            break; //结束协程
                        }
                        $this->ext->sendOnePack($pack);
                    }
                });
                return $ret;
            } else {
                $this->errMsg = $this->ext->errMsg;
                $this->errCode = $this->ext->errCode;
                $this->ext = null;
                return $ret;
            }
        } else {
            $this->errMsg = "the connection is connected";
            $this->errCode = CMPP_CONN_CONNECTED;
            return FALSE;
        }
    }

    public function realSubmit($mobile, $unicode_text, $ext, float $timeout = -1, int $udhi = -1, int $smsTotalNumber = 1, int $i = 1)
    {
        again:
        $ret = $this->ext->submit($mobile, $unicode_text, $ext, $udhi, $smsTotalNumber, $i);
        if ($ret === FALSE) {
            $this->syncErr();
            $this->connBroken();
            return FALSE;
        }
        if (is_double($ret)) {
            \Swoole\Coroutine::sleep($ret);
            goto again;
        }

        $cRet = $this->sendChannel->push($ret['packdata'], $timeout);
        if ($cRet === FALSE) {
            $this->errCode = $this->sendChannel->errCode;
            return FALSE;
        }
        return $ret['sequence_id'];
    }

    public function recv(float $timeout = -1)
    {
        again:
        $ret = $this->ext->recvOnePack($timeout);
        if ($ret === false) {
            $this->syncErr();
            if ($this->errCode === CMPP_CONN_BROKEN) {
                $this->connBroken();
            }
            return FALSE;
        }
        switch ($ret['Command']) {
            case CMPP2_ACTIVE_TEST_RESP:
            case CMPP2_TERMINATE_RESP://对端主动断开给回复的包
                $this->sendChannel->push($ret["packdata"]);
                goto again;
            case CMPP2_DELIVER:
                $this->sendChannel->push($ret["packdata"]);
                unset($ret["packdata"]);
                return $ret;
            case CMPP2_SUBMIT_RESP:
                return $ret;
            default :
                return false;
//                throw new \Exception("error command " . $ret['Command']);
        }
    }

    /*
     * 发送term包 收到回复的term resp后 recv返回false 断开连接
     */

    public function logout()
    {
        $packdata = $this->ext->logout();
        $this->sendChannel->push($packdata);
    }

}

class Cmpp3 extends Cmpp2
{

    public function __construct($set)
    {
        $set['protocal'] = 'cmpp3';
        parent::__construct($set);
    }

}

class SgipClient extends CmppAbstract
{//sgip容器类

    public function __construct($set)
    {
        $this->setting = $set;
    }

    public function login($ip, $port, $uname, $pwd, float $timeout = -1)
    {
        if (is_null($this->ext)) {
            $this->ext = new \Swoole\Coroutine\Sgip($this->setting);
            $ret = $this->ext->bind($ip, $port, $uname, $pwd, $timeout);
            if (is_array($ret) && $ret['Result'] == 0) {
                $this->sendChannel = new \Swoole\Coroutine\Channel(3);
                //专门的发送协程
                \Swoole\Coroutine::create(function () {
                    while (1) {
                        $pack = $this->sendChannel->pop();
                        if (is_null($this->ext)) {
                            break; //结束协程
                        }
                        $this->ext->sendOnePack($pack);
                    }
                });
                return $ret;
            } else {
                $this->syncErr();
                $this->ext = null;
                return $ret;
            }
        } else {
            $this->errMsg = "the connection is connected";
            $this->errCode = CMPP_CONN_CONNECTED;
            return FALSE;
        }
    }

    public function realSubmit($mobile, $unicode_text, $ext, float $timeout = -1, int $udhi = -1, int $smsTotalNumber = 1, int $i = 1)
    {
        $ret = $this->ext->submit($mobile, $unicode_text, $ext, $udhi, $smsTotalNumber, $i);
        if ($ret === FALSE) {
            $this->errCode = CMPP_CONN_BROKEN;
            return FALSE;
        }

        $cRet = $this->sendChannel->push($ret['packdata'], $timeout);
        if ($cRet === FALSE) {
            $this->errCode = $this->sendChannel->errCode;
            return FALSE;
        }
        return $ret['sequence_id'];
    }

    public function recv(float $timeout = -1)
    {
        again:
        $ret = $this->ext->recvOnePack($timeout);
        if ($ret === false) {
            $this->syncErr();
            if ($this->ext->errCode != SOCKET_ETIMEDOUT) {
                $this->errCode = CMPP_CONN_BROKEN;
            }
            return FALSE;
        }
        switch ($ret['Command']) {
            case SGIP_UNBIND_RESP://对端主动断开给回复的包
                $this->sendChannel->push($ret["packdata"]);
                goto again;
            case SGIP_SUBMIT_RESP:
                return $ret;
            default :
                $this->errCode = CMPP_CONN_BROKEN;
                return FALSE;
//                throw new \Exception("error command " . $ret['Command']);
        }
    }

    /*
     * 发送term包 收到回复的term resp后 recv返回false 断开连接
     */

    public function logout()
    {
        $this->sendChannel->push(\Swoole\Coroutine\Sgip::unbindPack());
    }

}

class SgipServer extends Server
{
    /*
     * array(
     *    client_ips = ['a','b','c'],
     * *    client_ips = '0.0.0.0'代表不限制
     *    max_connection = 10,//每个ip最多多少个连接
      'bind_timeout'=>5,//tcp连上后 多久不发bind包就close连接。
     * )
     */

    public static $sgipSet = [];
    public static $connLimit = [];

    public function sgipSet($arr)
    {
        self::$sgipSet = $arr;
    }

    public function start(): bool
    {
        if (!isset(self::$sgipSet['client_ips']) || !isset(self::$sgipSet['max_connection']) || !isset(self::$sgipSet['bind_timeout'])) {
            throw new \Exception("sgip set error");
        }
        $this->running = true;
        if ($this->fn == null) {
            $this->errCode = SOCKET_EINVAL;
            return false;
        }
        $socket = $this->socket;
        $this->setting = array_merge($this->setting, [
            'open_length_check' => 1,
            'package_length_type' => 'N',
            'package_length_offset' => 0, //第N个字节是包长度的值
            'package_body_offset' => 0, //第几个字节开始计算长度
            'package_max_length' => 2000000, //协议最大长度
            'open_tcp_nodelay' => true //TCP底层合并传输,true表示关闭
        ]);
        if (!$socket->setProtocol($this->setting)) {
            $this->errCode = SOCKET_EINVAL;
            return false;
        }

        while ($this->running) {
            /** @var $conn Socket */
            $conn = $socket->accept();

            //IP限制
            $peer = $conn->getpeername();
            $client_ips = self::$sgipSet['client_ips'];
            if ($client_ips != "0.0.0.0") {
                if (!in_array($peer['address'], self::$sgipSet['client_ips'])) {
                    $conn->close();
                    continue;
                }
            }

            //连接数限制
            @self::$connLimit[$peer['address']] ++;
            if (self::$connLimit[$peer['address']] > self::$sgipSet['max_connection']) {
                $conn->close();
                self::$connLimit[$peer['address']] --;
                continue;
            }

            if ($conn) {
                //accept会产生新的socket，此处设置粘包选项
                $conn->setProtocol($this->setting);
                if (!$conn->setProtocol($this->setting)) {
                    $this->errCode = SOCKET_EINVAL;
                    return false;
                }
                if (\Swoole\Coroutine::create($this->fn, new SGIPConnection($conn)) < 0) {
                    goto _wait;
                }
            } else {
                if ($socket->errCode == SOCKET_EMFILE or $socket->errCode == SOCKET_ENFILE) {
                    _wait:
                    \Swoole\Coroutine::sleep(1);
                    continue;
                } elseif ($socket->errCode == SOCKET_ETIMEDOUT) {
                    continue;
                } elseif ($socket->errCode == SOCKET_ECANCELED) {
                    break;
                } else {
                    trigger_error("accept failed, Error: {$socket->errMsg}[{$socket->errCode}]", E_USER_WARNING);
                    break;
                }
            }
        }

        return true;
    }

    public function OnConnect(callable $fn)
    {
        $this->fn = $fn;
    }

}

class SgipConnection extends Server\Connection
{

    public $sendChannel = null;
    private $status = "START";
    public $peerName = "";

    public function recv(float $timeout = 0)
    {
        if (is_null($this->sendChannel)) {
            $this->sendChannel = new \Swoole\Coroutine\Channel(3);
            //专门的发送协程
            \Swoole\Coroutine::create(function () {
                while (1) {
                    $pack = $this->sendChannel->pop();
                    $ret = $this->socket->sendAll($pack, -1);
                    if ($ret === false) {//链接出错
                        return NULL;
                    }
                }
            });
        }
        if ($this->status == "START") {
            //bind_timeout
            go(function() {
                \Swoole\Coroutine::sleep(\Co\SgipServer::$sgipSet['bind_timeout']);
                if ($this->status == "START") {
                    $this->socket->close();
                }
            });
        }
        again:
        $raw = $this->socket->recv($timeout);
        if (empty($raw)) {
            $peer = $this->socket->getpeername();
            \Co\SgipServer::$connLimit[$peer['address']] --;
            $this->socket->close();
            return $raw;
        }
        $arr = \Swoole\Coroutine\Sgip::parseServerRecv($raw);
        switch ($arr['Command']) {
            case SGIP_CONNECT:
                $this->status = "BIND";
                $this->sendChannel->push($arr['packdata']); //回复resp
                goto again;
            case SGIP_UNBIND://继续接收，直到收到tcp的fin
                $this->status = "UNBIND";
                $this->sendChannel->push($arr['packdata']); //回复resp
                goto again;
            case SGIP_UNBIND_RESP:
                $this->status = "UNBIND";
                $this->socket->close();
                goto again;
            case SGIP_REPORT:
                $this->sendChannel->push($arr['packdata']); //回复resp
                unset($arr['packdata']);
                return $arr;
            case SGIP_DELIVER:
                $this->sendChannel->push($arr['packdata']); //回复resp
                unset($arr['packdata']);
                return $arr;

            default:
                return NULL;
//                throw new \Exception("error command " . $arr['Command']);
        }
    }

    public function logout()
    {
        $this->sendChannel->push(\Swoole\Coroutine\Sgip::unbindPack());
    }

}

class_alias("Swoole\\Coroutine\\SgipClient", "Co\\SgipClient", true);
class_alias("Swoole\\Coroutine\\SgipServer", "Co\\SgipServer", true);
class_alias("Swoole\\Coroutine\\SgipConnection", "Co\\SgipConnection", true);
class_alias("Swoole\\Coroutine\\Cmpp2", "Co\\Cmpp2", true);
class_alias("Swoole\\Coroutine\\Cmpp3", "Co\\Cmpp3", true);
