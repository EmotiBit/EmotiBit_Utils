/// EmotiBitWifiHost
///
/// Supports WiFi communications with the EmotiBit 
///
/// Written by produceconsumerobot Dec 2019


#pragma once

#include <unordered_set>

// ToDo: Extend code to work with Android
#include "ofMain.h"
#include "ofxNetwork.h"
#include "EmotiBitPacket.h"
#include "EmotiBitComms.h"

class EmotiBitWiFiHost
{
public:
	static const uint8_t SUCCESS = 0;
	static const uint8_t FAIL = -1;
	static const int32_t EMOTIBIT_AVAILABLE = -1;
	uint16_t advertisingInterval = 1000;		// Milliseconds between sending advertising messages
	uint16_t startCxnInterval = 100;

	ofxUDPManager advertisingCxn;
	ofxUDPManager dataCxn;
	ofxTCPServer controlCxn;

	std::mutex controlCxnMutex;
	std::mutex emotibitIpsMutex;

	uint16_t advertisingPort;
	uint16_t dataPort;
	uint16_t controlPort;
	string advertisingIp;				// broadcast address
	unordered_map<string, bool> _emotibitIps;	// list of EmotiBit IP addresses
	string connectedEmotibitIp;
	bool isConnected;
	bool isStartingConnection;

	uint16_t outgoingPacketCount;

	int8_t begin();
	int8_t processAdvertising();
	int8_t connect(string ip);
	int8_t disconnect();
	int8_t sendControl(string& packet);
	//uint8_t readControl(string& packet);
	int8_t readData(string& message);
	//int8_t sendUdp(WiFiUDP& udp, const String& message, const IPAddress& ip, const uint16_t& port);
	unordered_map<string, bool> getEmotiBitIPs();	// <IP address, availability to connect>
	vector<string> getLocalIPs();
	string createPacket(string typeTag, string data = "", uint16_t dataLength = 0, uint16_t protocolVersion = 1, uint16_t dataReliability = 100);
	string createPacket(string typeTag, vector<string> data, uint16_t protocolVersion = 1, uint16_t dataReliability = 100);

	string ofGetTimestampString(const string& timestampFormat); // Adds %f for microseconds
};