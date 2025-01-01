#pragma once

#include <iostream>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>

using boost::asio::ip::tcp;

void http_post(const std::string &server, const std::string &path, const std::string &post_data, const std::string &port)
{

    try
    {
        // 创建 io_context 对象
        boost::asio::io_context io_context;

        // 解析服务器地址（IP 或域名）和端口（默认 HTTP 端口 80）
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(server, port);
        // 创建 TCP 套接字并连接到服务器
        tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);

        // 构建 HTTP POST 请求
        std::string request = "POST " + path + " HTTP/1.1\r\n";
        request += "Host: " + server + "\r\n";
        request += "Content-Type: application/json\r\n";
        request += "Content-Length: " + std::to_string(post_data.size()) + "\r\n";
        request += "Connection: close\r\n";
        request += "\r\n";    // 空行表示请求头结束
        request += post_data; // 请求体数据

        // 发送请求到服务器
        boost::asio::write(socket, boost::asio::buffer(request));

        // 接收服务器的响应
        boost::asio::streambuf response;
        boost::asio::read_until(socket, response, "\r\n");

        // 检查 HTTP 响应状态
        std::istream response_stream(&response);
        std::string http_version;
        unsigned int status_code;
        std::string status_message;

        response_stream >> http_version >> status_code;
        std::getline(response_stream, status_message);

        if (!response_stream || http_version.substr(0, 5) != "HTTP/")
        {
            std::cerr << "Invalid response" << "\n";
            return;
        }

        std::cout << "Response code: " << status_code << " " << status_message << "\n";

        // 读取并显示剩余的响应数据
        boost::asio::read(socket, response, boost::asio::transfer_all());
        std::cout << "Response body: " << "\n";
        std::cout << &response << "\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }
}
