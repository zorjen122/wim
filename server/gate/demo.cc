#include <iostream>
#include <memory>

struct C;

struct B;
struct A {
  A(std::shared_ptr<C> c) : _c(c) {
  }
  std::weak_ptr<C> _c;
};

struct B {
  B(std::shared_ptr<C> c) : a(c) {
  }
  A a;
};

struct C : public std::enable_shared_from_this<C> {
  C() : b(shared_from_this()) { std::cout << "C constructor" << std::endl; }
  B b;
};

int main() {
  std::shared_ptr<C> c = std::make_shared<C>();
  std::cout << "c: " << c.use_count() << std::endl;
}
