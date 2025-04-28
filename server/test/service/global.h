#pragma once

#define __GateTestPath__ "/test"

#define __GateSigninPath__ "/post-signIn"

#define __GateSignupPath__ "/post-signUp"

#define __GateSignoutPath__ "/post-signOut"

#define __GateInitUserInfo__ "/post-init-user-info"

#define __GateForgetPasswordPath__ "/post-forget-password"

#define __GateArrhythmiaPath__ "/post-arrhythmia"

#include <string>
struct Endpoint {
  Endpoint() {}
  Endpoint(const std::string &ip, const std::string &port)
      : ip(ip), port(port) {}
  std::string ip;
  std::string port;
};