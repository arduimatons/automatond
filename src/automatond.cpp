#include "automatond.h"

using namespace std;
using namespace rapidjson;

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

  RF24 radio(22, 0);
  RF24Network network(radio);

  ArduiRFMQTT relay("rf_relay", "localhost", 1883, network);

 if(radio.begin()){

  radio.setChannel(115);
  network.begin(00);
  delay(100);

  relay.sendHeartbeat();
 }

    while(1)
    {
    network.update();  
    relay.loop(0,1);

   if( network.available() ){   // we have a message from the sensor network
        RF24NetworkHeader header;
        network.peek(header);
        char payloadJ[120]; //max payload for rf24 network fragment, could be sent over multiple packets
        size_t readP = network.read(header,&payloadJ, sizeof(payloadJ));
        payloadJ[readP] ='\0';
        cout << "got payload from rf!: " << payloadJ << endl;
        cout << "from node: " << oct << header.from_node << endl;
        cout << "of type: " << static_cast<int>(header.type) << endl;
        uint8_t rawSize = 0;
        unsigned long hb;
       // handleIncomingRF24Msg(nn_sock, network);  // also forward the 0 but going to register this node and send a heatbeat for it
        if(header.type == 0){
         // sendHeartbeat(network);
        }
    }

  if(time(0) > (relay.lastBeatSent() + HEARTBEAT_INTERVAL)){ relay.sendHeartbeat(); }
  


  }
 return 0;
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


unsigned long ArduiRFMQTT::lastBeatSent()
{
  return this->last_beat;
}

ArduiRFMQTT::ArduiRFMQTT(const char * _id, const char * _host, int _port, RF24Network& network) : mosquittopp(_id), _network(network)
 {
   mosqpp::lib_init();        // Mandatory initialization for mosquitto library
   this->keepalive = 60;    // Basic configuration setup for myMosq class
   this->id = _id;
   this->port = _port;
   this->host = _host;
   connect(host, port, keepalive);
 };


ArduiRFMQTT::~ArduiRFMQTT() {
 loop_stop();            // Kill the thread
 mosqpp::lib_cleanup();    // Mosquitto library cleanup
 }


bool ArduiRFMQTT::send_message(uint8_t from_node, const  char * _message)
 {
  stringstream topic;
  topic << "/rf24/out/" << oct << from_node;
  string topic_str(topic.str());
  
  cout << "SendingMessage: " <<  _message <<  endl;
  cout << "OnTopic: " << topic_str << endl;
 // Send message - depending on QoS, mosquitto lib managed re-submission this the thread
 //
 // * NULL : Message Id (int *) this allow to latter get status of each message
 // * topic : topic to be used
 // * lenght of the message
 // * message
 // * qos (0,1,2)
 // * retain (boolean) - indicates if message is retained on broker or not
 // Should return MOSQ_ERR_SUCCESS
 int ret = publish(NULL,topic_str.c_str(),strlen(_message),_message,1,false);
 return ( ret == MOSQ_ERR_SUCCESS );
 }


void ArduiRFMQTT::on_disconnect(int rc) {
 std::cout << ">> myMosq - disconnection(" << rc << ")" << std::endl;
 }

 void ArduiRFMQTT::on_connect(int rc)
 {
 if ( rc == 0 ) {
   std::cout << "ArduiRFMQTT - connected with server" << std::endl;
   subscribe(NULL, "/rf24/to/+");
 } else {
 std::cout << "ArduiRFMQTT - Impossible to connect with server(" << rc << ")" << std::endl;
 }
 }

 void ArduiRFMQTT::on_publish(int mid)
 {
 std::cout << "ArduiRFMQTT - Message (" << mid << ") succeed to be published " << std::endl;
 }

 void ArduiRFMQTT::on_subscribe(int mid, int qos_count, const int *granted_qos)
{
  std::cout << "Subscription succeeded." << std::endl;
}


void ArduiRFMQTT::on_message(const struct mosquitto_message *message)
{
  char payload_chars[message->payloadlen+1];
  memcpy(payload_chars,message->payload,message->payloadlen);

  string payloadToEncode(payload_chars);
  string encodedAndSignedPL = generateSignedPayload(payloadToEncode);


  std::cout << "Got a message!" << std::endl;
  std::cout << message->topic << std::endl;
  std::cout << message->payloadlen << std::endl;
  std::cout << payloadToEncode << std::endl;
  std::cout << encodedAndSignedPL << std::endl;
}

void handleIncomingRF24Msg(RF24Network &net, ArduiPi_OLED &dis) //pass socket and network through as reference
{
  RF24NetworkHeader header; // create header
  char payloadJ[120]; //max payload for rf24 network fragment, could be sent over multiple packets
  uint8_t rawSize = 0;
  unsigned long hb;
  size_t readP = net.read(header,&payloadJ, sizeof(payloadJ));
  std::memcpy(&rawSize, payloadJ, 1);     //first byte raw pl size; 
  std::memcpy(&hb, payloadJ+1, 10);  //next 10 bytes have heartbeat

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
           // string forOled = beatMsg.str();
           // dis.clearDisplay();
           // writeToOLED(dis, forOled, 0, 0);
          //  writeToOLED(dis, decodedMSG.c_str(), 0, 48);
          #endif

          Document in_rf_msg;       // create document for incoming message
          in_rf_msg.SetObject();  
          Document::AllocatorType& allocator = in_rf_msg.GetAllocator();  // grab allocator
          // send it over nn socket
          in_rf_msg.AddMember("from_node", Value().SetInt(header.from_node) , allocator); // add integer to incoming msg json: from_node
          in_rf_msg.AddMember("type", Value().SetInt(header.type), allocator);            // add integer to incoming msg json: type 
          in_rf_msg.AddMember("msg", Value(decodedMSG.c_str(), decodedMSG.size(), allocator).Move(), allocator);
          //write json to string
          StringBuffer buffer;
          Writer<StringBuffer> writer(buffer);
          in_rf_msg.Accept(writer);
          // send validated msg over nn socket
        }

    } else {
      cout << "Invalid message!! :(" << endl;
      cout << decodedMSG << endl;
    }
  }
}


string ArduiRFMQTT::generateSignedPayload(string msg_payload){
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

void ArduiRFMQTT::sendHeartbeat(){
  unsigned long beat = time(0);
  RF24NetworkHeader header(00, 5); // i dono send to self? what address for multicast
  stringstream beatss;
  beatss << beat;
  string beat_str(beatss.str());
  string signed_heartbeat = generateSignedPayload(beat_str);
  char payload[signed_heartbeat.length()]; // max payload for rf24 network fragment, could be sent over multiple packets
  strncpy(payload, signed_heartbeat.c_str(), signed_heartbeat.length()); // copy data into payload char, will probably segfault if over 144 char...
  //heartbeat_pl heart = { time(0) }; 
  payload[signed_heartbeat.length()] = '\0';
  if(_network.multicast(header, &payload, sizeof(payload), 1)){
      cout << "sent heartbeat: "  << payload << endl;
      this->last_beat = beat;
   } else {
     cout << "hmm multicast failed try again next loop..." << endl;
   }
}
