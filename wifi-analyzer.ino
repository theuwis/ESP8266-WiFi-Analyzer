#include <stdio.h>
#include <string.h>
#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include "ESP8266WiFi.h"
#include "user_interface.h"



//TODO timer zelf resetten => loopt nu gewoon door denk ik (bool op true)


/*
    GPIO    NodeMCU   Name  |   Uno
  ===================================
     15       D8      CS    |   D10
     13       D7      MOSI  |   D11
     12       D6      MISO  |   D12
     14       D5      SCK   |   D13

     2        D4      DC    |   D9
     0        D3      RST   |   D8

*/

#define UPDATE_INTERVAL 2000
#define MAX_SSID_LEN    7
#define SCR_WIDTH  320
#define SCR_HEIGHT 240


#define ESP8266
//#define UNO
//#define DEBUG

#ifdef ESP8266
	#define TFT_MISO 12
	#define TFT_CLK  14
	#define TFT_MOSI 13
	#define TFT_DC    2
	#define TFT_RST   0
	#define TFT_CS   15
#endif

#ifdef UNO
	#define TFT_MISO 12
	#define TFT_CLK  13
	#define TFT_MOSI 11
	#define TFT_DC    9
	#define TFT_RST   8
	#define TFT_CS   10
#endif



// ch_coord[] stores the pixel coord of the center of the 13 channels
const int ch_coord[15] = {23, 43, 64, 85, 106, 127, 148, 169, 190, 211, 232, 253, 274, 295, 314};

// ch_color[] stores the different colors of the 13 channels
const int ch_color[13] = {		
				0xF800, //red
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

// nr_of_netw_per_ch[] stores the amount of networks that use each channel
int nr_of_netw_per_ch[13] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

int nr_of_netw = 0;			// stores total nr of netw discoverd in current scan
int nr_ch1, nr_ch6,			// used to calculate the optimal channel
    nr_ch11, nr_ch_lowest;
int idle_state = 0;			// used for 'running' animation
char idle[5] = "/-\\|";		// used for 'running' animation
char nr_of_netw_buff[5];	// used to pad the nr of networks
bool refresh_flag = false;	// used as flag when networks are refreshed

// wrapper for controlling the tft screen
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);;

// wrapper for timer used to refresh networks
os_timer_t refresh_timer;


// callback for refresh timer
void timer_callback(void *pArg) {
	refresh_flag = true;
}

void setup(){
	// start the serial communictation (used for debugging => #define DEBUG)
	Serial.begin(115200);

	// set WiFi to station mode and disconnect from an AP (if it was previously connected)
	WiFi.mode(WIFI_STA);
	WiFi.disconnect();
	delay(100);

	#ifdef DEBUG
		Serial.printf("wifi setup done\n");
	#endif
  

	// start the screen and set the correct rotation
	// pinheaders are on the left side in this case
	tft.begin();
	tft.setRotation(3);

	#ifdef DEBUG
		Serial.printf("tft setup done\n");
	#endif


	// setup the cursor for text
	tft.setCursor(0, 0);
	tft.setTextColor(ILI9341_WHITE);
	tft.setTextSize(1);
	tft.setTextWrap(false);

  // reset the screen => make is completely black
  tft.fillScreen(ILI9341_BLACK);

	// create the red top header bar
	tft.fillRect(0, 0, 320, 19, ILI9341_RED);
	tft.setCursor(34, 2);
	tft.setTextSize(2);
	tft.print("ESP8266 WiFi Analyzer");

	// show general info about the networks, show 0 netw found at startup
	update_general_netw_info(0);

	// draw the box that contains the signal triangles
	tft.drawRect(22, 30, 294, 190, ILI9341_WHITE);

	// create the vertical axis
	//	 => draw tickmarks on the vertical axis to indicate sign strength
	//  => also draw vertical lines to extend these tickmarks (in a subtle color)
	for(int i = 38; i < 218; i+= 10){
		tft.drawPixel(23, i, ILI9341_WHITE);
		tft.drawPixel(24, i, ILI9341_WHITE);

		tft.drawFastHLine(25, i, 290, 0x2104);
	}

	//  => show the signal strength on the vertical axis (y-axis values)
	//  => also, print the unit first (dBm)
	tft.setCursor(2, 22);
	tft.print("dBm");

	for(int i = 8; i <= 90; i += 10){
		tft.setCursor(2, i * 2 + 20);
		tft.print("-");
		tft.print(i + 2, DEC);
	}

	// draw tick marks on horizontal axis to idicate the channels 1-13
	for(int i = 1; i < 14; i++){
		tft.drawPixel(ch_coord[i], 218, ILI9341_WHITE);
	}
  
	// show channel numbers on horizontal axis (x-axis values)
	tft.setTextColor(ILI9341_WHITE);
	tft.setCursor(0, 223);
	tft.print("   ch");
    
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
	tft.setTextColor(ILI9341_WHITE);
	tft.setCursor(0, 232);
	tft.print("#netw");

	// reset the text to it's default
	tft.setCursor(0, 0);
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.setTextSize(1);

	// set up the refresh timer
	os_timer_setfn(&refresh_timer, timer_callback, NULL);
	#ifdef DEBUG
		Serial.printf("timer setup done\n");
	#endif

	// start the timer
	os_timer_arm(&refresh_timer, UPDATE_INTERVAL, true);
}




void loop(void) {
	update_idle();
//	delay(100);

	if(refresh_flag){
		refresh_flag = false;

		#ifdef DEBUG
			Serial.printf("scan started\n");
		#endif

		update_idle();
		// scan for networks
		nr_of_netw = WiFi.scanNetworks(false, true);
		update_idle();

		#ifdef DEBUG
			Serial.printf("scan done\n");
		#endif
	  
		// clear the screen
		clear_netw_screen();
		update_idle();

		// reset the number of networks for all channels before starting the scan
		for(int i = 0; i < 13; i++){
			nr_of_netw_per_ch[i] = 0;
		}

		// run through all discovered networks, update the amount of networks per channel
		// and draw each network on the screen
		for(int i = 0; i < nr_of_netw; ++i){
			nr_of_netw_per_ch[WiFi.channel(i) - 1]++;
			draw_netw_str(WiFi.channel(i), WiFi.RSSI(i), WiFi.SSID(i).c_str(), WiFi.encryptionType(i) != ENC_TYPE_NONE);

			update_idle();
		}

		// update info about networks
		update_general_netw_info(nr_of_netw);
		update_idle();

		// update the networks per channel counters
		update_nr_of_netw_per_ch();
		update_idle();

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
	 } 
}


void update_idle(){
	/*
	tft.setCursor(306, 21);
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.print(idle[idle_state]);

	if(idle_state < 3){
		idle_state++;
	}
	else{
		idle_state = 0;

		// draw extra pixel in the center of |, because there is no pixel there for some reason
		tft.drawPixel(308, 24, ILI9341_WHITE);
	}
	*/
}


void update_general_netw_info(int nr_of_netw){
	// calculate best netw (least crowded on ch 1, 6, 11)
	// dummy variables for easier typing
	nr_ch1  = nr_of_netw_per_ch[0];
	nr_ch6  = nr_of_netw_per_ch[5];
	nr_ch11 = nr_of_netw_per_ch[10];

	// calculate lowest amount of netw over 3 ch
	nr_ch_lowest = nr_ch1;
	if(nr_ch_lowest > nr_ch6)  nr_ch_lowest = nr_ch6;
	if(nr_ch_lowest > nr_ch11) nr_ch_lowest = nr_ch11;

	// create a string for the best networks
	char sugg_ch_buff[15] = "";
	memset(sugg_ch_buff, 0, sizeof(sugg_ch_buff));

	if(nr_ch1 == nr_ch_lowest){
		strcat(sugg_ch_buff, "1");

		if(nr_ch6 == nr_ch_lowest)  strcat(sugg_ch_buff, ", 6");
		if(nr_ch11 == nr_ch_lowest) strcat(sugg_ch_buff, ", 11");
	}
	else if(nr_ch6 == nr_ch_lowest){
		strcat(sugg_ch_buff, "6");

		if(nr_ch11 == nr_ch_lowest) strcat(sugg_ch_buff, ", 11");
	}
	else{
		strcat(sugg_ch_buff, "11");
	}

	// print the info on the screen
	// nr_of_netw get padded to 3 digits (right justified), sugg_ch gets padded to 8
	tft.setCursor(20, 20);
	tft.setTextSize(1);
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.printf("%3d network(s) found -- suggested ch: %-8s", nr_of_netw, sugg_ch_buff);	
}

void update_nr_of_netw_per_ch(){
	// iterate over all 13 channels
	for(int ch_nr = 0; ch_nr < 13; ch_nr++){
		// update the cursor position and color, to match channel color
		// the position is 1 ch shifted to the left, so -8 pixels
		tft.setCursor(ch_coord[ch_nr + 1] - 8, 232);
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
	}
}

void clear_netw_screen(){
	// clear the screen
	tft.fillRect( 23, 31, 292, 188, ILI9341_BLACK); // clear content of the bounding box
	tft.fillRect(316, 30,   3, 189, ILI9341_BLACK); // fixes the part right of the box, where names sometimes appear
	tft.drawFastVLine(315, 30, 190, ILI9341_WHITE); // redraw the right line of the bounding box

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

	// crop the SSID to a max length of MAX_SSID_LEN
	// append with '$' if the SSID is too long
	char temp[50];
	memset(temp, 0, sizeof(temp));
	strncpy(temp, ssid, MAX_SSID_LEN);
	if(strlen(ssid) > MAX_SSID_LEN) temp[MAX_SSID_LEN - 1] = '$';

	// draw the signal, sign str that are too high (>0dBm) or too low (<-100dBm)
	if(sig_str_coord <= 220 && sig_str_coord > 20){
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
		tft.printf("%s (%ddB)", temp, sig_str);
		if(!protc) tft.print("*");
	}
}
