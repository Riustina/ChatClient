#ifndef GLOBAL_H
#define GLOBAL_H
#include <QString>

enum ReqId {
    ID_GET_VERIFY_CODE = 1001,  // 获取验证码
    ID_REG_USER = 1002,         // 注册用户
    ID_RESET_PWD = 1003,        // 重置密码
    ID_LOGIN_USER = 1004,       // 用户登录
    ID_CHAT_LOGIN = 1005,       // 登录聊天服务器
    ID_CHAT_LOGIN_RSP = 1006,   // 登录聊天服务器回包
    ID_SEARCH_USER_REQ = 1007,  // 搜索用户
    ID_SEARCH_USER_RSP = 1008,  // 搜索用户回包
    ID_ADD_FRIEND_REQ = 1009,   // 发起好友申请
    ID_ADD_FRIEND_RSP = 1010,   // 发起好友申请回包
    ID_GET_FRIEND_REQUESTS_REQ = 1011, // 拉取待处理好友申请
    ID_GET_FRIEND_REQUESTS_RSP = 1012, // 拉取待处理好友申请回包
    ID_HANDLE_FRIEND_REQUEST_REQ = 1013, // 处理好友申请
    ID_HANDLE_FRIEND_REQUEST_RSP = 1014, // 处理好友申请回包
};

enum Modules {
    REGISTERMOD = 0,
    RESETMOD = 1,
    LOGINMOD = 2,
};

struct ServerInfo{
    QString Host;
    QString Port;
    QString Token;
    int Uid;
};

enum ErrorCodes {
    SUCCESS = 0,
    ERR_JSON = 1,               // JSON 解析失败
    ERR_NETWORK = 2,            // 网络错误
};

extern QString gate_url_prefix;

#endif // GLOBAL_H
