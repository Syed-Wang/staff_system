#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// 操作码

#define REGISTER 1 // 注册
#define LOGIN 2 // 登录
#define QUIT 3 // 退出
#define STATE 4 // 状态
#define INSERT 6 // 添加员工
#define UPDATA 7 // 修改员工信息
#define QUERY 8 // 查询员工信息
#define SHOW 9 // 展示所有员工信息
#define DELE 10 // 删除员工信息
#define MUSK 11 // 管理员验证操作码
#define ERRLOG(err)                                          \
    do {                                                     \
        printf("%s--%s(%d):", __FILE__, __func__, __LINE__); \
        perror(err);                                         \
        exit(-1);                                            \
    } while (0)
typedef struct
{
    int type; // 操作码
    int id; // 工号
    int password; // 密码
    int root; // 权限1表示管理员用户，0表示普通用户
    int age;
    int salary;
    int column;
    int row;
    char sex[8];
    char post[8];
    char state[8]; // 状态
    char name[16]; // 姓名
    char data[512]; // password or word or remark
} MSG;

void root_info(int socketfd, MSG* msg);
void user_info(int socketfd, MSG* msg);
int do_musk(int socketfd, MSG* msg);
void do_register(int socketfd, MSG* msg);
int do_login(int socketfd, MSG* msg);
void do_insert(int socketfd, MSG* msg);
void do_query(int socketfd, MSG* msg);
void do_updata(int socketfd, MSG* msg);
void do_show(int socketfd, MSG* msg);
void do_dele(int socketfd, MSG* msg);
void do_query_myself(int socketfd, MSG* msg);
void do_updata_myself(int socketfd, MSG* msg);

int main(int argc, char* argv[])
{

    int socketfd;
    struct sockaddr_in server_addr;
    MSG msg;
    if (argc < 3) {
        printf("Usage : %s <serv_ip> <serv_port>\n", argv[0]);
        exit(-1);
    }
    if (-1 == (socketfd = socket(PF_INET, SOCK_STREAM, 0))) {
        perror("fail to socket");
        exit(-1);
    }

    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = PF_INET;
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    server_addr.sin_port = htons(atoi(argv[2]));

    if (-1 == connect(socketfd, (struct sockaddr*)&server_addr, sizeof(server_addr))) {
        perror("fail to connect");
        exit(-1);
    }
    // 这里是主界面
    int choose = 0, ret;
    while (1) {
        printf("***********************************\n");
        printf("* 1: register  2: login   3: quit *\n");
        printf("***********************************\n");

        printf("please choose : ");

        if (scanf("%d", &choose) <= 0) {
            perror("scanf");
            exit(-1);
        }

        switch (choose) {
        case REGISTER:
            // 跳到管理员注册界面
            if (1 == (ret = do_musk(socketfd, &msg))) {

                do_register(socketfd, &msg);
            } else {
                printf("password eror,plase try again\n");
            }
            break;
        case LOGIN:
            // 跳到登录界面,通过返回值判断用户是普通用户还是管理员用户
            ret = do_login(socketfd, &msg);
            // 返回1表示管理员用户
            if (1 == ret) {
                root_info(socketfd, &msg);

            } // 返回0表示普通用户
            else if (0 == ret) {
                user_info(socketfd, &msg);
            }
            // 返回其他表示登录失败
            break;
        case QUIT:
            close(socketfd);
            exit(0);
        default:
            printf("intput error,plase try again\n");
            break;
        }
    }
    return 0; // 这个return是主函数的别删
}

// 这里是管理员管理员工信息的界面
void root_info(int socketfd, MSG* msg)
{
    int choose;
    while (1) {
        printf("************************************************************************************\n");
        printf("* 6: add_staff   7: updata_staff  8: query_staff 9:show_info 10 :dele_staff 3:Back *\n");
        printf("************************************************************************************\n");
        printf("please choose : ");

        if (scanf("%d", &choose) <= 0) {
            perror("scanf");
            exit(-1);
        }
        switch (choose) {
        case INSERT:
            // 这里是添加用户的函数
            do_insert(socketfd, msg);
            continue;
        case UPDATA:
            // 这里是修改员工信息的函数
            do_updata(socketfd, msg);
            continue;
        case QUERY:
            // 这里是查询员工信息的函数
            do_query(socketfd, msg);
            continue;
        case SHOW:
            // 这里是查询所有员工信息的函数
            do_show(socketfd, msg);
            continue;
        case DELE:
            // 这里是删除员工信息的函数
            do_dele(socketfd, msg);
            continue;
        case QUIT:
            // 返回上一级
            break;
        default:
            printf("input error,plase try again\n");
            continue;
        }
        break;
    }
    return;
}

// 这里是普通用户信息的界面
void user_info(int socketfd, MSG* msg)
{
    int choose;
    while (1) {
        printf("*************************************************\n");
        printf("* 8: query_myself   7: updata_myself  3: Back   *\n");
        printf("*************************************************\n");
        printf("please choose : ");

        if (scanf("%d", &choose) <= 0) {
            perror("scanf");
            exit(-1);
        }
        switch (choose) {
        case QUERY:
            // 这里是查询个人信息的函数
            do_query_myself(socketfd, msg);
            continue;
        case UPDATA:
            // 这里是修改个人信息的函数
            do_updata_myself(socketfd, msg);
            continue;
        case QUIT:
            // 返回上一级
            break;
        default:
            printf("input error,plase try again\n");
        }
        break;
    }
    return;
}

// 这里是验证管理员的函数
int do_musk(int socketfd, MSG* msg)
{
    memset(msg, 0, sizeof(MSG));
    int ret;
    // 指定为管理员验证操作码
    msg->type = MUSK;
    printf("input root password :\n");
    scanf("%d", &msg->password);
    // 发送数据
    if (-1 == (ret = send(socketfd, msg, sizeof(MSG), 0)))
        ERRLOG("send error");

    // 接收数据并判断密码是否正确，正确继续注册，错误返回上一级

    if (-1 == (ret = recv(socketfd, msg, sizeof(MSG), 0)))
        ERRLOG("send error");
    if (!strcmp(msg->data, "ture")) {
        return 1;
    } else {
        return -1;
    }
    return 0;
}
// 这里是注册管理员的函数
void do_register(int socketfd, MSG* msg)
{
    memset(msg, 0, sizeof(MSG));
    int ret;

    // 指定操作码
    msg->type = REGISTER;
    msg->root = 1;
    printf("input your workid :");
    scanf("%d", &msg->id);
    // 输入密码
    printf("input your password:");
    scanf("%d", &msg->password);
    // 输入管理员用户名
    printf("input conservator name:");
    scanf("%s", msg->name);
    // 发送数据
    if (-1 == (ret = send(socketfd, msg, sizeof(MSG), 0)))
        ERRLOG("send error");

    // 接收数据并输出
    if (-1 == (ret = recv(socketfd, msg, sizeof(MSG), 0)))
        ERRLOG("send error");

    printf("register : %s\n", msg->data);

    return;
}
// 这里是登录的函数
int do_login(int socketfd, MSG* msg)
{
    memset(msg, 0, sizeof(MSG));
    int ret;
    // 设置操作码
    msg->type = LOGIN;
    // 输入id
    printf("input your id:");
    scanf("%d", &msg->id);
    // 输入密码
    printf("input your password:");
    scanf("%d", &msg->password);
    // 发送数据
    if (-1 == (ret = send(socketfd, msg, sizeof(MSG), 0)))
        ERRLOG("send error");

    // 接收数据
    if (-1 == (ret = recv(socketfd, msg, sizeof(MSG), 0)))
        ERRLOG("send error");

    // 判断是管理员用户还是普通用户
    if (strncmp(msg->data, "root", 5) == 0) {
        printf("longin root\n");
        return 1; // 管理员返回1;
    } else if (strncmp(msg->data, "user", 5) == 0) {
        printf("longin user\n");
        return 0; // 普通用户返回0;
    }

    // 登录失败返回-1
    printf("login : %s\n", msg->data);
    return -1;
}
// 这里是添加用户的函数
void do_insert(int socketfd, MSG* msg)
{
    memset(msg, 0, sizeof(MSG));
    int ret;
    msg->type = INSERT;
    printf("intput staff id >>");
    scanf("%d,", &msg->id);
    printf("intput staff password >>");
    scanf("%d", &msg->password);
    printf("intput staff root >>");
    scanf("%d", &msg->root);
    printf("intput staff name >>");
    scanf("%s", msg->name);
    printf("intput staff age >>");
    scanf("%d", &msg->age);
    printf("intput staff sex >>");
    scanf("%s", msg->sex);
    printf("intput staff post >>");
    scanf("%s", msg->post);
    printf("intput staff salary >>");
    scanf("%d", &msg->salary);

    if (-1 == (ret = send(socketfd, msg, sizeof(MSG), 0)))
        ERRLOG("send error");

    if (-1 == (ret = recv(socketfd, msg, sizeof(MSG), 0)))
        ERRLOG("send error");

    if (strncmp(msg->data, "OK", 3) == 0) {
        printf("adduser : %s\n", msg->data);
        return;
    }
    printf("adduser : %s\n", msg->data);
}
// 这里是查询员工信息的函数
void do_query(int socketfd, MSG* msg)
{
    memset(msg, 0, sizeof(MSG));
    int ret, i, j;
    msg->type = QUERY;
    printf("input staff id:");
    scanf("%d", &msg->id);
    if (-1 == (ret = send(socketfd, msg, sizeof(MSG), 0)))
        ERRLOG("send error");

    while (1) {
        if (-1 == (ret = recv(socketfd, msg, sizeof(MSG), 0)))
            ERRLOG("send error");

        if (strcmp(msg->data, "staff info no find") == 0) {
            printf("staff query: %s\n", msg->data);
            return;
        } else if (strcmp(msg->data, "over") == 0) {
            return;
        }
        printf("msg.data = %s\n", msg->data);
    }
    return;
}
// 这里是修改员工信息的函数
void do_updata(int socketfd, MSG* msg)
{
    int ret;
    char choose;
    memset(msg, 0, sizeof(MSG));
    msg->type = UPDATA;
    msg->root = 1;
    printf("plase input updata staff id >>");
    scanf("%d", &msg->id);
    printf("plase input  staff new name >>");
    scanf("%s", msg->name);
    printf("plase input  staff new password >>");
    scanf("%d", &msg->password);
    printf("plase input  staff new root >>");
    scanf("%d", &msg->root);
    printf("plase input  staff new age >>");
    scanf("%d", &msg->age);
    printf("plase input  staff new sex >>");
    scanf("%s", msg->sex);
    printf("plase input  staff new post >>");
    scanf("%s", msg->post);
    printf("plase input  staff new salary >>");
    scanf("%d", &msg->salary);

    if (-1 == (ret = send(socketfd, msg, sizeof(MSG), 0)))
        ERRLOG("send error");

    if (-1 == (ret = recv(socketfd, msg, sizeof(MSG), 0)))
        ERRLOG("send error");

    if (strncmp(msg->data, "OK", 3) == 0) {
        printf("updata staff : %s\n", msg->data);
        return;
    }
    printf("updata staff : %s\n", msg->data);
}
// 这里是查询所有员工信息的函数
void do_show(int socketfd, MSG* msg)
{
    memset(msg, 0, sizeof(MSG));
    int ret;
    msg->type = SHOW;
    if (-1 == (ret = send(socketfd, msg, sizeof(MSG), 0)))
        ERRLOG("send error");

    while (1) {
        if (-1 == (ret = recv(socketfd, msg, sizeof(MSG), 0)))
            ERRLOG("send error");

        if (strcmp(msg->data, "staff info no find") == 0) {
            printf("staff query: %s\n", msg->data);
            return;
        } else if (strcmp(msg->data, "over") == 0) {
            return;
        }
        char buf[512];
        char *start, *end;
        printf("msg.data = %s\n", msg->data);
        sprintf(buf, "%s", msg->data);
        start = buf;
    }
}
// 这里是删除员工信息的函数
void do_dele(int socketfd, MSG* msg)
{
    int ret;
    memset(msg, 0, sizeof(MSG));
    msg->type = DELE;
    printf("intput delete staff id>> ");
    scanf("%d", &msg->id);
    if (-1 == (ret = send(socketfd, msg, sizeof(MSG), 0)))
        ERRLOG("send error");

    if (-1 == (ret = recv(socketfd, msg, sizeof(MSG), 0)))
        ERRLOG("send error");

    if (strncmp(msg->data, "OK", 3) == 0) {
        printf("delete staff : %s\n", msg->data);
        return;
    }
    printf("delete staff : %s\n", msg->data);

    return;
}

void do_query_myself(int socketfd, MSG* msg)
{
    int ret, i, j;
    msg->type = QUERY;
    printf("id = %d\n", msg->id);
    if (-1 == (ret = send(socketfd, msg, sizeof(MSG), 0)))
        ERRLOG("send error");

    // 这里是展示员工信息
    while (1) {
        if (-1 == (ret = recv(socketfd, msg, sizeof(MSG), 0)))
            ERRLOG("send error");

        if (strcmp(msg->data, "staff info no find") == 0) {
            printf("staff query: %s\n", msg->data);
            return;
        } else if (strcmp(msg->data, "over") == 0) {
            return;
        }
        printf("msg.data = %s\n", msg->data);
    }
    return;
}

void do_updata_myself(int socketfd, MSG* msg)
{
    int ret, i, j;
    msg->type = UPDATA;
    msg->root = 0;
    printf("plase input  your new name >>");
    scanf("%s", msg->name);
    printf("plase input  your new password >>");
    scanf("%d", &msg->password);
    printf("plase input  your new age >>");
    scanf("%d", &msg->age);
    printf("plase input  your new sex >>");
    scanf("%s", msg->sex);

    if (-1 == (ret = send(socketfd, msg, sizeof(MSG), 0)))
        ERRLOG("send error");

    if (-1 == (ret = recv(socketfd, msg, sizeof(MSG), 0)))
        ERRLOG("send error");

    printf("updata myinfo : %s\n", msg->data);

    return;
}