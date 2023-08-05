# ChatServer
使用ngix负载均衡，redis作为消息中间件的集群聊天服务器和客户端代码，软件分层，基于muduo网络库,mysql，使用Cmake构建
功能：
- 登录，注册，注销
- 聊天，发送离线消息
- 创建群组，加入群组，群组聊天
# 环境
1. muduo网络库
2. mysql数据库
3. ngix（tcp模块）
4. redis

在 `ngix.conf` 中添加配置信息，然后启动ngix
```makefile
#ngix tcp loadbalance config
stream{
    upstream MyServer{
        server 127.0.0.1:6000 weight=1 max_fails=3 fail_timeout=30s;
        server 127.0.0.1:6002 weight=1 max_fails=3 fail_timeout=30s;
    }

    server{
        proxy_connect_timeout 1s;
        listen 8000;
        proxy_pass MyServer;
        tcp_nodelay on;
    }
}
```

# 编译
`./autoCompile.sh` 即可编译

# 运行
进入 `/bin` 目录
