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

  return 0;

}