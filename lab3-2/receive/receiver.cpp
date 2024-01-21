#include<iostream>
#include<time.h>
#include<cstdio>
#include"WinSock2.h"
#include <Ws2tcpip.h>
#include<fstream>
#pragma comment(lib,"ws2_32.lib")
using namespace std;

#define HOST "127.0.0.1"
#define HOST_PORT 9997
#define MAX_SIZE 4096
#define FLAG_USE 1
#define FLAG_SYN 1<<1
#define FLAG_ACK 1<<2
#define FLAG_STA 1<<3
#define FLAG_END 1<<4
#define FLAG_FIN 1<<5
#define FLAG_NACK 1<<6
char receivebuf[10000][MAX_SIZE]={0};
SOCKET client;
SOCKADDR_IN sourceAd;
SOCKADDR_IN targetAd;
u_short clientseq=0;
timeval timeout;
int WindowSize = 1;
int exceptedSeqNum = 1;
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
        //设置目的IP和端口
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

UDP_packet RecvList[10000];


//打印接收端窗口情况
void printWindowInfo(){
    cout<<"ExceptedSeqNum="<<exceptedSeqNum<<endl;
    return;
}

bool makeConnection(SOCKET client,UDP_packet receive,SOCKADDR_IN s,SOCKADDR_IN t){
    //握手建立连接
    //第二次握手Seq为0,Ack为0+1=1
    UDP_packet send;
    send.setSourceIP(s);
    send.setTargetIP(t);
    send.setFlags(FLAG_SYN);
    send.setSeq(0);
    send.setFlags(FLAG_USE);
    send.setFlags(FLAG_ACK);
    send.setAck(1);
    send.calChecksum();
    sendto(client,(char*)&send,sizeof(send),0,(SOCKADDR*)&t,sizeof(t));
    cout<<"[Info]--------开始握手--------"<<endl;
    printf("[Recv] ");receive.printInfo();
    cout<<"[Info]收到请求包，开始第二次握手"<<endl;
    printf("[Send] ");send.printInfo();
    int len=sizeof(t);
    //超时重传
    time_t start=clock();
    time_t end;
    while(1)
    {
        //接收第三次握手包，Ack=0+1=1,Seq=1
        recvfrom(client,(char*)&receive,sizeof(receive),0,(SOCKADDR*)&t,&len);
        if(receive.flags&(FLAG_ACK)&&receive.Ack==send.Seq+1)
        {
            cout<<"[Info]收到确认包"<<endl;
            printf("[Recv] ");receive.printInfo();
            cout<<"[Info]--------三次握手完成--------"<<endl;
            return 1;
        }
        //超时重传
        end=clock();
        if(((end-start)/CLOCKS_PER_SEC)>=2){
            start=clock();
            sendto(client,(char*)&send,sizeof(send),0,(SOCKADDR*)&t,sizeof(t));
            cout<<"[Info]未收到第三次握手包，尝试重发......"<<endl;
        }
    }

}

void endConnection(SOCKET client,UDP_packet receive,SOCKADDR_IN s,SOCKADDR_IN t){
    //四次挥手断开连接
    UDP_packet send,send2,receive2;
    //发送第二次挥手ACK包
    send.setSourceIP(s);
    send.setTargetIP(t);
    send.setFlags(FLAG_ACK);
    send.setFlags(FLAG_USE);
    send.setAck(receive.Seq);
    send.setSeq(exceptedSeqNum++);
    send.calChecksum();
    cout<<"[Info]--------开始挥手--------"<<endl;
    cout<<"[Info]收到第一次挥手包"<<endl;
    printf("[Recv] ");receive.printInfo();
    cout<<"[Info]发送第二次挥手包"<<endl;
    printf("[Send] ");send.printInfo();
    sendto(client,(char*)&send,sizeof(send),0,(SOCKADDR*)&t,sizeof(t));
    int len=sizeof(t);
    time_t start=clock();
    time_t end;
    //发送第三次挥手FIN包
    send2.setSourceIP(s);
    send2.setTargetIP(t);
    send2.setFlags(FLAG_USE);
    send2.setFlags(FLAG_FIN);
    send2.setAck(send.Ack);
    send2.setSeq(exceptedSeqNum++);
    send2.calChecksum();
    while(1)
    {
        sendto(client,(char*)&send2,sizeof(send2),0,(SOCKADDR*)&t,sizeof(t));    
        cout<<"[Info]发送第三次挥手包"<<endl;
        printf("[Send] ");send2.printInfo();
        //printf("[Send] ACK=%d SEQ=%d CHECKSUM=%x LEN=%d ISACK=%d ISSTA=%d ISEND=%d ISFIN=%d\n",send2.Ack,send2.Seq,send2.checksum,send2.length,send2.flags&FLAG_ACK,send2.flags&FLAG_STA,send2.flags&FLAG_END,send2.flags&FLAG_FIN);
        recvfrom(client,(char*)&receive2,sizeof(receive2),0,(SOCKADDR*)&t,&len);
        //收到第四次挥手ACK包
        if(receive2.flags&(FLAG_USE)&&receive2.Ack==send2.Seq&&receive2.flags&FLAG_ACK)
        {  
            cout<<"[Info]收到第四次挥手包"<<endl;
            printf("[Recv] ");receive2.printInfo();
            //printf("[Recv] ACK=%d SEQ=%d CHECKSUM=%x LEN=%d ISACK=%d ISSTA=%d ISEND=%d ISFIN=%d\n",receive2.Ack,receive2.Seq,receive2.checksum,receive2.length,receive2.flags&FLAG_ACK,receive2.flags&FLAG_STA,receive2.flags&FLAG_END,receive2.flags&FLAG_FIN);
            cout<<"[Info]---------四次挥手完成---------"<<endl;
            return; 
        }
        //收到的包不是对应的ACK包
        else if(receive2.flags&(FLAG_USE)&&receive2.Ack!=send2.Seq&&receive2.flags&FLAG_ACK)
        {
            cout<<"[Info]发送端未收到第二次挥手报文，重发......"<<endl;
            sendto(client,(char*)&send,sizeof(send),0,(SOCKADDR*)&t,sizeof(t));
        }
        else {
            cout<<"Error packet ";
            receive2.printInfo();
        }
        //超时重传
        end=clock();
        if(((end-start)/CLOCKS_PER_SEC)>=2){
            cout<<"[Info]未收到ACK确认报文，尝试重发......"<<endl;
            start=clock();
            sendto(client,(char*)&send2,sizeof(send2),0,(SOCKADDR*)&t,sizeof(t));
        }
    }
}

void recvFile(SOCKET client,UDP_packet& pk,SOCKADDR_IN s,SOCKADDR_IN t){
    //接收并保存文件的部分
    for(int i=0;i<MAX_SIZE;i++){
        memset(receivebuf[i],0,MAX_SIZE);
    }
    //收到STA包后返回ACK应答
    UDP_packet answer;
    u_short amount,len;
    answer.setAck(pk.Seq);
    answer.setSeq(0);
    answer.setSourceIP(s);
    answer.setTargetIP(t);
    answer.setFlags(FLAG_USE);
    answer.setFlags(FLAG_ACK);
    answer.calChecksum();
    sendto(client,(char*)&answer,sizeof(answer),0,(SOCKADDR*)&t,sizeof(SOCKADDR));
    amount=pk.number;
    len=pk.length;
    cout<<"[Info]收到开始传输报文"<<endl;
    //解析STA包
    printf("[Recv] ");pk.printInfo();
    printf("[Recv] ");answer.printInfo();
    //printf("[Recv] ACK=%d SEQ=%d CHECKSUM=%x LEN=%d ISACK=%d ISSTA=%d ISEND=%d ISFIN=%d\n",pk.Ack,pk.Seq,pk.checksum,pk.length,pk.flags&FLAG_ACK,pk.flags&FLAG_STA,pk.flags&FLAG_END,pk.flags&FLAG_FIN);
    cout<<"上传文件的路径是："<<endl;
    for(int i=0;i<len;i++){
        char a=(char)pk.data[i];
        cout<<a;
    }
    cout<<endl;
    //接收接下来的amount+1个不同的有效数据包到缓冲区
    int i = 0;
    int length = 0;
    while(1){
        UDP_packet receive,send;
        send.setFlags(FLAG_USE);
        send.setFlags(FLAG_ACK);
        send.setSourceIP(sourceAd);
        send.setTargetIP(targetAd);
        int len = sizeof(SOCKADDR);
        recvfrom(client,(char*)&receive,sizeof(receive),0,(SOCKADDR*)&t,&len);
        if(receive.checkChecksum()&&receive.flags&FLAG_USE){
            if(receive.Seq == exceptedSeqNum&&receive.checkChecksum()){
                //收到的是期望的包，接收并返回ACK
                RecvList[exceptedSeqNum]=receive;
                exceptedSeqNum++;
                send.setAck(receive.Seq);
                send.calChecksum();
                sendto(client,(char*)&send,sizeof(send),0,(SOCKADDR*)&t,sizeof(SOCKADDR));
                cout<<"[Recv] ";
                receive.printInfo(); 
                cout<<"[Info] ";
                printWindowInfo();
                cout<<"[Send] ";
                send.printInfo();
            }
            else if(receive.Seq>exceptedSeqNum||!receive.checkChecksum()){
                //收到的是大于期望值的包，说明收到乱序包/校验和不通过，包有丢失，重发之前的ACK
                send.setAck(exceptedSeqNum-1);
                send.calChecksum();
                sendto(client,(char*)&send,sizeof(send),0,(SOCKADDR*)&t,sizeof(SOCKADDR));
                cout<<"[Info]收到乱序的数据包"<<receive.Seq<<endl;
                cout<<"[Resend] ";
                send.printInfo();
                continue;
            }        
            else if (receive.Seq<exceptedSeqNum){
                cout<<"[Info]收到重复的数据包"<<receive.Seq<<endl;
            }
            if(receive.flags&FLAG_END){
                length = receive.length;
                cout<<"[Info]该数据包为最后一个包"<<endl;
                cout<<"[Info]--------文件接受完成--------"<<endl;
                break;
            }
        }
    }
    //获取用户输入的文件名，并从缓冲区保存到磁盘中
    cout<<"输入文件名:"<<endl;
    char fileName[50];
    cin>>fileName;
    ofstream file;
    file.open(fileName,ios::binary);
    if(file.is_open()==0){
        cout<<"文件创建失败"<<endl;
        file.close();
        return;
    }
    for(int i=1;i<=amount;i++){
        if(i==amount){
        for(int j=0;j<length;j++)file<<RecvList[i].data[j];
        }
        else{
        for(int j=0;j<MAX_SIZE;j++)file<<RecvList[i].data[j];
        }
    }
    file.close();
    cout<<"[Info]---------文件保存成功---------"<<endl;
    return;
}

int main()
{
    WSADATA wd;
    SOCKET server;
    //设置socket缓冲区大小
    unsigned int rcv_size = 131072;
    socklen_t optlen = sizeof(rcv_size);
    setsockopt(server , SOL_SOCKET , SO_RCVBUF ,(char*)&rcv_size , optlen);
    SOCKADDR_IN sourceAd;
    SOCKADDR_IN targetAd;
    if(WSAStartup(MAKEWORD(2,2),&wd)!=0)
    {
        cout<<"初始化socket失败!"<<endl;
        return 0;
    }
    server=socket(AF_INET,SOCK_DGRAM,0);
    if(server==INVALID_SOCKET)
    {
        cout<<"sock初始化失败!"<<endl;
        return 0;
    }
    sourceAd.sin_addr.s_addr=inet_addr(HOST);
    targetAd.sin_addr.s_addr=inet_addr(HOST);
    sourceAd.sin_family=targetAd.sin_family=AF_INET;
    sourceAd.sin_port=htons(9997);
    targetAd.sin_port=htons(9996);
    //绑定地址和端口
    bind(server,(sockaddr*)&sourceAd,sizeof(sockaddr));
    bool state=0;
    while(1){
        //循环接收SYN包以建立连接、STAB包以开启接收、接收FIN包以断开连接
        UDP_packet pk;
        int len=sizeof(SOCKADDR_IN);
        recvfrom(server,(char*)&pk,sizeof(pk),0,(sockaddr*)&targetAd,&len);
        if(pk.flags&FLAG_USE){
            if(pk.flags&FLAG_SYN){
                if(makeConnection(server,pk,sourceAd,targetAd))state=1;
            }
            else if(pk.flags&FLAG_STA){
                if(state){
                    cout<<"[Info]--------开始接收文件--------"<<endl;
                    exceptedSeqNum=1;
                    recvFile(server,pk,sourceAd,targetAd);
                }
            }
            else if(pk.flags&FLAG_FIN){
                cout<<"[Info]发送端尝试断开连接"<<endl;
                endConnection(server,pk,sourceAd,targetAd);
                break;
            }
        }
    }
    closesocket(server);//关闭socket
	WSACleanup();
    system("pause");
	return 0;
}