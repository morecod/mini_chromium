#include "crbase/at_exit.h"
#include "crbase/message_loop/message_loop.h"
#include "crbase/run_loop.h"
#include "crbase/logging.h"

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

}  // namespace

class GameServer : public crnet::StreamServer::Delegate {
 public:
  GameServer() = default;
  virtual ~GameServer() = default;

  void SetUp() ;

 protected:
  void OnConnect(int connection_id) override;
  int OnRecvData(int connection_id, const char* data, int data_len) override;
  void OnClose(int connection_id) override;

 private:
  std::unique_ptr<crnet::StreamServer> server_;
};

void GameServer::SetUp() {
  std::unique_ptr<crnet::TCPServerSocket> server_socket(
      new crnet::TCPServerSocket());
  server_socket->ListenWithAddressAndPort("127.0.0.1", 8090, 1);
  server_.reset(new crnet::StreamServer(std::move(server_socket), this));

  crnet::IPEndPoint ip;
  server_->GetLocalAddress(&ip);
  CR_LOG(INFO) << "ServerAddress:" << ip.ToString();
}

void GameServer::OnConnect(int connection_id) {
  CR_LOG(INFO) << "NewConnection: id=" << connection_id;
}

int GameServer::OnRecvData(int connection_id, const char* data, int data_len) {
  std::string msg;
  msg.assign(data, data_len);
  CR_LOG(INFO) << "GotMessage[" << connection_id << "]:" << msg;
  return data_len;
}

void GameServer::OnClose(int connection_id) {
  CR_LOG(INFO) << "ConnectionClosed: id=" << connection_id;
}

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv) {
  ::DefWindowProc(NULL, 0, 0, 0);

  InitLogging();

  crbase::AtExitManager at_exit_manager;
  crbase::MessageLoop message_loop(crbase::MessageLoop::TYPE_IO);
  

  GameServer server;
  server.SetUp();

  crbase::RunLoop run_loop;
  run_loop.Run();
  return 0;
}
