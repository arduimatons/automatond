#include <string> // strings
#include <iostream>
#include <stdio.h> 
#include <stdlib.h>
#include <iostream>  
#include <sstream>  // string streams
#include <stdio.h>      /* printf */
#include <time.h>       /* time_t, struct tm, difftime, time, mktime */


#include "ArduiPi_OLED_lib.h"
#include "Adafruit_GFX.h"
#include "ArduiPi_OLED.h"


// Instantiate the display
ArduiPi_OLED display;

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
h = (h>12) ? h-12 : h;
(h<10) ? h_ss << "0" : h_ss;
h_ss << h;
(time_info->tm_min < 10) ? m_ss << "0" << time_info->tm_min : m_ss << time_info->tm_min;
(time_info->tm_sec < 10) ? s_ss << "0" << time_info->tm_sec : s_ss << time_info->tm_sec;
std::stringstream time_str;
time_str << h_ss.str() << ":" << m_ss.str()  << ":" << s_ss.str() ;
return std::string(time_str.str());
}

// defines how display should render
struct toPrint {
  bool in_msg = false;
  bool out_msg = false;
  bool invaid = false;
  uint16_t node_addr;
  unsigned char type;
  std::string topic;
  std::string payload;
};


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


int main(int argc, char **argv)
{
 // init display
display.init(OLED_I2C_RESET, 3);
display.begin();
display.setTextSize(1);
display.setTextColor(WHITE);
display.clearDisplay();

printHeader();


time_t last_displayed;
time_t last_sent;
time_t last_cleared;
int clear_delay = 10;
int fake_msg_timer = 20;

//display.fillRect(0, 23, display.width(), display.height()-1, WHITE);
while(1){
  // display new time every second
 if(time(0) > (last_displayed))
 {
  printTime();
  //toPrint to_display;
  //to_display.out_msg = true;
  //display.clearDisplay();
  //printDisplay(to_display);

  last_displayed = time(0);
 }
 // clear main area every 10 seconds
  if(time(0) > (last_cleared + clear_delay))
 {
  clearMainArea();
  //toPrint to_display;
  //to_display.out_msg = true;
  //display.clearDisplay();
  //printDisplay(to_display);
  last_cleared = time(0);

 }


 // diplay new message every 20 seconds
 if(time(0) > (last_sent + fake_msg_timer))
 {
  toPrint to_display;
  to_display.out_msg = true;
  to_display.type = 65;
  to_display.node_addr = 0344;
  to_display.topic = "sample/topic";
  to_display.payload = "this ia a sample payload";

  printDisplay(to_display);
  last_sent = time(0);

 }

}


}