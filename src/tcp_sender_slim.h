#ifndef TCP_SENDER_SLIM_H
#define TCP_SENDER_SLIM_H

#include <vector>
#include <atomic>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#else
#include <thread>
#include <mutex>
#include <condition_variable>
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif


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
#ifdef _WIN32
    sock = INVALID_SOCKET;
    my_res = NULL;
    ptr = NULL;
#else
    sock = -1;
#endif

    //Return code; sometimes useful.
    mainStatus = -1;

    //On Windows, make our CS/condition and initialize our thread to "error".
#ifdef _WIN32
    mainThread = HANDLE();
    InitializeCriticalSection (&critical);
    InitializeConditionVariable (&condition);
#endif
  }

  ~TcpSenderSlim() {
  	bool doWait = false;
  	{
#ifdef _WIN32
      EnterCriticalSection (&critical);
#else
      std::unique_lock<std::mutex> lock(mutex);
#endif
      if (this->running) {
        this->running = false;
        doWait = true;
      }

#ifdef _WIN32
      LeaveCriticalSection (&critical);
#endif
    }

    if (doWait) {
#ifdef _WIN32
      WakeAllConditionVariable (&condition);
      WaitForSingleObject((HANDLE)mainThread, INFINITE);
#else
      condition.notify_all();
      mainThread.join();
#endif
    }

    //CS no longer needed.
#ifdef _WIN32
    //CloseHandle( critical ); //????
#endif
  }

  void startMainThread();

  //Non-threaded: initialize the server
  int init(std::string host, uint16_t port) {
  	{
  	  //Localhost
  	  if (host=="localhost") { host = "127.0.0.1"; }

      //Reset props, start thread if required.
#ifdef _WIN32
      EnterCriticalSection (&critical);
#else
      std::unique_lock<std::mutex> lock(mutex);
#endif
      if ((this->host != host) || (this->port != port)) {
        this->messages.clear(); //Destined for old receiver.
        this->host = host;
        this->port = port;
      }
      if (!this->running) {
        startMainThread();
        this->running = true;
        return mainStatus.load();
      }

#ifdef _WIN32
      LeaveCriticalSection (&critical);
#endif
    }

#ifdef _WIN32
    WakeAllConditionVariable (&condition);
#else
    condition.notify_one();
#endif
    return mainStatus.load();
  }

  //Non-threaded: send a message and return without waiting for a reply
  int send_only(std::string msg) {
    //Skip empty messages, and ensure they end in newlines.
    if (msg.empty()) { return 0; }
    if (msg[msg.length()-1] != '\n') { msg += "\n"; }

    //Add to message queue.
    {
#ifdef _WIN32
      EnterCriticalSection (&critical);
#else
      std::unique_lock<std::mutex> lock(mutex);
#endif
      messages.push_back(msg);
#ifdef _WIN32
      LeaveCriticalSection (&critical);
#endif
    }

#ifdef _WIN32
    WakeAllConditionVariable (&condition);
#else
    condition.notify_one();
#endif
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
#ifdef _WIN32
        EnterCriticalSection (&critical);
#else
        std::unique_lock<std::mutex> lock(mutex);
#endif
        if (!running) { return; }
        currHost = host;
        currPort = port;
        currMessages = messages;
        messages.clear();
#ifdef _WIN32
        LeaveCriticalSection (&critical);
#endif
      }

      //Re-connect, if needed.
      if ((sockHost!=currHost) || (sockPort!=currPort)) {
        //Update
        sockHost = currHost;
        sockPort = currPort;

        //Close.
#ifdef _WIN32
        if (sock != INVALID_SOCKET) {
          closesocket(sock);
        } else {
          //More Windows grumbling
          if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
            mainStatus = 666;
            return;
          }
        }
#else
        if (sock != -1) {
          ::close(sock);
        }
#endif

        //New socket.
#ifdef _WIN32
        //Windows needs some hand-holding.
        ZeroMemory( &hints, sizeof(hints) );
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        if (getaddrinfo(sockHost.c_str(), std::to_string(sockPort).c_str(), &hints, &my_res) != 0) {
          WSACleanup();
          mainStatus = 668;
          return;
        }
#else
        //Socket props.
        server = sockaddr_in();
        server.sin_addr.s_addr = inet_addr(sockHost.c_str());
        server.sin_family = AF_INET;
        server.sin_port = htons(sockPort);

        //Socket
        sock = ::socket(AF_INET , SOCK_STREAM , 0);
        if (sock == -1) {
          mainStatus = 2;
          return;
        }
#endif

        //Connect.
#ifdef _WIN32
        for(ptr=my_res; ptr != NULL ;ptr=ptr->ai_next) {
          sock = ::socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
          if (sock == INVALID_SOCKET) {
              WSACleanup();
              mainStatus = 667;
              return;
          }

          if (::connect( sock, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR) {
              closesocket(sock);
              sock = INVALID_SOCKET;
              continue;
          }
          break;
        }

        if (sock == INVALID_SOCKET) {
          WSACleanup();
          mainStatus = 669;
          return;
        }
#else
        if (::connect(sock, reinterpret_cast<sockaddr*>(&server), sizeof(server)) < 0) {
          mainStatus = 3;
          return;
        }
#endif

        mainStatus = 0;
      }

      //Send pending messages 
      for (const std::string& msg : currMessages) {
#ifdef _WIN32
        if(::send(sock, msg.c_str(), msg.length(), 0) == SOCKET_ERROR) {
          closesocket(sock);
          WSACleanup();
          mainStatus = 4;
          return;
        }
#else
        if(::send(sock, msg.c_str(), msg.length(), 0) < 0) {
          mainStatus = 4;
          return;
        }
#endif
      }

      //Wait for notification of activity (which we will verify at the top of the loop).
      {
#ifdef _WIN32
        EnterCriticalSection (&critical);
#else
        std::unique_lock<std::mutex> lock(mutex);
#endif


#ifdef _WIN32
        bool done = false;
        while (!done) {
          SleepConditionVariableCS (&condition, &critical, INFINITE);
          if (!running) { done=true; }
          if ((sockHost!=host) || (sockPort!=port)) { done=true; }
          if (!messages.empty()) { done=true; }
        }
#else
        condition.wait(lock, [this] { 
          if (!running) { return true; }
          if ((sockHost!=host) || (sockPort!=port)) { return true; }
          if (!messages.empty()) { return true; }
          return false;
        } );
#endif

#ifdef _WIN32
        LeaveCriticalSection (&critical);
#endif
      }
    }

    //Done
#ifdef _WIN32
    if (sock != INVALID_SOCKET) {
      closesocket(sock);
      WSACleanup();
    }
#else
    if (sock != -1) {
      ::close(sock);
    }
#endif
  }


private:
  //Current socket config. NOT thread-safe.
  std::string sockHost;
  int16_t sockPort;
#ifdef _WIN32
  addrinfo* my_res;
  addrinfo* ptr;
  addrinfo hints;
  WSADATA wsaData;

  SOCKET sock;
#else
  int sock;
  sockaddr_in server;
#endif

  //Exit early if not running.
  bool running;

  //Connection settings.
  std::string host;
  int16_t port;

  //Messages waiting to be sent.
  std::vector<std::string> messages;

  //Main thread.
#ifdef _WIN32
  HANDLE mainThread;
#else
  std::thread mainThread;
#endif

  //Notifier.
#ifdef _WIN32
  CRITICAL_SECTION critical;
  CONDITION_VARIABLE condition;
#else
  std::mutex mutex;
  std::condition_variable condition;
#endif

  //Return value; status code.
  std::atomic<int> mainStatus;


};


//Grumble, Windows
#ifdef _WIN32
  DWORD WINAPI __stdcall static_run(void* p_this) {
    TcpSenderSlim* p_self = static_cast<TcpSenderSlim*>(p_this);
    p_self->run();
  }
#endif


void TcpSenderSlim::startMainThread()
{
#ifdef _WIN32
  mainThread = CreateThread(NULL, 0, &static_run, this, 0, NULL);
#else
  mainThread = std::thread();
#endif
}




#endif //TCP_SENDER_SLIM_H
