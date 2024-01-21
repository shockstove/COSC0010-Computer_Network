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
        //����Ŀ��IP��ַ�Ͷ˿�
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

void flushSeq(){
    //�������к�
    if(clientseq==1) clientseq=0;
    else clientseq=1;
    return;
}

bool stopWaitSend(UDP_packet& send,UDP_packet& receive,SOCKET client,SOCKADDR_IN s,SOCKADDR_IN t){
    //ͣ�Ȼ��Ʒ��Ͷ˵Ĳ��֣�ʵ�ֽ���ȷ�ϡ���ʱ�ش�
    send.setSeq(clientseq);
    flushSeq();
    send.setFlags(1);
    send.calChecksum();
    sendto(client,(char*)&send,sizeof(send),0,(SOCKADDR*)&t,sizeof(t));
    printf("[Send] ");send.printInfo();
    int len=sizeof(t);
    //���ü�ʱ���Գ�ʱ�ش�
    time_t start=clock();
    time_t end;
    int count=1;
    while(1)
    {
        recvfrom(client,(char*)&receive,sizeof(receive),0,(SOCKADDR*)&t,&len);
        //����ȷ�ϲ���
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
        //��ʱ�ش�����
        if(((end-start)/CLOCKS_PER_SEC)>=1){
            if(count == 6){
                cout<<"[Error]�ط���������"<<endl;
                system("pause");
                return 0;
            }
            cout<<"[Info]δ�յ�ACKȷ�ϱ��ģ����Ե�"<<count<<"���ط�......"<<endl;
            count++;
            start=clock();
            sendto(client,(char*)&send,sizeof(send),0,(SOCKADDR*)&t,sizeof(t));
        }
    }
}


bool makeConnection(SOCKADDR_IN s,SOCKADDR_IN t,SOCKET client){
    //���ֽ�������
    //��һ�����֣�Seq��Ϊ0
    UDP_packet send,receive;
    send.setSourceIP(s);
    send.setTargetIP(t);
    send.setFlags(FLAG_SYN);
    send.setSeq(clientseq);
    flushSeq();
    send.setFlags(FLAG_USE);
    send.calChecksum();
    sendto(client,(char*)&send,sizeof(send),0,(SOCKADDR*)&t,sizeof(t));
    cout<<"[Info]�������������ʼ��һ������"<<endl;
    printf("[Send] ");send.printInfo();
    int len=sizeof(t);
    //��ʱ�ش�
    time_t start=clock();
    time_t end;
    while(1)
    {
        recvfrom(client,(char*)&receive,sizeof(receive),0,(SOCKADDR*)&t,&len);
        //�ڶ������ְ�Ack=1,Seq=0
        if(receive.flags&(FLAG_USE)&&receive.Ack==send.Seq+1)
        {
            printf("[Recv] ");receive.printInfo();
            cout<<"[Info]�յ�ȷ�ϰ�����ʼ����������"<<endl;
            //���������ְ�Ack=1,Seq=1
            send.setAck(receive.Seq+1);
            send.setSeq(clientseq);
            flushSeq();
            send.setFlags(0);
            send.setFlags(FLAG_USE);
            send.setFlags(FLAG_ACK);
            sendto(client,(char*)&send,sizeof(send),0,(SOCKADDR*)&t,sizeof(t));
            printf("[Send] ");send.printInfo();
            cout<<"[Info]--------�����������--------"<<endl;
            return 1;
        }
        //��ʱ�ش�
        end=clock();
        if(((end-start)/CLOCKS_PER_SEC)>=2){
            start=clock();
            sendto(client,(char*)&send,sizeof(send),0,(SOCKADDR*)&t,sizeof(t));
            cout<<"[Info]δ�յ��ڶ������ְ��������ط�......"<<endl;
        }
    }

}

void endConnection(SOCKADDR_IN s,SOCKADDR_IN t,SOCKET client){
    //�Ĵλ��ֶϿ�����
    UDP_packet send,receive;
    send.setSourceIP(s);
    send.setTargetIP(t);
    send.setFlags(FLAG_USE);
    send.setFlags(FLAG_FIN);
    flushSeq();
    cout<<"[Info]���͵�һ�λ��ְ�"<<endl;
    //���͵�һ�λ��֣���ͣ�ȵڶ��λ���ACK��
    stopWaitSend(send,receive,client,s,t);
    UDP_packet send2,receive2;
    int len=sizeof(t);
    //���յ����λ��ֵ�FIN��
    recvfrom(client,(char*)&receive2,sizeof(receive2),0,(SOCKADDR*)&t,&len);
    if(receive2.flags&FLAG_FIN){
        cout<<"[Info]�յ������λ��ְ�"<<endl;
        printf("[Recv] ");receive2.printInfo();
        cout<<"[Info]���͵��Ĵλ��ְ�"<<endl;
        //���͵��Ĵλ���ACK��
        send2.setFlags(FLAG_USE);
        send2.setFlags(FLAG_ACK);
        send2.setAck(receive2.Seq);
        send2.setSeq(clientseq);
        flushSeq();
        sendto(client,(char*)&send2,sizeof(send2),0,(SOCKADDR*)&t,sizeof(t));
        printf("[Send] ");send2.printInfo();
        cout<<"[Info]---------�Ͽ�����--------"<<endl;
        return;
    }
}

void sendFile(SOCKADDR_IN s,SOCKADDR_IN t,char* filePath,SOCKET &client){
    //��ȡ�������ļ��Ĳ���
    for(int i=0;i<10000;i++)
    {
        memset(sendbuf[i],0,MAX_SIZE);
    }
    //��ȡ�ļ�������������¼�ܹ��İ��������һ���������ݳ���
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
    //����STA���������ļ�·����������
    send.setNumber(amount);
    send.setSourceIP(s);
    send.setTargetIP(t);
    send.setFlags(FLAG_STA);
    send.setLength(strlen(filePath));
    memcpy(send.data,filePath,strlen(filePath));
    time_t startsend=clock();
    time_t endsend;
    cout<<"[Info]---------��ʼ�����ļ�---------"<<endl;
    if(stopWaitSend(send,receive,client,s,t)==0){
        cout<<"[Info]��ʼ�ļ�����ʧ��"<<endl;
        return;
    }
    cout<<"[Info]��ʼ���䱨�ķ��ͳɹ�"<<endl;
    //���ʹ������ݵ����ݰ�����amount+1��
    for(int i=0;i<=amount;i++){
        UDP_packet pk;
        pk.setSourceIP(s);
        pk.setTargetIP(t);
        pk.setNumber(amount);
        pk.setFlags(FLAG_USE);
        if(i==amount){
            //���һ�����ݰ�
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
            cout<<"[Info]�������ķ��ͳɹ�"<<endl;
        }
    }
    endsend=clock();
    //������δ�����������
    long fileSize=(long)(amount*MAX_SIZE+p);
    double time=(double)(endsend-startsend)/CLOCKS_PER_SEC;
    double rate=(double)(fileSize*8.0)/(time*1000);
    printf("[Info]����ʱ�䣺%lfs\n�ܴ�С��%dBytes\n�����ʣ�%lfKbps\n",time,fileSize,rate);
    cout<<"[Info]---------�ļ����ͳɹ�---------"<<endl;
    return ;
}




int main()
{
    //�׽��ֳ�ʼ��
    WSADATA wd;
    SOCKET client;
    SOCKADDR_IN sourceAd;
    SOCKADDR_IN targetAd;
    if(WSAStartup(MAKEWORD(2,2),&wd)!=0)
    {
        cout<<"��ʼ��socket��ʧ��!"<<endl;
        return 0;
    }
    client=socket(AF_INET,SOCK_DGRAM,0);
    if(client==INVALID_SOCKET)
    {
        cout<<"sock��ʼ��ʧ��!"<<endl;
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
    //�󶨵�ַ�Ͷ˿�
    bind(client,(sockaddr*)&sourceAd,sizeof(sockaddr));
    while(1){
        //ѭ�������ļ����˳�
        char filePath[50];
        cout<<"�����ļ�·�����˳�(q)"<<endl;
        cin>>filePath;
        if(filePath[0]=='q')break;
        else {
            if(!makeConnection(sourceAd,targetAd,client))
            {
                cout<<"��������ʧ�ܣ�"<<endl;
                return 0;
            }
            sendFile(sourceAd,targetAd,filePath,client);
        }
        clientseq=0;
    }
    //����
    endConnection(sourceAd,targetAd,client);
    closesocket(client);
    WSACleanup();
    system("pause");
    return 0;
}