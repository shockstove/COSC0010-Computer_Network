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
        //�հ׳�ʼ��
        sourceIP=targetIP=sourcePort=targetPort=Seq=Ack=flags=number=length=checksum=0;
        memset(&data,0,sizeof(data));
    }
    void setSourceIP(SOCKADDR_IN ip){
        //����ԴIP��ַ�Ͷ˿�
        sourceIP = ip.sin_addr.S_un.S_addr;
	    sourcePort = ip.sin_port;
    }
    void setTargetIP(SOCKADDR_IN ip){
        //����Ŀ��IP�Ͷ˿�
        targetIP = ip.sin_addr.S_un.S_addr;
	    targetPort = ip.sin_port;
    }
    void setSeq(u_short seq){
        //�������к�
        Seq=seq;
    }
    void setAck(u_short ack){
        //����ȷ�����к�
        Ack=ack;
    }
    void setFlags(u_short flag){
        //����FLAG��־λ������Ϊ0����ձ�־λ�����򽫶�Ӧλ��λ
        if(flag==0)
        flags=flag;
        else 
        flags=flags|flag;
    }
    void setNumber(u_short num){
        //���ð�����
        number=num;
    }
    void setLength(u_short len){
        //���ð�����
        length=len;
    }
    void calChecksum(){
        //����У���
        u_int sum=0;
        u_short *pointer=(u_short*)this;
        //���ײ��ֶΰ�16λ���
        for(int i=0;i<10;i++)
        {
            sum+=pointer[i];
        }
        //����16λ��ѭ�����
        while(sum&0xffff0000)
        sum=sum>>16+sum&0xffff;
        //ȡ��
        checksum=(u_short)~sum;
    }
    bool checkChecksum(){
        //����У���
        u_int sum=0;
        u_short *pointer=(u_short*)this;
        //���ײ��ֶΰ�16λ���
        for(int i=0;i<10;i++)
        {
            sum+=pointer[i];
        }
        //����16λ��ѭ�����
        while(sum&0xffff0000)
        sum=sum>>16+sum&0xffff;
        //�ۼӺͺ�У������Ϊȫ1��У��ͨ��
        if((u_short)sum+checksum==0xffff)
            return true;
        else return false;
    }
    void printInfo(){
        //��ӡ����Ϣ
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


//��ӡ���ն˴������
void printWindowInfo(){
    cout<<"ExceptedSeqNum="<<exceptedSeqNum<<endl;
    return;
}

bool makeConnection(SOCKET client,UDP_packet receive,SOCKADDR_IN s,SOCKADDR_IN t){
    //���ֽ�������
    //�ڶ�������SeqΪ0,AckΪ0+1=1
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
    cout<<"[Info]--------��ʼ����--------"<<endl;
    printf("[Recv] ");receive.printInfo();
    cout<<"[Info]�յ����������ʼ�ڶ�������"<<endl;
    printf("[Send] ");send.printInfo();
    int len=sizeof(t);
    //��ʱ�ش�
    time_t start=clock();
    time_t end;
    while(1)
    {
        //���յ��������ְ���Ack=0+1=1,Seq=1
        recvfrom(client,(char*)&receive,sizeof(receive),0,(SOCKADDR*)&t,&len);
        if(receive.flags&(FLAG_ACK)&&receive.Ack==send.Seq+1)
        {
            cout<<"[Info]�յ�ȷ�ϰ�"<<endl;
            printf("[Recv] ");receive.printInfo();
            cout<<"[Info]--------�����������--------"<<endl;
            return 1;
        }
        //��ʱ�ش�
        end=clock();
        if(((end-start)/CLOCKS_PER_SEC)>=2){
            start=clock();
            sendto(client,(char*)&send,sizeof(send),0,(SOCKADDR*)&t,sizeof(t));
            cout<<"[Info]δ�յ����������ְ��������ط�......"<<endl;
        }
    }

}

void endConnection(SOCKET client,UDP_packet receive,SOCKADDR_IN s,SOCKADDR_IN t){
    //�Ĵλ��ֶϿ�����
    UDP_packet send,send2,receive2;
    //���͵ڶ��λ���ACK��
    send.setSourceIP(s);
    send.setTargetIP(t);
    send.setFlags(FLAG_ACK);
    send.setFlags(FLAG_USE);
    send.setAck(receive.Seq);
    send.setSeq(exceptedSeqNum++);
    send.calChecksum();
    cout<<"[Info]--------��ʼ����--------"<<endl;
    cout<<"[Info]�յ���һ�λ��ְ�"<<endl;
    printf("[Recv] ");receive.printInfo();
    cout<<"[Info]���͵ڶ��λ��ְ�"<<endl;
    printf("[Send] ");send.printInfo();
    sendto(client,(char*)&send,sizeof(send),0,(SOCKADDR*)&t,sizeof(t));
    int len=sizeof(t);
    time_t start=clock();
    time_t end;
    //���͵����λ���FIN��
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
        cout<<"[Info]���͵����λ��ְ�"<<endl;
        printf("[Send] ");send2.printInfo();
        //printf("[Send] ACK=%d SEQ=%d CHECKSUM=%x LEN=%d ISACK=%d ISSTA=%d ISEND=%d ISFIN=%d\n",send2.Ack,send2.Seq,send2.checksum,send2.length,send2.flags&FLAG_ACK,send2.flags&FLAG_STA,send2.flags&FLAG_END,send2.flags&FLAG_FIN);
        recvfrom(client,(char*)&receive2,sizeof(receive2),0,(SOCKADDR*)&t,&len);
        //�յ����Ĵλ���ACK��
        if(receive2.flags&(FLAG_USE)&&receive2.Ack==send2.Seq&&receive2.flags&FLAG_ACK)
        {  
            cout<<"[Info]�յ����Ĵλ��ְ�"<<endl;
            printf("[Recv] ");receive2.printInfo();
            //printf("[Recv] ACK=%d SEQ=%d CHECKSUM=%x LEN=%d ISACK=%d ISSTA=%d ISEND=%d ISFIN=%d\n",receive2.Ack,receive2.Seq,receive2.checksum,receive2.length,receive2.flags&FLAG_ACK,receive2.flags&FLAG_STA,receive2.flags&FLAG_END,receive2.flags&FLAG_FIN);
            cout<<"[Info]---------�Ĵλ������---------"<<endl;
            return; 
        }
        //�յ��İ����Ƕ�Ӧ��ACK��
        else if(receive2.flags&(FLAG_USE)&&receive2.Ack!=send2.Seq&&receive2.flags&FLAG_ACK)
        {
            cout<<"[Info]���Ͷ�δ�յ��ڶ��λ��ֱ��ģ��ط�......"<<endl;
            sendto(client,(char*)&send,sizeof(send),0,(SOCKADDR*)&t,sizeof(t));
        }
        else {
            cout<<"Error packet ";
            receive2.printInfo();
        }
        //��ʱ�ش�
        end=clock();
        if(((end-start)/CLOCKS_PER_SEC)>=2){
            cout<<"[Info]δ�յ�ACKȷ�ϱ��ģ������ط�......"<<endl;
            start=clock();
            sendto(client,(char*)&send2,sizeof(send2),0,(SOCKADDR*)&t,sizeof(t));
        }
    }
}

void recvFile(SOCKET client,UDP_packet& pk,SOCKADDR_IN s,SOCKADDR_IN t){
    //���ղ������ļ��Ĳ���
    for(int i=0;i<MAX_SIZE;i++){
        memset(receivebuf[i],0,MAX_SIZE);
    }
    //�յ�STA���󷵻�ACKӦ��
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
    cout<<"[Info]�յ���ʼ���䱨��"<<endl;
    //����STA��
    printf("[Recv] ");pk.printInfo();
    printf("[Recv] ");answer.printInfo();
    //printf("[Recv] ACK=%d SEQ=%d CHECKSUM=%x LEN=%d ISACK=%d ISSTA=%d ISEND=%d ISFIN=%d\n",pk.Ack,pk.Seq,pk.checksum,pk.length,pk.flags&FLAG_ACK,pk.flags&FLAG_STA,pk.flags&FLAG_END,pk.flags&FLAG_FIN);
    cout<<"�ϴ��ļ���·���ǣ�"<<endl;
    for(int i=0;i<len;i++){
        char a=(char)pk.data[i];
        cout<<a;
    }
    cout<<endl;
    //���ս�������amount+1����ͬ����Ч���ݰ���������
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
                //�յ����������İ������ղ�����ACK
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
                //�յ����Ǵ�������ֵ�İ���˵���յ������/У��Ͳ�ͨ�������ж�ʧ���ط�֮ǰ��ACK
                send.setAck(exceptedSeqNum-1);
                send.calChecksum();
                sendto(client,(char*)&send,sizeof(send),0,(SOCKADDR*)&t,sizeof(SOCKADDR));
                cout<<"[Info]�յ���������ݰ�"<<receive.Seq<<endl;
                cout<<"[Resend] ";
                send.printInfo();
                continue;
            }        
            else if (receive.Seq<exceptedSeqNum){
                cout<<"[Info]�յ��ظ������ݰ�"<<receive.Seq<<endl;
            }
            if(receive.flags&FLAG_END){
                length = receive.length;
                cout<<"[Info]�����ݰ�Ϊ���һ����"<<endl;
                cout<<"[Info]--------�ļ��������--------"<<endl;
                break;
            }
        }
    }
    //��ȡ�û�������ļ��������ӻ��������浽������
    cout<<"�����ļ���:"<<endl;
    char fileName[50];
    cin>>fileName;
    ofstream file;
    file.open(fileName,ios::binary);
    if(file.is_open()==0){
        cout<<"�ļ�����ʧ��"<<endl;
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
    cout<<"[Info]---------�ļ�����ɹ�---------"<<endl;
    return;
}

int main()
{
    WSADATA wd;
    SOCKET server;
    //����socket��������С
    unsigned int rcv_size = 131072;
    socklen_t optlen = sizeof(rcv_size);
    setsockopt(server , SOL_SOCKET , SO_RCVBUF ,(char*)&rcv_size , optlen);
    SOCKADDR_IN sourceAd;
    SOCKADDR_IN targetAd;
    if(WSAStartup(MAKEWORD(2,2),&wd)!=0)
    {
        cout<<"��ʼ��socketʧ��!"<<endl;
        return 0;
    }
    server=socket(AF_INET,SOCK_DGRAM,0);
    if(server==INVALID_SOCKET)
    {
        cout<<"sock��ʼ��ʧ��!"<<endl;
        return 0;
    }
    sourceAd.sin_addr.s_addr=inet_addr(HOST);
    targetAd.sin_addr.s_addr=inet_addr(HOST);
    sourceAd.sin_family=targetAd.sin_family=AF_INET;
    sourceAd.sin_port=htons(9997);
    targetAd.sin_port=htons(9996);
    //�󶨵�ַ�Ͷ˿�
    bind(server,(sockaddr*)&sourceAd,sizeof(sockaddr));
    bool state=0;
    while(1){
        //ѭ������SYN���Խ������ӡ�STAB���Կ������ա�����FIN���ԶϿ�����
        UDP_packet pk;
        int len=sizeof(SOCKADDR_IN);
        recvfrom(server,(char*)&pk,sizeof(pk),0,(sockaddr*)&targetAd,&len);
        if(pk.flags&FLAG_USE){
            if(pk.flags&FLAG_SYN){
                if(makeConnection(server,pk,sourceAd,targetAd))state=1;
            }
            else if(pk.flags&FLAG_STA){
                if(state){
                    cout<<"[Info]--------��ʼ�����ļ�--------"<<endl;
                    exceptedSeqNum=1;
                    recvFile(server,pk,sourceAd,targetAd);
                }
            }
            else if(pk.flags&FLAG_FIN){
                cout<<"[Info]���Ͷ˳��ԶϿ�����"<<endl;
                endConnection(server,pk,sourceAd,targetAd);
                break;
            }
        }
    }
    closesocket(server);//�ر�socket
	WSACleanup();
    system("pause");
	return 0;
}