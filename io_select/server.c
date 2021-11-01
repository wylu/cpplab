#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

int main() {
    // 1. 创建监听的套接字
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd == -1) {
        perror("socket");
        exit(1);
    }

    // 2. 绑定
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9000);
    // addr.sin_addr.s_addr == INADDR_ANY;
    inet_pton(AF_INET, "0.0.0.0", &addr.sin_addr.s_addr);
    int ret = bind(lfd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret == -1) {
        perror("bind");
        exit(1);
    }

    // 3. 设置监听
    ret = listen(lfd, 128);
    if (ret == -1) {
        perror("listen");
        exit(1);
    }

    // 将监听的fd的状态检测委托给内核
    int maxfd = lfd;
    // 初始化检测的读集合
    fd_set rdset;
    fd_set rdtemp;
    // 清零
    FD_ZERO(&rdset);
    // 将监听的lfd设置到检测的读集合中
    FD_SET(lfd, &rdset);
    // 通过select委托内核检测读集合中的文件描述符状态, 检测read缓冲区有没有数据
    // 如果有数据, select解除阻塞返回
    // 应该让内核持续检测
    while (1) {
        // 默认阻塞
        // rdset 中是委托内核检测的所有的文件描述符
        rdtemp = rdset;
        int num = select(maxfd + 1, &rdtemp, NULL, NULL, NULL);
        // rdset中的数据被内核改写了, 只保留了发生变化的文件描述的标志位上的1,
        // 没变化的改为0 只要rdset中的fd对应的标志位为1 -> 缓冲区有数据了 判断
        // 有没有新连接
        if (FD_ISSET(lfd, &rdtemp)) {
            // 接受连接请求，这个调用不阻塞
            struct sockaddr_in cliaddr;
            int cliLen = sizeof(cliaddr);
            int cfd = accept(lfd, (struct sockaddr *)&cliaddr, &cliLen);

            // 得到了有效的文件描述符
            // 通信的文件描述符添加到读集合
            // 在下一轮select检测的时候，就能得到缓冲区的状态
            FD_SET(cfd, &rdset);
            // 重置最大的文件描述符
            maxfd = cfd > maxfd ? cfd : maxfd;
        }

        // 没有新连接，通信
        for (int i = 0; i < maxfd + 1; i++) {
            // 判断从监听的文件描述符之后到maxfd这个范围内的文件描述符是否读缓冲区有数据
            if (i != lfd && FD_ISSET(i, &rdtemp)) {
                // 接受数据
                char buf[10] = {0};
                // 一次只能接收10个字节, 客户端一次发送100个字节
                // 一次是接收不完的, 文件描述符对应的读缓冲区中还有数据
                // 下一轮select检测的时候,
                // 内核还会标记这个文件描述符缓冲区有数据 -> 再读一次
                // 循环会一直持续, 直到缓冲区数据被读完位置
                int len = read(i, buf, sizeof(buf));
                if (len == 0) {
                    printf("客户端关闭了连接。\n");
                    // 将检测的文件描述符从读集合中删除
                    FD_CLR(i, &rdset);
                    close(i);
                } else if (len > 0) {
                    // 收到了数据
                    // printf("%s", buf);
                    // 发送数据
                    write(i, buf, strlen(buf) + 1);
                } else {
                    // 异常
                    perror("read");
                }
            }
        }
    }
    return 0;
}