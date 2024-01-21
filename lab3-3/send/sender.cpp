 #include<iostream>
#include<time.h>
#include<cstdio>
#include<stdlib.h>
#include"WinSock2.h"
#include<fstream>
#pragma comment(lib,"ws2_32.lib")
using namespace std;

#define HOST "127.0.0.1"
#define HOST_PORT 9995
#define MAX_SIZE 4096
#define FLAG_USE 1
#define FLAG_SYN 1<<1
#define FLAG_ACK 1<<2
#define FLAG_STA 1<<3
#define FLAG_END 1<<4
#define FLAG_FIN 1<<5
#define FLAG_NACK 1<<6
char sendbuf[10000][MAX_SIZE]={0};
SOCKET client;
SOCKADDR_IN sourceAd;
SOCKADDR_IN targetAd;
timeval timeout;
int WindowSize = 1;//窗口大小
int base = -1;//窗口左侧最大已确认的包位置
int lastWritten = 0;//最后的包位置
int nextSeqNum = 0;//下一个待发送的包位置
#pragma pack(1)
class UDP_packet{
public:
    u_int sourceIP;
    u_int targetIP;
    u_short sourcePort;
    u_short targetPort;
    u_short Seq;
    u_short Ack;
    u_short flags;
    u_short number;
    u_short length;
    u_short checksum;
    char data[MAX_SIZE];
    UDP_packet(){
        //空白初始化
        sourceIP=targetIP=sourcePort=targetPort=Seq=Ack=flags=number=length=checksum=0;
        memset(&data,0,sizeof(data));
    }
    void setSourceIP(SOCKADDR_IN ip){
        //设置源IP地址和端口
        sourceIP = ip.sin_addr.S_un.S_addr;
	    sourcePort = ip.sin_port;
    }
    void setTargetIP(SOCKADDR_IN ip){
        //设置目的IP地址和端口
        targetIP = ip.sin_addr.S_un.S_addr;
	    targetPort = ip.sin_port;
    }
    void setSeq(u_short seq){
        //设置序列号
        Seq=seq;
    }
    void setAck(u_short ack){
        //设置确认序列号
        Ack=ack;
    }
    void setFlags(u_short flag){
        //设置FLAG标志位，参数为0则清空标志位，否则将对应位置位
        if(flag==0)
        flags=flag;
        else 
        flags=flags|flag;
    }
    void setNumber(u_short num){
        //设置包总数
        number=num;
    }
    void setLength(u_short len){
        //设置包长度
        length=len;
    }
    void calChecksum(){
        //计算校验和
        u_int sum=0;
        u_short *pointer=(u_short*)this;
        //对首部字段按16位求和
        for(int i=0;i<10;i++)
        {
            sum+=pointer[i];
        }
        //超出16位则循环相加
        while(sum&0xffff0000)
        sum=sum>>16+sum&0xffff;
        //取反
        checksum=(u_short)~sum;
    }
    bool checkChecksum(){
        //检验校验和
        u_int sum=0;
        u_short *pointer=(u_short*)this;
        //对首部字段按16位求和
        for(int i=0;i<10;i++)
        {
            sum+=pointer[i];
        }
        //超出16位则循环相加
        while(sum&0xffff0000)
        sum=sum>>16+sum&0xffff;
        //累加和和校验和相加为全1则校验通过
        if((u_short)sum+checksum==0xffff)
            return true;
        else return false;
    }
    void printInfo(){
        //打印包信息
        printf("ACK=%d SEQ=%d CHECKSUM=%x LEN=%d ",Ack,Seq,checksum,length);
        if(flags&FLAG_SYN)cout<<"[SYN]";
        if(flags&FLAG_ACK)cout<<"[ACK]";
        if(flags&FLAG_STA)cout<<"[STA]";
        if(flags&FLAG_END)cout<<"[END]";
        if(flags&FLAG_FIN)cout<<"[FIN]";
        if(flags&FLAG_NACK)cout<<"[NACK]";
        cout<<endl;
    }
};
#pragma pack(0)

//发送窗口包队列
UDP_packet sendList[10000];
bool recvAck[10000]={0};
bool haveSend[10000]={0};
time_t startTime[10000];

//打印发送端窗口情况
void printWindowInfo(bool fin=0){
    cout<<"Win="<<WindowSize<<" Base="<<base<<" NextSeqNum="<<nextSeqNum<<endl;
    return;
}

bool makeConnection(SOCKADDR_IN s,SOCKADDR_IN t,SOCKET client){
    //握手建立连接
    //第一次握手，Seq恒为0
    UDP_packet send,receive;
    send.setSourceIP(s);
    send.setTargetIP(t);
    send.setFlags(FLAG_SYN);
    send.setSeq(0);
    send.setFlags(FLAG_USE);
    send.calChecksum();
    sendto(client,(char*)&send,sizeof(send),0,(SOCKADDR*)&t,sizeof(t));
    cout<<"[Info] 发送请求包，开始第一次握手"<<endl;
    printf("[Send] ");send.printInfo();
    int len=sizeof(t);
    //超时重传
    time_t start=clock();
    time_t end;
    while(1)
    {
        recvfrom(client,(char*)&receive,sizeof(receive),0,(SOCKADDR*)&t,&len);
        //第二次握手包Ack=1,Seq=0
        if(receive.flags&(FLAG_USE)&&receive.Ack==1)
        {
            printf("[Recv] ");receive.printInfo();
            cout<<"[Info] 收到确认包，开始第三次握手"<<endl;
            //第三次握手包Ack=1,Seq=1
            send.setAck(1);
            send.setSeq(1);
            send.setFlags(0);
            send.setFlags(FLAG_USE);
            send.setFlags(FLAG_ACK);
            sendto(client,(char*)&send,sizeof(send),0,(SOCKADDR*)&t,sizeof(t));
            printf("[Send] ");send.printInfo();
            cout<<"[Info]--------三次握手完成--------"<<endl;
            return 1;
        }
        //超时重传
        end=clock();
        if(((end-start)/CLOCKS_PER_SEC)>=2){
            start=clock();
            sendto(client,(char*)&send,sizeof(send),0,(SOCKADDR*)&t,sizeof(t));
            cout<<"[Info]未收到第二次握手包，尝试重发......"<<endl;
        }
    }

}

void endConnection(SOCKADDR_IN s,SOCKADDR_IN t,SOCKET client){
    //四次挥手断开连接
    UDP_packet send,receive;
    UDP_packet send2,receive2;
    send.setSourceIP(s);
    send.setTargetIP(t);
    send.setFlags(FLAG_USE);
    send.setFlags(FLAG_FIN);
    send.setSeq(nextSeqNum++);
    send.calChecksum();
    cout<<"[Info]发送第一次挥手包"<<endl;
    sendto(client,(char*)&send,sizeof(send),0,(SOCKADDR*)&t,sizeof(t));
    cout<<"[Send] ";send.printInfo();
    //发送第一次挥手，并等待第二次挥手ACK包
    time_t start=clock();
    time_t end;
    int len = sizeof(SOCKADDR);
    while(1)
    {
        recvfrom(client,(char*)&receive,sizeof(receive),0,(SOCKADDR*)&t,&len);
        //第二次握手包
        if(receive.flags&(FLAG_USE)&&receive.flags&FLAG_ACK&&receive.Ack==send.Seq&&receive.checkChecksum())
        {
            printf("[Recv] ");receive.printInfo();
            cout<<"[Info]收到第二次挥手包"<<endl;
            recvfrom(client,(char*)&receive2,sizeof(receive2),0,(SOCKADDR*)&t,&len);
            if(receive2.flags&FLAG_FIN&&receive2.flags&FLAG_USE&&receive2.checkChecksum()){
                cout<<"[Info]收到第三次挥手包"<<endl;
                printf("[Recv] ");receive2.printInfo();
                cout<<"[Info]发送第四次挥手包"<<endl;
                //发送第四次挥手ACK包
                send2.setFlags(FLAG_USE);
                send2.setFlags(FLAG_ACK);
                send2.setAck(receive2.Seq);
                send2.setSeq(nextSeqNum++);
                send2.calChecksum();
                sendto(client,(char*)&send2,sizeof(send2),0,(SOCKADDR*)&t,sizeof(t));
                printf("[Send] ");send2.printInfo();
                cout<<"[Info]---------断开链接--------"<<endl;
                return;
            }
        }
        //超时重传
        end=clock();
        if(((end-start)/CLOCKS_PER_SEC)>=2){
            start=clock();
            sendto(client,(char*)&send,sizeof(send),0,(SOCKADDR*)&t,sizeof(t));
            cout<<"[Info]未收到第二次挥手包，尝试重发......"<<endl;
        }
    }
}

//发送线程
DWORD WINAPI sendHandle(LPVOID lparam){
    time_t end;
    while(base<lastWritten){
        //从窗口最左侧开始向右移动遍历   
        for(nextSeqNum = base+1;nextSeqNum<=base+WindowSize&&nextSeqNum<=lastWritten;nextSeqNum++){
            if(recvAck[nextSeqNum]==0){
                //如果遍历到的位置未收到过ACK
                if(haveSend[nextSeqNum]==0){
                    //之前没有发送过这个位置对应的包
                    //发送，并启动定时器，标记该位置已发送过
                    startTime[nextSeqNum] = clock();
                    sendto(client,(char*)&sendList[nextSeqNum],sizeof(sendList[nextSeqNum]),0,(SOCKADDR*)&targetAd,sizeof(targetAd));
                    haveSend[nextSeqNum]=1;
                    cout<<"[Send]发送第"<<nextSeqNum<<"个数据包 ";
                    sendList[nextSeqNum].printInfo();
                    printWindowInfo();
                }
                else{
                    //之前发送过，检测是否超时，超时则重发并重启该位置定时器
                    time_t end = clock();
                    if((double)(end-startTime[nextSeqNum])/CLOCKS_PER_SEC >= 1){
                        startTime[nextSeqNum] = clock();
                        sendto(client,(char*)&sendList[nextSeqNum],sizeof(sendList[nextSeqNum]),0,(SOCKADDR*)&targetAd,sizeof(targetAd));
                        cout<<"[Resend]重发第"<<nextSeqNum<<"个数据包";
                        sendList[nextSeqNum].printInfo();
                        printWindowInfo();
                    }
                }
            }
        }
    }
    return 1;
}

//接收线程
DWORD WINAPI receiveHandle(LPVOID lparam){
    while(base < lastWritten){
        UDP_packet receive;
        int len=sizeof(targetAd);
        recvfrom(client,(char*)&receive,sizeof(receive),0,(SOCKADDR*)&targetAd,&len);
        if(receive.checkChecksum()&&receive.flags&FLAG_USE&&receive.flags&FLAG_ACK){
            recvAck[receive.Ack]=1;
            cout<<"[Recv] ";
            receive.printInfo();
            cout<<"[Info] ";
            printWindowInfo();
            if(base + 1 == receive.Ack){
                //如果收到的ACK是窗口最左端的，向右滑动到下一个未收到确认的分组之前
                int i=base+1;
                for(;i<=base+WindowSize;i++){
                    if(!recvAck[i])break;
                }
                base = i-1;
            }
        }
    }
    return 1;
}

void sendFile(SOCKADDR_IN s,SOCKADDR_IN t,char* filePath,SOCKET &client){
    //读取并发送文件的部分
    for(int i=0;i<10000;i++)
    {
        memset(sendbuf[i],0,MAX_SIZE);
    }
    //读取文件到缓冲区并记录总共的包数和最后一个包的数据长度
    u_short amount=1;
    ifstream file;
    file.open(filePath,ios::in|ios::binary);
    char temp;
    int p=0;
    while(file.get(temp))
    {
        sendbuf[amount][p]=temp;
        p++;
        if(p%MAX_SIZE==0){
            p=0;
            amount++;
        }
    }
    file.close(); 
    lastWritten = amount;
    UDP_packet send,receive;
    //发送STA包，传递文件路径、包总数
    send.setNumber(amount);
    send.setSourceIP(s);
    send.setTargetIP(t);
    send.setFlags(FLAG_USE);
    send.setFlags(FLAG_STA);
    send.setLength(strlen(filePath));
    memcpy(send.data,filePath,strlen(filePath));
    send.calChecksum();
    sendList[0]=send;

    //把带有数据的包存入发送缓冲队列
    for(int i=1;i<=amount+1;i++){
        UDP_packet pk;
        pk.setSourceIP(s);
        pk.setTargetIP(t);
        pk.setNumber(amount);
        pk.setFlags(FLAG_USE);
        pk.setSeq(i);
        if(i==amount){
            //最后一个数据包
            pk.setFlags(FLAG_END);
            pk.setLength(p);
            for(int j=0;j<p;j++)
            {
                pk.data[j]=sendbuf[i][j];
            }
        }
        else{
            pk.setLength(MAX_SIZE);
            for(int j=0;j<MAX_SIZE;j++)
            {
                pk.data[j]=sendbuf[i][j];
            }
        }
        pk.calChecksum();
        sendList[i]=pk;
    }
    time_t startsend=clock();
    time_t endsend;
    cout<<"[Info]---------开始传输文件---------"<<endl;
    if(base<lastWritten){
        HANDLE recvThread = CreateThread(NULL,NULL,receiveHandle,LPVOID(),0,NULL);
        HANDLE sendThread = CreateThread(NULL,NULL,sendHandle,LPVOID(),0,NULL);
        WaitForSingleObject(recvThread,INFINITE);
        WaitForSingleObject(sendThread,INFINITE);
        if(base == lastWritten){
            cout<<"[Info]---------传输完成---------"<<endl;
            endsend = clock();
        }
    }
    //输出本次传输的相关数据
    long fileSize=(long)(amount*MAX_SIZE+p);
    double time=(double)(endsend-startsend)/CLOCKS_PER_SEC;
    double rate=(double)(fileSize*8.0)/(time*1000);
    printf("[Info]发送时间：%lfs\n总大小：%dBytes\n吞吐率：%lfKbps\n",time,fileSize,rate);
    cout<<"[Info]---------文件发送成功---------"<<endl;
    return ;
}




int main()
{
    //套接字初始化
    WSADATA wd;
    if(WSAStartup(MAKEWORD(2,2),&wd)!=0)
    {
        cout<<"初始化socket库失败!"<<endl;
        return 0;
    }
    client=socket(AF_INET,SOCK_DGRAM,0);
    if(client==INVALID_SOCKET)
    {
        cout<<"sock初始化失败!"<<endl;
        return 0;
    }
    timeout.tv_sec=2;
    timeout.tv_usec=0;
    setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,(const char*)&timeout,sizeof(timeout));
    sourceAd.sin_addr.s_addr=inet_addr(HOST);
    targetAd.sin_addr.s_addr=inet_addr(HOST);
    sourceAd.sin_family=targetAd.sin_family=AF_INET;
    sourceAd.sin_port=htons(9995);
    targetAd.sin_port=htons(9996);
    //绑定地址和端口
    bind(client,(sockaddr*)&sourceAd,sizeof(sockaddr));
    cout<<"输入窗口大小："<<endl;
    cin>>WindowSize;
    while(1){
        //循环传输文件或退出
        char filePath[50];
        cout<<"输入文件路径或退出(q)"<<endl;
        cin>>filePath;
        if(filePath[0]=='q')break;
        else {
            if(!makeConnection(sourceAd,targetAd,client))
            {
                cout<<"建立连接失败！"<<endl;
                return 0;
            }
            base = -1;
            lastWritten = 0;
            nextSeqNum = 0;
            memset(&recvAck,0,10000);
            memset(&haveSend,0,10000);
            sendFile(sourceAd,targetAd,filePath,client);
        }
    }
    //挥手
    endConnection(sourceAd,targetAd,client);
    closesocket(client);
    WSACleanup();
    system("pause");
    return 0;
}