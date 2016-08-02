// Author: Daniel Mendelsohn
// Written for 6.S08, Spring 2016
// Unofficial version 1.5
//
// This library provides non-blocking web connectivity via an ESP8266 chip.
//
// In order for the library to function properly, you will need to edit the 
// file 'serial1.c' and change the value of the macro RX_BUFFER_SIZE from 64
// to something larger, like 1024

#ifndef Wifi_S08_H
#define Wifi_S08_H
#endif

#define ESP_VERSION "1.5"
#define wifiSerial Serial1

#define GET 0
#define POST 1

// Sizes of character arrays
#define BUFFERSIZE 8192
#define RESPONSESIZE 8192
#define MACSIZE 17
#define SSIDSIZE 32
#define PASSWORDSIZE 64
#define DOMAINSIZE 256
#define PATHSIZE 256
#define DATASIZE 1024

// Timing constants
#define INTERRUPT_MICROS 50000
#define AT_TIMEOUT 1000
#define MAC_TIMEOUT 1000
#define CWMODE_TIMEOUT 1000
#define CWAUTOCONN_TIMEOUT 1000
#define RST_TIMEOUT 7000
#define RESTORE_TIMEOUT 7000
#define CONNCHECK_TIMEOUT 10000
#define CIPSTATUS_TIMEOUT 5000
#define CWJAP_TIMEOUT 15000
#define CIPSTART_TIMEOUT 15000
#define CIPSEND_TIMEOUT 5000
#define DATAOUT_TIMEOUT 5000
#define HTTP_TIMEOUT 12000

// AT Commands, some of which require appended arguments
#define AT_BASIC "AT"
#define AT_CWMODE "AT+CWMODE_DEF=1"
#define AT_CWAUTOCONN "AT+CWAUTOCONN=0"
#define AT_RST "AT+RST"
#define AT_RESTORE "AT+RESTORE"
#define AT_CIPAPMAC "AT+CIPAPMAC?"
#define AT_CIPSTATUS "AT+CIPSTATUS"
#define AT_CWJAP "AT+CWJAP_DEF="
#define AT_CIPSTART "AT+CIPSTART=\"TCP\","
#define AT_CIPSEND "AT+CIPSEND="
#define AT_CIPCLOSE "AT+CIPCLOSE"

#define HTTP_POST "POST "
#define HTTP_GET "GET "
#define HTTP_0 " HTTP/1.1\r\nHost: "
#define HTTP_1 "\r\nAccept:*/*\r\nContent-Length: "
#define HTTP_2 "\r\nContent-Type: application/x-www-form-urlencoded"
#define HTTP_END "\r\n\r\n"

//macros for length of boilerplate part of GET and POST requests
//-3 offset to ignore null terminators, +4 offset for "?" and ":" and \r\n
#define HTTP_GET_FIXED_LEN sizeof(HTTP_GET)+sizeof(HTTP_0)+sizeof(HTTP_END)-3+2
//-5 offset to ignore null terminators, +3 offset for ":", and /r/n
#define HTTP_POST_FIXED_LEN sizeof(HTTP_POST)+sizeof(HTTP_0)+sizeof(HTTP_1)\
	+sizeof(HTTP_2)+sizeof(HTTP_END)-5+3


#include <WString.h>
#include <Arduino.h>

class ESP8266 {
	public:
		ESP8266();
		ESP8266(bool verboseSerial);
		void begin();
		bool isConnected();
		void connectWifi(String ssid, String password);
		bool isBusy();
		void sendRequest(int type, String domain, int port, String path, 
				String data);
		void sendRequest(int type, String domain, int port, String path,
				String data, bool auto_retry);
		void clearRequest();
		int benchmark;
		bool hasResponse();
		String getResponse();
		String getMAC();
		String getVersion();
		bool restore();
		bool reset();
		String sendCustomCommand(String command, unsigned long timeout);
		bool isAutoConn();
		void setAutoConn(bool value);
		int getTransmitCount();
		void resetTransmitCount();
		int getReceiveCount();
		void resetReceiveCount();

	private:
		static ESP8266 * _instance; //Static instance of this singleton class

		//String constants for processing ESP8266 responses
		static char const READY[];
		static char const OK[];
		static char const OK_PROMPT[];
		static char const SEND_OK[];
		static char const ERROR[];
		static char const FAIL[];
		static char const STATUS[];
		static char const ALREADY_CONNECTED[];
		static char const HTML_START[];
		static char const HTML_END[];

		// Private enums and structs
		enum RequestType {GET_REQ, POST_REQ};
		struct Request {
			volatile char domain[DOMAINSIZE];
			volatile char path[PATHSIZE];
			volatile char data[DATASIZE];
			volatile int port;
			volatile RequestType type;
			volatile bool auto_retry;
		};
		enum State {
			IDLE, //When nothing is happening
			CIPSTATUS, //awaiting CIPSTATUS response
			CWJAP, //connecting to network
			CIPSTART, //awaiting CIPSTART response
			CIPSEND, //awaiting CIPSEND response
			DATAOUT, //awaiting "SEND OK" confirmation
			AWAITRESPONSE, //awaiting HTTP response
		};

		// Functions for strictly non-ISR context
		void enableTimer();
		void disableTimer();
		void init(bool verboseSerial);
		bool checkPresent();
		void getMACFromDevice();
		bool waitForTarget(const char *target, unsigned long timeout);
		bool stringToVolatileArray(String str, volatile char arr[], 
				uint32_t len);

		// Functions for ISR context
		static void handleInterrupt(void);
		void processInterrupt();
		bool isTargetInResp(const char target[]);
		bool getStringFromResp(const char *target, char *result);
		bool getStringFromResp(const char *startTarget, const char *endTarget,
				char *result);
		int getStatusFromResp(); //Only call if we got an OK CIPSTATUS resp
		void loadRx();
		void emptyRx();
		void emptyRxAndBuffer();

		// Non-ISR variables
		String MAC;
		IntervalTimer timer;

		// Shared variables between user calls and interrupt routines
		volatile bool serialYes;
		volatile bool newNetworkInfo;
		volatile char ssid[SSIDSIZE];
		volatile char password[PASSWORDSIZE];
		volatile bool connected;
		volatile bool doAutoConn;
		volatile bool hasRequest;
		volatile Request *request_p;
		volatile bool responseReady;
		volatile char response[RESPONSESIZE];
		volatile int transmitCount;
		volatile int receiveCount;
	
		
		// Variables for interrupt routines
		volatile State state;
		volatile unsigned long lastConnectionCheck;
		volatile unsigned long timeoutStart;
		volatile char inputBuffer[BUFFERSIZE];	// Serial input loaded here
};
