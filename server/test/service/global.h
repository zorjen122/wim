#pragma once

#define __GateTestPath__ "/test"

#define __GateSigninPath__ "/post-signIn"

#define __GateSignupPath__ "/post-signUp"

#define __GateSignoutPath__ "/post-signOut"

#define __GateForgetPasswordPath__ "/post-forget-password"

#define __GateArrhythmiaPath__ "/post-arrhythmia"

#include <functional>
class Defer {
public:
  Defer(std::function<void()> func) : func(func) {}

  ~Defer() { func(); }

private:
  std::function<void()> func;
};