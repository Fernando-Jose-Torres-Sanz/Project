#ifndef __M_TCP_H__
#define __M_TCP_H__
#include"tcpsocket.hpp"
#include<fstream>
#include"threadpool.hpp"
#include"epollwait.hpp"
#include"http.hpp"
#include<boost/filesystem.hpp>
#include<iostream>
#define WWW_ROOT "./www"
using namespace std;
class Serve{
    public:
        bool Start(uint16_t port){
            bool ret;
            //创建一个套接字, 绑定地址信息,开始监听
            ret=_pool.ThreadPoolInit();
            if(ret==false){
                return false;
            }
            ret=_lst_sock.SockInit(port);
            if(ret==false){
                return false;
            }
            //创建epoll监控套接字
            ret=_epoll.EpollInit();
            if(ret==false){
                return false;
            }
            //将监听套接字用epoll监控起来
            _epoll.Add(_lst_sock);
            while(1){
                vector<TcpSocket> list;
                ret=_epoll.Wait(list);
                if(ret==false){
                    continue;
                }
                //循环对就绪的数组进行处理
                for(size_t i=0;i<list.size();i++){
                    //当前就绪套接字为监听套接字
                    if(list[i].Getfd()==_lst_sock.Getfd()){
                        TcpSocket _cli_sock;
                        ret=_lst_sock.Accept(_cli_sock);
                        if(ret==false){
                            continue;
                        }
                        _cli_sock.SetNonBlock();
                        _epoll.Add(_cli_sock);
                        //当前套接字为客户端套接字
                    }
                    else{
                        ThreadTask tt;
                        tt.SetTask(list[i].Getfd(),HttpHandler);
                        _pool.TaskPush(tt);
                        _epoll.Clr(list[i]);//为了防止循环第二次到来时,在将此套接字重新抛入到线程池中, 因此需要将其从poll中移除
                    }
                }
            }
            _lst_sock.Close();
            return true;
        }

    public:
        static void HttpHandler(int sockfd){
            TcpSocket clisock;
            clisock.Setfd(sockfd);
            //Request类用来接收数据,解析请求
            HttpRequest req;
            HttpResponse rsp;
            int status=req.RequestParse(clisock);
            if(status!=200){
                rsp._status=status;
                rsp.ErrorResponse(clisock);
                clisock.Close();
                return;
            }

            //根据请求进行处理,将处理结果放在rsp中
            Process(req,rsp);
            //将处理结果响应给套接字
            rsp.SuccessResponse(clisock);
            clisock.Close();
            return;
        }
        static bool Process(HttpRequest &req,HttpResponse &rsp){
            string real_path = WWW_ROOT + req._path;
            if(!boost::filesystem::exists(real_path)){
                rsp._status=404;
                return false;
            }
            cout<<"--------------"<<endl;
            cout<<"real_path["<<real_path<<"]"<<endl;
            cout<<"method["<<req._method<<"]"<<endl;
            if((req._method=="GET"&&req._param.size()!=0)||req._method=="POST"){
                //当前请求是一个文件上传请求
                CGIprocess(req,rsp);
            }else{
                //当前的请求是一个文件下载或者文件列表展示的请求       
                if(boost::filesystem::is_directory(real_path)){
                    //查看目录列表请求
                    ListShow(real_path,rsp._body);
                    rsp.SetKeyandVal("Content_Type","text/html");
                }else{
                    //文件下载请求
                    Download(real_path,rsp._body);
                    rsp.SetKeyandVal("Content-Type","application/octet-stream");
                }
            }
            rsp._status=200;
            return true;
        }    

        static bool CGIprocess(HttpRequest &req,HttpResponse &rsp){
            //利用管道将正文信息传递给子进程
            //子进程从out中去读,从in中去写
            int pipe_in[2],pipe_out[2];
            if(pipe(pipe_in) < 0 || pipe(pipe_out) < 0){
                cerr<<"create pipe error"<<endl;
                return false;
            }
            int pid=fork();
            if(pid<0){
                cerr<<"create fork error"<<endl;
                return false;
            }else if(pid==0){
                //在子进程中创建换将变量获取头部信息
                close(pipe_out[1]);//用于父进程读,子进程写,关闭读端
                close(pipe_in[0]);//用于父进程写,子进程读,关闭写端
                dup2(pipe_out[0],0);
                dup2(pipe_in[1],1);
                setenv("METHOD",req._method.c_str(),1);
                setenv("PATH",req._path.c_str(),1);
                for(auto i:req._headers){
                    setenv(i.first.c_str(),i.second.c_str(),1);
                }
                //创建子进程成功.程序替换
                string real_path=WWW_ROOT+req._path;
                execl(real_path.c_str(),real_path.c_str(),NULL);
                exit(0);
            }
            //父进程
            close(pipe_in[1]);
            close(pipe_out[0]);
            write(pipe_out[1],&req._body[0],req._body.size());
            while(1){
                char buf[1024]={0};
                int ret=read(pipe_in[0],buf,1024);
                if(ret==0){
                    break;
                }
                buf[ret]='\0';
                rsp._body=rsp._body+buf;
            }
            close(pipe_in[0]);
            close(pipe_out[1]);
            return true;
        }
        static bool Download(string &path,string &body){
            int64_t fsize=boost::filesystem::file_size(path);
            body.resize(fsize);
            ifstream file(path);
            if(!file.is_open()){
                return false;
            }
            file.read(&body[0],fsize);
            if(!file.good()){
                return false;
            }
            file.close();
            return true;
        }
        static bool ListShow(string &path,string &body){
            stringstream tmp;
            tmp<<"<html><head><style>";
            tmp<<"*{margin : 0;}";
            tmp<<".main-window {height : 100%;width : 80%;margin : 0 auto;}";
            tmp<<".upload {position : relative;height : 20%;width : 100%;background-color :#33c0b9; text-align:center;}";
            tmp<<".listshow {position : relative;height : 80%;width : 100%;background : #6fcad6;}";
            tmp<<"</style></head>";
            tmp<<"<body><div class='main-window'>";

            tmp<<"<div class='upload'>";
            tmp<<"<form action='/upload' method='POST' enctype='multipart/form-data'>";
            tmp<<"<div class='upload-btn'>";
            tmp<<"<input type='file' name='fileupload'>";
            tmp<<"<input type='submit' name='submit' value='input'>";
            tmp<<"</div></form>";

            tmp<<"</div><hr />";
            tmp<<"<div class='listshow'><ol>";
            //..........
            boost::filesystem::directory_iterator begin(path);
            boost::filesystem::directory_iterator end;
            string www=WWW_ROOT;
            size_t pos=path.find(WWW_ROOT);
            if(pos==string::npos){
                return false;
            }
            string req_path=www.substr(www.size());
            for(;begin!=end;begin++){
                int64_t mtime,ssize;//文件时间与大小
                //获取文件路径
                string pathname=begin->path().string();
                //获取文件名
                string name=begin->path().filename().string();
                //当前迭代器对应的是一个目录
                cout<<"pathname["<<name<<"]"<<endl;
                string uri=req_path+name;
                if(boost::filesystem::is_directory(pathname)){
                    tmp<<"<li><strong><a href='";
                    tmp<<uri<<"'>'";
                    tmp<<name<<"/";
                    tmp<<"</a><br /></strong><small>";
                    tmp<<"fileTyope : directory ";
                    tmp<<"</small></li>";
                    //当前迭代器对应的是一个文件
                }else{
                    mtime=boost::filesystem::last_write_time(begin->path());
                    ssize=boost::filesystem::file_size(begin->path());
                    tmp<<"<li><strong><a href='";
                    tmp<<uri<<"'>";
                    tmp<<name;
                    tmp<<"</a><br /></strong>";
                    tmp<<"<small> modified :";
                    tmp<<mtime;
                    tmp<<"<br /> fileTyope : application-ostream ";
                    tmp<<ssize/1024<<" -bit";
                    tmp<<"</small></li>";
                }
            }
            tmp<<"</ol></div><hr /></div></body></html>";
            body=tmp.str();
            return true;
        }
    private:
        TcpSocket _lst_sock;
        ThreadPool _pool;
        Epoll _epoll;
};
#endif
