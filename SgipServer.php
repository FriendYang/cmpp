<?php

//用来接收delivery和report
Co\run(function () {
    //可以配合Swoole\Process开启多个进程利用多核，每个进程都监听9501端口
    $server = new Co\SGIPServer('0.0.0.0', '9501', false, true);
    $server->sgipSet(
            array(
                'client_ips' => ['192.168.99.100', '127.0.0.1', '192.168.99.102'],//允许的对端ip
//                'client_ips' => '0.0.0.0',//代表不限制ip
                'max_connection' => 1,//每个ip最大连接数
                'bind_timeout' => 5//tcp连接上之后 几s之后不发送bind就断开连接
            )
    );
    //接收到新的连接请求  每个连接进来自动开个协程处理                                                                                                            
    $server->OnConnect(function (Co\SgipConnection $conn) {
        while (1) {
            $data = $conn->recv();
            if (empty($data)) {
                //连接断开 退出协程 
                echo "连接断开\n";
                return;
            }
            //主协程死循环收包，每个包单独开一个协程去处理
            go(function() use ($data) {
                switch ($data['Command']) {
                    case SGIP_REPORT:
                        var_dump($data);
                        break;
                    case SGIP_DELIVER:
                        var_dump($data);
                        break;
                    default://should not happend
                        break;
                };
            });
//            $conn->logout();//发送unbind
        }
    });
    //开始监听端口                                                                                                                       
    $server->start();
});





