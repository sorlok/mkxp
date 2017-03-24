#ifndef TCP_SENDER_SLIM_H
#define TCP_SENDER_SLIM_H

#include <vector>
#include <atomic>
#include <fstream>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <thread>
#include <mutex>
#include <condition_variable>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif


// Worker macros
#define NoOp() ((void)0);

#ifdef _WIN32
  //Private member variables of the TcpSenderSlim class.
  #define MemberVars   \
    addrinfo* my_res;  \
    addrinfo* ptr;     \
    addrinfo hints;    \
    WSADATA wsaData;   \
    SOCKET sock;       \
    HANDLE mainThread; \
    CRITICAL_SECTION critical; \
    HANDLE newDataEvent;


  //Called in TcpSenderSlim()
  #define PlatformInit()   \
    sock = INVALID_SOCKET; \
    my_res = NULL;         \
    ptr = NULL;            \
    InitializeCriticalSection (&critical); \
    newDataEvent = CreateEvent ( NULL , false , false , NULL ); \
    if (newDataEvent == NULL) { initFailed = true; }

  //Called in ~TcpSenderSlim(), but only if the thread was running.
  #define PlatformDone()   \
    SendNotifySignal(); \
    WaitForSingleObject((HANDLE)mainThread, INFINITE);

  //Cleanup inside the thread.
  #define ThreadCleanup() \
    if (sock != INVALID_SOCKET) {  \
      get_logfile() << "Closing socket" << std::endl;  \
      closesocket(sock);  \
      WSACleanup();  \
    }

  //Called in ~TcpSenderSlim() once the thread is done.
  #define PlatformClose()   \
    CloseHandle(newDataEvent);

  //Close the existing socket, if it exists.
  #define CloseOldSocket() \
    if (sock != INVALID_SOCKET) { \
      get_logfile() << "Closing old socket" << std::endl; \
      closesocket(sock); \
    } else { \
      /*More Windows grumbling*/  \
      get_logfile() << "Starting up WSAStartup" << std::endl; \
      if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) { \
        mainStatus = 666; \
        return; \
      } \
    }

  //Called to lock/unlock our mutex or enter/leave our critical section.
  #define GetLock()      EnterCriticalSection (&critical);
  #define ReleaseLock()  LeaveCriticalSection (&critical);

  //Get the lock, but only if it's needed to wait on the condition object.
  //Same for release.
  #define GetLockForCondition()      NoOp();
  #define ReleaseLockForCondition()  NoOp();

  //Get the lock, but only if it's needed to perform the check (i.e., we didn't get it for the condition)
  #define GetLockForCheck()      GetLock();
  #define ReleaseLockForCheck()  ReleaseLock();

  //Send a signal (notification) to wake up the sleeper.
  //Or, wait for that signal.
  #define SendNotifySignal()   SetEvent ( newDataEvent );
  #define WaitForSignal()      WaitForSingleObject ( newDataEvent, INFINITE );

#else //Linux

  //Private member variables of the TcpSenderSlim class.
  #define MemberVars         \
    int sock;                \
    sockaddr_in server;      \
    std::thread mainThread;  \
    std::mutex mutex;        \
    std::condition_variable condition;

  //Called in TcpSenderSlim()
  #define PlatformInit()   \
    sock = -1;

  //Called in ~TcpSenderSlim(), but only if the thread was running.
  #define PlatformDone()    \
    SendNotifySignal();     \
    mainThread.join();

  #define ThreadCleanup() \
    if (sock != -1) {  \
      get_logfile() << "Closing socket" << std::endl;  \
      ::close(sock);  \
    }

  //Called in ~TcpSenderSlim() once the thread is done.
  #define PlatformClose() NoOp();

  //Close the existing socket, if it exists.
  #define CloseOldSocket() \
    if (sock != -1) { \
      get_logfile() << "Closing old socket" << std::endl; \
      ::close(sock); \
    }

  //Called to lock/unlock our mutex or enter/leave our critical section.
  //Note that in Linux, the lock will *also* be released when it goes out of scope.
  #define GetLock()      std::unique_lock<std::mutex> lock(mutex);
  #define ReleaseLock()  lock.unlock();

  //Get the lock, but only if it's needed to wait on the condition object.
  //Same for release.
  #define GetLockForCondition()      GetLock();
  #define ReleaseLockForCondition()  ReleaseLock();

  //Get the lock, but only if it's needed to perform the check (i.e., we didn't get it for the condition)
  #define GetLockForCheck()      NoOp();
  #define ReleaseLockForCheck()  NoOp();

  //Send a signal (notification) to wake up the sleeper.
  //Or, wait for that signal.
  #define SendNotifySignal()   condition.notify_one();
  #define WaitForSignal()      condition.wait(lock, [this] { return true; });

#endif


// Simple class for sending one-way TCP messages.
// Windows-y complexity arises from us using MinGW+MXE,
// plus whatever Microsoft's excuse is.
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

    //Return code; sometimes useful.
    mainStatus = -1;

    //Are we really in a bad state?
    initFailed = false;

    //Initialize our sockets and such.
    PlatformInit();
  }

  ~TcpSenderSlim() {
    bool doWait = false;
    {
      GetLock();

      if (this->running) {
        this->running = false;
        doWait = true;
      }

      ReleaseLock();
    }

    if (doWait) {
      get_logfile() << "Waiting for thread to close down..." << std::endl;
      PlatformDone();
    }

    //Clean up resources
    PlatformClose()
  }

  void startMainThread();

  bool trySend(const std::string& msg);

  bool tryConnect();

  //Non-threaded: initialize the server
  int init(std::string host, uint16_t port) {
    if (!log_file.is_open()) {
      log_file.open("tcp_log.txt");
    }

    if ( initFailed ) {
      get_logfile() << "ERROR: Main init failed." << std::endl;
      mainStatus = 441;
      return mainStatus.load();
    }

    //Localhost
    if (host=="localhost") { host = "127.0.0.1"; }

    get_logfile() << "New host connection: " <<host <<":" <<port << std::endl;

    {
      //Reset props, start thread if required.
      GetLock();

      if ((this->host != host) || (this->port != port)) {
        this->messages.clear(); //Destined for old receiver.
        this->host = host;
        this->port = port;
      }
      if (!this->running) {
        get_logfile() << "Starting main thread..." << std::endl;
        startMainThread();
        this->running = true;

        ReleaseLock();

        return mainStatus.load();
      }

      ReleaseLock();
    }

    //Wake up our main thread.
    SendNotifySignal();

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
      GetLock();
      messages.push_back(msg);
      ReleaseLock();
    }

    //Wake up our main thread.
    SendNotifySignal();

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

        GetLock();

        if (firstTime) {
          firstTime = false;
          get_logfile() << "Top of run loop, with running: " <<running << std::endl;
        }
        if (!running) {

          ReleaseLock();

          return;
        }
        currHost = host;
        currPort = port;
        currMessages = messages;
        messages.clear();

        ReleaseLock();
      }

      //Re-connect, if needed.
      if ((sockHost!=currHost) || (sockPort!=currPort)) {
        get_logfile() << "Updating socket from: " <<sockHost <<":" <<sockPort <<" to: " <<currHost <<":" <<currPort << std::endl;

        //Update
        sockHost = currHost;
        sockPort = currPort;

        //Close.
        CloseOldSocket();

        //New socket, connect
        if (!tryConnect()) {
          return;
        }

        mainStatus = 0;
      }

      //Send pending messages 
      for (const std::string& msg : currMessages) {
        get_logfile() << "Sending pending message: '" <<msg <<"'" << std::endl;
        if (!trySend(msg)) {
          mainStatus = 4;
          return;
        }
      }

      //Wait for notification of activity (which we will verify at the top of the loop).
      {
        get_logfile() << "Waiting for notification." << std::endl;

        GetLockForCondition();

        //We could be more efficient with our condition variable on Linux, but this
        // allows us to share more code with Windows.
        bool resDone = false;
        while (!resDone) {
          WaitForSignal();

          GetLockForCheck();

          if (!running) { resDone=true; }
          if ((sockHost!=host) || (sockPort!=port)) { resDone=true; }
          if (!messages.empty()) { resDone=true; }

          ReleaseLockForCheck();

          get_logfile() << "Woke on notify; done is: " <<resDone << std::endl;
        }

        ReleaseLockForCondition();
      }
    }

    //Done
    ThreadCleanup();
  }


private:
  //Current socket config. NOT thread-safe.
  std::string sockHost;
  int16_t sockPort;

  MemberVars;

  //Detects certain errors.
  bool initFailed;

  //Exit early if not running.
  bool running;

  //Connection settings.
  std::string host;
  int16_t port;

  //Messages waiting to be sent.
  std::vector<std::string> messages;
  
  //Return value; status code.
  std::atomic<int> mainStatus;

  //Debug
  std::ofstream log_file;
};



#ifdef _WIN32

//Grumble, Windows
DWORD WINAPI __stdcall static_run(void* p_this) {
  TcpSenderSlim* p_self = static_cast<TcpSenderSlim*>(p_this);
  p_self->run();
}

void TcpSenderSlim::startMainThread()
{
  mainThread = CreateThread(NULL, 0, &static_run, this, 0, NULL);
}

bool TcpSenderSlim::trySend(const std::string& msg)
{
  if(::send(sock, msg.c_str(), msg.length(), 0) == SOCKET_ERROR) {
    closesocket(sock);
    WSACleanup();
    return false;
  }
  return true;
}

bool TcpSenderSlim::tryConnect()
{
  //Windows needs some hand-holding.
  ZeroMemory( &hints, sizeof(hints) );
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  //Get address
  get_logfile() << "Requesting address info" << std::endl;
  if (getaddrinfo(sockHost.c_str(), std::to_string(sockPort).c_str(), &hints, &my_res) != 0) {
    WSACleanup();
    mainStatus = 668;
    return false;
  }

  //New socket, connect
  get_logfile() << "Trying a series of ptr_options" << std::endl;
  for(ptr=my_res; ptr != NULL ;ptr=ptr->ai_next) {
    get_logfile() << "...trying to make socket" << std::endl;
    sock = ::socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        mainStatus = 667;
        return false;
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
    return false;
  }

  return true;
}

#else //Linux

void TcpSenderSlim::startMainThread()
{
  mainThread = std::thread(&TcpSenderSlim::run, this);
}

bool TcpSenderSlim::trySend(const std::string& msg)
{
  if (::send(sock, msg.c_str(), msg.length(), 0) < 0) {
    return false;
  }
  return true;
}

bool TcpSenderSlim::tryConnect()
{
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
    return false;
  }

  //Connect
  get_logfile() << "Trying to connect to ONE socket, because Linux is awesome." << std::endl;
  if (::connect(sock, reinterpret_cast<sockaddr*>(&server), sizeof(server)) < 0) {
    mainStatus = 3;
    return false;
  }

  return true;
}

#endif





#endif //TCP_SENDER_SLIM_H
