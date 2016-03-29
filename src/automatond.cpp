#include "automatond.h"

using namespace std;

int main(int argc, char *argv[])
{
 
  //variables to store cmdline params
  string this_node_id_arg;
  int daemonFlag = 0;
  int oledFlag = 0;
  int channel_arg = -1;
  int dataRate_arg = -1;
  int paLevel_arg = -1;
  
  int option_char = 0;
  while ((option_char = getopt(argc, argv,"i:c:p:r:do")) != -1) {
    switch (option_char)
      {  
         case 'i': this_node_id_arg = optarg;
                   break;
         case 'c': channel_arg =  atoi (optarg);
                   break;
         case 'p': paLevel_arg = atoi (optarg);
                   break;
         case 'r': dataRate_arg = atoi (optarg);
                   break;
        case 'd': daemonFlag++;
                   break;
      #ifdef OLED_DEBUG
        case 'o': oledFlag++;
                   break;
      #endif
         default: show_usage(argv[0]);
                  return 0;
      }
  }

 
  uint16_t this_node_id_oct = 00;
  // set non-default nodeId
  if(this_node_id_arg.size() > 0){
    std::istringstream(this_node_id_arg) >> std::oct >> this_node_id_oct; // make sure node id is octal, sent as string 
  }
  
  uint8_t channel = 115;
  // set non-default channel
  if(channel_arg > 0 && channel_arg <= 125){
    channel = (uint8_t)channel_arg;
  } else if(channel_arg > 125 ) {
    cout << "Invalid Channel - enter channel from 1-125" << endl;
    return 0;
  } 

  rf24_pa_dbm_e paLevel = (rf24_pa_dbm_e)3;
  // set non-default PaLevel
  if(paLevel_arg > 0){
    paLevel = (rf24_pa_dbm_e)paLevel_arg;
  }

   rf24_datarate_e dataRate = (rf24_datarate_e)0;
  // set non-default dataRate
   if(dataRate_arg > 0){
    dataRate = (rf24_datarate_e)dataRate_arg;
  } 

  setlogmask(LOG_UPTO(LOG_NOTICE));
  openlog(DAEMON_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_USER);

  if(daemonFlag){
    pid_t pid, sid;
     //Fork the Parent Process
    pid = fork();

     if (pid < 0) { exit(EXIT_FAILURE); }

      //We got a good pid, Close the Parent Process
     if (pid > 0) { exit(EXIT_SUCCESS); }

    //Change File Mask
      umask(0);

      //Create a new Signature Id for our child
    sid = setsid();
    if (sid < 0) { exit(EXIT_FAILURE); }

      //Change Directory
      //If we cant find the directory we exit with failure.
     if ((chdir("/")) < 0) { exit(EXIT_FAILURE); }

      //Close Standard File Descriptors
      close(STDIN_FILENO);
      close(STDOUT_FILENO);
      close(STDERR_FILENO);

    //logMsg("fork", "process_forked" , to_string(pid));
  } 

   Rf24Relay(this_node_id_oct, channel, dataRate, paLevel);
}


void Rf24Relay(uint16_t this_node_id, uint8_t channel, rf24_datarate_e dataRate, rf24_pa_dbm_e paLevel)
{
  long lastTime = time(0);

  signal(SIGTERM, signalHandler);
  signal(SIGINT, signalHandler);
  string ipc_addr = "ipc:///tmp/rf24d.ipc";

  logMsg("dataRate", to_string(dataRate), "config");
  logMsg("paLevel", to_string(paLevel), "config");
  logMsg("this_node_id", to_string(this_node_id), "config");
  logMsg("channel", to_string(channel), "config");

  logMsg("OK", "starting RF24d", "start_up");
  
  // start radio
  RF24 radio(22, 0);
  RF24Network network(radio);

  // init display
  ArduiPi_OLED display;
  display.init(OLED_I2C_RESET, 3);
  display.begin();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.print("Starting automatond!\n");
  display.display();

  // create and connect nanomsg pair socket
  nnxx::socket nn_sock { nnxx::SP, nnxx::PAIR };
  nn_sock.connect(ipc_addr.c_str());

  if(radio.begin()){
    radio.setDataRate(dataRate);
    radio.setPALevel(paLevel);
    radio.setChannel(channel);
    delay(250);
    if(network.is_valid_address(this_node_id)){
      logMsg("network_begin", "with_node_id", to_string(this_node_id));
      network.begin(this_node_id);
      delay(100);
      //start looping
      sendHeartbeat(lastTime, network); // send heatbeat right away...

      while (1){
        network.update(); //keep network up
        // handing incoming messages from nn socket to forward to RFnetwork
        handleIncomingNNMsg(nn_sock, network);
  
        // handing incoming messages from RF24 network
        if( network.available() ){   // we have a message from the sensor network
          RF24NetworkHeader peekedHeader;
          network.peek(peekedHeader);               // peak at the header so we know if it is a 0
          handleIncomingRF24Msg(nn_sock, network, display);  // also forward the 0 but going to register this node and send a heatbeat for it
          if(peekedHeader.type == 0){
            sendHeartbeat(network);
          }
        } 
         // send heart beat on regular interval
        if(time(0) > (lastTime + HEARTBEAT_INTERVAL)){  sendHeartbeat(lastTime, network); }
      
      } // restart loop
    } else {
       logMsg("network_begin", "invalid node address");
    }
  } else {
     logMsg("radio_begin", "rf24 radio begin, false");
  }
} 


static void show_usage(string name)
{
    cerr << "Usage: " << name << " <option(s)>\n"
              << "Options:\n"
              << "\t-h,--help - Show this help message\n"
              << "\t-d,--deamon - daemonize, logs to syslog\n"
               #ifdef OLED_DEBUG
                << "\t-o,--oled - display node stats on 128x64 i2c display\n"
               #endif
              << "\t-i,--id NODEID - Set Node ID - Parsed as octal: 00\n"
              << "\t-c,--channel CHANNEL# - Set RF24 Channel - 1-255: 115\n"
              << "\t-r,--rate RATE - Set Data Rate [0-2] - 1_MBPS|2_MBPS|250KBPS: 0\n"
              << "\t-p,--level POWER - Set PA Level [0-4] - RF24_PA_MIN|RF24_PA_LOW|RF24_PA_HIGH|RF24_PA_MAX|RF24_PA_ERROR: 3\n"
              << endl;
}



using namespace rapidjson;
using namespace std;

#ifdef OLED_DEBUG
  void writeToOLED(ArduiPi_OLED &dis, string stringToWrite, int x, int y)
  {
      dis.setCursor(x,y);
      dis.print(stringToWrite.c_str());
      dis.display();
  }
#endif



void sendHeartbeat(long &last_time, RF24Network &net){
  RF24NetworkHeader header(00, 5); // i dono send to self? what address for multicast
  stringstream beat;
  beat << time(0);
  string beat_str(beat.str());
  string signed_heartbeat = generateSignedPayload(beat_str);
  char payload[signed_heartbeat.length()]; // max payload for rf24 network fragment, could be sent over multiple packets
  strncpy(payload, signed_heartbeat.c_str(), signed_heartbeat.length()); // copy data into payload char, will probably segfault if over 144 char...
  //heartbeat_pl heart = { time(0) }; 
  payload[signed_heartbeat.length()] = '\0';
  if(net.multicast(header, &payload, sizeof(payload), 1)){
      cout << "sent heartbeat: "  << payload << endl;
      last_time = time(0);//last time is re initialized
   } else {
     cout << "hmm multicast failed try again next loop..." << endl;
   }
}

void sendHeartbeat(RF24Network &net){
  RF24NetworkHeader header(00, 5); // i dono send to self? what address for multicast
  stringstream beat;
  beat << time(0);
  string beat_str(beat.str());
  string signed_heartbeat = generateSignedPayload(beat_str);
  char payload[signed_heartbeat.length()]; // max payload for rf24 network fragment, could be sent over multiple packets
  strncpy(payload, signed_heartbeat.c_str(), signed_heartbeat.length()); // copy data into payload char, will probably segfault if over 144 char...
  //heartbeat_pl heart = { time(0) }; 
  payload[signed_heartbeat.length()] = '\0';
  if(net.multicast(header, &payload, sizeof(payload), 1)){
      cout << "sent heartbeat after node registered: "  << payload << endl;
   } else {
     cout << "hmm multicast failed try again next loop..." << endl;
   }
}

string generateSignedPayload(string msg_payload){
    char pl_to_hash[msg_payload.length()];
    strncpy(pl_to_hash, msg_payload.c_str(), msg_payload.size());
    uint8_t hashout[DIGEST_SIZE];
    blake2s(hashout, pl_to_hash, SECRET_KEY, DIGEST_SIZE, sizeof(pl_to_hash), sizeof(SECRET_KEY)); //generate hash of payload
    stringstream hashHex;
    for(int i=0; i< DIGEST_SIZE; i++){
        hashHex << std::hex << +hashout[i];   // convert uint8_t bytes to hex chars
    }
    string HMAC(hashHex.str()); // create string object from hexchar string
    cout << HMAC << endl; 
    string encodedHMAC;
    bn::encode_b64(HMAC.begin(), HMAC.end(), back_inserter(encodedHMAC));
    cout << msg_payload << endl;
    string encodedPL;
    bn::encode_b64(msg_payload.begin(), msg_payload.end(), back_inserter(encodedPL));
    cout << encodedPL << endl;
    string fullPL(encodedPL + "." + encodedHMAC);
    cout << fullPL << endl;
    cout << fullPL.length() << endl;
    return fullPL;
}

void logMsg(string info, string msg, string type){
  Document log_msg;
  log_msg.SetObject();
  Document::AllocatorType& allocator = log_msg.GetAllocator();
  log_msg.AddMember("info", Value(info.c_str(), info.size(), allocator).Move() , allocator);
  log_msg.AddMember("type", Value(type.c_str(), type.size(), allocator).Move(), allocator);
  log_msg.AddMember("msg", Value(msg.c_str(), msg.size(), allocator).Move(), allocator);

  StringBuffer buffer;
  Writer<StringBuffer> writer(buffer);
  log_msg.Accept(writer);

  syslog (LOG_NOTICE, buffer.GetString());
}

void sendBackErr(nnxx::socket &to, string error_for_ex)
{
  Document nn_out_err;
  nn_out_err.SetObject();
  Document::AllocatorType& allocator = nn_out_err.GetAllocator();
  nn_out_err.AddMember("error",Value(error_for_ex.c_str(), error_for_ex.size(), allocator).Move(), allocator);
  StringBuffer out_s_buf;
  Writer<StringBuffer> writer(out_s_buf);
  nn_out_err.Accept(writer);
  to.send(out_s_buf.GetString(), nnxx::DONTWAIT);
}

void handleIncomingRF24Msg(nnxx::socket &nn_socket, RF24Network &net, ArduiPi_OLED &dis) //pass socket and network through as reference
{
  RF24NetworkHeader header; // create header
  Document in_rf_msg;       // create document for incoming message
  in_rf_msg.SetObject();  
  Document::AllocatorType& allocator = in_rf_msg.GetAllocator();  // grab allocator
  char payloadJ[120]; //max payload for rf24 network fragment, could be sent over multiple packets
  size_t readP = net.read(header,&payloadJ, sizeof(payloadJ));
  payloadJ[readP] ='\0';
  //cout << payloadJ << endl;
  string payloadStr(payloadJ); // create string object to strip nonsense off buffer
  if(payloadStr.size() != readP){ payloadStr.resize(readP); } // resize payloadStr 
  //cout << payloadStr << endl; 
  //in_rf_msg.AddMember("msg_id", Value().SetInt(header.id) , allocator); 

  std::size_t delim = payloadStr.find_first_of(".");
  if(delim != string::npos){
    string encodedMsg = payloadStr.substr(0, delim);
    string encodedHash = payloadStr.substr(delim+1, payloadStr.length());

    // find padding
    int msgPadCount = count(encodedMsg.begin(), encodedMsg.end(), '='); 
    int hashPadCount = count(encodedHash.begin(), encodedHash.end(), '='); 
    
    // decode b64 encoded string of msg, store it in a new string
    string decodedMSG;
    bn::decode_b64(encodedMsg.begin(), encodedMsg.end(), back_inserter(decodedMSG));
    // resize string to remove any bad bytes left over from padding
    decodedMSG.resize(decodedMSG.length()-msgPadCount);
    
    // decode b64 encoded string of hash, store it in a new string
    string decodedHASH;
    bn::decode_b64(encodedHash.begin(), encodedHash.end(), back_inserter(decodedHASH));
    // resize string to remove any bad bytes left over from padding
    decodedHASH.resize(decodedHASH.length()-hashPadCount);
    // copy decoded msg into char* to hash ourselves.
    char pl_to_test[decodedMSG.length()];
    strncpy(pl_to_test, decodedMSG.c_str(), decodedMSG.length()+1); // grab null byte.
    
     
    uint8_t hashout[DIGEST_SIZE];
    blake2s(hashout, pl_to_test, SECRET_KEY, DIGEST_SIZE, strlen(pl_to_test), sizeof(SECRET_KEY)); //generate hash of payload
    stringstream hashHex;
    for(int i=0; i< DIGEST_SIZE; i++){
        hashHex << std::hex << +hashout[i];   // convert uint8_t bytes to hex chars
    }
    // create string object from hashHex stringstream
    string msgHash(hashHex.str()); 
    // test if delivered hash and our version of hash match
    if(decodedHASH == msgHash){
      cout << "Valid message!" << endl;
      cout << decodedMSG << endl;

      StringStream s(decodedMSG.c_str());
      Document d;
      d.ParseStream(s);
        unsigned long beatFromMsg = 0;
        unsigned long myBeat = time(0);

      if(d.IsObject()){
        cout << "JSON object recieved!" << endl;
          beatFromMsg = d["hb"].GetInt();

      } else if(d.IsArray()){
        beatFromMsg = d[3].GetInt();
      } else {
         cout << "not a json array or object:(" << endl;
      }
        unsigned int diff = myBeat - beatFromMsg;
        //1 second margin of error on time, seems to be working pretty well
        if( diff <= 1){
          std::ostringstream beatMsg;
          beatMsg << " My Beat: " << myBeat << "\nMsg Beat: " << beatFromMsg << "\nDiff: " << diff << "\nFrom : " << std::oct << header.from_node;
          cout << beatMsg << endl;
          #ifdef OLED_DEBUG
            string forOled = beatMsg.str();
            dis.clearDisplay();
            writeToOLED(dis, forOled, 0, 0);
            writeToOLED(dis, decodedMSG.c_str(), 0, 48);
          #endif

          // send it over nn socket
          in_rf_msg.AddMember("from_node", Value().SetInt(header.from_node) , allocator); // add integer to incoming msg json: from_node
          in_rf_msg.AddMember("type", Value().SetInt(header.type), allocator);            // add integer to incoming msg json: type 
          in_rf_msg.AddMember("msg", Value(decodedMSG.c_str(), decodedMSG.size(), allocator).Move(), allocator);
          //write json to string
          StringBuffer buffer;
          Writer<StringBuffer> writer(buffer);
          in_rf_msg.Accept(writer);
          // send validated msg over nn socket
          int sentBytes = nn_socket.send(buffer.GetString(), nnxx::DONTWAIT);
          if(sentBytes > 0){ //log
              logMsg("sent", buffer.GetString(), "to_nn");                  
          }
        }



    } else {
      cout << "Invalid message!! :(" << endl;
      cout << decodedMSG << endl;
    }
  }
}

void handleIncomingNNMsg(nnxx::socket &nn_socket, RF24Network &net) //pass socket and network through as reference
{
  try {
    stringstream errMsg;
    nnxx::message_istream msg { nn_socket.recv(nnxx::DONTWAIT) }; //try and grab message
    std::string msg_str(std::istream_iterator<char>(msg), {});  // turn whatever we got into a string
    if(msg_str.size() > 0 ){  // if we acually got something, size > 0
      Document nn_in_msg; // create doc to store message if its a json
      try {
        if (nn_in_msg.Parse(msg_str.c_str()).HasParseError() == false){ // we have a json    
          uint16_t to_node_id_oct;      
          std::istringstream(nn_in_msg["to_node"].GetString()) >> std::oct >> to_node_id_oct; // make sure node id is octal, sent as string 
          if(net.is_valid_address(to_node_id_oct)){   // only send if we have a valid address
            string msgPayload(nn_in_msg["msg"].GetString());      //grab payload put it in a string
            string signedPayload = generateSignedPayload(msgPayload);
            unsigned char msg_type = nn_in_msg["type"].GetInt();  // grap message type as unsigned char 
            char payload[signedPayload.length()]; // max payload for rf24 network fragment, could be sent over multiple packets
            strncpy(payload, signedPayload.c_str(), signedPayload.length()); // copy data into payload char, will probably segfault if over 144 char...
            RF24NetworkHeader header( to_node_id_oct , msg_type ); //create RF24Network header
            size_t bytesSent = net.write(header, &payload, sizeof(payload));
            if (bytesSent > 0) {
              logMsg(to_string(msg_type), to_string(to_node_id_oct), "to_rf24");
            } else { 
              errMsg << "FAILED_TO_NODE: " << to_node_id_oct;
              throw errMsg.str();
            }      
          } else {
            errMsg << "INVALID_RF24_ADDR " << to_node_id_oct;
            throw errMsg.str();
          }  
        } else {
          errMsg << "RAPID_JSON_PARSE_ERROR: " << msg_str;
          throw errMsg.str();
        }      
      } catch(string &e){ // catch RF24d errors
        sendBackErr(nn_socket, e);
        logMsg("RF24d_ERR", e);
      }   
    } 
  } catch(const std::system_error &e) { // catch nn errors
    std::cerr << e.what() << std::endl;
  }
}



void signalHandler( int signum )
{
  syslog (LOG_NOTICE, "Closing RF24d!");
  closelog ();
  exit(signum);  
}
