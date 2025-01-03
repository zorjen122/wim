#include <cstring>
#include <iostream>
#include <string>


int main()
{
  std::string host = "\"127.0.0.1\"\\n";

  auto first = host.find_first_of("\"");
  auto last = host.find_last_of("\"");

  std::cout << first << "\t" << last << "\n";
  auto s = host.substr(first + 1, last - 1);

  std::cout << "normal: " << host << "\n";
  std::cout << "change after: " << s << "\n";

  short p = 10, p2 = 20;
  char buf[1024]{};

  memcpy(buf, &p, sizeof(short));
  memcpy(&buf[2], &p2, sizeof(short));
  std::cout << "buf: " << *(short*)buf << "\n";
  std::cout << "buf: " << *(short*)&buf[2] << "\n";


  
  return 0;

}