/*
muduo网络库给用户提供了两个主要的类
TcpServer: 用来编写服务器程序的
TcpClient: 用来编写客户端程序的

封装了：epoll+线程池
好处： 能够把网络IO的代码和业务代码分开
                    用户的链接和断开 用户的读写事件
*/

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <iostream>
#include <functional>
#include <string>

using namespace std;
using namespace muduo;
using namespace muduo::net;
using namespace placeholders; //bind占位符

/*基于muduo网络库开发服务器程序
1. 组合Tcp对象
2. 创建EventLoop事件循环对象的指针
3. 明确TcpServer的构造函数需要什么参数，在ChatServer的构造函数中体现
4. 在当前服务器类的构造函数当中，注册处理连接的回调函数和处理事件读写的回调函数
5. 设置合适服务端线程数量，muduo库会自己分配IO线程和worker线程
*/
class ChatServer{
public:
    ChatServer(EventLoop* loop,     //事件循环
            const InetAddress& listenAddr, //IP+Port
            const string& nameArg)  //服务器的名字
            :_server(loop,listenAddr,nameArg), _loop(loop)
        {
            //给服务器注册用户连接和断开的回调 [成员函数有this参数，所以用到绑定器]
            _server.setConnectionCallback(std::bind(&ChatServer::onConnection,this, _1));
            //给服务器注册用户读写事件的回调
            _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));
            //设置服务器端的线程数量 1个IO线程，三个worker线程
            _server.setThreadNum(4);
        }
    //开启事件循环
    void start(){
        _server.start();
    }
private:
    //处理用户连接创建和断开  epoll listenfd accept
    void onConnection(const TcpConnectionPtr& conn)
    {
        if(conn->connected())
        {
            cout<<conn->peerAddress().toIpPort()<<"->"<<conn->localAddress().toIpPort()<<" status:online"<<endl;
        }
        else
        {
            cout<<conn->peerAddress().toIpPort()<<"->"<<conn->localAddress().toIpPort()<<" status:offline"<<endl;
            conn->shutdown(); //close(fd)
            // _loop->quit();
        }
    }
    void onMessage(const TcpConnectionPtr& conn, //连接
                    Buffer* buffer,    //缓冲区
                    Timestamp time)    //接受到数据的时间信息
    {
        string strbuf = buffer->retrieveAllAsString();
        cout<<"recv: "<<strbuf<<" time:"<<time.toString()<<endl;
        conn->send(strbuf);
    }

    muduo::net::TcpServer _server; //#1
    muduo::net::EventLoop *_loop;  //#2 epoll
};

int main()
{
    EventLoop loop;
    InetAddress addr("127.0.0.1", 6000);
    ChatServer server(&loop, addr, "ChatServer");

    server.start(); //listenfd epoll_ctl=>epoll
    loop.loop();    //epoll_wait 以阻塞方式等待新用户连接，已连接用户的读写事件

    return 0;
}