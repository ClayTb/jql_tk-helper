#include "workerthread.h"
#include "mqtt.h"
#include "misc.h"

using namespace std;
string MAC;
string hostname;
int cloudSetup()
{
    //初始化和云端的通信
    MAC = getMac();
    hostname = exec("hostname");
    if(macOK(MAC) != 5)
    {
        log(3, "mac 错误");
        return 1;
    }
    //"cmd/IotApp/fa:04:39:46:16:2b/+/+/+/#"
    //string CMD = "cmd/IotApp/";
    CCMD = CCMD + MAC +"/+/+/+/#";
    cout << "云端cmd topic：" << CCMD << endl;
    //cmd_resp/:ProductName/:DeviceName/:CommandName/:RequestID/:MessageID
    CRSP = CRSP + MAC +"/";
}

//把云端的命令下发下去
int cloudThread()
{
    //数据pop出来
    cout<<"send cloud cmd thread\n";
    log(6,"send cloud cmd thread");
    string data, ldata;
    int ret = -1;
    while(1) 
    {      
        //无论本地是否已经连接，都pop出来命令
        if(!local_q.queue_.empty())
        {

            data = local_q.pop();
            //先解析一下，再发给本地梯控
            if(parseCloud(data))
            {
                ret = mqtt_send(mosq_l, LCMD, data.c_str());
                if(ret != 0)
                {
                    //如果本地没有连接，这里也会报错
                    log(4, "mqtt_send local error=%i\n", ret);
                }
            }
            else
            {
                log(4, "云端数据出错");
            }
        }
        else
        {
            std::this_thread::sleep_for(chrono::milliseconds(10)); 
        }            

    }
    return 1;
}

int REGISTERED = 0;
/*0：未呼梯 1：已经呼梯 2：已经自动开门，当已经开门，但是检测到楼层已经过了，就取消开门，要不然电梯在运行的时候开门键一直按着
*/
//发送电梯状态线程
int localStateThread()
{
	//数据pop出来
	srand(time(0));
    cout<<"send state msg thread\n";
    log(6,"send state msg thread");
    string data, topic;
    int ret = -1;
    //数据pop出来，转换成云端数据
    while(1) 
    {                       
        //if(connected_c == 1 && !cloud_state_q.queue_.empty())
        //发送给云端这里要判断是否有连接，要不然mqtt_send会一直报错
        if(connected_c == 1)
        {
            //data = cloud_state_q.pop();
            data = cloud_state;
            topic = "upload_data/IotApp/"+MAC+"/sample/"+randomstring(26);
            ret = mqtt_send(mosq_c, topic,data.c_str());
            //cout << topic << ": " << data << endl;
            if(ret != 0)
            {
                log(4, "mqtt_send cloud state error=%i\n", ret);
				connected_c = 0;
            }             
        }
        else
        {
            std::this_thread::sleep_for(chrono::milliseconds(10)); 
            //sleep(1);
        }    
        //这里去检测是否到了目的楼层，然后自动开门10s
        if(REGISTERED == 1)
        {
            autoOpen(cloud_state); 
        }
        //自动开门之后，不断去检测现在电梯状态
        //else if(2 == REGISTERED)
        //{
        //    cancelAuto(cloud_state);
        //}
        //定时300ms发送       
        std::this_thread::sleep_for(chrono::milliseconds(300)); 
        


    }
    return 0;
}

//发送电梯回复指令线程
int localRspThread()
{
	//数据pop出来
    cout<<"send rsp msg thread\n";
    log(6,"send rsp msg thread");
    std::string data, topic;
    int ret = -1;
    //数据pop出来，转换成云端数据
    while(1) 
    {                       
        if(connected_c == 1 && !cloud_rsp_q.queue_.empty())
        {
            //cmd_resp/:ProductName/:DeviceName/:CommandName/:RequestID/:MessageID
            topic = CRSP+randomstring(26)+"/"+randomstring(10);
            //cout << topic << endl;
            data = cloud_rsp_q.pop();
            ret = mqtt_send(mosq_c, topic,data.c_str());
            log(6, "%s : %s", topic.c_str(),data.c_str());
            if(ret != 0)
            {
                log(4, "mqtt_send cloud rsp error=%i\n", ret);
            }             
        }
        else
        {
            std::this_thread::sleep_for(chrono::milliseconds(10)); 
            //sleep(1);
        }            

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

string floor = "unknown";
string door = "unknown";
string regFloor = "unknown";


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

void  registerFloor(string floor)
{
    cout << "register " << floor << endl;
    regFloor = floor;
    REGISTERED = 1;
}


//增加实时读取config
int readConfigThread(void)
{
    struct stat file_stat;
    int err;
    time_t   mtime_now, mtime_pre;
    cout << "readConfigThread beigin" << endl;
    //这里使用文件修改时间参数
    while(1)
    {
        err = stat("/home/tikong/production/config.ini", &file_stat);
        if(err != 0)
        {
            perror("stat");
            log(3,"stat error");   
            return 1;
        }
        mtime_now = file_stat.st_mtime;
        if(mtime_now != mtime_pre)
        {
            printf("file changed, reload\n");
            log(4, "file changed, reload\n");
            mtime_pre = mtime_now;
            readAuto();
        }
        std::this_thread::sleep_for(chrono::seconds(3));
    }
  return 0;
}


int eleMonitorThread(void)
{
    //2. 先建立与ali私有监控云端线程，因为要初始化ele map数组
    thread (eleAliCloudThread).detach();
    //初始化云端mqtt，需要把错误状态publish到云端
    mqtt_setup_alicloud();
    //初始化本地mqtt，订阅楼层信息
    //无须再订阅本地消息，另外一个线程已经订阅了并把消息压入队列了
    //mqtt_setup_local();
    //建立本地解析监控电梯数据的线程
    thread (monitorEleState).detach();


    //3. 不断检测是不是有错误要上报
    while(1)
    {
        //sleep xs
        std::this_thread::sleep_for(chrono::seconds(CHECKTIME)); 
        checkEle();     
    }
    
    return 0;
    
}