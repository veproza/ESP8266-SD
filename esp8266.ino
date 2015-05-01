#include <SoftwareSerial.h>
SoftwareSerial dbgSerial(6, 7); // RX, TX

class ESP
{
public:
	ESP(int rstPin);
	void init(Stream *serial);
	bool waitFor(String expected);
	bool waitFor(String expected, int timeout);
	bool startServer(int port);
	bool sendData(char data[], unsigned int len);
	bool closeConnection();
	bool closeConnection(char connectionId);
	void drain();
private:
	void reset();
	int _rstPin;
	char currentConnectionId;
	char ipdString[5];
	uint8_t ipdPosition;
	void checkIpd(char currentByte);
	Stream *_serial;
};

ESP::ESP(int rstPin)
{
	_rstPin = rstPin;
	currentConnectionId = '0';
	ipdString[0] = '+';
	ipdString[1] = 'I';
	ipdString[2] = 'P';
	ipdString[3] = 'D';
	ipdString[4] = ',';
}

void ESP::init(Stream *serial)
{
	_serial = serial;
	pinMode(_rstPin, OUTPUT);
	reset();
	
}
void ESP::reset()
{
	digitalWrite(_rstPin, LOW);
	delay(100);
	digitalWrite(_rstPin, HIGH);
	waitFor("ready", 50000);

}
bool ESP::startServer(int port)
{
	_serial->println("AT+CIPMUX=1");
	if (false == waitFor("OK")) {
		return false;
	}
	_serial->print("AT+CIPSERVER=1,");
	_serial->println(port);
	if (false == waitFor("OK")) {
		return false;
	}
	drain();
	return true;
}

bool ESP::sendData(char data[], unsigned int len)
{
	_serial->print("AT+CIPSEND=");
	_serial->print(currentConnectionId);
	_serial->print(',');
	_serial->println(len);
	waitFor(">");
	_serial->println(data);
	waitFor("SEND OK");
}
bool ESP::closeConnection()
{
	return closeConnection(currentConnectionId);
}
bool ESP::closeConnection(char connectionId)
{
	dbgSerial.print("Cl: ");
	dbgSerial.println(connectionId);
	_serial->print("AT+CIPCLOSE=");
	_serial->println(connectionId);
	return true;
	//return waitFor("Unlink");
}
void ESP::drain()
{
	while(_serial->available()) {
		_serial->read();
	}
}
bool ESP::waitFor(String expected)
{
	waitFor(expected, 100000);
}
bool ESP::waitFor(String expected, int timeout)
{
	int t0 = millis();
	int position = 0;
	int length = expected.length();
	char currentByte;
	char expectedText[127];
	expected.toCharArray(expectedText,length + 1);	
	do {
		if(_serial->available()) {
			currentByte = _serial->read();
			checkIpd(currentByte);
			if(currentByte == expectedText[position]) {
				position++;
				if(position == length) {
					return true;
				}
			} else {
				position = 0;
			}
		}
	} while (millis() - t0 < timeout);
	return false;
}

void ESP::checkIpd(char currentByte)
{
	if(ipdPosition == 5) { // currentByte is now connection ID
		if(currentByte != '0') {
			closeConnection(currentByte);
		}
		ipdPosition = 0;
	}
	if(currentByte == ipdString[ipdPosition]) {
		ipdPosition++;
	} else {
		ipdPosition = 0;
	}
}


ESP esp(12); // DIO to RST
void setup()
{
	dbgSerial.begin(115200);
	pinMode(13, OUTPUT);
	Serial.begin(115200);
	dbgSerial.println("Init");
	esp.init(&Serial);
	if(esp.startServer(80)) {
		dbgSerial.println("Ready.");
	};
	
}

void loop()
{
	while(Serial.available()) {
		bool result = esp.waitFor("GET ", 200);
		if(result) {
			char address[64];
			if(getRequestPath(address, 64))
			{
				dbgSerial.println(address);
				handleAddress(address);
			}
		}
	}
	delay(500);
}
void handleAddress(char address[])
{
	if(0 == strcmp(address, "/status")) {
		dbgSerial.println("STATUS");
		char data[] = "HTTP/1.0 200 OK\n\nprdel";
		esp.sendData(data, sizeof(data));
		esp.closeConnection();
	} else {
		dbgSerial.println("Unknown");
		char data[] = "HTTP/1.0 404\n\n";
		esp.sendData(data, sizeof(data));
		esp.closeConnection();
	}
}
bool getRequestPath(char data[], uint8_t len) 
{
	char current;
	int position = 0;
	unsigned long t0 = millis();
	while(millis() - t0 < 200) {
		while(Serial.available()) {
			current = Serial.read();
			if(current == ' ') {
				data[position] = '\0';
				return true;
			}
			if(position == len) {
				return false;
			}
			data[position] = current;
			position++;
		}
	}
	return false;
}
