# ChatServer
使用ngix负载均衡，redis作为消息中间件的集群聊天服务器和客户端代码。软件分层设计，基于muduo网络库,mysql，使用Cmake构建。

功能：
- 登录，注册，注销
- 聊天，发送离线消息
- 创建群组，加入群组，群组聊天

# 环境
1. muduo网络库
2. mysql数据库
3. ngix（tcp模块）
4. redis
5. Cmake

在 `ngix.conf` 中添加配置信息：对外暴露端口8000，以轮询方式负载均衡到 `127.0.0.1:6000` 和 `127.0.0.1:6002`

如有需要可自行添加服务器
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
- 运行服务器： `./ChatServer 127.0.0.1 port`
- 运行客户端： `./ChatClient IP 8000`

