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
};


//This class wraps all the functionality required to read sensor data from a Wii Remote.
class WiiRemoteMgr {
public:
	WiiRemoteMgr() : running(true) {
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
				poll(*remote);
			}

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


	void poll(WiiRemote& remote) {
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


		//TODO: Need to respond to poll data here. Also need to handle the second half of the handshake, and other things (like report types).

	}


	//TODO: This should actually be more like "make it a message".
	size_t send_to_wii_remote(WiiRemote& remote, unsigned char reportType, unsigned char* message, int length) {
		//Prepare a new-style message (the old method won't work on new remotes.)
		unsigned char buffer[32];
		buffer[0] = 0xa2;
		buffer[1] = reportType;
		memcpy(buffer+2, message, length);

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
	//Holds the main loop
	std::thread main_thread;

	//Holds our set of Wii Remotes.
	std::vector<std::shared_ptr<WiiRemote>> remotes;

	//Used to flag the main loop to stop.
	std::atomic<bool> running;
};























#endif // WII_REMOTE_H