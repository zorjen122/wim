#pragma once

#include <string>

std::string generateRandomUserName(int length);
std::string generateRandomEmail();

#include <vector>
struct User {
    int id;
    int uid;
    std::string username;
    std::string email;
    std::string password;
    std::string host;
    std::string port;
};

using UserManager = std::vector<User>;
void fetchUsersFromDatabase(UserManager*);

int generateRandomNumber(int bound);
void toNormalString(std::string &str) ;