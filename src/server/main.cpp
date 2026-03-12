#include "server/Server.hpp"
#include <unistd.h>

int main()
{
    Server server(8080, 10);
    server.start();
    // 打印当前工作目录
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        std::cout << "[Server] Current working directory: " << cwd << std::endl;
    }
    else
    {
        std::cout << "[Server] ERROR: Cannot get current working directory" << std::endl;
    }
    return 0;
}