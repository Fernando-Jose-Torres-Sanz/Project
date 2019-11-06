//实现一个线程池
#ifndef __M_THREAD_H__
#define __M_THREAD_H__
#include<iostream>
#include<sstream>
#include<vector>
#include<queue>
#include<unistd.h>
#include<pthread.h>
#include<stdlib.h>
#include<thread>
#include<time.h>
using namespace std;

//定义一个函数指针
typedef void (*handler_t)(int);
//定义任务队列的结点
class ThreadTask{
public:
    //设置任务
    void SetTask(int data,handler_t handle){
        _data=data;
        _handle=handle;
        return;
    }
    //让指定的函数跑起来
    void Run(){
        return _handle(_data);
    }
private:
    int _data;
    handler_t _handle;
};


//封装线程池
class ThreadPool{
public:
    ThreadPool(int maxt=5,int maxq=10)
        :_capacity(maxq)
        ,_thr_max(maxt)
    {
        pthread_mutex_init(&_mutex,NULL);
        pthread_cond_init(&_cond_con,NULL);
        pthread_cond_init(&_cond_pro,NULL);
    }


    ~ThreadPool()
    {
        pthread_mutex_destroy(&_mutex);
        pthread_cond_destroy(&_cond_con);
        pthread_cond_destroy(&_cond_pro);
    }
    //线程初始化
    bool ThreadPoolInit(){
        for(int i=0;i<_thr_max;i++){
            thread thr(&ThreadPool::thr_start,this);
            thr.detach();
        }
        return true;
    }
    //任务入队
    bool TaskPush(ThreadTask& tt){
        pthread_mutex_lock(&_mutex);
        while(_queue.size()==_capacity){
            pthread_cond_wait(&_cond_pro,&_mutex);
        }
        _queue.push(tt);
        pthread_mutex_unlock(&_mutex);
        pthread_cond_signal(&_cond_con);
        return true;
    }
private:
    pthread_mutex_t _mutex;
    pthread_cond_t _cond_con;
    pthread_cond_t _cond_pro;
    queue<ThreadTask> _queue;
    int _capacity;//任务队列的最大数量

    int _thr_max;//线程的最大数量
private:
    void thr_start(){
        while(1){
            pthread_mutex_lock(&_mutex);
            while(_queue.empty()){
                pthread_cond_wait(&_cond_con,&_mutex);
            }
            ThreadTask tt=_queue.front();
            _queue.pop();
            pthread_mutex_unlock(&_mutex);
            pthread_cond_signal(&_cond_pro);
            //任务处理应该放在解锁之后,否知同一时间只有一个线程在处理任务
            tt.Run();
        }
        return ;
    }
};
#endif








