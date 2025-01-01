#include "ServiceSystem.h"

#include "Const.h"
#include "MysqlManager.h"
#include "RedisManager.h"
#include "RpcChatClient.h"
#include "RpcStatusClient.h"
#include "UserManager.h"
#include "test.h"

ServiceSystem::ServiceSystem() : _isStop(false) {
  init();
  _worker_thread = std::thread(&ServiceSystem::DoActive, this);
}

ServiceSystem::~ServiceSystem() {
  _isStop = true;
  _consume.notify_one();
  _worker_thread.join();
}

void ServiceSystem::init() {
  _serviceGroup[ID_CHAT_LOGIN_INIT] =
      std::bind(&ServiceSystem::LoginHandler, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3);

  _serviceGroup[ID_SEARCH_USER_REQ] =
      std::bind(&ServiceSystem::SearchInfo, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3);

  _serviceGroup[ID_ADD_FRIEND_REQ] =
      std::bind(&ServiceSystem::AddFriendApply, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3);

  _serviceGroup[ID_AUTH_FRIEND_REQ] =
      std::bind(&ServiceSystem::AuthFriendApply, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3);

  _serviceGroup[ID_TEXT_CHAT_MSG_REQ] =
      std::bind(&ServiceSystem::PushTextMessage, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3);

  _serviceGroup[__test::TEST_PUSH_ID] =
      std::bind(&ServiceSystem::testPush, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3);
}

void ServiceSystem::PushLogicPackage(
    std::shared_ptr<protocol::LogicPackage> msg) {
  std::unique_lock<std::mutex> unique_lk(_mutex);
  _messageGroup.push(msg);
  // ��0��Ϊ1����֪ͨ�ź�
  if (_messageGroup.size() == 1) {
    unique_lk.unlock();
    _consume.notify_one();
  }
}

void ServiceSystem::DoActive() {
  for (;;) {
    std::unique_lock<std::mutex> unique_lk(_mutex);
    // �ж϶���Ϊ�������������������ȴ������ͷ���
    while (_messageGroup.empty() && !_isStop) {
      _consume.wait(unique_lk);
    }

    // �ж��Ƿ�Ϊ�ر�״̬���������߼�ִ��������˳�ѭ��
    if (_isStop) {
      while (!_messageGroup.empty()) {
        auto package = _messageGroup.front();
        std::cout << "[ServiceSystem::DoActive] recv_msg id  is "
                  << package->_recvPackage->id << "\n";
        auto call_back_iter = _serviceGroup.find(package->_recvPackage->id);
        if (call_back_iter == _serviceGroup.end()) {
          _messageGroup.pop();
          continue;
        }
        call_back_iter->second(package->_session, package->_recvPackage->id,
                               std::string(package->_recvPackage->data,
                                           package->_recvPackage->cur_len));
        _messageGroup.pop();
      }
      break;
    }

    // ���û��ͣ������˵��������������
    auto package = _messageGroup.front();
    std::cout << "[ServiceSystem::DoActive] recv_msg id  is "
              << package->_recvPackage->id << "\n";
    auto call_back_iter = _serviceGroup.find(package->_recvPackage->id);
    if (call_back_iter == _serviceGroup.end()) {
      _messageGroup.pop();
      std::cout << "msg id [" << package->_recvPackage->id
                << "] handler not found" << std::endl;
      continue;
    }
    call_back_iter->second(package->_session, package->_recvPackage->id,
                           std::string(package->_recvPackage->data,
                                       package->_recvPackage->cur_len));
    _messageGroup.pop();
  }
}

void ServiceSystem::LoginHandler(std::shared_ptr<ChatSession> session,
                                 const short &msg_id,
                                 const std::string &msg_data) {
  Json::Reader reader;
  Json::Value root;
  reader.parse(msg_data, root);
  Json::Value rtvalue;
  Defer defer([this, &rtvalue, session]() {
    std::string return_str = rtvalue.toStyledString();
    session->Send(return_str, ID_CHAT_LOGIN_INIT_RSP);
  });

  auto uid = root["uid"].asInt();
  auto token = root["token"].asString();
  std::cout << "user login uid is  " << uid << " user token  is " << token
            << "\n";

  // ��redis��ȡ�û�token�Ƿ���ȷ
  std::string uid_str = std::to_string(uid);
  std::string token_key = PREFIX_REDIS_USER_TOKEN + uid_str;
  std::string token_value = "";
  bool success = RedisManager::GetInstance()->Get(token_key, token_value);
  if (!success) {
    rtvalue["error"] = ErrorCodes::UidInvalid;
    return;
  }

  if (token_value != token) {
    rtvalue["error"] = ErrorCodes::TokenInvalid;
    return;
  }

  rtvalue["error"] = ErrorCodes::Success;

  std::string base_key = PREFIX_REDIS_USER_INFO + uid_str;
  auto user_info = std::make_shared<UserInfo>();
  bool b_base = ServiceSystem::GetBaseInfo(base_key, uid, user_info);
  if (!b_base) {
    rtvalue["error"] = ErrorCodes::UidInvalid;
    return;
  }
  rtvalue["uid"] = uid;
  rtvalue["pwd"] = user_info->pwd;
  rtvalue["name"] = user_info->name;
  rtvalue["email"] = user_info->email;
  rtvalue["nick"] = user_info->nick;
  rtvalue["desc"] = user_info->desc;
  rtvalue["sex"] = user_info->sex;
  rtvalue["icon"] = user_info->icon;

  // �����ݿ��ȡ�����б�
  std::vector<std::shared_ptr<ApplyInfo>> apply_list;
  auto b_apply = GetFriendApplyInfo(uid, apply_list);
  if (b_apply) {
    for (auto &apply : apply_list) {
      Json::Value obj;
      obj["name"] = apply->_name;
      obj["uid"] = apply->_uid;
      obj["icon"] = apply->_icon;
      obj["nick"] = apply->_nick;
      obj["sex"] = apply->_sex;
      obj["desc"] = apply->_desc;
      obj["status"] = apply->_status;
      rtvalue["apply_list"].append(obj);
    }
  }

  // ��ȡ�����б�
  std::vector<std::shared_ptr<UserInfo>> friend_list;
  bool b_friend_list = GetFriendList(uid, friend_list);
  for (auto &friend_ele : friend_list) {
    Json::Value obj;
    obj["name"] = friend_ele->name;
    obj["uid"] = friend_ele->uid;
    obj["icon"] = friend_ele->icon;
    obj["nick"] = friend_ele->nick;
    obj["sex"] = friend_ele->sex;
    obj["desc"] = friend_ele->desc;
    obj["back"] = friend_ele->back;
    rtvalue["friend_list"].append(obj);
  }

  auto server_name = Configer::GetInstance().GetValue("SelfServer", "Name");
  // ����¼��������
  auto rd_res = RedisManager::GetInstance()->HGet(
      PREFIX_REDIS_USER_ACTIVE_COUNT, server_name);
  int count = 0;
  if (!rd_res.empty()) {
    count = std::stoi(rd_res);
  }

  count++;
  auto count_str = std::to_string(count);
  RedisManager::GetInstance()->HSet(PREFIX_REDIS_USER_ACTIVE_COUNT, server_name,
                                    count_str);
  // session���û�uid
  session->SetUserId(uid);
  // Ϊ�û����õ�¼ip server������
  std::string ipkey = PREFIX_REDIS_UIP + uid_str;
  RedisManager::GetInstance()->Set(ipkey, server_name);
  // uid��session�󶨹���,�����Ժ����˲���
  UserManager::GetInstance()->SetUserSession(uid, session);

  return;
}

void ServiceSystem::SearchInfo(std::shared_ptr<ChatSession> session,
                               const short &msg_id, const string &msg_data) {
  Json::Reader reader;
  Json::Value root;
  reader.parse(msg_data, root);
  auto uid_str = root["uid"].asString();
  std::cout << "user SearchInfo uid is  " << uid_str << "\n";

  Json::Value rtvalue;

  Defer defer([this, &rtvalue, session]() {
    std::string return_str = rtvalue.toStyledString();
    session->Send(return_str, ID_SEARCH_USER_RSP);
  });

  bool b_digit = IsPureDigit(uid_str);
  if (b_digit) {
    GetUserByUid(uid_str, rtvalue);
  } else {
    GetUserByName(uid_str, rtvalue);
  }
  return;
}

void ServiceSystem::AddFriendApply(std::shared_ptr<ChatSession> session,
                                   const short &msg_id,
                                   const string &msg_data) {
  Json::Reader reader;
  Json::Value root;
  reader.parse(msg_data, root);
  auto uid = root["uid"].asInt();
  auto applyname = root["applyname"].asString();
  auto bakname = root["bakname"].asString();
  auto touid = root["touid"].asInt();
  std::cout << "user login uid is  " << uid << " applyname  is " << applyname
            << " bakname is " << bakname << " touid is " << touid << "\n";

  Json::Value rtvalue;
  rtvalue["error"] = ErrorCodes::Success;
  Defer defer([this, &rtvalue, session]() {
    std::string return_str = rtvalue.toStyledString();
    session->Send(return_str, ID_ADD_FRIEND_RSP);
  });

  // �ȸ������ݿ�
  MysqlManager::GetInstance()->AddFriendApply(uid, touid);

  // ��ѯredis ����touid��Ӧ��server ip
  auto to_str = std::to_string(touid);
  auto to_ip_key = PREFIX_REDIS_UIP + to_str;
  std::string to_ip_value = "";
  bool b_ip = RedisManager::GetInstance()->Get(to_ip_key, to_ip_value);
  if (!b_ip) {
    return;
  }

  auto &cfg = Configer::GetInstance();
  auto self_name = cfg["SelfServer"]["Name"];
  // ֱ��֪ͨ�Է���������Ϣ
  if (to_ip_value == self_name) {
    auto session = UserManager::GetInstance()->GetSession(touid);
    if (session) {
      // ���ڴ�����ֱ�ӷ���֪ͨ�Է�
      Json::Value notify;
      notify["error"] = ErrorCodes::Success;
      notify["applyuid"] = uid;
      notify["name"] = applyname;
      notify["desc"] = "";
      std::string return_str = notify.toStyledString();
      session->Send(return_str, ID_NOTIFY_ADD_FRIEND_REQ);
    }

    return;
  }

  std::string base_key = PREFIX_REDIS_USER_INFO + std::to_string(uid);
  auto apply_info = std::make_shared<UserInfo>();
  bool b_info = GetBaseInfo(base_key, uid, apply_info);

  AddFriendReq add_req;
  add_req.set_applyuid(uid);
  add_req.set_touid(touid);
  add_req.set_name(applyname);
  add_req.set_desc("");
  if (b_info) {
    add_req.set_icon(apply_info->icon);
    add_req.set_sex(apply_info->sex);
    add_req.set_nick(apply_info->nick);
  }

  // ����֪ͨ
  ChatGrpcClient::GetInstance()->NotifyAddFriend(to_ip_value, add_req);
}

void ServiceSystem::AuthFriendApply(std::shared_ptr<ChatSession> session,
                                    const short &msg_id,
                                    const string &msg_data) {
  Json::Reader reader;
  Json::Value root;
  reader.parse(msg_data, root);

  auto uid = root["fromuid"].asInt();
  auto touid = root["touid"].asInt();
  auto back_name = root["back"].asString();
  std::cout << "from " << uid << " auth friend to " << touid << std::endl;

  Json::Value rtvalue;
  rtvalue["error"] = ErrorCodes::Success;
  auto user_info = std::make_shared<UserInfo>();

  std::string base_key = PREFIX_REDIS_USER_INFO + std::to_string(touid);
  bool b_info = GetBaseInfo(base_key, touid, user_info);
  if (b_info) {
    rtvalue["name"] = user_info->name;
    rtvalue["nick"] = user_info->nick;
    rtvalue["icon"] = user_info->icon;
    rtvalue["sex"] = user_info->sex;
    rtvalue["uid"] = touid;
  } else {
    rtvalue["error"] = ErrorCodes::UidInvalid;
  }

  Defer defer([this, &rtvalue, session]() {
    std::string return_str = rtvalue.toStyledString();
    session->Send(return_str, ID_AUTH_FRIEND_RSP);
  });

  // �ȸ������ݿ�
  MysqlManager::GetInstance()->AuthFriendApply(uid, touid);

  // �������ݿ����Ӻ���
  MysqlManager::GetInstance()->AddFriend(uid, touid, back_name);

  // ��ѯredis ����touid��Ӧ��server ip
  auto to_str = std::to_string(touid);
  auto to_ip_key = PREFIX_REDIS_UIP + to_str;
  std::string to_ip_value = "";
  bool b_ip = RedisManager::GetInstance()->Get(to_ip_key, to_ip_value);
  if (!b_ip) {
    return;
  }

  auto &cfg = Configer::GetInstance();
  auto self_name = cfg["SelfServer"]["Name"];
  // ֱ��֪ͨ�Է�����֤ͨ����Ϣ
  if (to_ip_value == self_name) {
    auto session = UserManager::GetInstance()->GetSession(touid);
    if (session) {
      // ���ڴ�����ֱ�ӷ���֪ͨ�Է�
      Json::Value notify;
      notify["error"] = ErrorCodes::Success;
      notify["fromuid"] = uid;
      notify["touid"] = touid;
      std::string base_key = PREFIX_REDIS_USER_INFO + std::to_string(uid);
      auto user_info = std::make_shared<UserInfo>();
      bool b_info = GetBaseInfo(base_key, uid, user_info);
      if (b_info) {
        notify["name"] = user_info->name;
        notify["nick"] = user_info->nick;
        notify["icon"] = user_info->icon;
        notify["sex"] = user_info->sex;
      } else {
        notify["error"] = ErrorCodes::UidInvalid;
      }

      std::string return_str = notify.toStyledString();
      session->Send(return_str, ID_NOTIFY_AUTH_FRIEND_REQ);
    }

    return;
  }

  AuthFriendReq auth_req;
  auth_req.set_fromuid(uid);
  auth_req.set_touid(touid);

  // ����֪ͨ
  ChatGrpcClient::GetInstance()->NotifyAuthFriend(to_ip_value, auth_req);
}

void ServiceSystem::PushTextMessage(std::shared_ptr<ChatSession> session,
                                    const short &msg_id,
                                    const string &msg_data) {
  Json::Reader reader;
  Json::Value root;
  reader.parse(msg_data, root);

  auto uid = root["from"].asInt();
  auto touid = root["to"].asInt();

  const Json::Value arrays = root["message"];

  Json::Value rtvalue;
  rtvalue["from"] = uid;
  rtvalue["to"] = touid;
  rtvalue["message"] = arrays;
  rtvalue["error"] = ErrorCodes::Success;

  Defer defer([this, &rtvalue, session]() {
    std::string return_str = rtvalue.toStyledString();
    session->Send(return_str, ID_TEXT_CHAT_MSG_RSP);
  });

  // ��ѯredis ����touid��Ӧ��server ip
  auto to_str = std::to_string(touid);
  auto to_ip_key = PREFIX_REDIS_UIP + to_str;
  std::string to_ip_value = "";
  bool b_ip = RedisManager::GetInstance()->Get(to_ip_key, to_ip_value);
  if (!b_ip) {
    return;
  }

  auto &cfg = Configer::GetInstance();
  auto self_name = cfg["SelfServer"]["Name"];

  // ֱ��֪ͨ�Է�����֤ͨ����Ϣ
  if (to_ip_value == self_name) {
    auto session = UserManager::GetInstance()->GetSession(touid);
    if (session) {
      // ���ڴ�����ֱ�ӷ���֪ͨ�Է�
      std::string return_str = rtvalue.toStyledString();
      session->Send(return_str, ID_NOTIFY_TEXT_CHAT_MSG_REQ);
    }

    return;
  }

  TextChatMsgReq text_msg_req;
  text_msg_req.set_fromuid(uid);
  text_msg_req.set_touid(touid);
  for (const auto &txt_obj : arrays) {
    auto content = txt_obj["content"].asString();
    auto msgid = txt_obj["msgid"].asString();
    std::cout << "content is " << content << std::endl;
    std::cout << "msgid is " << msgid << std::endl;
    auto *text_msg = text_msg_req.add_textmsgs();
    text_msg->set_msgid(msgid);
    text_msg->set_msgcontent(content);
  }

  // ����֪ͨ todo...
  ChatGrpcClient::GetInstance()->NotifyTextChatMsg(to_ip_value, text_msg_req,
                                                   rtvalue);
}

bool ServiceSystem::IsPureDigit(const std::string &str) {
  for (char c : str) {
    if (!std::isdigit(c)) {
      return false;
    }
  }
  return true;
}

void ServiceSystem::GetUserByUid(std::string uid_str, Json::Value &rtvalue) {
  rtvalue["error"] = ErrorCodes::Success;

  std::string base_key = PREFIX_REDIS_USER_INFO + uid_str;

  // ���Ȳ�redis�в�ѯ�û���Ϣ
  std::string info_str = "";
  bool b_base = RedisManager::GetInstance()->Get(base_key, info_str);
  if (b_base) {
    Json::Reader reader;
    Json::Value root;
    reader.parse(info_str, root);
    auto uid = root["uid"].asInt();
    auto name = root["name"].asString();
    auto pwd = root["pwd"].asString();
    auto email = root["email"].asString();
    auto nick = root["nick"].asString();
    auto desc = root["desc"].asString();
    auto sex = root["sex"].asInt();
    auto icon = root["icon"].asString();
    std::cout << "user  uid is  " << uid << " name  is " << name << " pwd is "
              << pwd << " email is " << email << " icon is " << icon << "\n";

    rtvalue["uid"] = uid;
    rtvalue["pwd"] = pwd;
    rtvalue["name"] = name;
    rtvalue["email"] = email;
    rtvalue["nick"] = nick;
    rtvalue["desc"] = desc;
    rtvalue["sex"] = sex;
    rtvalue["icon"] = icon;
    return;
  }

  auto uid = std::stoi(uid_str);
  // redis��û�����ѯmysql
  // ��ѯ���ݿ�
  std::shared_ptr<UserInfo> user_info = nullptr;
  user_info = MysqlManager::GetInstance()->GetUser(
      uid);  // SELECT * from user where uid = ?
  if (user_info == nullptr) {
    rtvalue["error"] = ErrorCodes::UidInvalid;
    return;
  }

  // �����ݿ�����д��redis����
  Json::Value redis_root;
  redis_root["uid"] = user_info->uid;
  redis_root["pwd"] = user_info->pwd;
  redis_root["name"] = user_info->name;
  redis_root["email"] = user_info->email;
  redis_root["nick"] = user_info->nick;
  redis_root["desc"] = user_info->desc;
  redis_root["sex"] = user_info->sex;
  redis_root["icon"] = user_info->icon;

  RedisManager::GetInstance()->Set(base_key, redis_root.toStyledString());

  // ��������
  rtvalue["uid"] = user_info->uid;
  rtvalue["pwd"] = user_info->pwd;
  rtvalue["name"] = user_info->name;
  rtvalue["email"] = user_info->email;
  rtvalue["nick"] = user_info->nick;
  rtvalue["desc"] = user_info->desc;
  rtvalue["sex"] = user_info->sex;
  rtvalue["icon"] = user_info->icon;
}

void ServiceSystem::GetUserByName(std::string name, Json::Value &rtvalue) {
  rtvalue["error"] = ErrorCodes::Success;

  std::string base_key = PREFIX_REDIS_NAME_INFO + name;

  // ���Ȳ�redis�в�ѯ�û���Ϣ
  std::string info_str = "";
  bool b_base = RedisManager::GetInstance()->Get(base_key, info_str);
  if (b_base) {
    Json::Reader reader;
    Json::Value root;
    reader.parse(info_str, root);
    auto uid = root["uid"].asInt();
    auto name = root["name"].asString();
    auto pwd = root["pwd"].asString();
    auto email = root["email"].asString();
    auto nick = root["nick"].asString();
    auto desc = root["desc"].asString();
    auto sex = root["sex"].asInt();
    std::cout << "user  uid is  " << uid << " name  is " << name << " pwd is "
              << pwd << " email is " << email << "\n";

    rtvalue["uid"] = uid;
    rtvalue["pwd"] = pwd;
    rtvalue["name"] = name;
    rtvalue["email"] = email;
    rtvalue["nick"] = nick;
    rtvalue["desc"] = desc;
    rtvalue["sex"] = sex;
    return;
  }

  // redis��û�����ѯmysql
  // ��ѯ���ݿ�
  std::shared_ptr<UserInfo> user_info = nullptr;
  user_info = MysqlManager::GetInstance()->GetUser(name);
  if (user_info == nullptr) {
    rtvalue["error"] = ErrorCodes::UidInvalid;
    return;
  }

  // �����ݿ�����д��redis����
  Json::Value redis_root;
  redis_root["uid"] = user_info->uid;
  redis_root["pwd"] = user_info->pwd;
  redis_root["name"] = user_info->name;
  redis_root["email"] = user_info->email;
  redis_root["nick"] = user_info->nick;
  redis_root["desc"] = user_info->desc;
  redis_root["sex"] = user_info->sex;

  RedisManager::GetInstance()->Set(base_key, redis_root.toStyledString());

  // ��������
  rtvalue["uid"] = user_info->uid;
  rtvalue["pwd"] = user_info->pwd;
  rtvalue["name"] = user_info->name;
  rtvalue["email"] = user_info->email;
  rtvalue["nick"] = user_info->nick;
  rtvalue["desc"] = user_info->desc;
  rtvalue["sex"] = user_info->sex;
}

bool ServiceSystem::GetBaseInfo(std::string base_key, int uid,
                                std::shared_ptr<UserInfo> &userinfo) {
  // ���Ȳ�redis�в�ѯ�û���Ϣ
  std::string info_str = "";
  bool b_base = RedisManager::GetInstance()->Get(base_key, info_str);
  if (b_base) {
    Json::Reader reader;
    Json::Value root;
    reader.parse(info_str, root);
    userinfo->uid = root["uid"].asInt();
    userinfo->name = root["name"].asString();
    userinfo->pwd = root["pwd"].asString();
    userinfo->email = root["email"].asString();
    userinfo->nick = root["nick"].asString();
    userinfo->desc = root["desc"].asString();
    userinfo->sex = root["sex"].asInt();
    userinfo->icon = root["icon"].asString();
    std::cout << "user login uid is  " << userinfo->uid << " name  is "
              << userinfo->name << " pwd is " << userinfo->pwd << " email is "
              << userinfo->email << "\n";
  } else {
    // redis��û�����ѯmysql
    // ��ѯ���ݿ�
    std::shared_ptr<UserInfo> user_info = nullptr;
    user_info = MysqlManager::GetInstance()->GetUser(uid);
    if (user_info == nullptr) {
      return false;
    }

    userinfo = user_info;

    // �����ݿ�����д��redis����
    Json::Value redis_root;
    redis_root["uid"] = uid;
    redis_root["pwd"] = userinfo->pwd;
    redis_root["name"] = userinfo->name;
    redis_root["email"] = userinfo->email;
    redis_root["nick"] = userinfo->nick;
    redis_root["desc"] = userinfo->desc;
    redis_root["sex"] = userinfo->sex;
    redis_root["icon"] = userinfo->icon;
    RedisManager::GetInstance()->Set(base_key, redis_root.toStyledString());

    b_base = true;
  }

  return b_base;
}

bool ServiceSystem::GetFriendApplyInfo(
    int to_uid, std::vector<std::shared_ptr<ApplyInfo>> &list) {
  // ��mysql��ȡ���������б�
  return MysqlManager::GetInstance()->GetApplyList(to_uid, list, 0, 10);
}

bool ServiceSystem::GetFriendList(
    int self_id, std::vector<std::shared_ptr<UserInfo>> &user_list) {
  // ��mysql��ȡ�����б�
  return MysqlManager::GetInstance()->GetFriendList(self_id, user_list);
}
