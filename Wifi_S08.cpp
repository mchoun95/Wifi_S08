//Author: Daniel Mendelsohn
//Editor: Mark Chounlakone
//Written for 6.S08 Spring 2016
//Edited for 6.S08 Spring 2017


#include <Wifi_S08.h>
#include <WString.h>
#include <Arduino.h>

ESP8266 * ESP8266::_instance;

// Substrings to look for from AT command responses
const char ESP8266::READY[] = "ready";
const char ESP8266::OK[] = "OK";
const char ESP8266::OK_PROMPT[] = "OK\r\n>";
const char ESP8266::SEND_OK[] = "SEND OK";
const char ESP8266::ERROR[] = "ERROR";
const char ESP8266::FAIL[] = "FAIL";
const char ESP8266::STATUS[] = "STATUS:";
const char ESP8266::ALREADY_CONNECTED[] = "ALREADY CONNECTED";
const char ESP8266::HTML_START[] = "<html>";
const char ESP8266::HTML_END[] = "</html>";

// Constructors and init method
ESP8266::ESP8266() {
	init(false);	//verboseSerial is true by default
}

void ESP8266::init(bool verboseSerial) {
	_instance = this;  //static reference to this object, for ISR handler
	serialYes = true;
	state = IDLE;

	hasRequest = false;
	responseReady = false;
	connected = false;
	doAutoConn = true;
	newNetworkInfo = false;
	MAC = ""; 

	receiveCount = 0;
	transmitCount = 0;

	ssid[0] = '\0'; 
	password[0] = '\0';
	response[0] = '\0';

	// Default initialization of request_p, to avoid NULL pointer exception
	request_p = (volatile Request *)malloc(sizeof(Request));
	request_p->domain[0] = '\0';
	request_p->path[0] = '\0';
	request_p->data[0] = '\0';
	request_p->port = 0;
	request_p->type = GET_REQ;
	request_p->auto_retry = false;
}

void ESP8266::begin() {
	delay(500);
	emptyRx();
	if (serialYes) {
		Serial.begin(115200);
		//while (!Serial);	//Loop until Serial is initialized
		Serial.flush();
	}
	wifiSerial.begin(115200);
	while (!wifiSerial); //Loop until wifiSerial is initialized
	if (checkPresent()) {
		reset();
		getMACFromDevice();
		emptyRx();
	}
	enableTimer();
}

bool ESP8266::isConnected() {
	return connected;
}

void ESP8266::connectWifi(String id, String pass) {
	if (id == "") {
		if (serialYes) {
			Serial.println("The empty string is not a valid SSID");
		}
		return;
	}
	bool idOk = stringToVolatileArray(id, ssid, SSIDSIZE);
	bool passOk = stringToVolatileArray(pass, password, PASSWORDSIZE);
	if (serialYes) {
		if (!idOk) {
			Serial.println("Given SSID is too long");
		} 
		if (!passOk) {
			Serial.println("Given password is too long");
		}
	}
	if (idOk && passOk) {
		newNetworkInfo = true;
	}
}

bool ESP8266::isBusy() {
	return hasRequest;
}

void ESP8266::sendRequest(int type, String domain, int port, String path, 
		String data) {
	sendRequest(type, domain, port, path, data, false);
}

void ESP8266::sendRequest(int type, String domain, int port, String path, 
		String data, bool auto_retry) {
	RequestType _type;
	if (type == GET) {
		_type = GET_REQ;
	} else if (type == POST) { 
		_type = POST_REQ;
	} else {
		Serial.println("Error: Request type must be GET or POST");
		return;
	}
	if (domain.length() > DOMAINSIZE - 1 ||
			path.length() > PATHSIZE - 1 ||
			data.length() > DATASIZE -1) {
		Serial.println("Domain, path, or data is too long");
	} else if (!hasRequest) { // Only send request if one isn't pending already
		disableTimer();
		domain.toCharArray((char *)request_p->domain, DOMAINSIZE);
		path.toCharArray((char *)request_p->path, PATHSIZE);
		data.toCharArray((char *)request_p->data, DATASIZE);
		request_p->port = port;
		request_p->type = _type;
		request_p->auto_retry = auto_retry;
		hasRequest = true;
		responseReady = false;	
		enableTimer();
		//benchmark = millis();
		Serial.println("Request Sent");
	} else if (serialYes) {
		Serial.println("Could not make request; one is already in progress");
	}
}

void ESP8266::clearRequest() {
	disableTimer();
	if (serialYes && hasRequest) {
		Serial.println("Cleared in-progress request");
	}
	hasRequest = false;
	enableTimer();
}

bool ESP8266::hasResponse() {
	return responseReady;
}

String ESP8266::getResponse() {
	disableTimer();
	String r = "";
	if (responseReady) {
		//benchmark = millis() - benchmark;
		//Serial.println(benchmark);
		r = ((char *)response);
		response[0] = '\0';
		responseReady = false; // after getting response, hasResponse() is false
	} else if (serialYes) {
		Serial.println("No response ready");
	}
	enableTimer();
	return r;
}

String ESP8266::getMAC() {
	return MAC;
}

String ESP8266::getVersion() {
	return ESP_VERSION;
}

bool ESP8266::restore() {
	disableTimer();
	bool ok = true;
	emptyRx();
	wifiSerial.println(AT_RESTORE);
	ok = ok && waitForTarget(READY, RESTORE_TIMEOUT);
	ok = ok && reset();
	enableTimer();
	return ok;
}

bool ESP8266::reset() {
	disableTimer();
	bool ok = true;
	emptyRx();
	wifiSerial.println(AT_CWAUTOCONN);
	ok = ok && waitForTarget(OK, CWAUTOCONN_TIMEOUT);
	emptyRx();
	wifiSerial.println(AT_CWMODE);
	ok = ok && waitForTarget(OK, CWMODE_TIMEOUT);
	emptyRx();
	wifiSerial.println(AT_RST);
	ok = ok && waitForTarget(READY, RST_TIMEOUT);
	if (serialYes) {
		if (ok) {
			Serial.println("Reset successful");
		} else {
			Serial.println("WARNING: Reset unsuccesful");
		}
		Serial.flush();
	}
	enableTimer();
	return ok;
}

String ESP8266::sendCustomCommand(String command, unsigned long timeout) {
	disableTimer();
	emptyRx();
	wifiSerial.println(command);
	String customResponse = "";
	unsigned long startTime = millis();
	while (millis() - startTime < timeout) {
		if (wifiSerial.available()) {
			char c = wifiSerial.read();
			customResponse = customResponse + String(c);
		}
	}
	enableTimer();
	return customResponse;
}

bool ESP8266::isAutoConn() {
	return doAutoConn;
}

void ESP8266::setAutoConn(bool value) {
	doAutoConn = value;
}

int ESP8266::getTransmitCount() {
	return transmitCount;
}

void ESP8266::resetTransmitCount() {
	transmitCount = 0;
}

int ESP8266::getReceiveCount() {
	return receiveCount;
}

void ESP8266::resetReceiveCount() {
	receiveCount = 0;
}

//// PRIVATE FUNCTIONS (Non-ISR only)
void ESP8266::enableTimer() {
	timer.begin(ESP8266::handleInterrupt, INTERRUPT_MICROS);
}

void ESP8266::disableTimer() {
	timer.end();
}

// Check if ESP8266 is present, this 
bool ESP8266::checkPresent() {
	emptyRx();
	wifiSerial.println(AT_BASIC);
	bool ok = waitForTarget(OK, AT_TIMEOUT);
	if (serialYes) {
		if (ok) {
			Serial.println("ESP8266 present");
		} else {
			Serial.println("ESP8266 not present");
		}
		Serial.flush();
	}
	return ok;
}

// Blocking function (with timeout) to get MAC address of ESP8266
void ESP8266::getMACFromDevice() {
	MAC = "";
	wifiSerial.println(AT_CIPAPMAC);	//Send MAC query
	unsigned long start = millis();
	bool foundMacStart = false;
	while (millis() - start < MAC_TIMEOUT) {
		if (wifiSerial.available() > 0) {
			char c = wifiSerial.read();
			if (serialYes) {
				Serial.print(c);
			}
			if (foundMacStart) {
				MAC = String(MAC+c);	
				if (MAC.length() >= MACSIZE) {
					unsigned long timeLeft = MAC_TIMEOUT - millis() + start;
					if (waitForTarget(OK,timeLeft)){ //Wait for rest of message
						return; //MAC now holds the MAC address
					} else {
						break; // timeout before "OK" received
					}
				}
			} else if (c == '"') {
				foundMacStart = true;
			}
		}
	}
	if (serialYes) {
		Serial.println("MAC address request timed out");
	}
}

// Empty wifi serial buffer
void ESP8266::emptyRx() {
	while (wifiSerial.available() > 0) {
		char c = wifiSerial.read();
		if (serialYes) {
			Serial.print(c);
		}
	}
}

// Wait until target received over wifiSerial, or timeout as elapsed
// Return whether target was received before the timeout
bool ESP8266::waitForTarget(const char *target, unsigned long timeout) {
	String resp = "";
	unsigned long start = millis();
	while (millis() - start < timeout) {
		if (wifiSerial.available() > 0) {
			char c = wifiSerial.read();
			if (serialYes) {
				Serial.print(c);
			}
			resp = String(resp+c);
			if (resp.endsWith(target)) {
				if (serialYes) {
					Serial.println();	// New line
				}
				return true;
			}
		}
	}
	return false;
}


bool ESP8266::stringToVolatileArray(String str, volatile char arr[],
	   	uint32_t len) {
	if (str.length() >= (len - 1)) { //string is too long
		return false;
	}
	for (uint32_t i = 0; i < str.length(); i++) {
		arr[i] = str[i];
	}
	arr[str.length()] = '\0';
	return true;
}

//// PRIVATE FUNCTIONS (ISR - no String class allowed)
// Static handler calls singleton instance's handler
void ESP8266::handleInterrupt(void) {
	_instance->processInterrupt();
}

// Main interrupt handler, ISR activity follows an FSM pattern
void ESP8266::processInterrupt() {
	switch (state) {
		case IDLE:
			{
			bool autoCheck = doAutoConn
				&& (millis() - lastConnectionCheck > CONNCHECK_TIMEOUT);
			if (ssid[0] != '\0' && (newNetworkInfo || autoCheck)) {
				// If we have an SSID, and it's new (or it's time to refresh),
				// then check network connection and reconnect if needed
				emptyRxAndBuffer();
				wifiSerial.println(AT_CIPSTATUS);
				timeoutStart = millis();
				newNetworkInfo = false;
				state = CIPSTATUS;
			} else if (connected && hasRequest) { // Process the request
				emptyRxAndBuffer();
				wifiSerial.print(AT_CIPSTART);
				wifiSerial.print("\"");
				wifiSerial.print((char *)request_p->domain);
				wifiSerial.print("\",");
				wifiSerial.println(request_p->port);
				timeoutStart = millis();
				responseReady = false;
				state = CIPSTART;
			}
			}	
			break;
		case CIPSTATUS:
			if (isTargetInResp(OK)) {
				int status = getStatusFromResp();
				if (status == -1) {
					if (serialYes) {
						Serial.println("Couldn't determine connection status");
					}
					lastConnectionCheck = millis();
					connected = false;
					state = IDLE;
				} else if (status == 2 || status == 3 || status == 4) {
					lastConnectionCheck = millis();
					connected = true;
					state = IDLE; // Connection ok, return to idle
				} else {
					if (serialYes) {
						Serial.println("Not connected, attempting to connect");
					}
					connected = false;
					emptyRxAndBuffer();	
					wifiSerial.print(AT_CWJAP);
					wifiSerial.print("\"");
					wifiSerial.print((char *)ssid);
					wifiSerial.print("\",\"");
					wifiSerial.print((char *)password);
					wifiSerial.println("\"");
					timeoutStart = millis();
					state = CWJAP;
				}
			} else if (isTargetInResp(ERROR)) {
				if (serialYes) {
					Serial.println("\nCouldn't determine connection status");
				}
				lastConnectionCheck = millis();
				connected = false;
				state = IDLE;
			} else if (millis() - timeoutStart > CIPSTATUS_TIMEOUT) {
				if (serialYes) {
					Serial.println("\nCIPSTATUS timed out");
				}
				lastConnectionCheck = millis();
				connected = false;
				state = IDLE;	// Hopefully it'll work next time
			}	
			break;
		case CWJAP:
			if (isTargetInResp(OK)) {
				lastConnectionCheck = millis(); //Connection succeeded
				connected = true;
				state = IDLE;
			} else if (isTargetInResp(FAIL)) {
				lastConnectionCheck = millis();
				state = IDLE;
			} else if (isTargetInResp(ERROR)) { //This shouldn't happen
				if (serialYes) {
					Serial.println("\nMalformed CWJAP instruction");
				}
				lastConnectionCheck = millis();
				state = IDLE;
			} else if (millis() - timeoutStart > CWJAP_TIMEOUT) {
				if (serialYes) {
					Serial.println("\nCWJAP instruction timed out");
				}
				lastConnectionCheck = millis();
				state = IDLE;
			}
			break;
		case CIPSTART:
			if ((isTargetInResp(ERROR) && isTargetInResp(ALREADY_CONNECTED))
					|| isTargetInResp(OK)) {
				//Compute the length of the request
				int len = strlen((char *)request_p->domain) 
					+ strlen((char *)request_p->path)
					+ strlen((char *)request_p->data);
				char portString[8];
				char dataLenString[8];
				sprintf(portString, "%d", request_p->port);
				sprintf(dataLenString, "%d", strlen((char *)request_p->data));
				len += strlen(portString);
				if (request_p->type==GET_REQ) {
					len += HTTP_GET_FIXED_LEN;
				} else {
					len += strlen(dataLenString);
					len += HTTP_POST_FIXED_LEN;
				}
				emptyRxAndBuffer();
				wifiSerial.print(AT_CIPSEND);
				wifiSerial.println(len);
				timeoutStart = millis();
				state = CIPSEND;
			} else if (isTargetInResp(ERROR)) {
				if (serialYes) {
					Serial.println("Could not make TCP connection");
				}
				hasRequest = request_p->auto_retry;
				state = IDLE;
			} else if (millis() - timeoutStart > CIPSTART_TIMEOUT) {
				if (serialYes) {
					Serial.println("TCP connection attempt timed out");
				}
				hasRequest = request_p->auto_retry;
				state = IDLE;
			}
			break;
		case CIPSEND:
			if (isTargetInResp(OK_PROMPT)) {
				emptyRxAndBuffer();
				if (request_p->type == GET_REQ) {
					wifiSerial.print(HTTP_GET);
					wifiSerial.print((char *)request_p->path);
					wifiSerial.print("?");
					wifiSerial.print((char *)request_p->data); //URL params
					wifiSerial.print(HTTP_0);
					wifiSerial.print((char *)request_p->domain);
					wifiSerial.print(":");
					wifiSerial.print(request_p->port);
					wifiSerial.println(HTTP_END);
					if (serialYes) {
						Serial.print(HTTP_GET);
						Serial.print((char *)request_p->path);
						Serial.print("?");
						Serial.print((char *)request_p->data); //URL params
						Serial.print(HTTP_0);
						Serial.print((char *)request_p->domain);
						Serial.print(":");
						Serial.print(request_p->port);
						Serial.println(HTTP_END);
					}
				} else {
					wifiSerial.print(HTTP_POST);
					wifiSerial.print((char *)request_p->path);
					wifiSerial.print(HTTP_0);
					wifiSerial.print((char *)request_p->domain);
					wifiSerial.print(":");
					wifiSerial.print(request_p->port);
					wifiSerial.print(HTTP_1);
					wifiSerial.print(strlen((char *)request_p->data));
					wifiSerial.print(HTTP_2);
					wifiSerial.print(HTTP_END);
					wifiSerial.println((char *)request_p->data);
					if (serialYes) {
						Serial.print(HTTP_POST);
						Serial.print((char *)request_p->path);
						Serial.print(HTTP_0);
						Serial.print((char *)request_p->domain);
						Serial.print(":");
						Serial.print(request_p->port);
						Serial.print(HTTP_1);
						Serial.print(strlen((char *)request_p->data));
						Serial.print(HTTP_2);
						Serial.print(HTTP_END);
						Serial.println((char *)request_p->data);
					}
				}
				timeoutStart = millis();
				state = DATAOUT;
			} else if (isTargetInResp(ERROR)) {
				if (serialYes) {
					Serial.println("CIPSEND command failed");
				}
				wifiSerial.println(AT_CIPCLOSE);
				hasRequest = request_p->auto_retry;
				state = IDLE;
			} else if (millis() - timeoutStart > CIPSEND_TIMEOUT) {
				if (serialYes) {
					Serial.println("CIPSEND command timed out");
				}
				wifiSerial.println(AT_CIPCLOSE);
				hasRequest = request_p->auto_retry;
				state = IDLE;
			}
			break;
		case DATAOUT:
			if (isTargetInResp(SEND_OK)) {
				timeoutStart = millis();
				transmitCount++; // ESP8266 has successfully sent request out into the world
				state = AWAITRESPONSE;
				benchmark = millis();
			} else if (isTargetInResp(ERROR)) {
				if (serialYes) {
					Serial.println("Problem sending HTTP data");
				}
				wifiSerial.println(AT_CIPCLOSE);
				hasRequest = request_p->auto_retry;
				state = IDLE;
			} else if (millis() - timeoutStart > DATAOUT_TIMEOUT) {
				if (serialYes) {
					Serial.println("Timeout while confirming HTTP send");
				}
				wifiSerial.println(AT_CIPCLOSE);
				hasRequest = request_p->auto_retry;
				state = IDLE;
			}	
			break;
		case AWAITRESPONSE:
			if (isTargetInResp(HTML_END)) {
				benchmark = millis() - benchmark;
				Serial.println(benchmark);
				getStringFromResp(HTML_START, HTML_END, (char *)response);
				if (serialYes) {
					Serial.println("Got HTTP response!");
				}
				wifiSerial.println(AT_CIPCLOSE);
				hasRequest = false; //We're done with this request
				responseReady = true;
				receiveCount++;	// ESP8266 has successfully received a response from the web
				state = IDLE;
			} else if (millis() - timeoutStart > HTTP_TIMEOUT) {
				if (serialYes) {
					Serial.println("HTTP timeout");
				}
				wifiSerial.println(AT_CIPCLOSE);
				hasRequest = request_p->auto_retry;
				state = IDLE;
			}
			break;
	}
}

// Returns true if and only if target is in inputBuffer
bool ESP8266::isTargetInResp(const char *target) {
	loadRx();
	return (strstr((char *)inputBuffer, target) != NULL);
}

// Looks for target in inputBuffer.  If target is found, loads all 
// preceding characters (including the target itself) into result array and 
// returns true, otherwise returns false.
bool ESP8266::getStringFromResp(const char *target, char *result) {
	loadRx();
	char *loc = strstr((char *)inputBuffer, target);
	if (loc != NULL) {
		int numChars = loc - inputBuffer + strlen(target);
		strncpy(result, (char *)inputBuffer, numChars);
		result[numChars] = '\0'; //Make sure we null terminate the result	
		return true;
	} else {
		return false;
	}
}

// Looks for start and end targets in inputBuffer.  If both are found, and 
// the start target is before the end target, this method loads the characters
// in between (including both targets) into result array and returns true.
// Otherwise, returns false. 
bool ESP8266::getStringFromResp(const char*startTarget, const char*endTarget,
		char *result) {
	loadRx();
	char *startLoc = strstr((char *)inputBuffer, startTarget);
	char *endLoc = strstr((char *)inputBuffer, endTarget);
	if (startLoc != NULL && endLoc != NULL && startLoc < endLoc) {
		int numChars = endLoc - startLoc + strlen(endTarget);
		strncpy(result, startLoc, numChars);
		result[numChars] = '\0';  //Make sure we null terminate the result
		return true;
	} else {
		return false;
	}
}

// Looks for target in inputBuffer.  If target is found, we clear inputBuffer
// through and including that target and return true.  Otherwise, return false
/*bool ESP8266::clearStringFromResp(const char *target) {
	loadRx();
	char *loc = strstr((char *)inputBuffer, target);
	if (loc != NULL) {
		int numChars = loc - inputBuffer + strlen(target); //num chars to cut
		int newLen = strlen((char *)inputBuffer) - numChars;
		for (int i = 0; i < newLen; i++) {
			inputBuffer[i] = loc[i];
		}
		inputBuffer[newLen] = '\0';
		return true;
	} else {
		return false;
	}
}*/

// Looks for a valid response to CIPSTATUS and returns the integer status
// If an integer status can't be parsed from result, returns -1
int ESP8266::getStatusFromResp() {
	loadRx();
	char result[BUFFERSIZE] = {0};
	bool success = getStringFromResp(OK, result);
	if (success) {
		char *loc = strstr((char *)inputBuffer, STATUS); //Find "STATUS:"
		if (loc != NULL) {
			loc += strlen(STATUS);
			char c = loc[0]; //If next character is digit, return that number
			if (c >= '0' && c <= '9') {
				return c - '0';
			}
		}
	}
	return -1; //Could not find valid status int in inputBuffer
}

// Load wifi serial buffer into character array (inputBuffer)
void ESP8266::loadRx() {
	int buffIndex = strlen((char *)inputBuffer);
	while (wifiSerial.available() > 0 && buffIndex < BUFFERSIZE-1) {
		char c = wifiSerial.read();
		if (serialYes) {
			Serial.print(c);
		}
		inputBuffer[buffIndex] = c;
		inputBuffer[buffIndex+1] = '\0';
		buffIndex++;
	}
	if (buffIndex >= BUFFERSIZE -1) {
		if (serialYes) {
			Serial.println("WARNING: inputBuffer is full");
		}
	}
}

void ESP8266::emptyRxAndBuffer() {
	emptyRx();
	inputBuffer[0] = '\0';
}

