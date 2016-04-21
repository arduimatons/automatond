#include "LinxRelay.h"




// cout << "keys in config: " << jsonConfig.MemberCount() << endl;
/*
 static const char* kTypeNames[] = 
    { "Null", "False", "True", "Object", "Array", "String", "Number" };
for (Value::ConstMemberIterator itr = jsonConfig.MemberBegin();
    itr != jsonConfig.MemberEnd(); ++itr)
{
    printf("Type of member %s is %s\n",
        itr->name.GetString(), kTypeNames[itr->value.GetType()]);
}
*/  

using namespace std;
using namespace rapidjson;

// Instantiate the display
ArduiPi_OLED display;
time_t last_msg = 0;

int main(int argc, char *argv[])
{

  #ifdef OLED_DEBUG
    display.init(OLED_I2C_RESET, 3);
    display.begin();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.clearDisplay();

    printHeader();
  #endif



  // create object to store config document
  Document jsonConfig = getConfig("/.automatond.json");
  // if we could open it we are good, if not close with err
  if(jsonConfig.IsObject())
  { 
    int allKeys = 1;
    //using rapidjson object to store some data, and pass it around as a parameter to functions
    string configKeys[] = {"ACL","MQTT_TOPIC", "MQTT_OUT", "MQTT_IN", "SECRET_KEY","MQTT_CLIENT", "ACL2"};
    for(const auto& configKey : configKeys) {
       allKeys = jsonConfig.HasMember(configKey.c_str());
    }
    if(allKeys > 0){cout << "got all the config keys we need!" << endl;
    } else {cout << "Invalid configuration file" << endl; return 1;}
  } else {
    // exit program with code 1
    return 1;
  }

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

  // RF24 radio constructor
  RF24 radio(22, 0);
  // RF24Network constructor
  RF24Network network(radio);

  // connects to MQTT broker 
  // need to initialize with ID...
  LinxMQTTRelay relay(jsonConfig["MQTT_CLIENT"]["ID"].GetString(), network, jsonConfig);

  // start RF24 radio
  if(radio.begin()){
    // configure radio and network
    radio.setChannel(115);
    network.begin(00);
    // delay for to make sure radio is up.
    delay(100);

    // send a heartbeat as soon as we come online
    relay.sendHeartbeat();
    time_t last_displayed_time = 0;

    int clear_delay = 15;
    int fake_msg_timer = 20;
    // start looping
    while(1)
    {

        if(time(0) > (last_displayed_time))
       {
        printTime();
        //toPrint to_display;
        //to_display.out_msg = true;
        //display.clearDisplay();
        //printDisplay(to_display);
        last_displayed_time = time(0);
       }
       // clear main 15 seconds after new display
      if(time(0) > (last_msg + clear_delay))
       {
        clearMainArea();
        //toPrint to_display;
        //to_display.out_msg = true;
        //display.clearDisplay();
        //printDisplay(to_display);
       }



      // keep RFNetwork up
      network.update();  
      // keep MQTT relay up 0 second timeout
      relay.loop(0,1);

      // we have a message from the sensor network
      if( network.available() ){
        // handle the message
        relay.handleIncomingRF24Msg();  
      }

      // send heartbeat every HEARTBEAT_INTERVAL seconds
      if(time(0) > (relay.lastBeatSent() + HEARTBEAT_INTERVAL)){ relay.sendHeartbeat(); }
    }
  }
  // could not start radio, exit.
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


Document getConfig(const char* configPath)
{ 
  // get users home path
  string userHome(getenv("HOME"));
  // add config path to it
  string configFileAbsPath(userHome + configPath);
  // open config json as file pointer
  FILE* configFilep = fopen(configFileAbsPath.c_str(), "rb"); 
  // create document to return
  Document jsonConfig;
  if (configFilep != NULL) // if we could open
  {
    cout << "Opened configFile: " << configFileAbsPath << endl;
    char readBuffer[3000];
    //create rapidjson FileReadStream
    FileReadStream configFileStream(configFilep, readBuffer, sizeof(readBuffer));
    // catch parsing error
    if (jsonConfig.ParseStream(configFileStream).HasParseError()) {
          fprintf(stderr, "\nError(offset %u): %s\n", 
        (unsigned)jsonConfig.GetErrorOffset(),
        GetParseError_En(jsonConfig.GetParseError()));
        fclose(configFilep);
        // should exit here.
    } 
  } else {
     cerr << "Could not open config file: " << configFileAbsPath << endl;
     fclose(configFilep);
     // should exit here
  }
  fclose(configFilep);
  return jsonConfig;
}

long LinxMQTTRelay::lastBeatSent()
{
  return this->last_beat;
}

LinxMQTTRelay::LinxMQTTRelay(const char* _id, RF24Network& network, Document& config) : mosquittopp(_id), _network(network)
 {
   mosqpp::lib_init();        // Mandatory initialization for mosquitto library
   this->keepalive = 60;    // Basic configuration setup for myMosq class
   this->id = _id;
   // grab ACL off json store move to ACL to iterate
   rapidjson::Value ACL = config["ACL"].GetObject() ; 
  // iterate rapidjson object values
  for (Value::ConstMemberIterator itr = ACL.MemberBegin();
        itr != ACL.MemberEnd(); ++itr)
  {
    //create a node struct to push onto node_list vector
    Node aNode;
    // convert config json data into C++ struct data
    aNode.addr = stoi(itr->name.GetString());
    aNode.type = ACL[itr->name.GetString()]["types"][0].GetString();
    this->node_list.push_back(aNode);
  }
  // sort node list by type, to be leverages later in subscribing to topics.
  std::sort(node_list.begin(), node_list.end(), compareByType);
   // set other options from json
   this->topic_outgoing = config["MQTT_OUT"].GetString();
   this->topic_incoming = config["MQTT_IN"].GetString();
   this->topic_root = config["MQTT_TOPIC"].GetString();
   this->secret_key_override = config["SECRET_KEY"].GetString();
   this->port = config["MQTT_CLIENT"]["PORT"].GetInt();
   this->host = config["MQTT_CLIENT"]["HOST"].GetString();
   // connect to MQTT broker. mosquittolib...
   connect(host, port, keepalive);
 };


LinxMQTTRelay::~LinxMQTTRelay() {
 loop_stop();            // Kill the thread
 mosqpp::lib_cleanup();    // Mosquitto library cleanup
 }


bool LinxMQTTRelay::send_to_mqtt(uint16_t from_node, const  char * _message)
 {
  stringstream topic;
  // build topic string from ss
  topic << "/" << this->topic_root << "/"<<  this->topic_outgoing << "/" << oct << from_node;
  // to a string
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


void LinxMQTTRelay::on_disconnect(int rc) {
 std::cout << "LinxMQTTRelay - disconnection(" << rc << ")" << std::endl;
 }

 void LinxMQTTRelay::on_connect(int rc)
 {
 if ( rc == 0 ) {

// create temp node_list to remove any nodes with duplicate types.
std::vector<Node> node_list_types_temp; 
std::copy(node_list.begin(), node_list.end(),  std::back_inserter(node_list_types_temp));

for(vector<Node>::iterator it=node_list_types_temp.begin(); it != node_list_types_temp.end(); ++it)
{
  stringstream topicT;
  cout << "Node from struct:" << endl;
  cout << "Addr: 0" << oct << static_cast<int>(it->addr) << endl;
  cout << "Type: " << it->type << endl;
  topicT << "/" << this->topic_root << "/"<<  this->topic_incoming << "/" << it->type << "+";
  cout << topicT << endl;
}
  


  std::cout << "LinxMQTTRelay - connected with server" << std::endl;
  stringstream topic;
  // build topic string from ss
  topic << "/" << this->topic_root << "/"<<  this->topic_incoming << "/" << "+";
  // to a string
  string topic_str(topic.str());

  subscribe(NULL, topic_str.c_str());
 } else {
 std::cout << "LinxMQTTRelay - Impossible to connect with server(" << rc << ")" << std::endl;
 }
 }

 void LinxMQTTRelay::on_publish(int mid)
 {
 std::cout << "LinxMQTTRelay - Message (" << mid << ") succeed to be published " << std::endl;
 }

 void LinxMQTTRelay::on_subscribe(int mid, int qos_count, const int *granted_qos)
{
  std::cout << "Subscription succeeded." << std::endl;
}


void LinxMQTTRelay::on_message(const struct mosquitto_message *message)
{
  std::cout << "Got a message for RF24Network!" << std::endl;
  string msg_topic(message->topic);
  msg_topic.erase(0,1);// get rid of leading / 
  size_t root_topic_pos = msg_topic.find_first_of("/");
  // find last period, which designates beggining of MAC
  size_t node_addr_pos = msg_topic.find_last_of("/");
  string node_addr_str = msg_topic.substr(node_addr_pos+1, msg_topic.length());
  uint16_t node_addr = stoi(node_addr_str, nullptr, 8);
  string root_topic_str = msg_topic.substr(0, root_topic_pos);

  if(_network.is_valid_address(node_addr))
  { 
    cout << "valid address! " << static_cast<int>(node_addr) << endl; 
    cout << "root topic: " << root_topic_str << endl; 
    // buffer to send over network
    char payload_chars[message->payloadlen+1];
    // copy payload to it?
    memcpy(payload_chars,message->payload,message->payloadlen);
    payload_chars[message->payloadlen] = '\0';
    // create string to encode and sign.
    string payloadToEncode(payload_chars);
    std::cout << "To encode: " << std::endl;
    std::cout << payloadToEncode << std::endl;
    // encode and sign
    string encodedAndSignedPL = genPayload(payloadToEncode);
    std::cout << "Encoded: " << std::endl;
    std::cout << encodedAndSignedPL << std::endl;

    size_t payload_size = encodedAndSignedPL.length();
    // RF24Network defaults to max 144byte payloads, make sure we dont try to send those..
    if(payload_size <= MAX_PAYLOAD_LEN){
      // need to be able to change message type, only the one for now...
      RF24NetworkHeader header(node_addr, 65); 
      char payload_for_rf24[payload_size]; 
      // copy data into payload_for_rf24 buffer
      strncpy(payload_for_rf24, encodedAndSignedPL.c_str(), payload_size); 
      // send on RFNetwork.
      if(_network.write(header, &payload_for_rf24, payload_size))
       {
        cout << "sent payload: " << payloadToEncode << endl;
        cout << "to node:  0" << static_cast<int>(node_addr) << endl;
        cout << "encoded payload size: " << payload_size << endl;
       } else {
        cout << "Failed sending payload: " << payloadToEncode << endl;
        cout << "to node:  0" << static_cast<int>(node_addr) << endl;
        cout << "encoded payload size: " << payload_size << endl;
       }
    }  else {
        cout << "Failed sending payload: " << payloadToEncode << endl;
        cout << "to node:  0" << static_cast<int>(node_addr) << endl;
        cout << "encoded payload size: " << payload_size << endl;
    }
  }
}

void LinxMQTTRelay::handleIncomingRF24Msg() //pass socket and network through as reference
{
  // create header object to store incoming header
  RF24NetworkHeader header;
  //max payload for rf24 network fragment, could be sent over multiple packets
  char incoming_msg_buf[144]; 
  // read RF24Network fragment, store it in incoming_msg_buf
  size_t readP = this->_network.read(header,&incoming_msg_buf, sizeof(incoming_msg_buf));
  incoming_msg_buf[readP] ='\0';
  // grab my time right after reading network fragment...
  time_t myBeat = std::time(nullptr);
  cout << "got payload from rf!: " << incoming_msg_buf << endl;
  cout << "from node: 0" << oct << header.from_node << endl;
  cout << "of type: " << static_cast<int>(header.type) << endl;
  stringstream nodeID_ss;
  nodeID_ss << "0" << oct << header.from_node;

  std::vector<Node>::iterator res = std::find_if(this->node_list.begin(), this->node_list.end(), 
         find_by_addr(header.from_node));

  if(res != this->node_list.end())
  {
    cout << "OK found this node in the ACL!!!: 0" << oct << header.from_node << endl;
  } else {
    cout << "BAD this node is NOT in the ACL!!!: 0" << oct << header.from_node << endl;
  }

  //cout << incoming_msg_buf << endl;
  // create string object to strip nonsense off buffer
  string payloadStr(incoming_msg_buf); 
  // resize payloadStr 
  if(payloadStr.size() != readP){ payloadStr.resize(readP); } 
  // find first period to determine header length, probably unesseary - should be 16
  size_t HEADER_LEN = payloadStr.find_first_of(".");
  // find last period, which designates beggining of MAC
  size_t MAC_POS = payloadStr.find_last_of(".");
  // grab MAC from end of payloadStr, store in encodedMAC
  string encodedMAC = payloadStr.substr(MAC_POS+1, payloadStr.length());
  // grab header+payload to hash
  string signedPL = payloadStr.substr(0, MAC_POS);
  // copy string to hash into tempbuf to be destroyed by hashing process, pretty sure...
  char signedPL_TO_HASH[signedPL.length()+1];
  signedPL.copy(signedPL_TO_HASH, signedPL.length(), 0);
  signedPL_TO_HASH[signedPL.length()] = '\0';
  // decode the MAC
  string decodedMAC = encodedMAC;
  // generate MAC of message
  uint8_t hashout[DIGEST_SIZE];
  //generate hash of payload
  blake2s(hashout, signedPL_TO_HASH, SECRET_KEY, DIGEST_SIZE, strlen(signedPL_TO_HASH), sizeof(SECRET_KEY)); 
  stringstream hashHex;
  for(int i=0; i< DIGEST_SIZE; i++){
     // convert uint8_t bytes to hex chars
     hashHex << std::hex << +hashout[i];   
  }
  // create string object from hashHex stringstream
  string msgHash(hashHex.str()); 
  // test if delivered hash and our version of hash match
  if(decodedMAC == msgHash){
    // slice up payload into a string for encoded header, and one for encoded payload 
    string encodedBEAT = payloadStr.substr(0, HEADER_LEN);
    string encodedMSG = payloadStr.substr(HEADER_LEN+1, (payloadStr.length()-(HEADER_LEN+encodedMAC.length()+2)));
    // decode msg beat to test if it aligns with our beat
    string decodedBEAT = encodedBEAT;
    // convert str to long
    long beatFromMsg = atol(decodedBEAT.c_str());
    printf("Beat from MSG: %ld\n", beatFromMsg);
    printf("My beat: %ld\n", myBeat);
    // test difference in time of beats
    long diff = (myBeat - beatFromMsg);
    // allow for one second of sway
    if(diff <= 3)
    { 
      // we know it is a valid message, time to decode and send over MQTT
      string decodedMSG = this->decode_b64(encodedMSG);
      cout << "Got a valid message: " << decodedMSG << endl;
      #ifdef OLED_DEBUG
        toPrint to_display;
        to_display.in_msg = true;
        to_display.type = static_cast<int>(header.type);
        to_display.node_addr = header.from_node;
        to_display.topic = "sample/topic";
        to_display.payload = decodedMSG.c_str();
        printDisplay(to_display);
      #endif
        last_msg = time(0);

      send_to_mqtt(header.from_node, decodedMSG.c_str());
    } else {
      cerr << "Invalid message beat!" << endl;
      cout << "from node: 0" << oct << header.from_node << endl;
      cout << "of type: " << static_cast<int>(header.type) << endl;
    }    
  } else {
    cerr << "Invalid MAC!" << endl;
    cout << "from node: 0" << oct << header.from_node << endl;
    cout << "of type: " << static_cast<int>(header.type) << endl;
  }   
}

string LinxMQTTRelay::genHash(string toHash)
{
    uint8_t hashout[DIGEST_SIZE];
    blake2s(hashout, toHash.c_str(), SECRET_KEY, DIGEST_SIZE, toHash.length(), sizeof(SECRET_KEY)); //generate hash of payload
    stringstream hashHex;
    for(int i=0; i< DIGEST_SIZE; i++){
      // convert uint8_t bytes to hex chars
      hashHex << std::hex << +hashout[i];   
    }
   string MAC(hashHex.str());
   return MAC;
}

//send a payload, with beat header
string LinxMQTTRelay::genPayload(std::string payload_msg)
{
  time_t beat = std::time(0);
  stringstream payloadBeatSS;
  // convert beat to string
  payloadBeatSS << beat;
  // encode beat
  string BeatStr(payloadBeatSS.str());
  // encode payload_msg
  string encodedPLStr = this->encode_b64(payload_msg);
  // create new string to hash,
  string toHash = BeatStr +"."+ encodedPLStr;
  // generate hash
  string HMAC = genHash(toHash);
  // build full payload
  string fullPL(toHash + "." +  HMAC);
  cout << "Generated payload: " << endl; 
  cout << fullPL << endl; 
  return fullPL;
}
//send a beat, no payload, beat header
string LinxMQTTRelay::genPayload()
{
  time_t beat = time(nullptr);
  stringstream payloadBeatSS;
  // convert beat to string
  payloadBeatSS << beat;
  // enncode beat
  string BeatStr(payloadBeatSS.str());
  string toHash = BeatStr +"."+ this->encode_b64(BeatStr);
  string HMAC = genHash(toHash);
  string fullPL(toHash + "." +  HMAC);
  cout << "Generated beat: " << endl; 
  cout << fullPL << endl; 
  return fullPL;
}

void LinxMQTTRelay::sendHeartbeat(){
  // i dono send to self? what address for multicast
  RF24NetworkHeader header(00,5);
  // genPayload with no params will create a heartbeat payload.
  string fullPL = this->genPayload();
  char payload[fullPL.length()]; // max payload for rf24 network fragment, could be sent over multiple packets
  strncpy(payload, fullPL.c_str(), fullPL.length()); // copy data into payload char, will probably segfault if over 144 char...
  //heartbeat_pl heart = { time(0) }; 
  payload[fullPL.length()] = '\0';
  if(_network.multicast(header, &payload, fullPL.length(),1)){
      time_t sent_beat = std::time(nullptr);
      printf("sent beat: %ld\n", sent_beat);
      this->last_beat = sent_beat;
   } else {
     cout << "hmm multicast failed try again next loop..." << endl;
   }
}

string LinxMQTTRelay::encode_b64(string to_encode)
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


string LinxMQTTRelay::decode_b64(string to_decode)
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


#ifdef OLED_DEBUG

// craft time string HH:MM:SS
std::string prettyTime()
{
time_t raw_time;
time(&raw_time);
struct tm * time_info;
time_info = localtime( &raw_time );
int h = time_info->tm_hour;
std::stringstream h_ss;
std::stringstream m_ss;
std::stringstream s_ss;
h = (h == 0) ? 12 : h; // midnight, 12AM
h = (h>12) ? h-12 : h; // PM 13-23
(h<10) ? h_ss << "0" : h_ss;  // add 0 to 1-9
h_ss << h;
(time_info->tm_min < 10) ? m_ss << "0" << time_info->tm_min : m_ss << time_info->tm_min; // add 0 to min string if less than 10
(time_info->tm_sec < 10) ? s_ss << "0" << time_info->tm_sec : s_ss << time_info->tm_sec; // add 0 to sec string if less than 10
std::stringstream time_str;
time_str << h_ss.str() << ":" << m_ss.str()  << ":" << s_ss.str() ;
return std::string(time_str.str());
}



void printHeader()
{
  display.setCursor(2,0);
  display.print("RF24 Master");
  //horizontal
  display.drawLine(0, 10, display.width()-1, 10, WHITE);

  // vertical
  display.drawLine(75, 0, 75, 22, WHITE);
  display.drawLine(75, 22, display.width()-1, 22, WHITE);
  display.setCursor(90,0);
  display.print("Time");
  display.display();
}

void clearMainArea()
{
  display.fillRect(0, 12, 75, 11, BLACK);
  display.fillRect(0, 23, display.width(), display.height()-1, BLACK);
  display.display();
}

void printTime()
{
//clear previous time
display.fillRect(76, 11, display.width()-1, 11, BLACK);
display.setCursor(79,13);
// set new time
display.print(prettyTime().c_str());
//display time
display.display();
}

void printDisplay(toPrint &to_print)
{
  clearMainArea();

if(to_print.out_msg || to_print.in_msg){

  //horizontal

display.setCursor(70,29);
display.print("Topic");
//display.drawLine(58, 10, 58, 20, WHITE);
//horizontal
display.drawLine(0, 27, display.width()-1, 27, WHITE);
display.drawLine(0, 37, display.width()-1, 38, WHITE);
display.drawLine(0, 47, display.width()-1, 47, WHITE);
// vertical
display.drawLine(24, 27, 24, 47, WHITE);
display.drawLine(44, 27, 44, 47, WHITE);

if(to_print.out_msg)
{
  display.setCursor(6,29);
  display.print("To");
  display.setCursor(10,16);
  display.print("Msg Out!");
} else if(to_print.in_msg)
{
  display.setCursor(2,29);
  display.print("From");
  display.setCursor(10,16);
  display.print("Msg In!");
}



  std::stringstream node_addr_ss;
  node_addr_ss << std::oct << to_print.node_addr;
  std::string node_addr_str(node_addr_ss.str());
  display.setCursor(1,39);
  display.print(node_addr_str.c_str());


  display.setCursor(28,29);
  display.print("Ty");

  std::stringstream msg_type_ss;
  msg_type_ss << to_print.type;
  std::string msg_type_str(msg_type_ss.str());
  display.setCursor(26,39);
  display.print(msg_type_str.c_str());



  display.setCursor(48,39);
  display.print(to_print.topic.c_str());

  display.setCursor(0,49);
  display.print(to_print.payload.c_str());


}

 display.display();
}

#endif  
// oled debug