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
int WindowSize = 1;//���ڴ�С
int base = -1;//������������ȷ�ϵİ�λ��
int lastWritten = 0;//���İ�λ��
int nextSeqNum = 0;//��һ�������͵İ�λ��
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

//���ʹ��ڰ�����
UDP_packet sendList[10000];
bool recvAck[10000]={0};
bool haveSend[10000]={0};
time_t startTime[10000];

//��ӡ���Ͷ˴������
void printWindowInfo(bool fin=0){
    cout<<"Win="<<WindowSize<<" Base="<<base<<" NextSeqNum="<<nextSeqNum<<endl;
    return;
}

bool makeConnection(SOCKADDR_IN s,SOCKADDR_IN t,SOCKET client){
    //���ֽ�������
    //��һ�����֣�Seq��Ϊ0
    UDP_packet send,receive;
    send.setSourceIP(s);
    send.setTargetIP(t);
    send.setFlags(FLAG_SYN);
    send.setSeq(0);
    send.setFlags(FLAG_USE);
    send.calChecksum();
    sendto(client,(char*)&send,sizeof(send),0,(SOCKADDR*)&t,sizeof(t));
    cout<<"[Info] �������������ʼ��һ������"<<endl;
    printf("[Send] ");send.printInfo();
    int len=sizeof(t);
    //��ʱ�ش�
    time_t start=clock();
    time_t end;
    while(1)
    {
        recvfrom(client,(char*)&receive,sizeof(receive),0,(SOCKADDR*)&t,&len);
        //�ڶ������ְ�Ack=1,Seq=0
        if(receive.flags&(FLAG_USE)&&receive.Ack==1)
        {
            printf("[Recv] ");receive.printInfo();
            cout<<"[Info] �յ�ȷ�ϰ�����ʼ����������"<<endl;
            //���������ְ�Ack=1,Seq=1
            send.setAck(1);
            send.setSeq(1);
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
    UDP_packet send2,receive2;
    send.setSourceIP(s);
    send.setTargetIP(t);
    send.setFlags(FLAG_USE);
    send.setFlags(FLAG_FIN);
    send.setSeq(nextSeqNum++);
    send.calChecksum();
    cout<<"[Info]���͵�һ�λ��ְ�"<<endl;
    sendto(client,(char*)&send,sizeof(send),0,(SOCKADDR*)&t,sizeof(t));
    cout<<"[Send] ";send.printInfo();
    //���͵�һ�λ��֣����ȴ��ڶ��λ���ACK��
    time_t start=clock();
    time_t end;
    int len = sizeof(SOCKADDR);
    while(1)
    {
        recvfrom(client,(char*)&receive,sizeof(receive),0,(SOCKADDR*)&t,&len);
        //�ڶ������ְ�
        if(receive.flags&(FLAG_USE)&&receive.flags&FLAG_ACK&&receive.Ack==send.Seq&&receive.checkChecksum())
        {
            printf("[Recv] ");receive.printInfo();
            cout<<"[Info]�յ��ڶ��λ��ְ�"<<endl;
            recvfrom(client,(char*)&receive2,sizeof(receive2),0,(SOCKADDR*)&t,&len);
            if(receive2.flags&FLAG_FIN&&receive2.flags&FLAG_USE&&receive2.checkChecksum()){
                cout<<"[Info]�յ������λ��ְ�"<<endl;
                printf("[Recv] ");receive2.printInfo();
                cout<<"[Info]���͵��Ĵλ��ְ�"<<endl;
                //���͵��Ĵλ���ACK��
                send2.setFlags(FLAG_USE);
                send2.setFlags(FLAG_ACK);
                send2.setAck(receive2.Seq);
                send2.setSeq(nextSeqNum++);
                send2.calChecksum();
                sendto(client,(char*)&send2,sizeof(send2),0,(SOCKADDR*)&t,sizeof(t));
                printf("[Send] ");send2.printInfo();
                cout<<"[Info]---------�Ͽ�����--------"<<endl;
                return;
            }
        }
        //��ʱ�ش�
        end=clock();
        if(((end-start)/CLOCKS_PER_SEC)>=2){
            start=clock();
            sendto(client,(char*)&send,sizeof(send),0,(SOCKADDR*)&t,sizeof(t));
            cout<<"[Info]δ�յ��ڶ��λ��ְ��������ط�......"<<endl;
        }
    }
}

//�����߳�
DWORD WINAPI sendHandle(LPVOID lparam){
    time_t end;
    while(base<lastWritten){
        //�Ӵ�������࿪ʼ�����ƶ�����   
        for(nextSeqNum = base+1;nextSeqNum<=base+WindowSize&&nextSeqNum<=lastWritten;nextSeqNum++){
            if(recvAck[nextSeqNum]==0){
                //�����������λ��δ�յ���ACK
                if(haveSend[nextSeqNum]==0){
                    //֮ǰû�з��͹����λ�ö�Ӧ�İ�
                    //���ͣ���������ʱ������Ǹ�λ���ѷ��͹�
                    startTime[nextSeqNum] = clock();
                    sendto(client,(char*)&sendList[nextSeqNum],sizeof(sendList[nextSeqNum]),0,(SOCKADDR*)&targetAd,sizeof(targetAd));
                    haveSend[nextSeqNum]=1;
                    cout<<"[Send]���͵�"<<nextSeqNum<<"�����ݰ� ";
                    sendList[nextSeqNum].printInfo();
                    printWindowInfo();
                }
                else{
                    //֮ǰ���͹�������Ƿ�ʱ����ʱ���ط���������λ�ö�ʱ��
                    time_t end = clock();
                    if((double)(end-startTime[nextSeqNum])/CLOCKS_PER_SEC >= 1){
                        startTime[nextSeqNum] = clock();
                        sendto(client,(char*)&sendList[nextSeqNum],sizeof(sendList[nextSeqNum]),0,(SOCKADDR*)&targetAd,sizeof(targetAd));
                        cout<<"[Resend]�ط���"<<nextSeqNum<<"�����ݰ�";
                        sendList[nextSeqNum].printInfo();
                        printWindowInfo();
                    }
                }
            }
        }
    }
    return 1;
}

//�����߳�
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
                //����յ���ACK�Ǵ�������˵ģ����һ�������һ��δ�յ�ȷ�ϵķ���֮ǰ
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
    //��ȡ�������ļ��Ĳ���
    for(int i=0;i<10000;i++)
    {
        memset(sendbuf[i],0,MAX_SIZE);
    }
    //��ȡ�ļ�������������¼�ܹ��İ��������һ���������ݳ���
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
    //����STA���������ļ�·����������
    send.setNumber(amount);
    send.setSourceIP(s);
    send.setTargetIP(t);
    send.setFlags(FLAG_USE);
    send.setFlags(FLAG_STA);
    send.setLength(strlen(filePath));
    memcpy(send.data,filePath,strlen(filePath));
    send.calChecksum();
    sendList[0]=send;

    //�Ѵ������ݵİ����뷢�ͻ������
    for(int i=1;i<=amount+1;i++){
        UDP_packet pk;
        pk.setSourceIP(s);
        pk.setTargetIP(t);
        pk.setNumber(amount);
        pk.setFlags(FLAG_USE);
        pk.setSeq(i);
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
        pk.calChecksum();
        sendList[i]=pk;
    }
    time_t startsend=clock();
    time_t endsend;
    cout<<"[Info]---------��ʼ�����ļ�---------"<<endl;
    if(base<lastWritten){
        HANDLE recvThread = CreateThread(NULL,NULL,receiveHandle,LPVOID(),0,NULL);
        HANDLE sendThread = CreateThread(NULL,NULL,sendHandle,LPVOID(),0,NULL);
        WaitForSingleObject(recvThread,INFINITE);
        WaitForSingleObject(sendThread,INFINITE);
        if(base == lastWritten){
            cout<<"[Info]---------�������---------"<<endl;
            endsend = clock();
        }
    }
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
    cout<<"���봰�ڴ�С��"<<endl;
    cin>>WindowSize;
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
            base = -1;
            lastWritten = 0;
            nextSeqNum = 0;
            memset(&recvAck,0,10000);
            memset(&haveSend,0,10000);
            sendFile(sourceAd,targetAd,filePath,client);
        }
    }
    //����
    endConnection(sourceAd,targetAd,client);
    closesocket(client);
    WSACleanup();
    system("pause");
    return 0;
}