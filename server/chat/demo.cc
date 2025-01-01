#include <iostream>
#include <memory>
#include <map>
#include <string>
#include <utility>
int main()
{

std::map<std::string, int> mf{};

   auto a =  mf.insert({"Hello", 1});
    auto b = mf.insert({"Hello", 1});

    std::cout << a.second << "\n";
    std::cout << b.second << "\n";

}