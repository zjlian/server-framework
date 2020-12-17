#include "src/hook.h"
#include "src/io_manager.h"
#include "src/log.h"

zjl::Logger::ptr g_logger = GET_ROOT_LOGGER();

int main()
{
    
    zjl::IOManager iom(1, true);
    LOG_DEBUG(g_logger, "main() 开始");
    for(int i = 0; i < 3; i++)
    {
        iom.schedule([](){
            LOG_DEBUG(g_logger, "sleep(2) 开始");
            sleep(2);
            LOG_DEBUG(g_logger, "sleep(2) 结束");
        });
        iom.schedule([](){
            LOG_DEBUG(g_logger, "sleep(3) 开始");
            sleep(3);
            LOG_DEBUG(g_logger, "sleep(3) 结束");
        });
    }
    
    LOG_DEBUG(g_logger, "main() 结束");
    return 0;
}
