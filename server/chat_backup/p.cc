#include <iostream>

void foo() { std::cout << "Hello, world!" << std::endl; }
#include <functional>
#include <map>

std::map<int, std::function<void()>> mf;

void reg(int id, std::function<void()> f) { mf[id] = f; }
int main() {

  reg(1, foo);
  mf[1]();
}