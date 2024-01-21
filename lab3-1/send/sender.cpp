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
u_short clientseq=0;
timeval timeout;
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

void flushSeq(){
    //更新序列号
    if(clientseq==1) clientseq=0;
    else clientseq=1;
    return;
}

bool stopWaitSend(UDP_packet& send,UDP_packet& receive,SOCKET client,SOCKADDR_IN s,SOCKADDR_IN t){
    //停等机制发送端的部分，实现接受确认、超时重传
    send.setSeq(clientseq);
    flushSeq();
    send.setFlags(1);
    send.calChecksum();
    sendto(client,(char*)&send,sizeof(send),0,(SOCKADDR*)&t,sizeof(t));
    printf("[Send] ");send.printInfo();
    int len=sizeof(t);
    //设置计时器以超时重传
    time_t start=clock();
    time_t end;
    int count=1;
    while(1)
    {
        recvfrom(client,(char*)&receive,sizeof(receive),0,(SOCKADDR*)&t,&len);
        //接收确认部分
        if((receive.flags&FLAG_NACK)&&(receive.flags&FLAG_USE)){
            sendto(client,(char*)&send,sizeof(send),0,(SOCKADDR*)&t,sizeof(t));
            start=clock();
            continue;
        }
        else if(receive.flags&(FLAG_USE)&&receive.Ack==send.Seq)
        {
            printf("[Recv] ");receive.printInfo();
            return 1;
        }
        end=clock();
        //超时重传部分
        if(((end-start)/CLOCKS_PER_SEC)>=1){
            if(count == 6){
                cout<<"[Error]重发超过次数"<<endl;
                system("pause");
                return 0;
            }
            cout<<"[Info]未收到ACK确认报文，尝试第"<<count<<"次重发......"<<endl;
            count++;
            start=clock();
            sendto(client,(char*)&send,sizeof(send),0,(SOCKADDR*)&t,sizeof(t));
        }
    }
}


bool makeConnection(SOCKADDR_IN s,SOCKADDR_IN t,SOCKET client){
    //握手建立连接
    //第一次握手，Seq恒为0
    UDP_packet send,receive;
    send.setSourceIP(s);
    send.setTargetIP(t);
    send.setFlags(FLAG_SYN);
    send.setSeq(clientseq);
    flushSeq();
    send.setFlags(FLAG_USE);
    send.calChecksum();
    sendto(client,(char*)&send,sizeof(send),0,(SOCKADDR*)&t,sizeof(t));
    cout<<"[Info]发送请求包，开始第一次握手"<<endl;
    printf("[Send] ");send.printInfo();
    int len=sizeof(t);
    //超时重传
    time_t start=clock();
    time_t end;
    while(1)
    {
        recvfrom(client,(char*)&receive,sizeof(receive),0,(SOCKADDR*)&t,&len);
        //第二次握手包Ack=1,Seq=0
        if(receive.flags&(FLAG_USE)&&receive.Ack==send.Seq+1)
        {
            printf("[Recv] ");receive.printInfo();
            cout<<"[Info]收到确认包，开始第三次握手"<<endl;
            //第三次握手包Ack=1,Seq=1
            send.setAck(receive.Seq+1);
            send.setSeq(clientseq);
            flushSeq();
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
    send.setSourceIP(s);
    send.setTargetIP(t);
    send.setFlags(FLAG_USE);
    send.setFlags(FLAG_FIN);
    flushSeq();
    cout<<"[Info]发送第一次挥手包"<<endl;
    //发送第一次挥手，并停等第二次挥手ACK包
    stopWaitSend(send,receive,client,s,t);
    UDP_packet send2,receive2;
    int len=sizeof(t);
    //接收第三次挥手的FIN包
    recvfrom(client,(char*)&receive2,sizeof(receive2),0,(SOCKADDR*)&t,&len);
    if(receive2.flags&FLAG_FIN){
        cout<<"[Info]收到第三次挥手包"<<endl;
        printf("[Recv] ");receive2.printInfo();
        cout<<"[Info]发送第四次挥手包"<<endl;
        //发送第四次挥手ACK包
        send2.setFlags(FLAG_USE);
        send2.setFlags(FLAG_ACK);
        send2.setAck(receive2.Seq);
        send2.setSeq(clientseq);
        flushSeq();
        sendto(client,(char*)&send2,sizeof(send2),0,(SOCKADDR*)&t,sizeof(t));
        printf("[Send] ");send2.printInfo();
        cout<<"[Info]---------断开链接--------"<<endl;
        return;
    }
}

void sendFile(SOCKADDR_IN s,SOCKADDR_IN t,char* filePath,SOCKET &client){
    //读取并发送文件的部分
    for(int i=0;i<10000;i++)
    {
        memset(sendbuf[i],0,MAX_SIZE);
    }
    //读取文件到缓冲区并记录总共的包数和最后一个包的数据长度
    u_short amount=0;
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
    UDP_packet send,receive;
    //发送STA包，传递文件路径、包总数
    send.setNumber(amount);
    send.setSourceIP(s);
    send.setTargetIP(t);
    send.setFlags(FLAG_STA);
    send.setLength(strlen(filePath));
    memcpy(send.data,filePath,strlen(filePath));
    time_t startsend=clock();
    time_t endsend;
    cout<<"[Info]---------开始传输文件---------"<<endl;
    if(stopWaitSend(send,receive,client,s,t)==0){
        cout<<"[Info]开始文件传输失败"<<endl;
        return;
    }
    cout<<"[Info]起始传输报文发送成功"<<endl;
    //发送带有数据的数据包，共amount+1个
    for(int i=0;i<=amount;i++){
        UDP_packet pk;
        pk.setSourceIP(s);
        pk.setTargetIP(t);
        pk.setNumber(amount);
        pk.setFlags(FLAG_USE);
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
        stopWaitSend(pk,receive,client,s,t);
        if(pk.flags&FLAG_END){
            cout<<"[Info]结束报文发送成功"<<endl;
        }
    }
    endsend=clock();
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
    SOCKET client;
    SOCKADDR_IN sourceAd;
    SOCKADDR_IN targetAd;
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
            sendFile(sourceAd,targetAd,filePath,client);
        }
        clientseq=0;
    }
    //挥手
    endConnection(sourceAd,targetAd,client);
    closesocket(client);
    WSACleanup();
    system("pause");
    return 0;
}