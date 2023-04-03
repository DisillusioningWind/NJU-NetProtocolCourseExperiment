#include "server_nb.h"

int main()
{
    //变量声明
    //监听套接字
    int lfd;
    //客户端地址
    sockaddr_in caddr;
    int addrlen = sizeof(caddr);
    
    lfd = server_listen(SERV_PORT);

    while(true)
    {
        //接受客户端连接
        int cfd = accept(lfd, (sockaddr*)&caddr, (socklen_t*)&addrlen);
        if(cfd < 0)
        {
            error(1, errno, "accept failed ");
        }
        //创建子线程处理客户端请求
        pthread_t tid;
        pthread_create(&tid, NULL, server_thread, (void*)&cfd);
        pthread_detach(tid);
    }
    close(lfd);
}