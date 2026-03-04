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
};

enum Modules {
    REGISTERMOD = 0,
    RESETMOD = 1,
    LOGINMOD = 2,
};

enum ErrorCodes {
    SUCCESS = 0,
    ERR_JSON = 1,               // JSON 解析失败
    ERR_NETWORK = 2,            // 网络错误
};

extern QString gate_url_prefix;

#endif // GLOBAL_H
