#include "server_nb.h"

int main()
{
    //变量声明
    int lfd, addrlen;
    sockaddr_in caddr;
    fd_set readfds, testfds;
    timeval timeout, testout;
    // 变量初始化
    addrlen = sizeof(caddr);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    lfd = server_listen(SERV_PORT);

    FD_ZERO(&readfds);
    FD_SET(0, &readfds);
    FD_SET(lfd, &readfds);

    while(true)
    {
        server_show_menu();
        testfds = readfds;
        testout = timeout;
        //select调用
        int rc = select(lfd + 1, &testfds, NULL, NULL, &testout);
        ERR(rc < 0, "select failed ")
        //检测标准输入是否有数据
        if(FD_ISSET(0, &testfds))
        {
            char buf[100];
            fgets(buf, 100, stdin);
            if(strncmp(buf, "#", 1) == 0) { break; }
        }
        //检测是否有新的客户端连接
        if(FD_ISSET(lfd, &testfds))
        {
            //接受客户端连接
            int cfd = accept(lfd, (sockaddr*)&caddr, (socklen_t*)&addrlen);
            ERR(cfd < 0, "accept failed ")
            //总线程数加1
            pthread_rwlock_wrlock(&rwlock);
            threadcnt++;
            pthread_rwlock_unlock(&rwlock);
            // 创建子线程处理客户端请求
            pthread_t tid;
            pthread_create(&tid, NULL, server_thread, (void*)&cfd);
            pthread_detach(tid);
        }
    }
    close(lfd);
    printf("server exit\n");
    return 0;
}