#include "EmotiBitWiFiHost.h"

int8_t EmotiBitWiFiHost::begin()
{
	advertisingPort = EmotiBitComms::WIFI_ADVERTISING_PORT;
	vector<string> ips = getLocalIPs();
	advertisingCxn.Create();
	vector<string> ipSplit = ofSplitString(ips.at(0), ".");
	string advertisingIp = ipSplit.at(0) + "." + ipSplit.at(1) + "." + ipSplit.at(2) + "." + ofToString(255);
	cout << advertisingIp << endl;
	advertisingCxn.Connect(advertisingIp.c_str(), advertisingPort);
	advertisingCxn.SetEnableBroadcast(true);
	advertisingCxn.SetNonBlocking(true);
	advertisingCxn.SetReceiveBufferSize(pow(2, 10));

	dataPort = 3001;
	dataCxn.Create();
	while (!dataCxn.Bind(dataPort))
	{
		// Try to bind dataPort until we find one that's available
		dataPort++;
	}
	//dataCxn.SetEnableBroadcast(false);
	dataCxn.SetNonBlocking(true);
	dataCxn.SetReceiveBufferSize(pow(2, 15));

	controlPort = 11999;
	controlCxn.setMessageDelimiter(ofToString(EmotiBitPacket::PACKET_DELIMITER_CSV));
	while (!controlCxn.setup(controlPort))
	{
		// Try to setup a controlPort until we find one that's available
		controlPort++;
	}

	cout << "EmotiBit data port: " << dataPort << endl;
	cout << "EmotiBit control port: " << controlPort << endl;

	outgoingPacketCount = 0;
	connectedEmotibitIp = "";
	isConnected = false;
	isStartingConnection = false;

	return SUCCESS;
}

int8_t EmotiBitWiFiHost::processAdvertising()
{
	const int maxSize = 32768;

	// Send advertising messages periodically
	static uint64_t advertizingTimer = ofGetElapsedTimeMillis();
	if (ofGetElapsedTimeMillis() - advertizingTimer > advertisingInterval)
	{
		advertizingTimer = ofGetElapsedTimeMillis();

		advertisingCxn.Connect(advertisingIp.c_str(), advertisingPort);
		advertisingCxn.SetEnableBroadcast(true);

		// Send advertising message
		string packet = createPacket(EmotiBitPacket::TypeTag::HELLO_EMOTIBIT);
		cout << packet << endl;
		advertisingCxn.Send(packet.c_str(), packet.length());
	}


	// Receive advertising messages
	static char udpMessage[maxSize];
	int msgSize = advertisingCxn.Receive(udpMessage, maxSize);
	if (msgSize > 0)
	{
		string message = udpMessage;
		cout << "Received: " << message << endl;
		// ToDo: Handle the case where multiple packets are included in a single UDP message
		//vector<string> packets = ofSplitString(message, ofToString(EmotiBitPacket::PACKET_DELIMITER_CSV));
		vector<string> splitPacket = ofSplitString(message, ",");
		EmotiBitPacket::Header header;
		EmotiBitPacket::getHeader(splitPacket, header);
		if (header.typeTag.compare(EmotiBitPacket::TypeTag::HELLO_HOST) == 0)
		{
			if (splitPacket.size() > EmotiBitPacket::headerLength)
			{
				int32_t emotiBitPort = ofToInt(splitPacket.at(6));
				int port;
				string ip;
				advertisingCxn.GetRemoteAddr(ip, port);
				cout << "Emotibit: " << ip << ", " << port << endl;
				_emotibitIps.emplace(ip, emotiBitPort == EMOTIBIT_AVAILABLE);	// Add ip address to our list
			}
		}
	}

	// Handle connecting to EmotiBit
	if (isStartingConnection) {
		// Send connect messages periodically
		static uint64_t startCxnTimer = ofGetElapsedTimeMillis();
		if (ofGetElapsedTimeMillis() - startCxnTimer > startCxnInterval)
		{
			startCxnTimer = ofGetElapsedTimeMillis();

			// Send a connect message to the selected EmotiBit
			advertisingCxn.Connect(connectedEmotibitIp.c_str(), advertisingPort);
			advertisingCxn.SetEnableBroadcast(false);
			string packet = createPacket(EmotiBitPacket::TypeTag::EMOTIBIT_CONNECT);
			// ToDo: send data/control ports in payload
			advertisingCxn.Send(packet.c_str(), packet.length());
		}
	}

	return SUCCESS;
}

int8_t EmotiBitWiFiHost::sendControl(string& packet)
{
	controlCxnMutex.lock();
	for (unsigned int i = 0; i < (unsigned int)controlCxn.getLastID(); i++)
	{
		if (!controlCxn.isClientConnected(i)) continue;
		// get the ip and port of the client
		string port = ofToString(controlCxn.getClientPort(i));
		string ip = controlCxn.getClientIP(i);

		if (ip.compare(connectedEmotibitIp) != 0) continue;	// Confirm this is the EmotiBit IP we're connected to

		isConnected = true;
		isStartingConnection = false;

		cout << "Sending: " << packet << endl;
		controlCxn.send(i, packet);
	}
	controlCxnMutex.unlock();

	return SUCCESS;
}

// ToDo: Implement readControl()
//uint8_t EmotiBitWiFiHost::readControl(string& packet)
//{
//	if (isConnected) {
//
//		// for each connected client lets get the data being sent and lets print it to the screen
//		for (unsigned int i = 0; i < (unsigned int)controlCxn.getLastID(); i++) {
//
//			if (!controlCxn.isClientConnected(i)) continue;
//
//			// get the ip and port of the client
//			string port = ofToString(controlCxn.getClientPort(i));
//			string ip = controlCxn.getClientIP(i);
//
//			if (ip.compare(connectedEmotibitIp) != 0) continue;
//			//string info = "client " + ofToString(i) + " -connected from " + ip + " on port: " + port;
//			//cout << info << endl;
//
//			packet = "";
//			string tmp;
//			do {
//				packet += tmp;
//				tmp = controlCxn.receive(i);
//			} while (tmp != EmotiBitPacket::PACKET_DELIMITER_CSV);
//
//			// if there was a message set it to the corresponding client
//			if (str.length() > 0) {
//				cout << "Message: " << str << endl;
//			}
//
//			cout << "Sending: m" << endl;
//			messageConn.send(i, "m");
//		}
//	}
//	return SUCCESS;
//}

int8_t EmotiBitWiFiHost::readData(string& message)
{
	const int maxSize = 32768;
	static char udpMessage[maxSize];
	string ip;
	int port;
	int msgSize;
	msgSize = dataCxn.Receive(udpMessage, maxSize);
	dataCxn.GetRemoteAddr(ip, port);
	if (ip.compare(connectedEmotibitIp) == 0)
	{
		message = udpMessage;
		if (message.length() > 0)
		{
			isConnected = true;
			isStartingConnection = false;
		}
	}

	return SUCCESS;
}

int8_t EmotiBitWiFiHost::disconnect()
{
	if (isConnected)
	{
		controlCxnMutex.lock();
		string packet = createPacket(EmotiBitPacket::TypeTag::EMOTIBIT_DISCONNECT);
		for (unsigned int i = controlCxn.getLastID() - 1; i >= 0; i--) {
			string ip = controlCxn.getClientIP(i);
			if (ip.compare(connectedEmotibitIp) == 0)
			{
				controlCxn.send(i, packet);
				connectedEmotibitIp = "";
				isConnected = false;
				isStartingConnection = false;
			}
		}
		controlCxnMutex.unlock();
	}

	return SUCCESS;
}

// Connecting is done asynchronously because attempts are repeated over UDP until connected
int8_t EmotiBitWiFiHost::connect(string ip)
{
	if (!isStartingConnection && !isConnected)
	{
		emotibitIpsMutex.lock();
		try
		{
			if (ip.compare("") != 0 && _emotibitIps.at(ip))	// If the ip is on our list and available
			{
				connectedEmotibitIp = ip;
				isStartingConnection = true;
			}
		}
		catch (const std::out_of_range& oor) {
			std::cout << "EmotiBit " << ip << " not found" << endl;
		}
		emotibitIpsMutex.unlock();
	}

	return SUCCESS;
}

unordered_map<string, bool> EmotiBitWiFiHost::getEmotiBitIPs()
{
	emotibitIpsMutex.lock();
	return _emotibitIps;
	emotibitIpsMutex.unlock();
}

string EmotiBitWiFiHost::createPacket(string typeTag, string data, uint16_t dataLength, uint16_t protocolVersion, uint16_t dataReliability)
{
	// ToDo: Generalize createPacket to work across more platforms inside EmotiBitPacket
	EmotiBitPacket::Header header = EmotiBitPacket::createHeader(typeTag, ofGetElapsedTimeMillis(), outgoingPacketCount++, dataLength, protocolVersion, dataReliability);
	return EmotiBitPacket::headerToString(header) + data + EmotiBitPacket::PACKET_DELIMITER_CSV;
}

string EmotiBitWiFiHost::createPacket(string typeTag, vector<string> data, uint16_t protocolVersion, uint16_t dataReliability)
{
	// ToDo: Template data vector
	// ToDo: Generalize createPacket to work across more platforms inside EmotiBitPacket
	EmotiBitPacket::Header header = EmotiBitPacket::createHeader(typeTag, ofGetElapsedTimeMillis(), outgoingPacketCount++, data.size(), protocolVersion, dataReliability);
	string packet = EmotiBitPacket::headerToString(header);
	for (string s : data)
	{
		packet += "," + s;
	}
	packet += EmotiBitPacket::PACKET_DELIMITER_CSV;
	return packet;
}

string EmotiBitWiFiHost::ofGetTimestampString(const string& timestampFormat)
{
	std::stringstream str;
	auto now = std::chrono::system_clock::now();
	auto t = std::chrono::system_clock::to_time_t(now);    std::chrono::duration<double> s = now - std::chrono::system_clock::from_time_t(t);
	int us = s.count() * 1000000;
	auto tm = *std::localtime(&t);
	constexpr int bufsize = 256;
	char buf[bufsize];

	// Beware! an invalid timestamp string crashes windows apps.
	// so we have to filter out %i (which is not supported by vs)
	// earlier.
	auto tmpTimestampFormat = timestampFormat;
	ofStringReplace(tmpTimestampFormat, "%i", ofToString(us / 1000, 3, '0'));
	ofStringReplace(tmpTimestampFormat, "%f", ofToString(us, 6, '0'));

	if (strftime(buf, bufsize, tmpTimestampFormat.c_str(), &tm) != 0) {
		str << buf;
	}
	auto ret = str.str();


	return ret;
}

vector<string> EmotiBitWiFiHost::getLocalIPs()
{
	vector<string> result;

#ifdef TARGET_WIN32

	string commandResult = ofSystem("ipconfig");
	//ofLogVerbose() << commandResult;

	for (int pos = 0; pos >= 0; )
	{
		pos = commandResult.find("IPv4", pos);

		if (pos >= 0)
		{
			pos = commandResult.find(":", pos) + 2;
			int pos2 = commandResult.find("\n", pos);

			string ip = commandResult.substr(pos, pos2 - pos);

			pos = pos2;

			if (ip.substr(0, 3) != "127") // let's skip loopback addresses
			{
				result.push_back(ip);
				//ofLogVerbose() << ip;
			}
		}
	}

#else

	string commandResult = ofSystem("ifconfig");

	for (int pos = 0; pos >= 0; )
	{
		pos = commandResult.find("inet ", pos);

		if (pos >= 0)
		{
			int pos2 = commandResult.find("netmask", pos);

			string ip = commandResult.substr(pos + 5, pos2 - pos - 6);

			pos = pos2;

			if (ip.substr(0, 3) != "127") // let's skip loopback addresses
			{
				result.push_back(ip);
				//ofLogVerbose() << ip;
			}
		}
	}

#endif

	return result;
}