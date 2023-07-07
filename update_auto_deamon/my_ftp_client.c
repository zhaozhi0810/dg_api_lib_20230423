#include <stdio.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>



#define BUF_SIZE 1024
#define SERVICE_READY 220
#define NEED_PASSWORD 331
#define LOGIN_SUCS 230
#define CONTROL_CLOSE 221
#define PATHNAME_CREATE 257
#define PASV_MODE 227
#define NO_SUCH_FILE 550
#define GET 1
#define PUT 2
#define PWD 3
#define DIR 4
#define CD 5
#define HELP 6
#define QUIT 7

static struct sockaddr_in server;  // 控制连接的socket
static struct hostent* hent;       // 服务器主机hostent*指针
static char user[20] = "ftp_hnhtjc";              // 用户名
static char pass[20] = "123456";              // 密码
static int data_port;              // 数据连接端口

// 报错
static void errorReport(char* err_info) {
    printf("# %s\n", err_info);
    //exit(-1);
}

// 发送命令，参数为 socket号 、 命令标示 和 命令参数
static void sendCommand(int sock_fd, const char* cmd, const char* info) {
    char buf[BUF_SIZE] = {0};
    strcpy(buf, cmd);
    strcat(buf, info);
    strcat(buf, "\r\n");
    if (send(sock_fd, buf, strlen(buf), 0) < 0)
        errorReport("Send command error!");
}

// 控制连接获取服务器应答，返回应答码
static int getReplyCode(int sockfd) {
    int r_code, bytes;
    char buf[BUF_SIZE] = {0}, nbuf[5] = {0};
    if ((bytes = read(sockfd, buf, BUF_SIZE - 2)) > 0) {
        r_code = atoi(buf);
        buf[bytes] = '\0';
        printf("%s", buf);
    }
    else
        return -1;
    if (buf[3] == '-') {
        char* newline = strchr(buf, '\n');
        if (*(newline+1) == '\0') {
            while ((bytes = read(sockfd, buf, BUF_SIZE - 2)) > 0) {
                buf[bytes] = '\0';
                printf("%s", buf);
                if (atoi(buf) == r_code)
                    break;
            }
        }
    }
    if (r_code == PASV_MODE) {
        char* begin = strrchr(buf, ',')+1;
        char* end = strrchr(buf, ')');
        strncpy(nbuf, begin, end - begin);
        nbuf[end-begin] = '\0';
        data_port = atoi(nbuf);
        buf[begin-1-buf] = '\0';
        end = begin - 1;
        begin = strrchr(buf, ',')+1;
        strncpy(nbuf, begin, end - begin);
        nbuf[end-begin] = '\0';
        data_port += 256 * atoi(nbuf);
    }

    return r_code;
}

// 连接到服务器，参数为 IP地址 和 端口号，返回是否连接成功的编号
static int connectToHost(char* ip, char* pt) {
    int sockfd;
    int port = atoi(pt);
    if (port <= 0 || port >= 65536)
        errorReport("Invalid Port Number!");
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    if ((server.sin_addr.s_addr = inet_addr(ip)) < 0) {
        if ((hent = gethostbyname(ip)) != 0)
            memcpy(&server.sin_addr, hent->h_addr, sizeof(&(server.sin_addr)));
        else
            errorReport("Invalid Host!");
    }
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        errorReport("Create Socket Error!");
    if (connect(sockfd, (struct sockaddr*)&server, sizeof(server)) < 0)
        errorReport("Cannot connect to server!");
    printf("Successfully connect to server: %s:%d\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));
    return sockfd;
}

// 用户登录
int userLogin(int sockfd) {
//    memset(user, 0, sizeof(user));
//    memset(pass, 0, sizeof(pass));
//    char buf[BUF_SIZE];
//    printf("Username: ");
//    fgets(buf, sizeof(buf), stdin);
//    if (buf[0] != '\n')
//        strncpy(user, buf, strlen(buf) - 1);
//    else
 //       strncpy(user, "anonymous", 9);
    sendCommand(sockfd, "USER ", user);
    if (getReplyCode(sockfd) == NEED_PASSWORD) {
    //    memset(buf, 0, sizeof(buf));
        // printf("Password: ");
        // fgets(buf, sizeof(buf), stdin);
        // if (buf[0] != '\n')
        //     strncpy(pass, buf, strlen(buf) - 1);
        // else
        //     strncpy(pass, "anonymous", 9);
        sendCommand(sockfd, "PASS ", pass);
        if (getReplyCode(sockfd) != LOGIN_SUCS) {
            printf("Password wrong. ");
            return -1;
        }
        else {
            printf("User %s login successfully!\n", user);
            return 0;
        }
    }
    else {
        printf("User not found! ");
        return -1;
    }
}

// 判断用户命令
static int cmdToNum(char* cmd) {
    cmd[strlen(cmd)-1] = '\0';
    if (strncmp(cmd, "get", 3) == 0)
        return GET;
    if (strncmp(cmd, "put", 3) == 0)
        return PUT;
    if (strcmp(cmd, "pwd") == 0)
        return PWD;
    if (strcmp(cmd, "dir") == 0)
        return DIR;
    if (strncmp(cmd, "cd", 2) == 0)
        return CD;
    if (strcmp(cmd, "?") == 0 || strcmp(cmd, "help") == 0)
        return HELP;
    if (strcmp(cmd, "quit") == 0)
        return QUIT;
    return -1;  // No command
}

// 下载服务器的一个文件
static void cmd_get(int sockfd, char* cmd) {
    int i = 0, data_sock, bytes;
    char filename[BUF_SIZE], buf[BUF_SIZE];

    while (i < strlen(cmd) && cmd[i] != ' ') i++;  //找到空格

    if (i == strlen(cmd)) {   //中间没有空格
        printf("Command error: %s\n", cmd);
        return;
    }

    while (i < strlen(cmd) && cmd[i] == ' ') i++;
    if (i == strlen(cmd)) {
        printf("Command error: %s\n", cmd);
        return;
    }
    strncpy(filename, cmd+i, strlen(cmd+i)+1);

    sendCommand(sockfd, "TYPE ", "I");
    getReplyCode(sockfd);
    sendCommand(sockfd, "PASV", "");
    if (getReplyCode(sockfd) != PASV_MODE) {
        printf("Error!\n");
        return;
    }
    server.sin_port = htons(data_port);
    if ((data_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        errorReport("Create socket error!");

    if (connect(data_sock, (struct sockaddr*)&server, sizeof(server)) < 0)
        errorReport("Cannot connect to server!");
    printf("Data connection successfully: %s:%d\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));
    sendCommand(sockfd, "RETR ", filename);
    if (getReplyCode(sockfd) == NO_SUCH_FILE) {
        close(sockfd);
        return;
    }

    FILE* dst_file;
    if ((dst_file = fopen(filename, "wb")) == NULL) {
        printf("Error!");
        close(sockfd);
        return;
    }
    while ((bytes = read(data_sock, buf, BUF_SIZE)) > 0)
        fwrite(buf, 1, bytes, dst_file);

    close(data_sock);
    getReplyCode(sockfd);
    fclose(dst_file);
}

// 上传给服务器一个文件
static void cmd_put(int sockfd, char* cmd) {
    int i = 0, data_sock, bytes;
    char filename[BUF_SIZE], buf[BUF_SIZE];
    while (i < strlen(cmd) && cmd[i] != ' ') i++;
    if (i == strlen(cmd)) {
        printf("Command error: %s\n", cmd);
        return;
    }
    while (i < strlen(cmd) && cmd[i] == ' ') i++;
    if (i == strlen(cmd)) {
        printf("Command error: %s\n", cmd);
        return;
    }
    strncpy(filename, cmd+i, strlen(cmd+i)+1);

    sendCommand(sockfd, "PASV", "");
    if (getReplyCode(sockfd) != PASV_MODE) {
        printf("Error!");
        return;
    }
    FILE* src_file;
    if ((src_file = fopen(filename, "rb")) == NULL) {
        printf("Error!");
        return;
    }
    server.sin_port = htons(data_port);
    if ((data_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        errorReport("Create socket error!");
    }
    if (connect(data_sock, (struct sockaddr*)&server, sizeof(server)) < 0)
        errorReport("Cannot connect to server!");
    printf("Data connection successfully: %s:%d\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));
    sendCommand(sockfd, "STOR ", filename);
    if (getReplyCode(sockfd) == NO_SUCH_FILE) {
        close(data_sock);
        fclose(src_file);
        return;
    }
    while ((bytes = fread(buf, 1, BUF_SIZE, src_file)) > 0)
        send(data_sock, buf, bytes, 0);

    close(data_sock);
    getReplyCode(sockfd);
    fclose(src_file);
}

// 显示远方当前目录
static void cmd_pwd(int sockfd) {
    sendCommand(sockfd, "PWD", "");
    if (getReplyCode(sockfd) != PATHNAME_CREATE)
        errorReport("Wrong reply for PWD!");
}

// 列出远方当前目录
static void cmd_dir(int sockfd) {
    int data_sock, bytes;
    char buf[BUF_SIZE] = {0};
    sendCommand(sockfd, "PASV", "");
    if (getReplyCode(sockfd) != PASV_MODE) {
        printf("Error!");
        return;
    }
    server.sin_port = htons(data_port);
    if ((data_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        errorReport("Create socket error!");
    if (connect(data_sock, (struct sockaddr*)&server, sizeof(server)) < 0)
        errorReport("Cannot connect to server!");
    printf("Data connection successfully: %s:%d\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));

    sendCommand(sockfd, "LIST ", "-al");
    getReplyCode(sockfd);
    printf("\n");
    // 数据连接获取服务器传输的数据
    while ((bytes = read(data_sock, buf, BUF_SIZE - 2)) > 0) {
        buf[bytes] = '\0';
        printf("%s", buf);
    }
    printf("\n");
    close(data_sock);
    getReplyCode(sockfd);
}

// 改变远方当前目录
static void cmd_cd(int sockfd, char* cmd) {
    int i = 0;
    char buf[BUF_SIZE];
    while (i < strlen(cmd) && cmd[i] != ' ') i++;
    if (i == strlen(cmd)) {
        printf("Command error: %s\n", cmd);
        return;
    }
    while (i < strlen(cmd) && cmd[i] == ' ') i++;
    if (i == strlen(cmd)) {
        printf("Command error: %s\n", cmd);
        return;
    }
    strncpy(buf, cmd+i, strlen(cmd+i)+1);
    sendCommand(sockfd, "CWD ", buf);
    getReplyCode(sockfd);
}

// 帮助
static void cmd_help() {
    printf(" get \t get a file from server.\n");
    printf(" put \t send a file to server.\n");
    printf(" pwd \t get the present directory on server.\n");
    printf(" dir \t list the directory on server.\n");
    printf(" cd \t change the directory on server.\n");
    printf(" ?/help\t help you know how to use the command.\n");
    printf(" quit \t quit client.\n");
}

// 退出
static void cmd_quit(int sockfd) {
    sendCommand(sockfd, "QUIT", "");
    if (getReplyCode(sockfd) == CONTROL_CLOSE)
        printf("Logout.\n");
}

// 运行客户端
// void run(char* ip, char* pt) {
//     int  sockfd = connectToHost(ip, pt);
//     if (getReplyCode(sockfd) != SERVICE_READY)
//         errorReport("Service Connect Error!");
//     if (userLogin(sockfd) != 0)      // 调用登录函数userLogin
//         printf("Please try again.\n");
//     int isQuit = 0;
//     char buf[BUF_SIZE];
//     while (!isQuit) {
//         printf("[Client command] ");
//         fgets(buf, sizeof(buf), stdin);
//         switch (cmdToNum(buf)) {
//             case GET:
//                 cmd_get(sockfd, buf);
//                 break;
//             case PUT:
//                 cmd_put(sockfd, buf);
//                 break;
//             case PWD:
//                 cmd_pwd(sockfd);
//                 break;
//             case DIR:
//                 cmd_dir(sockfd);
//                 break;
//             case CD:
//                 cmd_cd(sockfd, buf);
//                 break;
//             case HELP:
//                 cmd_help();
//                 break;
//             case QUIT:
//                 cmd_quit(sockfd);
//                 isQuit = 1;
//                 break;
//             default:
//                 cmd_help();
//                 break;
//         }
//     }
//     close(sockfd);
// }

// int main(int argc, char* argv[]) {
//     if (argc != 2 && argc != 3) {
//         printf("Usage: %s <host> [<port>]\n", argv[0]);
//         exit(-1);
//     }
//     else if (argc == 2)
//         run(argv[1], "21");
//     else
//         run(argv[1], argv[2]);
// }
// 
// 



// 下载服务器的一个文件
static int cmd_get_file(int sockfd, char* filename) {
    int data_sock, bytes;
    char buf[BUF_SIZE];

    sendCommand(sockfd, "TYPE ", "I");
    getReplyCode(sockfd);
    sendCommand(sockfd, "PASV", "");
    if (getReplyCode(sockfd) != PASV_MODE) {
        printf("Error!\n");
        return -1;
    }
    server.sin_port = htons(data_port);
    if ((data_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
    	errorReport("Create socket error!");
    	return -1;
    }
    if (connect(data_sock, (struct sockaddr*)&server, sizeof(server)) < 0)
    {
    	errorReport("Cannot connect to server!");
    	return -1;
    }
    printf("Data connection successfully: %s:%d\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));
    sendCommand(sockfd, "RETR ", filename);
    if (getReplyCode(sockfd) == NO_SUCH_FILE) {
        close(sockfd);
        return -1;
    }

    FILE* dst_file;
    if ((dst_file = fopen(filename, "wb")) == NULL) {
        printf("Error!");
        close(sockfd);
        return -1;
    }
    while ((bytes = read(data_sock, buf, BUF_SIZE)) > 0)
        fwrite(buf, 1, bytes, dst_file);

    close(data_sock);
    getReplyCode(sockfd);
    fclose(dst_file);

    return 0;  //表示成功
}





// filename 下载的文件名
// host_ip ftp服务器的ip，端口默认为21
// 返回0表示正常，其他表示异常
int ftp_client_get_file(char* filename,char* host_ip)
{
	int isQuit = 0,ret ;
    char buf[BUF_SIZE];
    int  sockfd = connectToHost(host_ip, "21");
    if (getReplyCode(sockfd) != SERVICE_READY)
    {
    	errorReport("Service Connect Error!");
    	return -1;
    }    
    if (userLogin(sockfd) != 0)      // 调用登录函数userLogin
    {
    	printf("userLogin failed.\n");
    	return -1;
    }    
    
   	ret = cmd_get_file(sockfd, filename);
    close(sockfd);

    return ret;
}



