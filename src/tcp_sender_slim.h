#ifndef TCP_SENDER_SLIM_H
#define TCP_SENDER_SLIM_H

#include <atomic>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <cstdio>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>


// Simple class for sending one-way TCP messages.
class TcpSenderSlim {
public:
  TcpSenderSlim() {
  	//Defaults
    running = false;
    host = "localhost";
    port = 16834;

    //Must differ from host/port so that we reconfigure the first time.
    sockHost = "";
    sockPort = 0;

    //Must be -1 to avoid error on close.
    sock = -1;

    //Return code; sometimes useful.
    mainStatus = -1;
  }

  ~TcpSenderSlim() {
  	bool doWait = false;
  	{
      std::unique_lock<std::mutex> lock(mutex);
      if (this->running) {
        this->running = false;
        doWait = true;
      }
    }

    if (doWait) {
      condition.notify_one();
      mainThread.join();
    }
  }

  //Non-threaded: initialize the server
  int init(std::string host, uint16_t port) {
  	{
  	  //Localhost
  	  if (host=="localhost") { host = "127.0.0.1"; }

      //Reset props, start thread if required.
      std::unique_lock<std::mutex> lock(mutex);
      if ((this->host != host) || (this->port != port)) {
        this->messages.clear(); //Destined for old receiver.
        this->host = host;
        this->port = port;
      }
      if (!this->running) {
        mainThread = std::thread(&TcpSenderSlim::run, this);
        this->running = true;
        return mainStatus.load();
      }
    }

    condition.notify_one();
    return mainStatus.load();
  }

  //Non-threaded: send a message and return without waiting for a reply
  int send_only(std::string msg) {
    //Skip empty messages, and ensure they end in newlines.
    if (msg.empty()) { return 0; }
    if (msg[msg.length()-1] != '\n') { msg += "\n"; }

    //Add to message queue.
    {
      std::unique_lock<std::mutex> lock(mutex);
      messages.push_back(msg);
    }

    condition.notify_one();
    return mainStatus.load();
  }

  //Main (threaded) loop.
  void run() {
    for (;;) {
      //Get a copy of the params, exit early if done.
      std::string currHost;
      int16_t currPort;
      std::vector<std::string> currMessages;
      {
        std::unique_lock<std::mutex> lock(mutex);
        if (!running) { return; }
        currHost = host;
        currPort = port;
        currMessages = messages;
        messages.clear();
      }

      //Re-connect, if needed.
      if ((sockHost!=currHost) || (sockPort!=currPort)) {
        //Update
        sockHost = currHost;
        sockPort = currPort;

        //Close.
        if (sock != -1) {
          ::close(sock);
        }

        //New socket.
        sock = ::socket(AF_INET , SOCK_STREAM , 0);
        if (sock == -1) {
          mainStatus = 2;
          return;
        }

        //Socket props.
        server = sockaddr_in();
        server.sin_addr.s_addr = inet_addr(sockHost.c_str());
        server.sin_family = AF_INET;
        server.sin_port = htons(sockPort);

        //Connect.
        if (::connect(sock, reinterpret_cast<sockaddr*>(&server), sizeof(server)) < 0) {
          mainStatus = 3;
          return;
        }

        mainStatus = 0;
      }

      //Send pending messages 
      for (const std::string& msg : currMessages) {
        if(::send(sock, msg.c_str(), msg.length(), 0) < 0) {
          mainStatus = 4;
          return;
        }
      }

      //Wait for notification of activity (which we will verify at the top of the loop).
      {
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock, [this] { 
          if (!running) { return true; }
          if ((sockHost!=host) || (sockPort!=port)) { return true; }
          if (!messages.empty()) { return true; }
          return false;
        } );
      }
    }

    //Done
    if (sock != -1) {
      ::close(sock);
    }
  }


private:
  //Current socket config. NOT thread-safe.
  std::string sockHost;
  int16_t sockPort;
  int sock;
  sockaddr_in server;

  //Exit early if not running.
  bool running;

  //Connection settings.
  std::string host;
  int16_t port;

  //Messages waiting to be sent.
  std::vector<std::string> messages;

  //Main thread.
  std::thread mainThread;

  //Notifier.
  std::mutex mutex;
  std::condition_variable condition;

  //Return value; status code.
  std::atomic<int> mainStatus;


};




#endif //TCP_SENDER_SLIM_H
