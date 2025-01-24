#include <climits>
#include <cstring>
#include <iostream>
#include <netinet/in.h>

void foo() {

  int id = 1020;
  int p = ntohs(id);
  int q = htons(p);
  std::cout << p << "\n";
  std::cout << q << "\n";
  char *buf = (char *)&p;
  std::cout << *(unsigned short *)buf << "\n";
}

void goo() {
  int a = 33;
  int k = 3333;
  char buf[1024]{};
  memcpy(buf, &a, sizeof(a));
  memcpy(buf + sizeof(a), &k, sizeof(k));
  std::cout << *(int *)buf << "\n";
  std::cout << *(int *)(buf + sizeof(a)) << "\n";
}
void hoo() {
  unsigned int a = UINT_MAX;
  std::cout << a << "\n";
  std::cout << a + 1 << "\n";
}

int main() {}