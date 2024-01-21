#include<iostream>
#include<time.h>
#include<cstdio>
#include"WinSock2.h"
#pragma comment(lib,"ws2_32.lib")
using namespace std;

#define HOST "127.0.0.1"
#define HOST_PORT 25565
#define MAXBUFSIZE 200
//������Ϣ�ṹ��
typedef struct{
    string type;//��Ϣ����
    string t;//����ʱ��
    string text;//�ı�
    string usr;//�����û�
    uint8_t tl;//�û��ǳƳ���
}Message;

//��Ϣת��Ϊ�ɷ��͵��ַ���
string messageToStr(Message msg)
{
    string str;
    str+=msg.type+(char)msg.tl+'['+msg.usr+(string)"]---"+msg.t+':'+msg.text+'\n';
    return str;
};

//�����߳�
DWORD WINAPI SendThread(LPVOID LpParam)
{
    //��ʼ���̲߳���ȡ�û��ǳ�
    SOCKET sock=(SOCKET)LpParam;
    char usrname[15];
    char SendBuf[MAXBUFSIZE];
    cout<<"�������ǳ�"<<endl;
    cin.getline(usrname,20);
    if(strlen(usrname)>15)
    {
        cout<<"���ȳ���"<<endl;
        closesocket(sock);
        WSACleanup();
    }
    uint8_t namelen=strlen(usrname);
    char sendContent[MAXBUFSIZE-50]={};

    //����������ͽ��������ҵ���Ϣ
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

    //ѭ��������Ϣ�����벢����
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

        //�ж���Ϣ�������˳�������������
        if(usrMessage.text==".exit")
        {
            usrMessage.type="exit";
        }
        else
        {
            usrMessage.type="chat";
        }

        //������Ϣ
        strcpy_s(SendBuf,messageToStr(usrMessage).c_str());
        int sendStatus=send(sock,SendBuf,MAXBUFSIZE,0);
        if(sendStatus==SOCKET_ERROR)
        {
            cout<<"����ʧ��"<<endl;
        }
        else
        {
            cout<<"���ͳɹ�"<<endl;
        }

        //�˳�
        if(usrMessage.text==".exit") 
        {
            closesocket(sock);
            WSACleanup();
            //exit(0);
        }
    }
}

//�����߳�
DWORD WINAPI ReceiveThread(LPVOID LpParam)
{
    SOCKET sock=(SOCKET)LpParam;
    char RecBuf[MAXBUFSIZE];
    int RecFlag;

    //ѭ�����շ���˷��͵İ�
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
    //�����׽���
    if(WSAStartup(MAKEWORD(2,2),&wd)!=0)
    {
        cout<<"����ʧ��!"<<endl;
        return 0;
    }
    //�����׽���
    sock=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(sock==INVALID_SOCKET)
    {
        cout<<"sock����ʧ��!"<<endl;
        return 0;
    }
    clientAddr.sin_family=AF_INET;
    clientAddr.sin_addr.s_addr=inet_addr(HOST);
    clientAddr.sin_port=htons(HOST_PORT);
    int len=sizeof(sockaddr_in);
    //���ӷ����
    if(connect(sock,(sockaddr*)&clientAddr,len)==SOCKET_ERROR)
    {
        cout<<"sock�Ͽ�����!"<<endl;
        closesocket(sock);
        WSACleanup();
        exit(0);
        return 0;
    }
    //���������̣߳��ֱ������շ�
    hThreadRec=CreateThread(NULL,0,SendThread,(LPVOID)sock,0,NULL);
    hThreadSend=CreateThread(NULL,0,ReceiveThread,(LPVOID)sock,0,NULL);
    //���õȴ�ʱ������
    WaitForSingleObject(hThreadRec,INFINITE);
    WaitForSingleObject(hThreadSend,INFINITE);
    return 0;
}