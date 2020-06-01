#include "workerthread.h"
#include "mqtt.h"
#include "misc.h"

using namespace std;


//把云端的命令下发下去
int cloudThread()
{
    //初始化云端mqtt，需要把状态以及回复publish到云端，并订阅cmd
    mqtt_setup_cloud();

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
    //初始化本地mqtt，订阅楼层信息以及梯控的回复转发到云端
    //另外把消息压入队列来监控电梯
    mqtt_setup_local();

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
