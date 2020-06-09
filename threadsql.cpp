//#include "threadsql.h"
// g++ threadsql.cpp -lmysqlclient
//sudo apt-get install libmysqlclient-dev
#include "misc.h"
#include <jsoncpp/json/json.h>
#include "mqtt.h"
#include <mysql/mysql.h>
#include <iostream>
#include <stdio.h>
#include <unistd.h>


const char* SQLCMD = "/cti/ele/cmd";
//const char* LRSP = "/cti/ele/cmd-rsp";
const char* SQLSTATE = "/cti/ele/state";


/*
电梯编号 device_id char 5 （00116）
当前楼层 floor varchar  5 （11M/10）
当前门状态 door  varchar 5 （open close）
上下行  state  varchar  4（up down）
命令  cmd  varchar 10 open/close/call 
命令来源 sender  varchar 10 wifi/4g
呼梯楼层 callfloor  varchar 5 
timestamp 
*/
string id, floo, doo, state, cmd, sender, callfloor, timestamp;
int elechange = 0;
int cmdchange = 0;

int sqlParse(string pkt)
{
    Json::Reader reader;
    Json::Value value;
    int duration = 0;
    bool err = false;

    if(!reader.parse(pkt, value))
    {
        return 1;
    }   

    if(!value["floorNum_r"].isNull())
    {
        floo = value["floorNum_r"].asString();
    }

    if(!value["state"].isNull())
    {
        state = value["state"].asString();
    }

    if(!value["door"].isNull())
    {
        doo = value["door"].asString();
       
    }
    elechange = 1;

    return 0;
}

int parseCmd(string pkt)
{
    Json::Reader reader;
    Json::Value value;

    if(!reader.parse(pkt, value))
    {
        return 1;
    }   

    if(!value["cmd"].isNull())
    {
        cmd = value["cmd"].asString();
    }

    if(!value["sender"].isNull())
    {
        sender = value["sender"].asString();
    }

    if(!value["floorNum_r"].isNull())
    {
        callfloor = value["floorNum_r"].asString();
       
    }

    cmdchange = 1;
    return 0;

}

/***本地mqtt部分***/
struct mosquitto *mosq_sql = NULL;
int connected_sql = 0;

void local_callback_sql(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message)
{
    if(message->payloadlen){
        //printf("topic: %s, payload: %s", message->topic, (char *)message->payload);
        //放入队列   
        //log(6,"recv %s: ", (char *)message->payload);
        std::string data = (char*)(message->payload);
        //std::string data = static_cast<char*>(message->payload);
        //放入云端队列，由云端线程处理 
        if(strcmp(message->topic, SQLSTATE) == 0)
        {
            //压入队列，监控梯控
            sqlParse(data);
            //给云端永远发送最新的状态
            //cloud_state = data;
        }
        else if(strcmp(message->topic, SQLCMD) == 0)
        {
            parseCmd(data);
        }

    }else{
        log(4, "message %s (null)\n", message->topic);
    }
    //fflush(stdout);
}

//这里去订阅楼层信息
/*std::map<string, string> topic = {
    { "state", "/cti/ele/state" },
    { "response", "/cti/ele/cmd-rsp" },
    { "cmd", "/cti/ele/cmd" }, 
};*/

void local_connect_callback_sql(struct mosquitto *mosq, void *userdata, int result)
{
    int i;
    if(!result){
        printf("local connected success\n");
        log(4, "local connected success");
        connected_sql = 1;
        //mosquitto_subscribe(mosq, NULL, "$SYS/#", 2);
        //这里订阅本地的cmd命令，2的意思Qos，0 表示This is the fastest method and requires only 1 message. 
        //It is also the most unreliable transfer mode.
        //订阅状态以及命令的回复
        mosquitto_subscribe(mosq, NULL, SQLSTATE, 0);
        mosquitto_subscribe(mosq, NULL, SQLCMD, 0);

    }else{
        fprintf(stderr, "local Connect failed\n");
    }
}

void mqtt_setup_sql()
{
    cout<<"set up local mqtt\n";
    log(6,"set up local mqtt");
    string host = "localhost";
    int port = 1883;
    int keepalive = 60;
    bool clean_session = true;
  
    mosquitto_lib_init();
    mosq_sql = mosquitto_new(NULL, clean_session, NULL);
    if(!mosq_sql){
        fprintf(stderr, "Error: Out of memory in local.\n");
        exit(1);
    }
    mosquitto_log_callback_set(mosq_sql, mosq_log_callback);
    //连接之后去订阅
    mosquitto_connect_callback_set(mosq_sql, local_connect_callback_sql);
    //订阅之后处理函数
    mosquitto_subscribe_callback_set(mosq_sql, my_subscribe_callback);
    //消息处理函数
    mosquitto_message_callback_set(mosq_sql, local_callback_sql);

    //keepalive 是client 定期发消息给 server
    if(mosquitto_connect(mosq_sql, host.c_str(), port, keepalive)){
        fprintf(stderr, "Unable to connect local.\n");
        log(3,"Unable to connect local.");
        //exit(2);
    }
    //都已经添加重连时间设置
    mosquitto_reconnect_delay_set(mosq_sql, 5, 30, true);

    //已经连接上的全局变量
    //connected_sql = 1;
    int loop = mosquitto_loop_start(mosq_sql);
    if(loop != MOSQ_ERR_SUCCESS){
        fprintf(stderr, "Unable to start loop in local: %i\n", loop);
        log(3, "Unable to start loop in local: %i\n", loop);
        exit(3);
    }
}
/***本地mqtt部分结束**/

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


string sqlID;

int readID()
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
        //cout << line << endl;
        auto delimeterPos = line.find(":");
        auto key = line.substr(0, delimeterPos);
        auto value = line.substr(delimeterPos+1);
        //cout << line << endl;
        if (key.compare("ID") == 0)
        {
            sqlID = value;
            cout << "ID: " << sqlID << endl;
        }  
        
    }//结束读取文件
    if(sqlID.empty())
    {
        //log(3, "没有找到AUTO_OPEN");
        printf("没有找到AUTO_OPEN\n");
        //直接退出
        exit(2);
    }
    return 0;
}



int sqlThread()
{

    MYSQL *conn;
    MYSQL_RES *res;
    MYSQL_ROW row;
    readID();
    mqtt_setup_sql();


    char server[] = "ksy.redsun.dev";
    char user[] = "lift";
    char password[] = "candelaiot888"; /*password is not set in this example*/
    char database[] = "liftctl";


    conn = mysql_init(NULL);
    /* Connect to database */
    if (!mysql_real_connect(conn, server, user, password, database, 0, NULL, 0))
    {
        printf("Failed to connect MySQL Server %s. Error: %s", server, mysql_error(conn));
        // return 0;
    }


    char mysql_command[256] = {};
/*
电梯编号 device_id char 5 （00116）
当前楼层 floor varchar  5 （11M/10）
当前门状态 door  varchar 5 （open close）
上下行  state  varchar  4（up down）
命令  cmd  varchar 10 open/close/call 
命令来源 sender  varchar 10 wifi/4g
呼梯楼层 callfloor  varchar 5 
timestamp 
*/
    
    while (1)
    {        
        //sprintf(mysql_command, "INSERT INTO  lift_state ( device_id ) VALUES ('00116')");
        if(elechange == 0 && cmdchange == 0)
        {
            continue;
        }
        /*if(elechange == 1 && cmdchange == 1)
        {
            elechange =0;
            cmdchange = 0;
            sprintf(mysql_command, "INSERT INTO  lift_state ( floor, door, state\
                                                    ) VALUES ('00116')");
            cmd = "";
            sender = "";
            callfloor = "";
        }*/
        if(elechange == 1)
        {
            elechange = 0;
            sprintf(mysql_command, "INSERT INTO  lift_state ( device_id, floor,door,state) VALUES ('%s', '%s', '%s', '%s')", \
                                    sqlID.c_str(),floo.c_str(), doo.c_str(), state.c_str() );
            printf("%s\n", mysql_command);
            if (mysql_query(conn, mysql_command) != 0)
            {
                mysql_close(conn);
                conn = NULL;
                printf("Insert Failure\n");            
                conn = mysql_init(NULL);

                /* Connect to database */
                if (!mysql_real_connect(conn, server, user, password, database, 0, NULL, 0))
                {
                    printf("Failed to connect MySQL Server %s. Error: %s", server, mysql_error(conn));
                    // continue;
                }
            }
            else
            {
                printf("insert ok\n");
            }

        }
        if(cmdchange == 1)
        {
            cmdchange = 0;
            sprintf(mysql_command, "INSERT INTO  lift_state (device_id,cmd, sender, callfloor) VALUES ('%s', %s', '%s', '%s')", \
                                        sqlID.c_str(), cmd.c_str(), sender.c_str(), callfloor.c_str());
            cmd = "";
            sender = "";
            callfloor = "";
            printf("%s\n", mysql_command);
            if (mysql_query(conn, mysql_command) != 0)
            {
                mysql_close(conn);
                conn = NULL;
                printf("Insert Failure\n");            
                conn = mysql_init(NULL);

                /* Connect to database */
                if (!mysql_real_connect(conn, server, user, password, database, 0, NULL, 0))
                {
                    printf("Failed to connect MySQL Server %s. Error: %s", server, mysql_error(conn));
                    // continue;
                }
            }
            else
            {
                printf("insert ok\n");
            }
        }

        
        sleep(3);
    }

    return 0;
}