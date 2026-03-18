#ifndef GLOBAL_H
#define GLOBAL_H

#include <QString>

enum ReqId {
    ID_GET_VERIFY_CODE = 1001,
    ID_REG_USER = 1002,
    ID_RESET_PWD = 1003,
    ID_LOGIN_USER = 1004,
    ID_CHAT_LOGIN = 1005,
    ID_CHAT_LOGIN_RSP = 1006,
    ID_SEARCH_USER_REQ = 1007,
    ID_SEARCH_USER_RSP = 1008,
    ID_ADD_FRIEND_REQ = 1009,
    ID_ADD_FRIEND_RSP = 1010,
    ID_GET_FRIEND_REQUESTS_REQ = 1011,
    ID_GET_FRIEND_REQUESTS_RSP = 1012,
    ID_HANDLE_FRIEND_REQUEST_REQ = 1013,
    ID_HANDLE_FRIEND_REQUEST_RSP = 1014,
    ID_FRIEND_REQUESTS_PUSH = 1015,
    ID_FRIEND_LIST_PUSH = 1016,
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
    ERR_JSON = 1,
    ERR_NETWORK = 2,
};

extern QString gate_url_prefix;

#endif // GLOBAL_H
