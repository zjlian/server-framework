#include "src/io_manager.h"
#include "src/log.h"
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

zjl::Logger::ptr g_logger = GET_ROOT_LOGGER();

void test_fiber()
{
    for (int i = 0; i < 3; i++)
    {
        LOG_INFO(g_logger, "hello test");
        zjl::Fiber::YieldToHold();
    }
}

void TEST_CreateIOManager()
{
    char buffer[1024];
    const char msg[] =  "懂的都懂";
    zjl::IOManager iom(2);
    iom.schedule(test_fiber);
    int sockfd;
    sockaddr_in server_addr{};
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("啊这");
        exit(1);
    }
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8800);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (connect(sockfd, (struct sockaddr*)(&server_addr), sizeof(struct sockaddr)) == -1)
    {
        perror("啊这");
        exit(1);
    }
    else
    {
        fcntl(sockfd, F_SETFL, O_NONBLOCK);
        LOG_INFO(g_logger, "开始了开始了");
        iom.addEventListener(sockfd, zjl::FDEventType::READ, [&]() {
            recv(sockfd, buffer, sizeof(buffer), 0);
            LOG_FMT_INFO(g_logger, "服务端回应: %s", buffer);
            iom.cancelAll(sockfd);
            close(sockfd);
        });
        iom.addEventListener(sockfd, zjl::FDEventType::WRITE, [&]() {
          memcpy(buffer, msg, sizeof(buffer));
          LOG_FMT_INFO(g_logger, "告诉服务端: %s", buffer);
          send(sockfd, buffer, sizeof(buffer), 0);
        });
    }
}

void TEST_timer()
{
    zjl::IOManager iom(2);
    // iom.schedule(test_fiber);
    iom.addTimer(1000, [](){ 
          LOG_INFO(g_logger, "sleep(1000)");
    }, true);
    iom.addTimer(500, [](){ 
          LOG_INFO(g_logger, "sleep(500)");
    }, true);
}

int main()
{
    // TEST_CreateIOManager();
    TEST_timer();
    return 0;
}
