#include "misc.h"
#include "timer.h"
#include <jsoncpp/json/json.h>
/*
struct ELESTATE
{
    Timer ts;
    Timer td; 
    int floor_last;
    string state_last;
    string door_last;
    int errType;
};

struct ELESATE eleState;
eleState.floor_last = 0;
eleState.state_last = "unknown";
eleState.door_last = "unknown";
eleState.errType = 0;*/

int floor_last = 0;
string state_last = "unknown";
string door_last = "unknown";
int errType = 0;


Queue<string> ele_monitor_q;


int reportErr(Json::Value rsp)
{
    Json::StreamWriterBuilder builder;
    builder["indentation"] = ""; // If you want whitespace-less output
    //发送成功给云端
    string data = Json::writeString(builder, rsp)+'\n';
    ele_monitor_q.push(data);
    log(3, "%s", data.c_str());
        
}
int parseEle(string pkt)
{
    Json::Value mcu;
    Json::Reader reader;
    Json::Value value;
    int duration = 0;
    bool err = false;
    Json::Value rsp;
    rsp["hostname"] = hostname;
    rsp["timestamp"] = getTimeStamp();
    rsp["ID"] = ID;
    if(!reader.parse(pkt, value))
    {
        return 1;
    }   
    //!=0 防止初始化时出错  floorNum 供自己内部使用，是真实隔磁板的数目，
    //但是也是从最底层-3开始，而不是从1开始
    /*
    {
       "ID" : "00101",
       "door" : "closed",
       "floorNum" : "6",
       "floorNum_r" : "30",
       "state" : "stop",
       "timestamp" : "1583737040199"
    }
    */
    //使用floorNum，因为只有真实隔磁板才会递增，而面板的楼层时不连续的，所以很难判断出错误
    //那low high需要填写面板上楼层，因为只有这样才不会小于 或者大于
    //2020-3-27 可以按照实际的填写，因为遮光板数量不可能大于面板上的楼层数的
    //这里需要避免不断的上报错误信息
    if(stoi(value["floorNum"].asString()) == 0)
    {
        return 2;
    }

    if(!value["state"].isNull())
    {
        string state = value["state"].asString();
        string state_l = state_last;

        if(state.compare("down") == 0)
        {
            state_last = state;
            if(state_l.compare("up") == 0)
            {
                rsp["msg"] = "电梯上行中突然下行";
                cout << "电梯上行中突然下行\n";
                reportErr(rsp);
            }
            //开始计时
            if(ts.getStatus() == STOPPED)
            {
                ts.start();
            }
        }
    
        else if(state.compare("up") == 0)
        {
            state_last = state;
            if(state_l.compare("down") == 0)
            {
                rsp["msg"] = "电梯下行中突然上行";
                cout << "电梯下行中突然上行\n";
                reportErr(rsp);
            }
            //开始计时
            if(ts.getStatus() == STOPPED)
            {
                printf("电梯状态开始计时\n");
                ts.start();
            }
        }
        else if(state.compare("stop") == 0)
        {
            state_last = state;
            //在电梯运行过程中只要是到达平层就停止计时
            if(ts.getStatus() == STARTED)
            {
                ts.stop();
            }
        }

    }
        
    if(!value["floorNum"].isNull())
    {
        //如果超出楼层范围
        //2020-4-3 屏蔽这里的楼层报错
        int floor = stoi(value["floorNum"].asString());
        //刚初始化的时候floor_last是0，会导致下面的判断出错
        if(floor_last == 0)
        {
            floor_last = floor;
        }

        else if(state_last.compare("up") == 0)
        {
            //上升过程
            if(floor_last > floor)
            {
                rsp["msg"] = "电梯在上行，楼层在减少";
                if(errType != 3)
                {
                    cout << "电梯在上行，楼层在减少\n";
                    reportErr(rsp);
                    errType = 3;
                }
            }
            //floor_last != floor 这里解决的是如果从上行到停止的时间里不要报错
            //floor_last != -1 是因为-1 到1 会跳变
            if(floor_last+1 != floor && floor_last != floor && floor_last != -1)
            {
                rsp["msg"] = "电梯在上行，楼层没有递增";
                if(errType != 4)
                {
                    cout <<  "电梯在上行，楼层没有递增\n";
                    reportErr(rsp);
                    errType = 4;
                }
            }
        }
        else if(state_last.compare("down") == 0)
        {
            //下降过程
            //这里要注意，floor_last初始化为0，会导致程序一启动就报错
            if(floor_last < floor)
            {
                rsp["msg"] = "电梯在下行，楼层在增加";
                //
                if(errType != 5)
                {
                    cout << "电梯在下行，楼层在增加\n";
                    errType = 5;
                    reportErr(rsp);
                }
            }
            if(floor_last-1 != floor && floor_last != floor && floor_last != 1)
            {
                rsp["msg"] = "电梯在下行，楼层没有递减";
                if(errType != 6)
                {
                    cout << "电梯在下行，楼层没有递减\n";
                    reportErr(rsp);
                    errType = 6;
                }
            }
        }
        floor_last = floor;
    }

    if(!value["door"].isNull())
    {
        string door = value["door"].asString();
        if(door_last.compare(door) != 0)
        {
            //如果不相同
            door_last = door;
            //如果已经计时，就停止计时
            if(td.getStatus() == STARTED)
            {
                td.stop();
            }
        }
        else
        {
            //如果状态相同，且为开门就开始计时，关门多久没关系
            if(td.getStatus() == STOPPED && door.compare("opened") == 0)
            {
                printf("door 开始计时\n");
                td.start();
            }
        }
    }

    return 0;
}


//{"ID":"", "hostname":"", "msg":"", "timestamp":""}
//2020-3-27 这里检测状态和电梯门状态，目的是为了检测出是否在维修
void checkEle()
{
    Json::Value rsp;
    Json::StreamWriterBuilder builder;
    builder["indentation"] = ""; // If you want whitespace-less output
    rsp["hostname"] = hostname;
    rsp["timestamp"] = getTimeStamp();
    rsp["ID"] = ID;
    time_t t = ele["door"].second.getStartedTime();


    if(ele["state"].second.getStartedTime() > STATETIMEOUT)
    {
        //2020-4-3：这里只根据一直上行或下行来判断是否在维护
        //这里一直报警没有问题
        //只有在up down的时候才会计时
        //if(ele["state"].first.compare("stop") != 0)
        {
            rsp["msg"] = "电梯状态超时，可能在维护";
            printf("电梯状态超时，可能在维护\n");
            log(3,"电梯状态超时，可能在维护");
            //发送成功给云端
            string data = Json::writeString(builder, rsp)+'\n';
            ele_monitor_q.push(data);
            //停止计时
            if(ele["state"].second.getStatus() == STARTED)
            {
                ele["state"].second.stop();
            }
        }
    }
    if(  t  > DOORTIMEOUT)
    {
        //这里没有问题，因为在报一次错误之后，会重新计时
        //只有开门的时候才开始计时，所以这里不需要判断是不是开门
        //if(ele["door"].first.compare("opened") == 0)
        {
            rsp["msg"] = "电梯门开门状态超时";
            log(3,"电梯开门状态超时");
            //发送成功给云端
            string data = Json::writeString(builder, rsp)+'\n';
            ele_monitor_q.push(data);
            //停止计时
            if(ele["door"].second.getStatus() == STARTED)
            {
                ele["door"].second.stop();
            }
        }
    }

    rsp["msg"] = "heartbeat";
    //无论上面有没有错误信息要发送，都要发送一个heartbeat
    string data = Json::writeString(builder, rsp)+'\n';
    ele_monitor_q.push(data);

}


