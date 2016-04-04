#ifndef AUTOMATOND_H
  #define AUTOMATOND_H
  
  #include "automatond_config.h"

  // stdlib etc
  #include <stdbool.h> 
  #include <algorithm> 
  #include <functional>  
  #include <ctime> // time
  #include <csignal> // signals
  #include <syslog.h> // syslog
  #include <errno.h> // error numbers
  #include <stdio.h> 
  #include <stdlib.h>
  #include <iostream>  
  #include <sstream>  // string streams
  #include <cstring>  
  #include <istream> // input stream
  #include <fstream> // file stream
  #include <streambuf> 
  #include <string> // strings
  #include <iostream>
  #include <sys/types.h>  
  #include <sys/stat.h> 
  #include <fcntl.h>  
  #include <unistd.h> 
  #include <getopt.h>  // cmdline option parsing
  #include <typeinfo> // not sure
  #include <vector>
  
// mqtt
#include <mosquitto.h>
#include <mosquittopp.h>
  
  //json
  #include "rapidjson/document.h"
  #include "rapidjson/writer.h"
  #include "rapidjson/filereadstream.h"
  #include "rapidjson/stringbuffer.h"
  #include "rapidjson/error/en.h"
  
  //hashing and encoding
  #include <blake2.h>
  #include <cryptopp/base64.h>

  //rf24
  #include <RF24/RF24.h>
  #include <RF24Network/RF24Network.h>
  

  #ifdef OLED_DEBUG
    #include "ArduiPi_OLED_lib.h"
    #include "Adafruit_GFX.h"
    #include "ArduiPi_OLED.h"
    //void writeToOLED(ArduiPi_OLED &, std::string ,int ,int);
  #endif

  #define DAEMON_NAME "automatond"

  static void show_usage(std::string);
  
  // struct to represent a node on the network.
 struct Node {
               uint16_t addr;
               // 10 chars + delim
               std::string name;
               std::string type;
               long last_msg_at;
               bool alive;
             };

struct find_by_addr
{
    uint16_t addr;
    find_by_addr(uint16_t addr) : addr(addr) {}
    bool operator () ( const Node& n ) const
    {
        return n.addr == addr;
    }
};

bool compareByType(const Node& a, const Node& b)
{
    return a.type < b.type;
}

enum NODE_TYPES { TEMP=1, MOTION, SWITCH, RGB};
// enums to for message types 0-5
enum NO_ACK_MSG_TYPES { NODE_STATUS = 0, NODE_MSG1, NODE_MSG2, NODE_MSG3, NODE_MSG4, BEAT };
// 65-67
enum ACK_MSG_TYPES { FUNCTION_1 = 65, FUNCTION_2, FUNCTION_3};

  // used to grab config file as rapidjson document.
  rapidjson::Document getConfig(const char*);

  class ArduiRFMQTT: public mosqpp::mosquittopp
  {
    public:
   //const char * _id
      ArduiRFMQTT(const char* _id, RF24Network& network, rapidjson::Document& config);
      ~ArduiRFMQTT();
      // sent message over mqtt
      bool send_to_mqtt(uint16_t from_node, const char * _message);
      //
      long lastBeatSent();
      //generate emptypayload, for hb
      std::string genPayload();
      // generate a payload with a message
      std::string genPayload(std::string);
      // multicasts a heartbeat
      void sendHeartbeat();
      // generates a hex, or encoded hash of string, returns hash
      std::string genHash(std::string, bool encoded = true);
      // helpers for encoding/decoding b64
      std::string encode_b64(std::string);
      std::string decode_b64(std::string);
      // function to validate incoming RF24Network message
      void handleIncomingRF24Msg();

        
    private:
     // keep track of nodes that are active.
     // also acts as ACL after loading in json data
     std::vector<Node> node_list;   
     
     std::string topic_outgoing;
     std::string topic_incoming;
     std::string topic_root;

     std::string secret_key_override;

     time_t last_beat;
     const char     *     host;
     const char    *     id;
     int                port;
     int                keepalive;
     // Mosquitto client callbacks.
     void on_connect(int rc);
     void on_disconnect(int rc);
     void on_publish(int mid);
     void on_subscribe(int mid, int qos_count, const int *granted_qos);
     // send message over rfnetwork
     void on_message(const struct mosquitto_message *message);
     RF24Network& _network;
  };



#endif 