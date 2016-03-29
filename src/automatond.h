#ifndef AUTOMATOND_H
  #define AUTOMATOND_H
  
  #include "automatond_config.h"
  //nanomsg
  #include <system_error>
  #include <nnxx/message.h>
  #include <nnxx/message_istream.h>
  #include <nnxx/pair.h>
  #include <nnxx/socket.h>
  
  //json
  #include "rapidjson/document.h"
  #include "rapidjson/writer.h"
  #include "rapidjson/stringbuffer.h"
  #include "rapidjson/error/en.h"
  
  //hashing and encoding
  #include <blake2.h>
  #include "basen.hpp" 
  
  //rf24
  #include <RF24/RF24.h>
  #include <RF24Network/RF24Network.h>
  
  // stdlib etc
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
  
  #ifdef OLED_DEBUG
    #include "ArduiPi_OLED_lib.h"
    #include "Adafruit_GFX.h"
    #include "ArduiPi_OLED.h"
    void writeToOLED(ArduiPi_OLED &, std::string ,int ,int);
  #endif

  #define DAEMON_NAME "automatond"

  void Rf24Relay(uint16_t, uint8_t , rf24_datarate_e, rf24_pa_dbm_e);
  static void show_usage(std::string);

  struct heartbeat_pl { long beat; };
  void logMsg(std::string = "", std::string = "", std::string = "error"); 
  void handleIncomingRF24Msg(nnxx::socket &, RF24Network &, ArduiPi_OLED &);
  void handleIncomingNNMsg(nnxx::socket &, RF24Network &);
  void sendBackErr(nnxx::socket &, std::string);
  void signalHandler( int signum );
  void sendHeartbeat(long &, RF24Network &);
  void sendHeartbeat(RF24Network &);
  std::string generateSignedPayload(std::string);

#endif 