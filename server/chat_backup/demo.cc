#include <iostream>
#include <jsoncpp/json/json.h>
#include <jsoncpp/json/reader.h>
#include <vector>

struct A {
  void incr() {
    a++;
    b += "!";
    c += 1.0;
  }
  int a;
  std::string b;
  double c;
};
int main() {
  Json::Value root;
  std::vector<A> vec;
  A a1;
  a1.a = 1;
  a1.b = "hello";
  a1.c = 3.14;
  for (int i = 0; i < 10; ++i)
    vec.push_back(a1), a1.incr();
  Json::Value arr(Json::arrayValue);

  for (auto &a : vec) {
    Json::Value obj(Json::objectValue);
    obj["a"] = a.a;
    obj["b"] = a.b;
    obj["c"] = a.c;
    arr.append(obj);
  }

  root["arr"] = arr;
  std::string value = root.toStyledString();
  // std::cout << value << std::endl;

  Json::Reader reader;
  Json::Value root2;
  reader.parse(value, root2);
  for (Json::Value obj : root2["arr"]) {
    A a;
    a.a = obj["a"].asInt();
    a.b = obj["b"].asString();
    a.c = obj["c"].asDouble();
    std::cout << a.a << " " << a.b << " " << a.c << std::endl;
  }
  return 0;
}