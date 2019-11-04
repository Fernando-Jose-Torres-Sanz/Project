#ifndef __M_EPOLL_H__
#define __M_EPOLL_H__
#include"tcpsocket.hpp"
#include<iostream>
#include<vector>
#include<stdlib.h>
#include<sys/epoll.h>
#include<string>
#define MAX_NUM 1024
using namespace std;
class Epoll{
    public:
        Epoll()
            :_epfd(-1)
        {}
        ~Epoll(){}
    public:
        bool EpollInit(){
            _epfd=epoll_create(1);
            if(_epfd<0){
                cerr<<"epoll create error"<<endl; 
                return false;
            }
            return true;
        }
        bool Add(TcpSocket &sock){
            int fd=sock.Getfd();
            struct epoll_event ev;
            ev.events=EPOLLIN;
            ev.data.fd=fd;
            int ret=epoll_ctl(_epfd,EPOLL_CTL_ADD,fd,&ev);
            if(ret<0){
                cerr<<"epoll ctl error"<<endl;
                return false;
            }
            return true;
        }
        bool Clr(TcpSocket &sock){
            int fd=sock.Getfd();
            int ret=epoll_ctl(_epfd,EPOLL_CTL_DEL,fd,NULL);
            if(ret<0){
                cerr<<"delete monitor error"<<endl;
            }
            return true;
        }
        bool Wait(vector<TcpSocket> &list,int timeval=3000){
           struct epoll_event ev[MAX_NUM];
           int ret=epoll_wait(_epfd,ev,MAX_NUM,timeval);
           if(ret<0){
               sleep(1);
               cerr<<"epoll wait error"<<endl;
               return false;
           }else if(ret==0){
               return false;
           }
           for(int i=0;i<ret;i++){
               TcpSocket sock;
               int fd=ev[i].data.fd;
               sock.Setfd(fd);
               list.push_back(sock);
           }
           return true;
        }
    private:
        int _epfd;
};
#endif
