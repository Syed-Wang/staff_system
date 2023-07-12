#include <arpa/inet.h>
#include <netinet/in.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define ERRLOG(errmsg)                                       \
    do {                                                     \
        printf("%s--%s(%d):", __FILE__, __func__, __LINE__); \
        perror(errmsg);                                      \
        exit(-1);                                            \
    } while (0)

#define DATABASE "employee_information_table"

// 操作码
#define REGISTER 1 // 注册
#define LOGIN 2 // 登录
#define STATE 4 // 状态标志位
#define INSERT 6 // 添加员工
#define UPDATA 7 // 修改员工信息
#define QUERY 8 // 查询员工信息
#define SHOW 9 // 展示所有员工信息
#define DELE 10 // 删除员工信息
#define MUSK 11 // 管理员验证操作码

#define PASSWORD 123 // 管理员密码
#define ROOT "1" // 判断是否为管理员的标志位

typedef struct
{
    int type; // 操作码
    int id; // 工号
    int password; // 密码
    int root; // 权限1表示管理员用户，0表示普通用户
    int age; // 年龄
    int salary; // 薪资
    int column; // 行
    int row; // 列
    char sex[8]; // 性别
    char post[8]; // 岗位
    char state[8]; // 状态
    char name[16]; // 姓名
    char data[512]; // password or word or remark
} MSG;

sqlite3* proc_init();
int do_client(int connectfd, sqlite3* db);
void do_register(int connectfd, MSG* msg, sqlite3* db);
void do_login(int connectfd, MSG* msg, sqlite3* db);
void do_insert(int connectfd, MSG* msg, sqlite3* db);
void do_updata(int connectfd, MSG* msg, sqlite3* db);
void do_query(int connectfd, MSG* msg, sqlite3* db);
void do_show(int connectfd, MSG* msg, sqlite3* db);
void do_dele(int connectfd, MSG* msg, sqlite3* db);
void do_musk(int connectfd, MSG* msg, sqlite3* db);

int main(int argc, char const* argv[])
{
    // 初始化数据库
    sqlite3* db = proc_init();
    int sockfd = 0, connectfd = 0;

    if (argc < 3) {
        printf("usage : %s<ip> <port>\n", argv[0]);
        exit(-1);
    }
    // 创建套接字
    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("fail to socket");
        exit(-1);
    }
    // 填充网络信息结构体
    struct sockaddr_in server_addr, client_addr;
    bzero(&server_addr, sizeof(server_addr));
    bzero(&client_addr, sizeof(client_addr));
    server_addr.sin_family = PF_INET;
    server_addr.sin_addr.s_addr = inet_addr(argv[1]); // 大端网络字节序
    server_addr.sin_port = htons(atoi(argv[2]));
    socklen_t server_addr_len = sizeof(server_addr);
    // 用来保存客户端网络信息
    socklen_t client_addr_len = sizeof(client_addr);
    // 将套接字与网络信息结构体绑定

    if ((bind(sockfd, (struct sockaddr*)&server_addr, server_addr_len)) < 0)
        ERRLOG("bind error");

    // 监听
    if (-1 == listen(sockfd, 10))
        ERRLOG("listen error");

    int epct, i, ret;
    char buf[128];
    int epfd;
    struct epoll_event event;
    struct epoll_event events[20];

    memset(events, 0, 20 * sizeof(struct epoll_event));
    if (-1 == (epfd = epoll_create(10)))
        ERRLOG("epoll create error");

    printf("epfd  = %d\n", epfd);
    event.data.fd = sockfd;
    // printf("event.data.fd = %d\n", event.data.fd);

    event.events = EPOLLIN;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &event))
        ERRLOG("epoll ctl error");

    while (1) {
        // epoll_wait返回的是准备好的文件描述符的个数
        epct = epoll_wait(epfd, events, 20, -1); // events是接收返回的准备好事件结构体的首地址
        if (epct == -1)
            ERRLOG("epoll wait error");

        for (i = 0; i < epct; i++) {
            if (events[i].data.fd == sockfd) {

                connectfd = accept(events[i].data.fd, NULL, NULL);
                if (-1 == connectfd)
                    ERRLOG("accept error");

                printf("!!!!!!!!!new fd = %d\n", connectfd);
                event.data.fd = connectfd;
                event.events = EPOLLIN;
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, connectfd, &event))
                    ERRLOG("client epoll ctl error");
                continue;
            } else {
                int ret;
                ret = do_client(events[i].data.fd, db);
                if (-1 == ret) {
                    if (epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, &event))
                        ERRLOG(" epoll ctl dele error");
                    printf("fd %d disconnected\n", events[i].data.fd);
                    close(events[i].data.fd);
                }
                continue;
            }
        }
    }

    return 0;
}

int do_client(int connectfd, sqlite3* db)
{
    int ret;
    MSG msg;
    memset(&msg, 0, sizeof(MSG));
    if ((ret = recv(connectfd, &msg, sizeof(MSG), 0)) <= 0)
        return -1;

    printf("type = %d\n", msg.type);
    switch (msg.type) {
    case MUSK: // 这里是解析到需要验证管理员密码的函数
        do_musk(connectfd, &msg, db);
        break;
    case REGISTER: // 这里是解析到需要注册的函数
        do_register(connectfd, &msg, db);
        break;
    case LOGIN: // 这里是解析到需要登录的函数
        do_login(connectfd, &msg, db);
        break;
    case INSERT: // 这里是解析到需要添加员工信息的函数
        do_insert(connectfd, &msg, db);
        break;
    case UPDATA: // 这里是解析到需要修改员工信息的函数
        do_updata(connectfd, &msg, db);
        break;
    case QUERY: // 这里是解析到需要查询员工信息的函数
        do_query(connectfd, &msg, db);
        break;
    case SHOW: // 这里是解析到需要查询所有员工信息的函数
        do_show(connectfd, &msg, db);
        break;
    case DELE: // 这里是解析到需要删除员工信息的函数
        do_dele(connectfd, &msg, db);
        break;
    default:
        return -1;
    }
    return 0;
}

// 系统初始化的函数
// 打开数据文件、尝试建表
sqlite3* proc_init()
{
    // 打开数据库文件
    sqlite3* sql_db = NULL;
    int ret = sqlite3_open(DATABASE, &sql_db);
    if (ret != SQLITE_OK) {
        printf("errcode[%d]  errmsg[%s]\n", ret, sqlite3_errmsg(sql_db));
        exit(-1);
    }
    // 建表
    //  IF NOT EXISTS  表不存在则创建  表存在则直接使用，而不是报错
    // 引号里面sql语句后面不用加分号
    char* errstr = NULL;
    char buff[256] = "CREATE TABLE IF NOT EXISTS employee_table\
	(id INT PRIMARY KEY,password INT,root INT,state char,name CHAR,age INT,sex CHAR,post CHAR,salary INT)";
    // id,password,root,name,age,sex,post,salary
    ret = sqlite3_exec(sql_db, buff, NULL, NULL, &errstr);
    if (ret != SQLITE_OK) {
        printf("errcode[%d]  errmsg[%s]\n", ret, errstr);
        exit(-1);
    }
    // 释放错误信息的空间 防止内存泄漏
    sqlite3_free(errstr);
    return sql_db;
}

// 验证管理员密码的函数
void do_musk(int connectfd, MSG* msg, sqlite3* db)
{
    int ret;
    printf("password = %d\n", msg->password);
    if (PASSWORD == msg->password) {
        strcpy(msg->data, "ture");
    } else {
        strcpy(msg->data, "flase");
    }

    if (-1 == (ret = send(connectfd, msg, sizeof(MSG), 0)))
        ERRLOG("send error");
    return;
}
// 注册的函数
void do_register(int connectfd, MSG* msg, sqlite3* db)
{
    int ret;
    char* errmsg = NULL;
    char sqlstr[512] = { 0 };
    sprintf(sqlstr, "insert into employee_table(id,password,root,name) values(%d, %d,%d,'%s')", msg->id, msg->password, msg->root, msg->name);
    if (sqlite3_exec(db, sqlstr, NULL, NULL, &errmsg) != SQLITE_OK) {
        sprintf(msg->data, "user %s already exist!!!", msg->name);
    } else {
        strcpy(msg->data, "ok");
    }

    if (-1 == (ret = send(connectfd, msg, sizeof(MSG), 0)))
        ERRLOG("send error");

    sqlite3_free(errmsg);
    return;
}
// 登录的函数
void do_login(int connectfd, MSG* msg, sqlite3* db)
{
    char buf[6];
    char sqlstr[512] = { 0 };
    char *errmsg, **result;
    int nrow, ncolumn, ret, i, j;

    // sqlite3_get_table 内部会将 查询的到的结果的 行数写到row的地址里
    // 会将 查询的到的结果的 列数写到column的地址里
    // 会让 result 指向 sqlite3_get_table 申请的结果集的内存空间
    sprintf(sqlstr, "select * from employee_table where id = %d and password = %d", msg->id, msg->password);

    if (sqlite3_get_table(db, sqlstr, &result, &nrow, &ncolumn, &errmsg) != SQLITE_OK)
        printf("error : %s\n", errmsg);

    // 通过nrow参数判断是否能够查询到记录，如果值为0，则查询不到，如果值为非0，则查询到
    if (nrow == 0) {
        strcpy(msg->data, "name or password is wrony!!!");
    } else {
        sprintf(buf, "%s", result[ncolumn + 2]);

        if (!strncmp(buf, ROOT, 2)) {
            strncpy(msg->data, "root", 5);
        } else {
            strncpy(msg->data, "user", 5);
        }
        printf("----------------------(%s)\n", result[ncolumn + 2]);
    }

    if (-1 == (ret = send(connectfd, msg, sizeof(MSG), 0)))
        ERRLOG("send error");

    sqlite3_free(errmsg);
    sqlite3_free_table(result);
    return;
}

// 添加员工信息的函数
void do_insert(int connectfd, MSG* msg, sqlite3* db)
{
    int ret;
    char* errmsg = NULL;
    char sqlstr[512] = { 0 };
    sprintf(sqlstr, "insert into employee_table(id,password,root,name,age,sex,post,salary) values(%d,%d,%d,'%s',%d,'%s','%s',%d)", msg->id, msg->password, msg->root, msg->name, msg->age, msg->sex, msg->post, msg->salary);

    if (sqlite3_exec(db, sqlstr, NULL, NULL, &errmsg) != SQLITE_OK) {
        sprintf(msg->data, "staff %s insert error!!!", msg->name);
    } else {
        strncpy(msg->data, "OK", 3);
    }
    printf("msg.data = %s\n", msg->data);
    if (-1 == (ret = send(connectfd, msg, sizeof(MSG), 0)))
        ERRLOG("send error");

    sqlite3_free(errmsg);
}

// 修改员工信息的函数
void do_updata(int connectfd, MSG* msg, sqlite3* db)
{
    int ret;
    char* errmsg = NULL;
    char sqlstr[512] = { 0 };
    printf("msg.root = %d", msg->root);
    if (msg->root == 1) {
        sprintf(sqlstr, "update employee_table set name='%s',password = %d,root=%d,age=%d,sex='%s',post='%s',salary=%d where id=%d", msg->name, msg->password, msg->root, msg->age, msg->sex, msg->post, msg->salary, msg->id);

        if (sqlite3_exec(db, sqlstr, NULL, NULL, &errmsg) != SQLITE_OK) {
            sprintf(msg->data, "staff %d updata faild!!!", msg->id);
        } else {
            strncpy(msg->data, "OK", 3);
        }
        if (-1 == (ret = send(connectfd, msg, sizeof(MSG), 0)))
            ERRLOG("send error");
        sqlite3_free(errmsg);
    } else {
        sprintf(sqlstr, "update employee_table set name='%s',password = %d,age=%d,sex='%s' where id=%d", msg->name, msg->password, msg->age, msg->sex, msg->id);

        if (sqlite3_exec(db, sqlstr, NULL, NULL, &errmsg) != SQLITE_OK) {
            sprintf(msg->data, "staff %d updata faild!!!", msg->id);
        } else {
            strncpy(msg->data, "OK", 3);
        }
        if (-1 == (ret = send(connectfd, msg, sizeof(MSG), 0)))
            ERRLOG("send error");

        sqlite3_free(errmsg);
    }
    return;
}

// 查询员工信息的函数
void do_query(int connectfd, MSG* msg, sqlite3* db)
{
    char* errmsg = NULL;
    char sqlstr[512] = { 0 };
    char** result;
    int nrow, ncolumn, ret, i;
    sprintf(sqlstr, "select id,name,age,sex,post,salary from employee_table where id=%d", msg->id);

    if (sqlite3_get_table(db, sqlstr, &result, &nrow, &ncolumn, &errmsg) != SQLITE_OK) {
        printf("error : %s\n", errmsg);
    }
    if (nrow == 0) {
        sprintf(msg->data, "staff info no find");
    } else {
        char buf[128];
        int i, j, index;
        memset(msg, 0, sizeof(MSG));
        for (i = 0; i < ncolumn; i++) {
            sprintf(buf, "%s ", result[i]);
            strcat(msg->data, buf);
        }
        printf("msg.data = %s\n", msg->data);
        if (-1 == (ret = send(connectfd, msg, sizeof(MSG), 0)))
            ERRLOG("send error");

        memset(msg, 0, sizeof(MSG));
        index = i;

        for (i = 0; i < nrow; i++) {
            memset(msg, 0, sizeof(MSG));
            for (j = 0; j < ncolumn; j++) {
                sprintf(buf, "%s   ", result[index + j]);
                strcat(msg->data, buf);
            }
            printf("msg.data = %s\n", msg->data);
            if (-1 == (ret = send(connectfd, msg, sizeof(MSG), 0)))
                ERRLOG("send error");
        }
        memset(msg, 0, sizeof(MSG));
        strncpy(msg->data, "over", 5);
        if (-1 == (ret = send(connectfd, msg, sizeof(MSG), 0)))
            ERRLOG("send error");

        sqlite3_free(errmsg);
        sqlite3_free_table(result);
        return;
    }
}
// 查询所有员工信息的函数
void do_show(int connectfd, MSG* msg, sqlite3* db)
{
    char buf[6];
    char sqlstr[512] = { 0 };
    char *errmsg, **result;
    int nrow, ncolumn, ret;

    // sqlite3_get_table 内部会将 查询的到的结果的 行数写到row的地址里
    // 会将 查询的到的结果的 列数写到column的地址里
    // 会让 result 指向 sqlite3_get_table 申请的结果集的内存空间
    sprintf(sqlstr, "select id,name,age,sex,post,salary from employee_table");
    if (sqlite3_get_table(db, sqlstr, &result, &nrow, &ncolumn, &errmsg) != SQLITE_OK)
        printf("error : %s\n", errmsg);
    // 通过nrow参数判断是否能够查询到记录，如果值为0，则查询不到，如果值为非0，则查询到
    if (nrow == 0) {
        strcpy(msg->data, "Information not queried");
    } else {
        memset(msg, 0, sizeof(MSG));
        // 这里是要写将查询到的结果发送给客户端的代码
        char buf[128];
        int i, j, index;

        for (i = 0; i < ncolumn; i++) {
            sprintf(buf, "%s ", result[i]);
            strcat(msg->data, buf);
        }
        printf("msg.data = %s\n", msg->data);
        if (-1 == (ret = send(connectfd, msg, sizeof(MSG), 0)))
            ERRLOG("send error");

        memset(msg, 0, sizeof(MSG));
        index = i;

        for (i = 0; i < nrow; i++) {
            memset(msg, 0, sizeof(MSG));
            for (j = 0; j < ncolumn; j++) {
                sprintf(buf, "%s   ", result[index++]);
                strcat(msg->data, buf);
            }
            printf("msg.data = %s\n", msg->data);
            if (-1 == (ret = send(connectfd, msg, sizeof(MSG), 0)))
                ERRLOG("send error");
        }
    }
    memset(msg, 0, sizeof(MSG));
    strncpy(msg->data, "over", 5);
    if (-1 == (ret = send(connectfd, msg, sizeof(MSG), 0)))
        ERRLOG("send error");

    sqlite3_free(errmsg);
    sqlite3_free_table(result);
    return;
}
// 删除员工信息的函数
void do_dele(int connectfd, MSG* msg, sqlite3* db)
{
    int ret;
    char* errmsg = NULL;
    char sqlstr[512] = { 0 };
    sprintf(sqlstr, "delete from employee_table where id=%d", msg->id);
    printf("delete from employee_table where id=%d", msg->id);
    if (sqlite3_exec(db, sqlstr, NULL, NULL, &errmsg) != SQLITE_OK) {
        sprintf(msg->data, "staff %d delete faild!!!", msg->id);
    } else {
        strncpy(msg->data, "OK", 3);
    }
    if (-1 == (ret = send(connectfd, msg, sizeof(MSG), 0)))
        ERRLOG("send error");

    sqlite3_free(errmsg);
    return;
}