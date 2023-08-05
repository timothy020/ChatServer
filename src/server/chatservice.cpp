#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include "usermodel.hpp"
#include <vector>

using namespace std;
using namespace muduo;

//获取单例对象的接口函数
ChatService* ChatService::instance()
{
    static ChatService service;
    return &service;
}

//注册消息以及对应的Handler回调操作
ChatService::ChatService()
{
    // 用户基本业务管理相关事件处理回调注册
    _msgHandlerMap.insert({LOGIN_MSG, bind(&ChatService::login,this,_1,_2,_3)});
    _msgHandlerMap.insert({REG_MSG, bind(&ChatService::reg,this,_1,_2,_3)});
    _msgHandlerMap.insert({LOGOUT_MSG, bind(&ChatService::logout,this,_1,_2,_3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, bind(&ChatService::oneChat,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, bind(&ChatService::addfriend,this,_1,_2,_3)});

    // 群组业务管理相关事件处理回调注册
    _msgHandlerMap.insert({CREATE_GROUP_MSG, bind(&ChatService::creategroup,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, bind(&ChatService::addgroup,this,_1,_2,_3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, bind(&ChatService::groupchat,this,_1,_2,_3)});

    // 连接redis服务器
    if(_redis.connect())
    {
        //设置上报消息的回调
        _redis.init_notify_handler(bind(&ChatService::handleRedisSubscribeMessage,this,_1,_2));
    }
}

//获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    //记录错误日志， msgid没有对应的事件处理函数
    auto it = _msgHandlerMap.find(msgid);
    if(it == _msgHandlerMap.end())
    {
        //返回一个默认的处理器，空操作
        return [=](const TcpConnectionPtr& conn, json &js, Timestamp time){
            LOG_ERROR << "msgid: " << msgid << " can not find handler!"; 
        };
    }
    else
    {
        return _msgHandlerMap[msgid];
    }
}

//处理登录业务 id pwd
void ChatService::login(const TcpConnectionPtr& conn, json &js, Timestamp time)
{
    int id = js["id"].get<int>();
    string pwd = js["password"];

    User user = _userModel.query(id);

    //用户存在且密码正确
    if(user.getId() == id && user.getPwd() == pwd)
    {
        if(user.getState() == "online")
        {
            //用户已登录，不允许重复登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "this user has logined !!!";
            conn->send(response.dump());
        }
        else
        {
            //登录成功，记录用户连接信息
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }

            // id用户登录成功后，向redis订阅channel(id)
            _redis.subscribe(id);

            //登录成功，更新用户状态 offline => online
            user.setState("online");
            _userModel.updateState(user);

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();

            //查询用户是否有离线消息
            vector<string> msgVec = _offlineMsgModel.query(id);
            if(!msgVec.empty())
            {
                response["offlinemsg"] = msgVec;
                //读取该用户的离线消息后，把该用户所有的离线消息删除
                _offlineMsgModel.remove(id);
            }

            //查询改用户的好友信息并返回
            vector<User> userVec = _friendModel.query(id);
            if(!userVec.empty())
            {
                // response["friends"] = userVec; //无法序列化自定义类型
                vector<string> vec;
                for(auto &user : userVec) //把user类里的内容取出来单独序列化
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec.push_back(js.dump());
                }
                response["friends"] = vec;
            }

            // 查询用户的群组信息
            vector<Group> groupuserVec = _groupModel.queryGroups(id);
            if (!groupuserVec.empty())
            {
                // group:[{groupid:[xxx, xxx, xxx, xxx]}]
                vector<string> groupV;
                for (Group &group : groupuserVec)
                {
                    json grpjson;
                    grpjson["id"] = group.getId();
                    grpjson["groupname"] = group.getName();
                    grpjson["groupdesc"] = group.getDesc();
                    vector<string> userV;
                    for (GroupUser &user : group.getUsers())
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjson["users"] = userV;
                    groupV.push_back(grpjson.dump());
                }

                response["groups"] = groupV;
            }


            conn->send(response.dump());
        }
        
    }
    else
    {
        //登录失败
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        //用户不存在
        if(user.getId() == -1)
            response["errmsg"] = "user not exits !!!";
        //密码错误
        else
            response["errmsg"] = "password doesn't match !!!";
        conn->send(response.dump());
    }
}

//处理注册业务 name password
void ChatService::reg(const TcpConnectionPtr& conn, json &js, Timestamp time)
{
    string name = js["name"];
    string password = js["password"];

    User user;
    user.setName(name);
    user.setPwd(password);

    bool state = _userModel.insert(user);
    if(state)
    {
        //注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else
    {
        //注册失败
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        conn->send(response.dump());
    }
}


//处理用户异常退出
void ChatService::clientCloseException(const TcpConnectionPtr& conn)
{
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        for(auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it)
        {
            if(it->second == conn)
            {
                user.setId(it->first);
                //从连接表中删除用户的链接信息
                _userConnMap.erase(it);
                break;
            }
        }
    }
    
    //用户注销，相当与就是下线，在redis中取消订阅通道
    _redis.unsubscribe(user.getId());

    //更新用户的状态信息： MysqlAPI提供了线程安全
    if(user.getId() != -1)
    {
        user.setState("offline");
        _userModel.updateState(user);
    }
}

//一对一聊天业务 toid
void ChatService::oneChat(const TcpConnectionPtr& conn, json &js, Timestamp time)
{
    int toid = js["toid"].get<int>();

    {
        lock_guard<mutex> lock(mutex);
        auto it = _userConnMap.find(toid);
        if(it != _userConnMap.end())
        {
            //toid在线，转发消息 服务器主动推送消息给toid用户
            it->second->send(js.dump());
            return;
        }
    }

    //查询toid是否在线
    User user = _userModel.query(toid);
    if(user.getState() == "online")
    {
        _redis.publish(toid, js.dump());
        return;
    }

    //toid不在线，存储离线消息
    _offlineMsgModel.insert(toid, js.dump());
}

//服务器异常，业务重置方法
void ChatService::reset()
{
    //把online状态的用户，设置为offline
    _userModel.resetState();
}


//添加好友业务 msgid id friendid
void ChatService::addfriend(const TcpConnectionPtr& conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    //存储好友信息
    _friendModel.insert(userid, friendid);
}

//创建群组业务 userid groupname groupdesc
void ChatService::creategroup(const TcpConnectionPtr& conn, json &js, Timestamp time)
{
    int userid = js["userid"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    //存储新创建的群组信息
    Group group(-1, name, desc);
    if(_groupModel.createGroup(group))
    {
        //存储群组创建人的信息
        _groupModel.addGroup(userid, group.getId(), "creator");
    }
}

//加入群组业务 userid groupid
void ChatService::addgroup(const TcpConnectionPtr& conn, json &js, Timestamp time)
{
    int userid = js["userid"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid, groupid, "normal");
}

//群组聊天业务
void ChatService::groupchat(const TcpConnectionPtr& conn, json &js, Timestamp time)
{
    int userid = js["userid"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);


    //std::map非线程安全， onMessage在多线程环境， 要保证现场安全
    lock_guard<mutex> lock(_connMutex);
    for(int id : useridVec)
    {
        auto it = _userConnMap.find(id);
        if(it != _userConnMap.end())
        {
            //该用户在线： 发群消息
            it->second->send(js.dump());
        }
        else
        {
            //查询toid是否在线
            User user = _userModel.query(id);
            if(user.getState() == "online")
            {
                _redis.publish(id, js.dump());
                return;
            }
            else
            {
                //该用户不在线： 发离线群消息
                _offlineMsgModel.insert(id, js.dump());
            }
        }
    }
}

//注销业务
void ChatService::logout(const TcpConnectionPtr& conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();

    {
        lock_guard<mutex> lock(_connMutex);
        //从用户连接映射表中找到对应的表项，删除
        auto it = _userConnMap.find(userid);
        if(it != _userConnMap.end())
        {
            _userConnMap.erase(it);
        }
    }

    //用户注销，相当与就是下线，在redis中取消订阅通道
    _redis.unsubscribe(userid);

    //更新用户的状态信息： MysqlAPI提供了线程安全
    User user(userid,"","","offline");

    _userModel.updateState(user);
}


void ChatService::handleRedisSubscribeMessage(int userid, string message)
{
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if(it != _userConnMap.end())
    {
        it->second->send(message);
        return;
    }

    //转发消息过程中userid下线，存储离线消息
    _offlineMsgModel.insert(userid, message);
}