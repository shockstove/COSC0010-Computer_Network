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


//�����û��ṹ��
typedef struct{
    SOCKET sock;
    sockaddr_in addr;
}user;


vector<user> ClientVector;
//�ӽ��յ��ַ����л����Ϣ������
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

//�ӽ��յ��ַ����л���û����ǳ�
string strToMessName(string s)
{
    string name;
    int namelen=s[4];
    name=s.substr(6,namelen);
    return name;
}

//�ӽ��յ��ַ����л�ȡ��Ϣ��ʽ��[xxx]---yyyy:mm:dd hh:mm:ss:text
string strToSubstr(string s)
{
    string ss;
    ss=s.substr(5);
    return ss;
}

//�����߳�
DWORD WINAPI HandleThread(LPVOID LpParam)
{
    int RecFlag;
    int SendFlag;
    user* client =(user*)LpParam;
    SOCKET sock=client->sock;
    char RecBuf[MAXBUFSIZE];
    char SendBuf[MAXBUFSIZE];
    
    //ѭ�����ղ�ת����Ϣ
    while(1)
    {
        RecFlag=recv(sock,RecBuf,MAXBUFSIZE,0);
        if(RecFlag!=SOCKET_ERROR)
        {
            string text=RecBuf;
            strcpy_s(RecBuf,strToSubstr(text).c_str());

            //��Ϣ����Ϊ�˳�ʱ��ת���˳���Ϣ
            if(strToMessType(text)==0)
            {
                string exitStr=strToMessName(text)+"�뿪��������\n";
                strcpy_s(RecBuf,exitStr.c_str());
                closesocket(sock);
            }
            //��Ϣ����Ϊ����ʱ��ת��������Ϣ
            else if(strToMessType(text)==2)
            {
                string exitStr=strToMessName(text)+"������������\n";
                strcpy_s(RecBuf,exitStr.c_str());
            }
            //����ת����Ϣ
            time_t t=time(0);
            char tt[100]={0};
            for(int i=0;i<ClientVector.size();i++)
            {
                strftime(tt,sizeof(tt),"%Y-%m-%d %H:%M:%S",localtime(&t));
                SendFlag=send(ClientVector[i].sock,RecBuf,MAXBUFSIZE,0);
                if(SendFlag==SOCKET_ERROR)
                {
                    cout<<tt<<"-����"<<inet_ntoa(client->addr.sin_addr)<<":"<<strToMessName(text)<<"����Ϣת����"<<inet_ntoa(ClientVector[i].addr.sin_addr)<<"ʧ��"<<RecBuf<<endl;
                    ClientVector.erase(ClientVector.begin()+i);
                    i--;
                }
                else
                    cout<<tt<<"-����"<<inet_ntoa(client->addr.sin_addr)<<":"<<strToMessName(text)<<"����Ϣת����"<<inet_ntoa(ClientVector[i].addr.sin_addr)<<"�ɹ�-"<<RecBuf<<endl;
            }
        }
        else{
            cout<<"����ʧ��"<<endl;
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
    //�����׽���
    if(WSAStartup(MAKEWORD(2,2),&wd)!=0)
    {
        cout<<"�׽��ֿ���ʧ��"<<endl;
        return 0;
    }

    //��������Socket
    ListenSock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(ListenSock==INVALID_SOCKET)
    {
        cout<<"��������ʧ��"<<endl;
        return 0;
    }

    //�󶨼����˿ں͵�ַ
    ServerAddr.sin_family=AF_INET;
    ServerAddr.sin_addr.s_addr=INADDR_ANY;
    ServerAddr.sin_port=htons(HOST_PORT);
    bind(ListenSock,(sockaddr*)(&ServerAddr),sizeof(ServerAddr));
    //��ʼ����
    listen(ListenSock,10);
    //ѭ������
    while(1)
    {
        int len=sizeof(sockaddr_in);
        client=accept(ListenSock,(sockaddr*)(&ClientAddr),&len);
        if(client==INVALID_SOCKET)
        {
            cout<<"�����ͻ�ʧ��"<<endl;
            break;
        }
        //Ϊ���������û�����һ�������߳�
        user usr;
        usr.addr=ClientAddr;
        usr.sock=client;
        ClientVector.push_back(usr);
        hThread=CreateThread(NULL,0,HandleThread,(LPVOID)&usr,0,NULL);
        if(hThread==NULL)
        {
            cout<<"�����߳�ʧ��"<<endl;
            break;
        }
    }
    closesocket(ListenSock);
    closesocket(client);
    WSACleanup();
    return 0;
}