#ifndef TCP_SENDER_SLIM_H
#define TCP_SENDER_SLIM_H

#include <vector>
#include <atomic>
#include <fstream>
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
  //Get our output stream (file).
  std::ostream& get_logfile() {
    if (log_file.is_open()) {
      return log_file;
    }
    return std::cout; //Umm... better than crashing?
  }

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
    //InitializeConditionVariable (&condition);
    newDataEvent = CreateEvent ( NULL , false , false , NULL );
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
      get_logfile() << "Waiting for thread to close down..." << std::endl;
#ifdef _WIN32
      SetEvent ( newDataEvent ); //Notify
      //WakeAllConditionVariable (&condition);
      WaitForSingleObject((HANDLE)mainThread, INFINITE);
#else
      condition.notify_all();
      mainThread.join();
#endif
    }

    //CS no longer needed.
#ifdef _WIN32
    CloseHandle(newDataEvent);
    //CloseHandle( critical ); //????
#endif
  }

  void startMainThread();

  //Non-threaded: initialize the server
  int init(std::string host, uint16_t port) {
    if (!log_file.is_open()) {
      log_file.open("tcp_log.txt");
    }

    if ( newDataEvent==NULL ) {
      get_logfile() << "ERROR: Couldn't create event" << std::endl;
      mainStatus = 441;
      return mainStatus.load();
    }

    //Localhost
    if (host=="localhost") { host = "127.0.0.1"; }

    get_logfile() << "New host connection: " <<host <<":" <<port << std::endl;

    {
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
        get_logfile() << "Starting main thread..." << std::endl;
        startMainThread();
        this->running = true;
#ifdef _WIN32
      LeaveCriticalSection (&critical);
#endif
        return mainStatus.load();
      }

#ifdef _WIN32
      LeaveCriticalSection (&critical);
#endif
    }

#ifdef _WIN32
    SetEvent ( newDataEvent ); //Notify
    //WakeAllConditionVariable (&condition);
#else
    condition.notify_one();
#endif
    return mainStatus.load();
  }

  //Status update?
  int get_status() {
    return mainStatus.load();
  }

  //Non-threaded: send a message and return without waiting for a reply
  int send_only(std::string msg) {
    //Skip empty messages, and ensure they end in newlines.
    if (msg.empty()) { return 0; }
    if (msg[msg.length()-1] != '\n') { msg += "\n"; }

    get_logfile() << "Sending message: '" <<msg <<"'" << std::endl;

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
    SetEvent ( newDataEvent ); //Notify
    //WakeAllConditionVariable (&condition);
#else
    condition.notify_one();
#endif
    return mainStatus.load();
  }

  //Main (threaded) loop.
  void run() {
    bool firstTime = true;
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
        if (firstTime) {
          firstTime = false;
          get_logfile() << "Top of run loop, with running: " <<running << std::endl;
        }
        if (!running) {
#ifdef _WIN32
        LeaveCriticalSection (&critical);
#endif
          return;
        }
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
        get_logfile() << "Updating socket from: " <<sockHost <<":" <<sockPort <<" to: " <<currHost <<":" <<currPort << std::endl;

        //Update
        sockHost = currHost;
        sockPort = currPort;

        //Close.
#ifdef _WIN32
        if (sock != INVALID_SOCKET) {
          get_logfile() << "Closing old socket" << std::endl;
          closesocket(sock);
        } else {
          //More Windows grumbling
          get_logfile() << "Starting up WSAStartup" << std::endl;
          if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
            mainStatus = 666;
            return;
          }
        }
#else
        if (sock != -1) {
          get_logfile() << "Closing old socket" << std::endl;
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

        get_logfile() << "Requesting address info" << std::endl;
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
        get_logfile() << "Requesting address info" << std::endl;
        sock = ::socket(AF_INET , SOCK_STREAM , 0);
        if (sock == -1) {
          mainStatus = 2;
          return;
        }
#endif

        //Connect.
#ifdef _WIN32
        get_logfile() << "Trying a series of ptr_options" << std::endl;
        for(ptr=my_res; ptr != NULL ;ptr=ptr->ai_next) {
          get_logfile() << "...trying to make socket" << std::endl;
          sock = ::socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
          if (sock == INVALID_SOCKET) {
              WSACleanup();
              mainStatus = 667;
              return;
          }

          get_logfile() << "...trying to connect" << std::endl;
          if (::connect( sock, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR) {
              closesocket(sock);
              sock = INVALID_SOCKET;
              continue;
          }
          get_logfile() << "...good!" << std::endl;
          break;
        }

        if (sock == INVALID_SOCKET) {
          get_logfile() << "Couldn't find a decent socket" << std::endl;
          WSACleanup();
          mainStatus = 669;
          return;
        }
#else
        get_logfile() << "Trying to connect to ONE socket, because Linux is awesome." << std::endl;
        if (::connect(sock, reinterpret_cast<sockaddr*>(&server), sizeof(server)) < 0) {
          mainStatus = 3;
          return;
        }
#endif

        mainStatus = 0;
      }

      //Send pending messages 
      for (const std::string& msg : currMessages) {
        get_logfile() << "Sending pending message: '" <<msg <<"'" << std::endl;
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
        get_logfile() << "Waiting for notification." << std::endl;
#ifdef _WIN32
//        EnterCriticalSection (&critical);
#else
        std::unique_lock<std::mutex> lock(mutex);
#endif


#ifdef _WIN32
        bool resDone = false;
        while (!resDone) {
          //SleepConditionVariableCS (&condition, &critical, INFINITE);
          WaitForSingleObject ( newDataEvent, INFINITE );
#ifdef _WIN32
        EnterCriticalSection (&critical);
#endif
          if (!running) { resDone=true; }
          if ((sockHost!=host) || (sockPort!=port)) { resDone=true; }
          if (!messages.empty()) { resDone=true; }
#ifdef _WIN32
        LeaveCriticalSection (&critical);
#endif
          get_logfile() << "Woke on notify; done is: " <<resDone << std::endl;
        }
#else
        condition.wait(lock, [this] { 
          bool resDone = false;
          if (!running) { resDone=true; }
          if ((sockHost!=host) || (sockPort!=port)) { resDone=true; }
          if (!messages.empty()) { resDone=true; }
          get_logfile() << "Woke on notify; done is: " <<resDone << std::endl;
          return resDone;
        } );
#endif

#ifdef _WIN32
//        LeaveCriticalSection (&critical);
#endif
      }
    }

    //Done
#ifdef _WIN32
    if (sock != INVALID_SOCKET) {
      get_logfile() << "Closing socket" << std::endl;
      closesocket(sock);
      WSACleanup();
    }
#else
    if (sock != -1) {
      get_logfile() << "Closing socket" << std::endl;
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
  //CONDITION_VARIABLE condition;
  HANDLE newDataEvent;
#else
  std::mutex mutex;
  std::condition_variable condition;
#endif

  //Return value; status code.
  std::atomic<int> mainStatus;

  //Debug
  std::ofstream log_file;
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
