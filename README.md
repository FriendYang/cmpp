### install

 1. Make sure you have installed the latest version of swoole (swoole version >= 4.4)

 2. build:
    ``` 
    phpize  
    ./configure
    make && make install
    ```
 3. edit your php.ini :
     ```
     extension=swoole_cmpp.so
     ```
### demo 

 ```php

        \Co\run(function() {

        //$o = new \Co\Cmpp3(
        $o = new \Co\Cmpp2(
            [
        'sequence_start' => 100000,
        'sequence_end' => 10000000, //在这个区间循环使用id，重新登录时候将从新在sequence_start开始
        'active_test_interval' => 1.5, //1.5s检测一次
        'active_test_num' => 3, //10次连续失败就切断连接
        'service_id' => "BYHY", //业务类型
        'src_id_prefix' => "10690831", //src_id的前缀+submit的扩展号就是整个src_id
        'submit_per_sec' => 2000, //每秒多少条限速，达到这个速率后submit会自动Co sleep这个协程，睡眠的时间按照剩余的时间来
        //例如每秒100，会分成10分，100ms最多发10条，如果前10ms就发送完了10条，submit的时候会自动Co sleep 90ms。
        'fee_type' => '01', //资费类别
            ]
    );
    $arr = $o->login("127.0.0.1", 7890, "666666", "666666", 10); //10s登录超时
    //$arr是loginResponse
    if ($arr['Status'] === 0) {
        echo "登录成功\n";
    } else {
        var_dump($arr);
        var_dump($o->errMsg);
        var_dump($o->errCode); //此处需要业务自己做重连
        $arr = $o->login("127.0.0.1", 7890, "666666", "666666", 10); //10s登录超时
        var_dump($arr);
        die;
    }
//    发送短信协程
    go(function() use ($o) {
        $text = "测试短信";
        $i = 20000;
        $start = microtime(true) * 1000;
        while ($i--) {
            $req_arr = $o->submit("15811413647", $text, "0000", -1); //默认-1 永不超时
            if ($req_arr === false) {
                if ($o->errCode === CMPP_CONN_BROKEN) {
                    echo "连接断开\n";
                    return;//推出协程
                }
                var_dump($o->errMsg);
            }
//            var_dump($req_arr);
        }
        $end = microtime(true) * 1000;
        echo "take " . ($end - $start) . "\n";
    });
            //长短信mb_strlen($text, 'UTF-8')>70
        //    go(function() use ($o) {
        //        $text = "测试长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长"
        //                . "长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长"
        //                . "长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长"
        //                . "长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长"
        //                . "长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长长"
        //                . "长长长长短信";
        //        $start = microtime(true) * 1000;
        //        $udhi = 0;
        //        $req_arr = $o->submit("15811413647", $text, "0000", -1, $udhi); //长短信要求最后一个udhi参数，需要自己保证多连接别重用，0-255循环，建议udhi小于123
        //        var_dump($req_arr);
        //        $end = microtime(true) * 1000;
        //        echo "take " . ($end - $start) . "\n";
        //    });
            //接收协程
            $GLOBALS['tasking_num'] = 0;
            go(function() use ($o) {
                while (1) {
                    //默认-1永不超时 直到有数据返回；
                    //只会收到submit回执包 或 delivery的请求包
                    $data = $o->recv(-1);
                    if ($data === false && $o->errCode === CMPP_CONN_BROKEN) {
                        echo "连接断开2\n";
                         //推出协程或die掉重新拉起进程 
                        return;
                    }
                    //每个包都开个协程去处理，防止阻塞接收协程，导致数据积压在操作系统缓冲区和心跳包得不到响应
                    go(function() use ($data) {
                        $GLOBALS['tasking_num'] ++;
                        switch ($data['Command']) {
                            case CMPP2_SUBMIT_RESP:
        //                        var_dump($data);
                                //do something
                                /*
                                 * array(4) {
                                  ["Sequence_Id"]=>
                                  int(100002)
                                  ["Command"]=>
                                  string(11) "SUBMIT_RESP"
                                  ["Msg_Id"]=>
                                  int(694057508)
                                  ["Result"]=>
                                  int(0)
                                  }
                                 */
                                break;

                            case CMPP2_DELIVER:
        //                        var_dump($data);
                                //do some thing
                                /*
                                 * array(9) {
                                  ["Sequence_Id"]=>
                                  int(100003)
                                  ["Command"]=>
                                  string(7) "DELIVER"
                                  ["Msg_Id"]=>
                                  int(694057508)
                                  ["Dest_Id"]=>
                                  string(10) "6666663135"
                                  ["Service_Id"]=>
                                  string(4) "BYHY"
                                  ["Msg_Fat"]=>
                                  int(4)
                                  ["Src_terminal_Id"]=>
                                  string(11) "15811413647"
                                  ["Registered_Delivery"]=>
                                  int(1)
                                  Msg_Content有可能是字符串 根据Registered_Delivery字段决定
                                  ["Msg_Content"]=>
                                  array(6) {
                                  ["Msg_Id"]=>
                                  int(694057508)
                                  ["Stat"]=>
                                  string(7) "DELIVRD"
                                  ["Submit_time"]=>
                                  string(20) "20021823392002182339"
                                  ["Done_time"]=>
                                  string(10) "2002182339"
                                  ["Dest_terminal_Id"]=>
                                  string(0) ""
                                  ["SMSC_sequence"]=>
                                  int(100002)
                                  }
                                  }
                                 */
        //                        $o->logout();
                                break;
                            default:
                                break;
                        }
                        $GLOBALS['tasking_num'] --;
                    });
                    //$data包含$SequenceId
                }
            });

            //接收指令的协程 如关闭通道
        //    go(function() use ($o) {
        ////         $client = new Co\Client("127.0.0.1", 9501);
        ////         $client->recv();
        //        //确保'接收协程'里面的数据都处理完了才允许关闭通道 否则co sleep下。
        //        if ($GLOBALS['tasking_num'] == 0) {
        //            $o->logout();
        //        }
        //    });
        });
 ```

### SGIP

参考`SgipClient.php`和`SgipServer.php`

 
### 鸣谢

 特别鸣谢[美联软通](http://www.i5.com.cn/) 资助Swoole开发此协议客户端。
  

