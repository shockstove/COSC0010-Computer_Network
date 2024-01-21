#include<iostream>
#include<time.h>
#include<cstdio>
#include"WinSock2.h"
#pragma comment(lib,"ws2_32.lib")
using namespace std;

#define HOST "127.0.0.1"
#define HOST_PORT 25565
#define MAXBUFSIZE 200
//定义消息结构体
typedef struct{
    string type;//消息类型
    string t;//发送时间
    string text;//文本
    string usr;//发送用户
    uint8_t tl;//用户昵称长度
}Message;

//消息转换为可发送的字符串
string messageToStr(Message msg)
{
    string str;
    str+=msg.type+(char)msg.tl+'['+msg.usr+(string)"]---"+msg.t+':'+msg.text+'\n';
    return str;
};

//发送线程
DWORD WINAPI SendThread(LPVOID LpParam)
{
    //初始化线程并获取用户昵称
    SOCKET sock=(SOCKET)LpParam;
    char usrname[15];
    char SendBuf[MAXBUFSIZE];
    cout<<"请输入昵称"<<endl;
    cin.getline(usrname,20);
    if(strlen(usrname)>15)
    {
        cout<<"长度超限"<<endl;
        closesocket(sock);
        WSACleanup();
    }
    uint8_t namelen=strlen(usrname);
    char sendContent[MAXBUFSIZE-50]={};

    //向服务器发送进入聊天室的信息
    Message joinMessage;
    joinMessage.type="join";
    time_t joint=time(0);
    char jt[100]={0};
    strftime(jt,sizeof(jt),"%Y-%m-%d %H:%M:%S",localtime(&joint));
    joinMessage.usr=usrname;
    joinMessage.t=jt;
    joinMessage.tl=namelen;
    strcpy_s(SendBuf,messageToStr(joinMessage).c_str());
    send(sock,SendBuf,MAXBUFSIZE,0);

    //循环接受消息的输入并发送
    while(1)
    {
        cin.getline(sendContent,MAXBUFSIZE);
        Message usrMessage;
        time_t t=time(0);
        char tt[100]={0};
        strftime(tt,sizeof(tt),"%Y-%m-%d %H:%M:%S",localtime(&t));
        usrMessage.t=tt;
        usrMessage.text=sendContent;       
        usrMessage.usr=usrname;
        usrMessage.tl=namelen;

        //判断消息类型是退出还是正常聊天
        if(usrMessage.text==".exit")
        {
            usrMessage.type="exit";
        }
        else
        {
            usrMessage.type="chat";
        }

        //发送消息
        strcpy_s(SendBuf,messageToStr(usrMessage).c_str());
        int sendStatus=send(sock,SendBuf,MAXBUFSIZE,0);
        if(sendStatus==SOCKET_ERROR)
        {
            cout<<"发送失败"<<endl;
        }
        else
        {
            cout<<"发送成功"<<endl;
        }

        //退出
        if(usrMessage.text==".exit") 
        {
            closesocket(sock);
            WSACleanup();
            //exit(0);
        }
    }
}

//接收线程
DWORD WINAPI ReceiveThread(LPVOID LpParam)
{
    SOCKET sock=(SOCKET)LpParam;
    char RecBuf[MAXBUFSIZE];
    int RecFlag;

    //循环接收服务端发送的包
    while(1)
    {
        RecFlag=recv(sock,RecBuf,MAXBUFSIZE,0);
        if(RecFlag!=SOCKET_ERROR)
        {
            string text=RecBuf;
            cout<<text;
            memset(RecBuf,'\0',sizeof(RecBuf));
        }
        else break;
    }
    return 0;
}

int main()
{
    system("chcp 936");
    WSAData wd;
    HANDLE hThreadSend;
    DWORD  sendTreadId;
    HANDLE hThreadRec;
    DWORD recThreadId;
    SOCKET sock;
    sockaddr_in clientAddr;
    //开启套接字
    if(WSAStartup(MAKEWORD(2,2),&wd)!=0)
    {
        cout<<"加载失败!"<<endl;
        return 0;
    }
    //建立套接字
    sock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(sock==INVALID_SOCKET)
    {
        cout<<"sock连接失败!"<<endl;
        return 0;
    }
    clientAddr.sin_family=AF_INET;
    clientAddr.sin_addr.s_addr=inet_addr(HOST);
    clientAddr.sin_port=htons(HOST_PORT);
    int len=sizeof(sockaddr_in);
    //连接服务端
    if(connect(sock,(sockaddr*)&clientAddr,len)==SOCKET_ERROR)
    {
        cout<<"sock断开连接!"<<endl;
        closesocket(sock);
        WSACleanup();
        exit(0);
        return 0;
    }
    //建立两个线程，分别用于收发
    hThreadRec=CreateThread(NULL,0,SendThread,(LPVOID)sock,0,NULL);
    hThreadSend=CreateThread(NULL,0,ReceiveThread,(LPVOID)sock,0,NULL);
    //设置等待时间无限
    WaitForSingleObject(hThreadRec,INFINITE);
    WaitForSingleObject(hThreadSend,INFINITE);
    return 0;
}