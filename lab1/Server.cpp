#include<iostream>
#include<time.h>
#include"ws2tcpip.h"
#include"winsock2.h"
#include<vector>
#pragma comment(lib,"ws2_32.lib")
using namespace std;

#define HOST "127.0.0.1"
#define HOST_PORT 25565
#define MAXBUFSIZE 200


//定义用户结构体
typedef struct{
    SOCKET sock;
    sockaddr_in addr;
}user;


vector<user> ClientVector;
//从接收的字符串中获得消息的类型
int strToMessType(string s)
{
    string type;
    type=s.substr(0,4);
    if(type=="exit")
    return 0;
    else if(type=="join")
    return 2;
    else 
    return 1;
}

//从接收的字符串中获得用户的昵称
string strToMessName(string s)
{
    string name;
    int namelen=s[4];
    name=s.substr(6,namelen);
    return name;
}

//从接收的字符串中获取消息形式：[xxx]---yyyy:mm:dd hh:mm:ss:text
string strToSubstr(string s)
{
    string ss;
    ss=s.substr(5);
    return ss;
}

//处理线程
DWORD WINAPI HandleThread(LPVOID LpParam)
{
    int RecFlag;
    int SendFlag;
    user* client =(user*)LpParam;
    SOCKET sock=client->sock;
    char RecBuf[MAXBUFSIZE];
    char SendBuf[MAXBUFSIZE];
    
    //循环接收并转发消息
    while(1)
    {
        RecFlag=recv(sock,RecBuf,MAXBUFSIZE,0);
        if(RecFlag!=SOCKET_ERROR)
        {
            string text=RecBuf;
            strcpy_s(RecBuf,strToSubstr(text).c_str());

            //消息类型为退出时，转发退出信息
            if(strToMessType(text)==0)
            {
                string exitStr=strToMessName(text)+"离开了聊天室\n";
                strcpy_s(RecBuf,exitStr.c_str());
                closesocket(sock);
            }
            //消息类型为进入时，转发进入信息
            else if(strToMessType(text)==2)
            {
                string exitStr=strToMessName(text)+"进入了聊天室\n";
                strcpy_s(RecBuf,exitStr.c_str());
            }
            //正常转发信息
            time_t t=time(0);
            char tt[100]={0};
            for(int i=0;i<ClientVector.size();i++)
            {
                strftime(tt,sizeof(tt),"%Y-%m-%d %H:%M:%S",localtime(&t));
                SendFlag=send(ClientVector[i].sock,RecBuf,MAXBUFSIZE,0);
                if(SendFlag==SOCKET_ERROR)
                {
                    cout<<tt<<"-来自"<<inet_ntoa(client->addr.sin_addr)<<":"<<strToMessName(text)<<"的消息转发至"<<inet_ntoa(ClientVector[i].addr.sin_addr)<<"失败"<<RecBuf<<endl;
                    ClientVector.erase(ClientVector.begin()+i);
                    i--;
                }
                else
                    cout<<tt<<"-来自"<<inet_ntoa(client->addr.sin_addr)<<":"<<strToMessName(text)<<"的消息转发至"<<inet_ntoa(ClientVector[i].addr.sin_addr)<<"成功-"<<RecBuf<<endl;
            }
        }
        else{
            cout<<"接收失败"<<endl;
            break;
        }
    }
    return 0;
}

int main()
{
    system("chcp 936");
    WSAData wd;
    SOCKET ListenSock;
    SOCKET client;
    sockaddr_in ServerAddr;
    sockaddr_in ClientAddr;
    HANDLE hThread;
    //开启套接字
    if(WSAStartup(MAKEWORD(2,2),&wd)!=0)
    {
        cout<<"套接字开启失败"<<endl;
        return 0;
    }

    //开启监听Socket
    ListenSock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(ListenSock==INVALID_SOCKET)
    {
        cout<<"开启监听失败"<<endl;
        return 0;
    }

    //绑定监听端口和地址
    ServerAddr.sin_family=AF_INET;
    ServerAddr.sin_addr.s_addr=INADDR_ANY;
    ServerAddr.sin_port=htons(HOST_PORT);
    bind(ListenSock,(sockaddr*)(&ServerAddr),sizeof(ServerAddr));
    //开始监听
    listen(ListenSock,10);
    //循环监听
    while(1)
    {
        int len=sizeof(sockaddr_in);
        client=accept(ListenSock,(sockaddr*)(&ClientAddr),&len);
        if(client==INVALID_SOCKET)
        {
            cout<<"创建客户失败"<<endl;
            break;
        }
        //为监听到的用户创建一个处理线程
        user usr;
        usr.addr=ClientAddr;
        usr.sock=client;
        ClientVector.push_back(usr);
        hThread=CreateThread(NULL,0,HandleThread,(LPVOID)&usr,0,NULL);
        if(hThread==NULL)
        {
            cout<<"创建线程失败"<<endl;
            break;
        }
    }
    closesocket(ListenSock);
    closesocket(client);
    WSACleanup();
    return 0;
}