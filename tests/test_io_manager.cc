#include "src/io_manager.h"
#include "src/log.h"

zjl::Logger::ptr g_logger = GET_ROOT_LOGGER();

void test_fiber()
{
    LOG_INFO(g_logger, "hello test");
}

void TEST_CreateIOManager()
{
    zjl::IOManager iom(1);
    iom.schedule(test_fiber);

}

int main()
{
    TEST_CreateIOManager();
    return 0;
}