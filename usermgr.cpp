// UserMgr.cpp

#include "UserMgr.h"

UserMgr::UserMgr() : _uid(0) {}

void UserMgr::SetUid(int uid)         { _uid   = uid;   }
void UserMgr::SetName(const QString& name)   { _name  = name;  }
void UserMgr::SetToken(const QString& token) { _token = token; }

int     UserMgr::GetUid()   const { return _uid;   }
QString UserMgr::GetName()  const { return _name;  }
QString UserMgr::GetToken() const { return _token; }
