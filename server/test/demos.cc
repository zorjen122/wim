#include <chrono>
#include <iostream>

#include <memory>
#include <thread>

struct A {
  void foo() {
    t = 10;

    std::cout << "A::foo() called\n";
  }
  ~A() { std::cout << "A::~A() called\n"; }
  int t;
};
std::shared_ptr<A> &get() {
  static std::shared_ptr<A> p(new A());
  return p;
}
void test() {
  // auto p =  = get();
  std::weak_ptr<A> wp = get();
  std::cout << wp.use_count() << "\n";

  try {
    std::thread t([]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      get().reset();
    });
    auto p = wp.lock();
    if (p) {
      std::this_thread::sleep_for(std::chrono::seconds(2));
      p->foo();
    }
    // if (!wp.expired()) {
    //   std::this_thread::sleep_for(std::chrono::milliseconds(500));
    //   wp.lock()->foo();
    // }
    // if (t.joinable()) {
    //   std::cout << "Thread is joinable" << std::endl;
    //   t.join();
    // }
  } catch (std::exception &e) {
    std::cout << e.what() << std::endl;
  }
}
struct B {
  void copy(std::shared_ptr<A> p) {
    p2 = p;
    std::cout << "B::p2: " << p2.use_count() << "\n";
    std::cout << "B::copy(p): " << p.use_count() << "\n";
  }
  ~B() {}
  std::shared_ptr<A> p2;
};
int main() {
  std::shared_ptr<A> p(new A());
  std::shared_ptr<A> p2 = p;
  std::shared_ptr<A> p3(p.get());

  B b;
  b.copy(p2);
  std::cout << "main::p: " << p.use_count() << "\n";
  std::cout << "main::p2: " << p2.use_count() << "\n";
  std::cout << "main::p2: " << p3.use_count() << "\n";

  return 0;
}