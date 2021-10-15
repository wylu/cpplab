#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

int main(int argc, const char* argv[]) {
    // 创建监听的套接字
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd == -1) {
        perror("socket");
        exit(1);
    }

    // 绑定
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9000);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    // 127.0.0.1
    // inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr.s_addr);

    // 设置端口复用
    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 绑定端口
    int ret = bind(lfd, (struct sockaddr*)&addr, sizeof(addr));
    if (ret == -1) {
        perror("bind");
        exit(1);
    }

    // 监听
    ret = listen(lfd, 64);
    if (ret == -1) {
        perror("listen");
        exit(1);
    }

    fd_set reads, tmp;
    FD_ZERO(&reads);
    FD_SET(lfd, &reads);

    int maxfd = lfd;

    while (1) {
        tmp = reads;
        int ret = select(maxfd + 1, &tmp, NULL, NULL, NULL);
        if (ret == -1) {
            perror("select");
            exit(0);
        }

        if (FD_ISSET(lfd, &tmp)) {
            int cfd = accept(lfd, NULL, NULL);
            FD_SET(cfd, &reads);
            maxfd = cfd > maxfd ? cfd : maxfd;
        }

        for (int i = lfd + 1; i <= maxfd; i++) {
            if (FD_ISSET(i, &tmp)) {
                char buf[1024];
                int len = read(i, buf, sizeof(buf));
                if (len > 0) {
                    printf("client say: %s\n", buf);
                    write(i, buf, len);
                } else if (len == 0) {
                    printf("client disconneted.\n");
                    FD_CLR(i, &reads);
                    close(i);
                } else {
                    perror("read");
                    exit(0);
                }
            }
        }
    }

    return 0;
}
