#include "crbase/at_exit.h"
#include "crbase/message_loop/message_loop.h"
#include "crbase/run_loop.h"
#include "crbase/logging.h"

#include "crnet/base/net_errors.h"
#include "crnet/socket/tcp_server_socket.h"
#include "crnet/server/stream_connection.h"
#include "crnet/server/stream_server.h"

#include "crbase/import_libs.cc"

////////////////////////////////////////////////////////////////////////////////

namespace {

void InitLogging() {
  crbase_logging::LoggingSettings settings;
  settings.logging_dest = crbase_logging::LOG_TO_STDERR;

  crbase_logging::InitLogging(settings);
}

class TCPSimpleServer : public crnet::StreamServer::Delegate {
 public:
  TCPSimpleServer() = default;
  virtual ~TCPSimpleServer() = default;

  bool SetUp() ;

 protected:
  // crnet::StreamServer::Delegate overrides.
  void OnConnectionCreate(uint32_t connection_id) override;
  int OnConnectionData(uint32_t connection_id, 
                       const char* data, int data_len) override;
  void OnConnectionClose(uint32_t connection_id) override;

 private:
  std::unique_ptr<crnet::StreamServer> server_;
};

bool TCPSimpleServer::SetUp() {
  std::unique_ptr<crnet::TCPServerSocket> server_socket(
      new crnet::TCPServerSocket());
  int err = server_socket->ListenWithAddressAndPort("127.0.0.1", 3838, 1);
  if (err != crnet::OK) {
    CR_LOG(ERROR) << "ListenWithAddressAndPort() failed. " 
                  << crnet::ErrorToString(err);
    return false;
  }

  server_.reset(new crnet::StreamServer(std::move(server_socket), this));

  crnet::IPEndPoint ip;
  err = server_->GetLocalAddress(&ip);
  if (err != crnet::OK) {
    CR_LOG(ERROR) << "GetLocalAddress() failed. " 
                  << crnet::ErrorToString(err);
    server_.reset(nullptr);
    return false;
  }

  CR_LOG(INFO) << "Listening on " << ip.ToString();
  return true;
}

void TCPSimpleServer::OnConnectionCreate(uint32_t connection_id) {
  CR_LOG(INFO) << "Connection[" << connection_id << "] Created.";
}

int TCPSimpleServer::OnConnectionData(uint32_t connection_id, 
                                      const char* data, int data_len) {
  std::string msg;
  msg.assign(data, data_len);
  CR_LOG(INFO) << "Message[" << connection_id << "]:" << msg;
  return data_len;
}

void TCPSimpleServer::OnConnectionClose(uint32_t connection_id) {
  CR_LOG(INFO) << "Connection[" << connection_id << "] Closed.";
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv) {
#if defined(MINI_CHROMIUM_OS_WIN)
  ::DefWindowProc(NULL, 0, 0, 0);
#endif

  InitLogging();

  crbase::AtExitManager at_exit_manager;
  crbase::MessageLoop message_loop(crbase::MessageLoop::TYPE_IO);
  
  TCPSimpleServer server;
  if (!server.SetUp())
    return 1;

  crbase::RunLoop run_loop;
  run_loop.Run();
  return 0;
}
