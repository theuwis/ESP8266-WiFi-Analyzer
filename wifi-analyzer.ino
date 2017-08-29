#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include "ESP8266WiFi.h"

/*
    GPIO    NodeMCU   Name  |   Uno
  ===================================
     15       D8      CS    |   D10
     13       D7      MOSI  |   D11
     12       D6      MISO  |   D12
     14       D5      SCK   |   D13

     2        D4      DC    |   D9
     0        D3      RST   |   D8

#define TFT_MISO 12
#define TFT_CLK  13
#define TFT_MOSI 11
#define TFT_DC    9
#define TFT_RST   8
#define TFT_CS   10
*/

#define TFT_MISO 12
#define TFT_CLK  14
#define TFT_MOSI 13
#define TFT_DC    2
#define TFT_RST   0
#define TFT_CS   15

#define SCR_WIDTH  320
#define SCR_HEIGHT 240

const int ch_coord[15] = {23, 43, 64, 85, 106, 127, 148, 169, 190, 211, 232, 253, 274, 295, 314};
const int ch_color[13] = {0xF800, //red
                    0x07E0, //green
                    0xF81F, //magenta
                    0x07FF, //cyan
                    0xF810, //pink
                    0xFFE0, //yellow
                    0x001F, //blue
                    0xF800, //red
                    0x07E0, //green
                    0xF81F, //magenta
                    0x07FF, //cyan
                    0xFFE0, //yellow
                    0x001F  //blue
                   };

int nr_of_netw_per_ch[13] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

const char* const encryption_type_table[] = {
  "",
  "",
  "WPA/PSK",
  "",
  "WPA2/PSK",
  "WEP",
  "",
  "open netw",
  "WPA/WPA2/PSK"
}; // all const

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);
//Adafruit_ILI9341 tft = Adafruit_ILI9341(10, 9, 11, 13, 8, 12);  


void setup(){
  Serial.begin(115200);
  randomSeed(analogRead(0));

  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.println("WiFi setup done");
  

  // start the screen
  tft.begin();

  // set the correct rotation of screen
  // pinheaders are on the left side in this case
  tft.setRotation(3);

  // setup the cursor for text
  tft.setCursor(0, 0);
  tft.setTextColor(ILI9341_WHITE);
//  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setTextSize(1);
  tft.setTextWrap(false);

  // reset the screen => make is completely black
  tft.fillScreen(ILI9341_BLACK);

  /***** HEADER *****/
  // draw the top red header bar
  tft.fillRect(0, 0, 320, 19, ILI9341_RED);

  // show ESP8266 WiFi Analyzer text in this red header
  tft.setCursor(34, 2);
  tft.setTextSize(2);
  tft.print("ESP8266 WiFi Analyzer");

  // show general info about the networks, show 0 netw found at startup
  update_general_netw_info(0);

  // draw the box that contains the signal triangles
  tft.drawRect(22, 30, 294, 190, ILI9341_WHITE);
  /***** END HEADER *****/


  /***** Y AXIS *****/
  // draw tickmarks on the vertical axis to indicate sign strength
  // also draw vertical lines to extend these tickmarks (in a subtle color)
  for(int i = 38; i < 218; i+= 10){
    tft.drawPixel(23, i, ILI9341_WHITE);
    tft.drawPixel(24, i, ILI9341_WHITE);

    tft.drawFastHLine(25, i, 290, 0x2104);
  }

  // show the signal strength on the vertical axis (y-axis values)
  // print the unit first (dBm)
  tft.setCursor(2, 22);
  tft.print("dBm");
  // print all the values left of the tickmarks
  for(int i = 8; i <= 90; i += 10){
    tft.setCursor(2, i * 2 + 20);
    tft.print("-");
    tft.print(i + 2, DEC);
  }
  /***** END Y AXIS *****/


  /***** X AXIS *****/
  // draw tick marks on horizontal axis to idicate the channels 1-13
  for(int i = 1; i < 14; i++){
    tft.drawPixel(ch_coord[i], 218, ILI9341_WHITE);
  }
  
  // show channel numbers on horizontal axis (x-axis values)
  int ch_nr = 0;
  for(int i = 41; i <= 290; i += 21){
    tft.setTextColor(ch_color[ch_nr]);
    tft.setCursor(i, 223);
    tft.print(ch_nr + 1, DEC);
    ch_nr++;

    // numbers 10-13 need a slightly different alignment, which is fixed here
    if(i == 209){
      i = 206;
    }
  }

  // show nr of networks found on each channel below the channel nr
  update_nr_of_netw_per_ch();
  /*
  ch_nr = 0;
  for(int i = 35; i <= 290; i += 21){
    tft.setTextColor(ch_color[ch_nr]);
    tft.setCursor(i, 232);
    
    //int temp = random(0, 15);
    int temp = nr_of_netw_per_ch[ch_nr];
    if(temp == 0)     tft.print("   ");
    else if(temp > 9) tft.print("(x)");
    else{
      tft.print("(");
      tft.print(temp, DEC);
      tft.print(")");
    }
    
    ch_nr++;
  }
  */
  /***** END X AXIS *****/

  // testing...
  /*
  draw_netw_str(1, -90, "THEUWIS", true);
  draw_netw_str(5, -10, "test", false);
  draw_netw_str(6, -50, "hallo", true);
  draw_netw_str(11, -45, "de wifi", false);
  */
  /*
  for(int i = 0; i < 13; i++){
    int sig_str = random(60, 180);
    tft.drawTriangle(ch_coord[i], 219, ch_coord[i + 1], sig_str, ch_coord[i + 2], 219, ch_color[i]);

    tft.setCursor(ch_coord[i + 1] - 6, sig_str - 9);
    tft.setTextColor(ch_color[i]);
    tft.print("THEUWIS"); //TODO enkel x-aantal letters printen voor lange namen..
                          //TODO sign str (-85) + * als gn beveiliging
                          //TODO hoeveelheid netw per ch aanpassen
  }

  tft.drawFastHLine(22, 219, 294, ILI9341_WHITE);
  for(int i = 43; i <= 295; i += 21){
    tft.drawPixel(i, 218, ILI9341_WHITE);
  }
  */

  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
}


int i = 0;
int nr_of_netw = 0;
char wait[5] = "/-\\|";
char nr_of_netw_buff[5];

void loop(void) {
  #ifdef DEBUG
    Serial.printf("scan started\n");
  #endif

  // scan for networks
  nr_of_netw = WiFi.scanNetworks(false, true);  // check for amount of networks
                                                // 1st arg: don't use async scanning
                                                // 2nd arg: show hidden netw
  #ifdef DEBUG
    Serial.printf("scan done\n");
  #endif
  
  // update info about networks
  update_general_netw_info(nr_of_netw);

  // clear the screen
  clear_netw_screen();

  // run through all discovered networks, update the amount of networks per channel
  // and draw each network on the screen
  for(int i = 0; i < nr_of_netw; ++i){
    nr_of_netw_per_ch[WiFi.channel(i) - 1]++;
    draw_netw_str(WiFi.channel(i), WiFi.RSSI(i), WiFi.SSID(i).c_str(), WiFi.encryptionType(i) != ENC_TYPE_NONE);
  }

  // update the networks per channel counters
  update_nr_of_netw_per_ch();

  // debugging info for terminal of each discovered network
  #ifdef DEBUG
    if(nr_of_netw == 0){
      Serial.printf("no networks found\n");
    }
    else{
      Serial.printf("%d network(s) found\n", nr_of_netw);
      Serial.printf("==================\n");
      Serial.printf("nr   SSID           ch | strength | security\n");
      Serial.printf("------------------\n");
      
      for(int i = 0; i < nr_of_netw; ++i){
        Serial.printf("#%d: %s @ ch:%d (%ddBm) %s\n", i + 1, WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i), WiFi.encryptionType(i) == ENC_TYPE_NONE ? "open" : "");
      }
    }
    Serial.printf("\n");
  #endif



  // Wait a bit before scanning again
  delay(2000); 
}

void update_general_netw_info(int nr_of_netw){
  // setup cursor and font
  tft.setCursor(20, 20);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);

  // pad the nr of netw with spaces on the right (always 3 char)
  sprintf(nr_of_netw_buff, "%3d", nr_of_netw);

  // print the info on the screen
  tft.print(nr_of_netw_buff);
  tft.print(" network(s) found -- suggested ch: 1, 6, 11");
}

void update_nr_of_netw_per_ch(){
  int ch_nr = 0;

  // iterate over all 13 channels
  for(int i = 35; i <= 290; i += 21){
    // update the cursor position and color, to match channel color
    tft.setCursor(i, 232);
    tft.setTextColor(ch_color[ch_nr], ILI9341_BLACK);

    // print the amount of networks in the channel, exceptions are:
    //  - no netw  => don't show a number (print spaces to overwrite prev data)
    //  - >9 netw  => print (x), since two digits are too big to display
    //  - >0 & <10 => print nr of netw
    if(nr_of_netw_per_ch[ch_nr] == 0){
      tft.print("   ");
    }
    else if(nr_of_netw_per_ch[ch_nr] > 9){
      tft.print("(x)");
    }
    else{
      tft.print("(");
      tft.print(nr_of_netw_per_ch[ch_nr], DEC);
      tft.print(")");
    }

    ch_nr++;
  }

  // reset the number of networks for all channels for the next scan
  for(int i = 0; i < 13; i++){
    nr_of_netw_per_ch[i] = 0;
  }
}

void clear_netw_screen(){
  // clear the screen
  tft.fillRect( 23, 31, 292, 188, ILI9341_BLACK);
  tft.fillRect(316, 30,   3, 189, ILI9341_BLACK); // part right of the box, where names sometimes appear
  tft.drawFastVLine(315, 30, 190, ILI9341_WHITE);

  //tft.drawRect(22, 30, 294, 190, ILI9341_WHITE);

  // redraw the vertical and horizontal tickmarks
  for(int i = 38; i < 218; i+= 10){
    tft.drawPixel(23, i, ILI9341_WHITE);
    tft.drawPixel(24, i, ILI9341_WHITE);

    tft.drawFastHLine(25, i, 290, 0x2104);
  }
  for(int i = 1; i < 14; i++){
    tft.drawPixel(ch_coord[i], 218, ILI9341_WHITE);
  }
  
}

void draw_netw_str(int ch, int sig_str, const char * ssid, bool protc){
  // convert sign strength (dBm) to coordinates
  int sig_str_coord = (-sig_str * 2) + 18;

  if(sig_str_coord < 220 && sig_str_coord > 20){
    // draw the signal and redraw the bottom white line
    tft.drawTriangle(ch_coord[ch - 1], 219, ch_coord[ch], sig_str_coord, ch_coord[ch + 1], 219, ch_color[ch - 1]);
    tft.drawFastHLine(ch_coord[ch - 1], 219, 45, ILI9341_WHITE);
    
    // redraw tick marks on horizontal axis to idicate the channels 1-13
    for(int i = 1; i < 14; i++){
      tft.drawPixel(ch_coord[i], 218, ILI9341_WHITE);
    }
  
    // show SSID, strength and if the netw is protected or not (*)
    tft.setCursor(ch_coord[ch] - 5, sig_str_coord - 9);
    tft.setTextColor(ch_color[ch - 1]);
    tft.printf("%s (%ddB)", ssid, sig_str);
    if(!protc) tft.print("(*)");
  }
}
