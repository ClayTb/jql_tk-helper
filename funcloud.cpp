


#include <sys/inotify.h>
#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include "misc.h"
#include <iostream>
#include <fstream>
#include "mqttcloud.h"
using namespace std;


string floor = "unknown";
string door = "unknown";
string regFloor = "unknown";


string autoTime;
string ID;
int REGISTERED = 0;
void  registerFloor(string floor)
{
    cout << "register " << floor << endl;
    regFloor = floor;
    REGISTERED = 1;
}

int readAuto()
{
    std::string path;
    path.append("/home/tikong/production/config.ini");
    ifstream sup(path);

    if(!sup.is_open())
    {
        return 1;
    }
    std::string line;
    while(getline(sup, line))
    {
       if(line.find("AUTO_OPEN") != std::string::npos)
       {
            //cout << line << endl;
            auto delimeterPos = line.find(":");
            auto key = line.substr(0, delimeterPos);
            auto value = line.substr(delimeterPos+1);
            //cout << line << endl;
            if (key.compare("AUTO_OPEN") == 0)
            {
                //去除换行
                //mcu_ser = value.substr(0,value.length()-1);
                //mcu_ser = value;
                autoTime = value;
                cout<< "AUTO_OPEN: "<<autoTime.c_str()<< endl;
                log(6, "AUTO_OPEN: %s", autoTime.c_str());
            }
            else if (key.compare("ID") == 0)
            {
                ID = value;
                cout << "ID: " << ID << endl;
                log(6, "ID: %s", ID.c_str());
            }  
        }

    }//结束读取文件
    if(autoTime.empty())
    {
        log(3, "没有找到AUTO_OPEN");
        printf("没有找到AUTO_OPEN\n");
        //直接退出
        exit(2);
    }
    return 0;
}



/*
{"ID":"", "requestID":"", "cmd":"call", "floorNum":""}
{"ID":"", "requestID":"", "cmd":"close", "duration":""}
{"ID":"", "requestID":"", "cmd":"open", "duration":""}
{"ID":"", "requestID":"", "cmd":"cancelclose"}
{"ID":"", "requestID":"", "cmd":"cancelopen"}
*/
//将cloud的数据变成本地的json数据
bool parseCloud(string data)
{
    //{"ID":"8888", "requestID":"6666", "cmd":"open", "duration":"undefined"}
    //防止出现这样的问题
    Json::Value mcu;
    Json::Reader reader;
    Json::Value value;
    int duration = 0;
    bool err = false;
    Json::Value rsp;
    if(!reader.parse(data, value))
    {  
        return false; 
    }
    if(value["cmd"].isNull())
    {
        return false;
    }
    //操作电梯指令
    string cmd = value["cmd"].asString();
    if(cmd.compare("call") == 0)
    {
        if(value["floorNum_r"].isNull())
        {
            return false;
        }
        //登记呼梯楼层，到达后自动开门10秒，做到config里
        string floor = value["floorNum_r"].asString();
        registerFloor(floor);
    }
    else if(cmd.compare("close") == 0 || cmd.compare("open") == 0)
    {
        if(value["duration"].isNull())
        {
            return false;
        }
        string duration = value["duration"].asString();
        if(!isNum(duration))
        {
            return false;
        }
    }
    else if(cmd.compare("cancelopen") == 0)
    {

    }
    else if(cmd.compare("cancelclose") == 0)
    {

    }
    else
    {
        return false;
    }
    return true;
}



void autoOpen(string state)
{
    Json::Value mcu;
    Json::Reader reader;
    Json::Value value;
    bool err = false;
    if(reader.parse(state, value))
    {       
        /*
        {
           "ID" : "00101",
           "door" : "closed", / "opened"
           "floorNum" : "6",
           "floorNum_r" : "30",
           "state" : "stop",
           "timestamp" : "1583737040199e"
        }
        */
        string floor = value["floorNum_r"].asString();
        string door = value["door"].asString();
        string state = value["state"].asString();
        //楼层已经到达，并且已经呼梯了，并且门已经在打开的情况下，自动开门10s一次
        if(floor == regFloor && REGISTERED == 1 && door == "opened")
        {
            string temp = "{\"ID\":\"00000\", \"sender\":\"autoOpen\",\"requestID\":";
            //string data =  temp + '"' + randomstring(10) + '"' + ",\"timestamp\":" + '"' + getTimeStamp() + '"' + ",\"cmd\":\"open\", \"duration\":" + '"' + autoTime + '"}';
            string data =  temp + '"' + randomstring(10) + '"' + ",\"timestamp\":" + '"' + getTimeStamp() + '"' + ",\"cmd\":\"open\", \"duration\":" + '"' + autoTime + "\"}";
            //string data = '{"ID":"00000", "sender":"autoOpen","requestID":'+ '"' + randomstring(10) + '"' + ',"timestamp":' + '"' + getTimeStamp() + '"' + ',"cmd":"open", "duration":"10"}';
            cout << data << endl;
            int ret = mqtt_send(mosq_l, LCMD, data.c_str());
            if(ret != 0)
            {
                //如果本地没有连接，这里也会报错
                log(4, "mqtt_send local autoOpen error=%i\n", ret);
            }
            //这时候状态转
            REGISTERED = 0;
        }
    }
}

void cancelAuto(string state)
{
    Json::Value mcu;
    Json::Reader reader;
    Json::Value value;
    bool err = false;
    if(reader.parse(state, value))
    {       
        /*
        {
           "ID" : "00101",
           "door" : "closed", / "opened"
           "floorNum" : "6",
           "floorNum_r" : "30",
           "state" : "stop",
           "timestamp" : "1583737040199e"
        }
        */
        string floor = value["floorNum_r"].asString();
        string door = value["door"].asString();
        string state = value["state"].asString();
      
        //如果这时候已经按了开门键，但是楼层已经不是目的楼层了，就需要释放开门键
        if(2 == REGISTERED)
        {
            if(floor != regFloor || door == "closed" || state != "stop")
            {
                //释放开门键
                string temp = "{\"ID\":\"00000\", \"sender\":\"autoOpen\",\"requestID\":";
                string data =  temp + '"' + randomstring(10) + '"' + ",\"timestamp\":" + '"' + getTimeStamp() + '"'+",\"cmd\":\"cancelopen\"}";
                //string data = '{"ID":"00000", "sender":"autoOpen","requestID":'+ '"' + randomstring(10) + '"' + ',"timestamp":' + '"' + getTimeStamp() + '"' + ',"cmd":"open", "duration":"10"}';
                cout << data << endl;
                int ret = mqtt_send(mosq_l, LCMD, data.c_str());
                if(ret != 0)
                {
                    //如果本地没有连接，这里也会报错
                    log(4, "mqtt_send local autoOpen cancelopen error=%i\n", ret);
                }
                REGISTERED = 0;
            }           
        }
    }
}


