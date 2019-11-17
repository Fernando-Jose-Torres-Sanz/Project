#include<iostream>
#include<fstream>
#include<sstream>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<vector>
#include<boost/algorithm/string.hpp>
#define WWW_ROOT "./www/"
using namespace std;
class Boundary{
    public:
        int64_t _string_addr;
        int64_t _data_len;
        string _name="null";
};
bool GetName(string& header,Boundary& node){
    vector<string> list;
    boost::split(list,header,boost::is_any_of("\r\n"),boost::token_compress_on);
    for(int i=0;i<list.size();i++){
        size_t pos=list[i].find(": ");
        if(pos==string::npos){
            return false;
        }
        string key=list[i].substr(0,pos);
        string val=list[i].substr(pos+1);
        if(key!="Content-Disposition"){
            continue;
        }
        string filename="filename=\"";
        pos=val.find(filename);
        if(pos==string::npos){
            return false;
        }
        pos+=filename.size();
        size_t npos=val.find("\"",pos);
        if(npos==string::npos){
            return false;
        }
        node._name=val.substr(pos,npos-pos);
    }
    return true;
}
bool BoundaryParse(string& body,vector<Boundary>& list){

    size_t pos=0,npos=0;
    string _header;
    string boundary;
    char* cont_type=getenv("Content-Type");
    if(cont_type!=NULL){
        string tmp(cont_type);
        size_t pos=tmp.find("boundary=");
        if(pos==string::npos){
            return false;
        }
        boundary=tmp.substr(pos+9);
    }
    string f_boundary="--"+boundary;
    string m_boundary="\r\n"+f_boundary;
    string l_boundary="\r\n"+f_boundary+"--";

    pos=body.find(f_boundary,pos);
    if(pos==string::npos){
        return false;
    }
    while(1){
        Boundary b;
        npos=pos+f_boundary.size()+2;
        pos=body.find("\r\n\r\n",npos);
        if(pos==string::npos){
            return false;
        }
        _header=body.substr(npos,pos-npos);
        GetName(_header,b);
        npos=pos+4;
        b._string_addr=npos;
        pos=body.find(m_boundary,npos);
        if(pos==string::npos){
            return false;
        }
        b._data_len=pos-npos;
        list.push_back(b);
        size_t tmp_pos;
        tmp_pos=pos+m_boundary.size();
        if(body.substr(tmp_pos,1)=="-"){
            break;
        }
    }
    return true;
}

bool StorageFile(string &body,vector<Boundary> &list){
    for(int i=0;i<list.size();i++){
        if(list[i]._name=="null"){
            continue;
        }
        string real_path=WWW_ROOT+list[i]._name;
        ofstream file;
        file.open(real_path,ofstream::out | ofstream::app );
        if(!file.is_open()){
            return false;
        }
        file.write(&body[list[i]._string_addr],list[i]._data_len);
        if(!file.good()){
            return false;
        }
        file.close();
    }
    return true;
}

int main(int argc,char *argv[],char *env[]){
    string err="<html>Failed!!</html>";
    string suc="<html>Seccess!!</html>";
    for(int i=0;env[i]!=NULL;i++){
        cerr<<"env["<<i<<"]"<<"==========["<<env[i]<<"]"<<endl;
    }
    string body;
    char* content_len=getenv("Content-Length");
    if(content_len!=NULL){
        stringstream tmp;
        tmp<<content_len;
        uint64_t fsize;
        tmp>>fsize;
        body.resize(fsize);
        int rlen=0;
        while(rlen<fsize){
            int ret= read(0,&body[0]+rlen,fsize-rlen);
            if(ret<=0){
                exit(-1);
            }
            rlen+=ret;
        }
    }
    vector<Boundary> list;
    bool ret;
    ret=BoundaryParse(body,list);
    if(ret==false){
        cerr<<"boundary parse error"<<endl;
        cout<<err;
        return -1;
    }
    ret=StorageFile(body,list);
    if(ret==false){
        cerr<<"Storage file error"<<endl;
        cout<<err;
        return -1;
    }
    cout<<suc<<endl;
    return 0;
}   
