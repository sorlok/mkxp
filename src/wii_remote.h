#ifndef WII_REMOTE_H
#define WII_REMOTE_H


#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <deque>
#include <memory>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/l2cap.h>

namespace {
	const unsigned int MAX_EVENT_LEGNTH = 32;
}


//Helper class: represents button presses.
struct Buttons {
	Buttons() : dpadLeft(false), dpadRight(false), dpadUp(false), dpadDown(false),
	            btnA(false), btnB(false), plus(false), minus(false), home(false), one(false), two(false) 
	            {}

	bool dpadLeft;
	bool dpadRight;
	bool dpadUp;
	bool dpadDown;
	bool btnA;
	bool btnB;
	bool plus;
	bool minus;
	bool home;
	bool one;
	bool two;
};


//Helper class: represent a request for data (from the EEPROM)
struct DataRequest {
	DataRequest() : buffer(nullptr), addr(0), length(0) {}
	unsigned char* buffer;  //Stores data when it is read. Must be freed by receiver.
	unsigned int addr;      //What address to start reading from in EEPROM.
	unsigned short length;  //How many bytes to read.
	//unsigned short wait;    //TODO: How many bytes are left to read.
};


//Helper class: represents a Wii Remote
struct WiiRemote {
	WiiRemote(bdaddr_t bdaddr) : bdaddr(bdaddr), sock(-1), ctrl_sock(-1), handshake_calib(false), waitingOnData(false) {}
	bdaddr_t bdaddr;  //Bluetooth address; i.e.,: 2C:10:C1:C2:D6:5E
	int sock;         //Data channel ("in_socket")
	int ctrl_sock;    //Control channel ("out_socket") (needed?)

	//Various handshake flags. If false, a handshake has not occurred for this element.
	bool handshake_calib;  //Calibration constants handshake.

	//List of pending requests. Front = oldest request.
	std::deque<DataRequest> requests;
	bool waitingOnData;  //If false, we can send out a new request.

	unsigned char event_buf[MAX_EVENT_LEGNTH]; //Stores the currently-read data.
};


//This class wraps all the functionality required to read sensor data from a Wii Remote.
class WiiRemoteMgr {
public:
	WiiRemoteMgr() : running(true), currStepCount(0) {
		//Start up the main thread.
		main_thread = std::thread(&WiiRemoteMgr::run_main, this);
	}

	void shutdown() {
		std::cout <<"WiiRemote: Shutdown start.\n";
		running = false;
		main_thread.join();
		std::cout <<"WiiRemote: Shutdown complete.\n";
	}



protected:
	void run_main() {
		std::cout <<"WiiRemote: Connect start.\n";
		if (!connect()) {
			return;
		}

		std::cout <<"WiiRemote: At start of main loop.\n";
		while (running.load()) {
			for (std::shared_ptr<WiiRemote>& remote : remotes) {
				sendPending(*remote);
			}
			poll();

			//Relinquish control (next Wii input comes in after 10ms)
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		disconnect();
	}


private:
	bool connect() {
		//First step: Scan for Wii Remotes.
		if (!scan_remotes()) {
			return false;
		}


		//Second step: connect to each one.
		for (std::shared_ptr<WiiRemote>& remote : remotes) {
			if (!btconnect(*remote)) {
				return false;
			}
			if (!handshake_calib(*remote)) {
				return false;
			}

			//TODO: We eventually want to turn on the Motion Plus.
		}

		//Third step: handshake each one (todo: separate out later)
		//TODO: Do we also want to set the initial reporting mode later?
		//TODO: And get status
		//TODO: And set initial LEDs
		//TODO: And turn on the Motion+ Extension (just in case we need it later).


		//Done.
		return true;
	}


	bool scan_remotes() {
		//Get the address of the first Bluetooth adapter.
		//bdaddr_t my_addr;
		//int device_id = hci_get_route(&my_addr);
		//if (device_id < 0) { throw "BAD"; }

		//The above code won't work for some reason, so we hard-code my adapter.
		//bdaddr_t my_addr;
		//str2ba("5C:F3:70:61:BF:DE", &my_addr);
		int device_id = 0; //Note: I think this actually picks the right ID.
		int device_sock = hci_open_dev(device_id);
		if (device_sock < 0) {
			std::cout <<"Error: couldn't find a Bluetooth adapter.\n";
			return false;
		}

		//Set up an inquiry for X devices.
		const size_t max_infos = 128;
		inquiry_info info[max_infos];
		inquiry_info* info_p = &info[0];
		memset(&info[0], 0, max_infos*sizeof(inquiry_info));

		//Make an inquiry for X devices
		const int timeout = 5; //5*1.28 seconds
		int num_devices = hci_inquiry(device_id, timeout, max_infos, NULL, &info_p, IREQ_CACHE_FLUSH);
		std::cout <<"Found " <<num_devices <<" candidate devices\n";

		//Now check if these are actually Wii Remotes.
		for (int i=0; i<num_devices; ++i) {
			//Check the device IDs.
			if ((info[i].dev_class[0] == 0x08) && (info[i].dev_class[1] == 0x05) && (info[i].dev_class[2] == 0x00)) {
				//Found
				std::shared_ptr<WiiRemote> remote(new WiiRemote(info[i].bdaddr));
				remotes.push_back(remote);


				char bdaddr_str[18]; //TODO: Make safer.
				ba2str(&remote->bdaddr, bdaddr_str);
				std::cout <<"Found Wii remote: " <<bdaddr_str <<"\n";
			}
		}

		//We are done with the socket, even if we don't have enough remotes.
		close(device_sock);

		//Enough remotes?
		if (remotes.size() != 2) {
			std::cout <<"Error: Expected 2 Wii Remotes; found " <<remotes.size() <<"\n";
			//return false; //TODO: Later.
		}

		//Good.
		return true;
	}


	bool btconnect(WiiRemote& remote) {
		std::cout <<"Connecting to a Wii Remote.\n";

		//Prepare a new L2cap connection.
		sockaddr_l2 addr;
		memset(&addr, 0, sizeof(addr));

		//Set up Bluetooth address and family.
		addr.l2_bdaddr = remote.bdaddr;
		addr.l2_family = AF_BLUETOOTH;

		//We only need what's considered the "input" channel (the data channel)
		remote.sock = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
		if (remote.sock == -1) {
			std::cout <<"Error: could not create input socket.\n";
			return false;
		}

		//NOTE: It seems like we need the control channel after all? (Answer: yes)
		remote.ctrl_sock = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
		if (remote.ctrl_sock == -1) {
			std::cerr <<"Error: ABC\n";
			return false;
		}
		addr.l2_psm = htobs(0x11);
		if (::connect(remote.ctrl_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
			std::cerr <<"Error: DEF\n";
			return false;
		}
		//END NOTE

		//Connect on the correct magic number.
		addr.l2_psm = htobs(0x13);

		//Actually connect.
		if (::connect(remote.sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
			std::cout <<"Error: connect() interrupt socket\n";
			return false;
		}

		//Success.
		std::cout <<"Initial connection to Wii Remote succeeded.\n";
		return true;
	}


	bool handshake_calib(WiiRemote& remote) {
		std::cout <<"Starting handshake (calibration) for Wii Remote\n";

		//We start with an LED change request.
		send_leds(remote, true, true, true, true);

		//Then set a good starting mode for data reporting.
		send_data_reporting(remote, true, 0x30);

		//Send a request for data.
		send_status_request(remote);

		//Maybe do this first?
		send_data_reporting(remote, true, 0x35);

		//Send request for calibration data.
		unsigned char* buf = (unsigned char*)malloc(sizeof(unsigned char) * 8); //TODO: leaks?

		//TODO: No callback needed; we can respond to this in the polling loop.
		read_data(remote, buf, 0x16, 7);
		return true;
	}


	void read_data(WiiRemote& remote, unsigned char* buffer, unsigned int addr, unsigned short length) {
		//Add it to the queue of requests.
		DataRequest req;
		req.buffer = buffer;
		req.addr = addr;
		req.length = length;
		remote.requests.push_back(req);
	}


	void sendPending(WiiRemote& remote) {
		//Send out pending data?
		if (!remote.waitingOnData && !remote.requests.empty()) {
			//Get the next request.
			DataRequest req = remote.requests.front();
			remote.requests.pop_front();
			remote.waitingOnData = true;

			//Prepare a message.
			unsigned char buf[6] = {0};

			//Big-Endian offset:
			*(unsigned int*)(buf) = htonl(req.addr);

			//Length is Big-Endian:
			*(unsigned short*)(buf + 4) = htons(req.length);

			std::cout <<"Requesting a read at address: " <<req.addr <<" of size: " <<req.length <<"\n";
			send_to_wii_remote(remote, 0x17, buf, 6);
		}
	}

	void send_status_request(WiiRemote& remote) {
		unsigned char buff = 0;
		send_to_wii_remote(remote, 0x15, &buff, 1);
	}

	void send_leds(WiiRemote& remote, bool L1, bool L2, bool L3, bool L4) {
		unsigned char buff = 0;
		if (L1) {
			buff |= 0x10;
		}
		if (L2) {
			buff |= 0x20;
		}
		if (L3) {
			buff |= 0x40;
		}
		if (L4) {
			buff |= 0x80;
		}

		send_to_wii_remote(remote, 0x11, &buff, 1);
	}

	void send_data_reporting(WiiRemote& remote, bool continuous, unsigned char mode) {
		unsigned char buff[] = {0,0};
		if (continuous) {
			buff[0] |= 0x4;
		}
		buff[1] = mode;

		send_to_wii_remote(remote, 0x12, buff, 2);
	}

	void poll() {
		//select() should block for at most 1/2000s
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 500;

		//Create a set of file descriptors to check for.
		fd_set fds;
		FD_ZERO(&fds);

		//Find the highest FD to check. select() will scan from 0..max_fd-1
		int max_fd = -1;
		for (std::shared_ptr<WiiRemote>& remote : remotes) {
			FD_SET(remote->sock, &fds);
			if (remote->sock > max_fd) {
				max_fd = remote->sock;
			}
		}

		//Nothing to poll?
		if (max_fd == -1) {
			return;
		}

		//select() and check for errors.
		if (select(max_fd+1, &fds, NULL, NULL, &tv) < 0) {
			std::cout <<"Error: Unable to select() the wiimote interrupt socket(s).\n";
			running = false;
			return ;
		}

		//Check each socket for an event.
		for (std::shared_ptr<WiiRemote>& remote : remotes) {
			if (FD_ISSET(remote->sock, &fds)) {
				//Read the pending message.
				memset(remote->event_buf, 0, MAX_EVENT_LEGNTH);
				int r = ::read(remote->sock, remote->event_buf, MAX_EVENT_LEGNTH);

				//Error reading?
				if (r < 0) {
					std::cout <<"Error receiving Wii Remote data\n";
					running = false;
					return;
				}

				//If select() returned true, but read() returns nothing, the remote was disconnected.
				if (r == 0) {
					std::cout <<"Wii Remote disconnected.\n";
					running = false;
					return;
				}

				std::cout <<"READ: " <<r <<" : ";
				for (int i=0; i<r; i++) {
					printf(",0x%x", remote->event_buf[i]);
				}
				std::cout <<"\n";

				//Now deal with it.
				handle_event(*remote, remote->event_buf[1], remote->event_buf+2);
			}
		}
	}


	//There are lots of different event types.
	////////////////////////
	//TODO: We should check that we actually read the correct amount of data.
	////////////////////////
	void handle_event(WiiRemote& remote, unsigned char eventType, unsigned char* message) {
		switch (eventType) {
			//Status response
			case 0x20: {
				//Always process buttons.
				proccess_buttons(message);

				//Now read the status information.
				process_status_response(remote, messagee+2);
				break;
			}


			//Memory/Register data reporting.
			case 0x21: {
				//We must process button data here, as regular input reporting is suspended during an EEPROM read.
				proccess_buttons(message);

				//Now handle the rest of the data.
				process_read_data(remote, message+2);
				break;
			}


			//Basic buttons only.
			case 0x30: {
				proccess_buttons(message);
				break;
			}

			//Basic buttons plus accelerometer plus 16 extension bytes.
			case 0x35: {
				proccess_buttons(message);
				proccess_accellerometer(message+2);
				break;
			}

			//Unknown: might as well crash.
			default: {
				std::cout <<"ERROR: Unknown Wii Remote event: " <<((int)eventType) <<"\n";
				running = false;
				return;
			}
		}
	}


	void proccess_buttons(unsigned char* btns) {
		//Save the old button state.
		prevButtons = currButtons;

		//First byte.
		currButtons.dpadLeft  = (btns[0]&0x01);
		currButtons.dpadRight = (btns[0]&0x02);
		currButtons.dpadDown  = (btns[0]&0x04);
		currButtons.dpadUp    = (btns[0]&0x08);
		currButtons.plus      = (btns[0]&0x10);

		//Second byte
		currButtons.two   = (btns[0]&0x01);
		currButtons.one   = (btns[0]&0x02);
		currButtons.btnB  = (btns[0]&0x04);
		currButtons.btnA  = (btns[0]&0x08);
		currButtons.minus = (btns[0]&0x10);
		currButtons.home  = (btns[0]&0x80);

		//TODO: We need to fire off Gamepad button presses here.
		//TODO: ...and we need to make sure we don't lose buttons.
	}

	//Process accelerometer bytes.
	void proccess_accellerometer(WiiRemote& remote, unsigned char* data) {
		//Retrieve.
		unsigned char accX = data[0];
		unsigned char accY = data[1];
		unsigned char accZ = data[2];

		//Do pedometer calculation.
		throw "TODO: Pull algorithm from desktop and cleanup.";
		currStepCount += 0;
	}


	void process_status_response(WiiRemote& remote, unsigned char* data) {
		//LED state (0xF0)>>4
		//Don't care.

		//Battery is flat.
		if (data[0] & 0x01) {
			std::cout <<"Error, battery is flat.\n";
			running = false;
			return;
		}

		//Extension connected.
		bool extensionConnected = data[0]&0x02;

		//Speaker status (0x04) 
		//Don't care.

		//IR camera enabled (0x08) 
		//Don't care.
	}


	void process_read_data(WiiRemote& remote, unsigned char* btns) {
		//Get the error state
		int err = btns[0]&0xF;
		if (err != 0) {
			std::cout <<"Error while reading from EEPROM: " <<err <<"\n";
			running = false;
			return;
		}

		//Get the size of the data read.
		unsigned short size = ((btns[0]&0xF0)>>4) + 1;

		//We are not currently robust for segmented messages.
		if (size >= 16) {
			std::cout <<"Error: this library doesn't support segmented data reading\n";
			running = false;
			return;
		}

		//Get the memory offset being read.
		unsigned short memOffset = ntohs(*(uint16_t*)(btns+1));

		//Now dispatch based on the handshake we are currently waiting for.
		if (!remote.handshake_calib) {
			finish_handshake_calib(remote, memOffset, size, btns+3);
			remote.handshake_calib = true;
		} else {
			std::cout <<"Error: received data when not waiting for a handshake.\n";
			running = false;
			return;
		}

		//Either way, we're done.
		remote.waitingOnData = false;
	}


	void finish_handshake_calib(WiiRemote& remote, unsigned short memOffset, unsigned short size, unsigned char* memory) {
		//Sanity check.
		if (memOffset != 0x16) {
			std::cout <<"Error: handshake calibration on unexpected memory address: " <<memOffset <<"\n";
			running = false;
			return;
		}

		//Sanity check 2.
		if (size != 7) {
			std::cout <<"Error: handshake calibration on unexpected size: " <<size <<"\n";
			running = false;
			return;
		}

		//Seems like we're good; read the calibration data.
		//TODO: We don't actually care about the calibration data.
		std::cout <<"Initial (calibration) handshake done.\n";
	}



	//TODO: This should actually be more like "make it a message".
	size_t send_to_wii_remote(WiiRemote& remote, unsigned char reportType, unsigned char* message, int length) {
		//Prepare a new-style message (the old method won't work on new remotes.)
		unsigned char buffer[32];
		buffer[0] = 0xa2;
		buffer[1] = reportType;
		memcpy(buffer+2, message, length);

		//TEMP
		std::cout <<"sending: [";
		for (size_t i=0; i<length+2; i++) {
			if (i!=0) {std::cout <<",";}
			printf("0x%x", buffer[i]);
		}
		std::cout <<"]\n";
		//END_TEMP

		//Make sure to disable rumble (really!)
		buffer[2] &= 0xFE;
		return ::write(remote.sock, buffer, length+2);
	}


	void disconnect() {
		std::cout <<"Disconnecting all Wii Remotes.\n";
		for (std::shared_ptr<WiiRemote>& remote : remotes) {
			if (remote->sock != -1) {
				close(remote->sock);
				remote->sock = -1;
			}
			if (remote->ctrl_sock != -1) {
				close(remote->ctrl_sock);
				remote->ctrl_sock = -1;
			}
		}
		remotes.clear();
		std::cout <<"All Wii Remotes disconnected.\n";
	}

private:
	//The current and previous set of button presses.
	Buttons prevButtons;
	Buttons currButtons;

	//Current step count (since last check).
	int currStepCount;

	//Holds the main loop
	std::thread main_thread;

	//Holds our set of Wii Remotes.
	std::vector<std::shared_ptr<WiiRemote>> remotes;

	//Used to flag the main loop to stop.
	std::atomic<bool> running;
};























#endif // WII_REMOTE_H