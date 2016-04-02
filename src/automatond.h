#ifndef AUTOMATOND_H
  #define AUTOMATOND_H
  
  #include "automatond_config.h"

  // stdlib etc
 #include <stdbool.h>
  #include <algorithm> 
  #include <functional> 
  #include <ctime>
  #include <csignal>
  #include <syslog.h>
  #include <errno.h>
  #include <stdio.h>
  #include <stdlib.h>
  #include <iostream>
  #include <sstream>  
  #include <cstring>
  #include <istream>
  #include <streambuf>
  #include <string>
  #include <iostream>
  #include <sys/types.h>
  #include <sys/stat.h>
  #include <fcntl.h>
  #include <unistd.h>
  #include <getopt.h> 
 #include <typeinfo>
  
// mqtt
#include <mosquitto.h>
#include <mosquittopp.h>
  
  //json
  #include "rapidjson/document.h"
  #include "rapidjson/writer.h"
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

  //void Rf24Relay(uint16_t, uint8_t , rf24_datarate_e, rf24_pa_dbm_e);
  static void show_usage(std::string);

  struct heartbeat_pl { long beat; };

  struct node {uint8_t node_id; char* node_name;};
  //void logMsg(std::string = "", std::string = "", std::string = "error"); 
  //void handleIncomingRF24Msg(nnxx::socket &, RF24Network &, ArduiPi_OLED &);
  //void handleIncomingNNMsg(nnxx::socket &, RF24Network &);
  //void sendBackErr(nnxx::socket &, std::string);
  //void signalHandler( int signum );
  //void sendHeartbeat(long &, RF24Network &);
  //void sendHeartbeat(RF24Network &);
  //std::string generateSignedPayload(std::string);

  class ArduiRFMQTT: public mosqpp::mosquittopp
  {
    public:
      ArduiRFMQTT(const char *id, const char *host, int port, RF24Network& network);
      ~ArduiRFMQTT();
      bool send_message(uint8_t from_node, const char * _message);
      long lastBeatSent();

      std::string genPayload();
      std::string genPayload(std::string);
      void sendHeartbeat();
      std::string constructPayload(std::string msg, bool beat = false);
      std::string genHash(std::string, bool encoded = true);
      std::string encode_b64(std::string);
      std::string decode_b64(std::string);
      void handleIncomingRF24Msg();
      

    private:
     std::vector<node> node_list;   
     
     long last_beat;
     const char     *     host;
     const char    *     id;
     int                port;
     int                keepalive;
     void on_connect(int rc);
     void on_disconnect(int rc);
     void on_publish(int mid);
     void on_subscribe(int mid, int qos_count, const int *granted_qos);
     void on_message(const struct mosquitto_message *message);
     RF24Network& _network;
  };



#endif 