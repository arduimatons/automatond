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

  //string hashed = relay.genHash(testHash, false);
  //string hashedEnc = relay.genHash(testHash, true);
  //cout << hashed << endl; 
  //cout << hashedEnc << endl; 

  if(radio.begin()){

    radio.setChannel(115);
    network.begin(00);
    delay(100);

    relay.sendHeartbeat();
    // start looping
    while(1)
    {
      network.update();  // keep RFNetwork up
      relay.loop(0,1);   // keep MQTT relay up 0 second timeout

      if( network.available() ){   // we have a message from the sensor network
        relay.handleIncomingRF24Msg();  // also forward the 0 but going to register this node and send a heatbeat for it
      }

      if(time(0) > (relay.lastBeatSent() + HEARTBEAT_INTERVAL)){ relay.sendHeartbeat(); }
    }
  }

  cout << "radio.begin failed!" << endl;
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


long ArduiRFMQTT::lastBeatSent()
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
 std::cout << "ArduiRFMQTT - disconnection(" << rc << ")" << std::endl;
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
  string encodedAndSignedPL = genPayload(payloadToEncode);


  std::cout << "Got a message!" << std::endl;
  std::cout << message->topic << std::endl;
  std::cout << message->payloadlen << std::endl;
  std::cout << payloadToEncode << std::endl;
  //std::cout << encodedAndSignedPL << std::endl;
}

void ArduiRFMQTT::handleIncomingRF24Msg() //pass socket and network through as reference
{
  RF24NetworkHeader header; // create header
  char payloadJ[120]; //max payload for rf24 network fragment, could be sent over multiple packets
  unsigned long hb;
  size_t readP = this->_network.read(header,&payloadJ, sizeof(payloadJ));
  payloadJ[readP] ='\0';
  cout << "got payload from rf!: " << payloadJ << endl;
  cout << "from node: " << oct << header.from_node << endl;
  cout << "of type: " << static_cast<int>(header.type) << endl;
  //cout << payloadJ << endl;
  string payloadStr(payloadJ); // create string object to strip nonsense off buffer
  if(payloadStr.size() != readP){ payloadStr.resize(readP); } // resize payloadStr 
  std::size_t HEADER_LEN = payloadStr.find_first_of(".");
  std::size_t MAC_POS = payloadStr.find_last_of(".");
  string encodedMAC = payloadStr.substr(MAC_POS+1, payloadStr.length());
  string signedPL = payloadStr.substr(0, MAC_POS);
  cout << "Signed encoded PL" << endl;
  cout << signedPL << endl;
  char signedPL_TO_HASH[signedPL.length()+1];
  signedPL.copy(signedPL_TO_HASH, signedPL.length(), 0);
  signedPL_TO_HASH[signedPL.length()] = '\0';
  string decodedMAC = this->decode_b64(encodedMAC);
  uint8_t hashout[DIGEST_SIZE];
  blake2s(hashout, signedPL_TO_HASH, SECRET_KEY, DIGEST_SIZE, strlen(signedPL_TO_HASH), sizeof(SECRET_KEY)); //generate hash of payload
  stringstream hashHex;
  for(int i=0; i< DIGEST_SIZE; i++){
     hashHex << std::hex << +hashout[i];   // convert uint8_t bytes to hex chars
  }
    // create string object from hashHex stringstream
  string msgHash(hashHex.str()); 
     // test if delivered hash and our version of hash match
  if(decodedMAC == msgHash){
    string encodedBEAT = payloadStr.substr(0, HEADER_LEN);
    string encodedMSG = payloadStr.substr(HEADER_LEN+1, (payloadStr.length()-(HEADER_LEN+encodedMAC.length()+2)));
    string decodedBEAT = this->decode_b64(encodedBEAT);
    string decodedMSG = this->decode_b64(encodedMSG);
    StringStream s(decodedMSG.c_str());
    Document d;
    d.ParseStream(s);
    long beatFromMsg = atol(decodedBEAT.c_str());
    long myBeat = time(0);
    printf("Beat from MSG: %ld\n", beatFromMsg);
    printf("My beat: %ld\n", myBeat);
     
    if(d.IsObject()){
        cout << "JSON object recieved!" << endl;
    } else if(d.IsArray()){
        cout << "JSON array recieved!" << endl;
    } else {
         cout << "not a json array or object:(" << endl;
    }
      
  }
}

string ArduiRFMQTT::genHash(string toHash, bool encoded)
{
    uint8_t hashout[DIGEST_SIZE];
    blake2s(hashout, toHash.c_str(), SECRET_KEY, DIGEST_SIZE, toHash.length(), sizeof(SECRET_KEY)); //generate hash of payload
    stringstream hashHex;
    for(int i=0; i< DIGEST_SIZE; i++){
      hashHex << std::hex << +hashout[i];   // convert uint8_t bytes to hex chars
    }
    string MAC(hashHex.str());
    if(encoded){ 
      string encMAC = this->encode_b64(MAC);
      return encMAC;
    } else {
      return MAC;
    } 
}

string ArduiRFMQTT::genPayload(std::string payload_msg)
{
  long beat = time(0);
  stringstream payloadBeatSS;
  // convert beat to string
  payloadBeatSS << beat;
  // encode beat
  string encodedBeatStr = this->encode_b64(payloadBeatSS.str());
  // encode payload_msg
  string encodedPLStr = this->encode_b64(payload_msg);
  // create new string to hash,
  string toHash = encodedBeatStr +"."+ encodedPLStr;
  // generate hash
  string encodedHMAC = genHash(toHash);
  // build full payload
  string fullPL(toHash + "." +  encodedHMAC);
  cout << "Generated payload: " << endl; 
  cout << fullPL << endl; 
  return fullPL;
}

string ArduiRFMQTT::genPayload()
{
  long beat = time(0);
  stringstream payloadBeatSS;
  // convert beat to string
  payloadBeatSS << beat;
  // enncode beat
  string encodedBeatStr = this->encode_b64(payloadBeatSS.str());
  string toHash = encodedBeatStr +"."+ encodedBeatStr;
  string encodedHMAC = genHash(toHash);
  string fullPL(toHash + "." +  encodedHMAC);
  cout << "Generated heartbeat: " << endl; 
  cout << fullPL << endl; 
  return fullPL;
}

void ArduiRFMQTT::sendHeartbeat(){
  // i dono send to self? what address for multicast
  RF24NetworkHeader header(00, 5); 
  // genPayload with no params will create a heartbeat payload.
  string fullPL = this->genPayload();
  char payload[fullPL.length()]; // max payload for rf24 network fragment, could be sent over multiple packets
  strncpy(payload, fullPL.c_str(), fullPL.length()); // copy data into payload char, will probably segfault if over 144 char...
  //heartbeat_pl heart = { time(0) }; 
  payload[fullPL.length()] = '\0';
  if(_network.multicast(header, &payload, fullPL.length(), 1)){
      long sent_beat = time(0);
      cout << "sent heartbeat: "  << sent_beat << endl;
      this->last_beat = sent_beat;
   } else {
     cout << "hmm multicast failed try again next loop..." << endl;
   }
}

string ArduiRFMQTT::encode_b64(string to_encode)
{
  string encoded; 
  CryptoPP::Base64Encoder encoder;

  encoder.Put( (byte*)to_encode.data(), to_encode.size() );
  encoder.MessageEnd();

  CryptoPP::word64 size = encoder.MaxRetrievable();
  if(size && size <= SIZE_MAX)
  {
     encoded.resize(size);   
     encoder.Get((byte*)encoded.data(), encoded.size());
  }
  // remove new line from base64, gonna be appending it to other strings...
  encoded.erase(std::remove(encoded.begin(), encoded.end(), '\n'), encoded.end());
  return encoded;
}


string ArduiRFMQTT::decode_b64(string to_decode)
{
  string decoded; 
  CryptoPP::Base64Decoder decoder;

  decoder.Put( (byte*)to_decode.data(), to_decode.size() );
  decoder.MessageEnd();

  CryptoPP::word64 size = decoder.MaxRetrievable();
  if(size && size <= SIZE_MAX)
  {
      decoded.resize(size);   
      decoder.Get((byte*)decoded.data(), decoded.size());
  }

  return decoded;
}
