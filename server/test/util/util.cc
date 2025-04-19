#include "util.h"

#include <ctime>
#include <iostream>
#include <random>
#include <string>

using namespace std;

// 生成一个随机字符串作为用户名
string generateRandomUserName(int length) {
  const string chars =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  string result;
  random_device rd;
  mt19937 gen(rd());
  uniform_int_distribution<> dis(0, chars.size() - 1);

  for (int i = 0; i < length; ++i) {
    result += chars[dis(gen)];
  }
  return result;
}

// 生成随机电子邮件（Gmail 或 QQ）
string generateRandomEmail() {
  // 随机选择邮箱的域名：Gmail 或 QQ
  random_device rd;
  mt19937 gen(rd());
  uniform_int_distribution<> dis(0, 1); // 0 -> Gmail, 1 -> QQ
  int domainChoice = dis(gen);

  string domain;
  if (domainChoice == 0) {
    domain = "@gmail.com";
  } else {
    domain = "@qq.com";
  }

  // 随机生成用户名，长度为8-12个字符
  int usernameLength = rand() % 5 + 8; // 生成一个8到12之间的随机长度
  string username = generateRandomUserName(usernameLength);

  return username + domain;
}

unsigned long long generateRandomNumber(unsigned long long left,
                                        unsigned long long right) {
  random_device rd;
  mt19937 gen(rd());
  uniform_int_distribution<> dis(left, right);

  return dis(gen);
}
void toNormalString(std::string &str) {
  // 去掉前后的换行符、空格、制表符等

  auto first = str.find_first_of("\"");
  auto last = str.find_last_of("\"");
  if (first == std::string::npos || last == std::string::npos)
    return;

  str = str.substr(first + 1, last - 1);
}

#include <cstdlib>
#include <ctime>

int generateGroupId(int min, int max) {
  std::srand(std::time(0)); // 使用当前时间作为随机数生成器的种子
  return std::rand() % (max - min + 1) + min;
}
