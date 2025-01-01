#include "ServiceSystem.h"

#include <random>
#include "test.h"

void ServiceSystem::testPush(std::shared_ptr<ChatSession> session, const short &msg_id, const string &msg_data)
{

    Json::Reader reader;
    Json::Value value;

    bool rt = reader.parse(msg_data, value);
    assert(rt);
    int from = value["from"].asInt();
    std::string msg = value["text"].toStyledString();

    std::random_device rd;                                              // 从随机数发生器获取真随机数
    std::mt19937 gen(rd());                                             // 使用rd()作种子初始化的梅森旋转算法随机数发生器
    std::uniform_int_distribution<> distrib(0, __test::idGroup.size()); // 均匀分布

    std::shared_ptr<ChatSession> sendSession;
    for (;;)
    {
        int v = distrib(gen);
        if (v == from)
            continue;

        int id = __test::idGroup[v];
        sendSession = __test::idMap[id];
        break;
    }
    sendSession->Send(msg, __test::TEST_PUSH_ID);
}
