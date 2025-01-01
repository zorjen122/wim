#include <json/json.h>
#include <json/reader.h>
#include <json/value.h>
#include <iostream>
#include "post.h"

//  4592  curl -k -X POST http://localhost:8080/user_register \ -d "{email: "xxx", user: "cao", password: "rootroot.", confirm: "rootroot.", icon: "null"}"

int main()
{
    Json::Reader reader;
    Json::Value data;

    data["email"] = "100100@qq.com";
    data["user"] = "cao";
    data["password"] = "root";
    data["confirm"] = "root";
    data["icon"] = "null";
    std::cout << data << "\n";

    Json::StreamWriterBuilder writer;
    const std::string str = Json::writeString(writer, data);

    http_post("localhost", "/user_register", str, "8080");
}