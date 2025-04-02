#include "crbase/at_exit.h"
#include "crbase/memory/ref_counted.h"
#include "crbase/memory/weak_ptr.h"
#include "crbase/message_loop/message_loop.h"
#include "crbase/run_loop.h"
#include "crbase/threading/single_thread_task_runner.h"
#include "crbase/threading/thread_task_runner_handle.h"
#include "crbase/strings/string_piece.h"

#include "crnet/base/ip_endpoint.h"
#include "crnet/base/io_buffer.h"
#include "crnet/base/net_errors.h"
#include "crnet/udp/udp_server_socket.h"

#include "crbase/import_libs.cc"

////////////////////////////////////////////////////////////////////////////////

namespace {
  
void InitLogging() {
  crbase_logging::LoggingSettings settings;
  settings.logging_dest = crbase_logging::LOG_TO_STDERR;

  crbase_logging::InitLogging(settings);
}

constexpr uint32_t kReadBufferSize = 1024;
constexpr uint32_t kDefaultSocketReceiveBuffer = 64 * 1024;

class UDPSimpleServer {
 public:

  UDPSimpleServer();
  virtual ~UDPSimpleServer();

  int SetUp(const cr::StringPiece& address, uint16_t port);
  void StartReading();
  void OnReadComplete(int result);
  void Shutdown();

 private:
  // Keeps track of whether a read is currently in flight, after which
  // OnReadComplete will be called.
  bool read_pending_ = false;

  // The source address of the current read.
  crnet::IPEndPoint client_address_;

  // The address that the server listens on.
  crnet::IPEndPoint server_address_;

  // Listening socket. Also used for outbound client communication.
  std::unique_ptr<crnet::UDPServerSocket> socket_;

  // The target buffer of the current read.
  cr::scoped_refptr<crnet::IOBufferWithSize> read_buffer_;

  // The number of iterations of the read loop that have completed synchronously
  // and without posting a new task to the message loop.
  int synchronous_read_count_;

  cr::WeakPtrFactory<UDPSimpleServer> weak_factory_;
};

UDPSimpleServer::UDPSimpleServer()
    : read_pending_(false), 
      synchronous_read_count_(0),
      read_buffer_(new crnet::IOBufferWithSize(kReadBufferSize)),
      weak_factory_(this) {

}

UDPSimpleServer::~UDPSimpleServer() {

}

int UDPSimpleServer::SetUp(const cr::StringPiece& address, uint16_t port) {
  std::unique_ptr<crnet::UDPServerSocket> socket(new crnet::UDPServerSocket);
  socket->AllowAddressReuse();

  int net_err = socket->ListenWithAddressAndPort(address.as_string(), port);
  if (net_err < 0) {
    CR_LOG(ERROR) << 
        "Listen() failed: " << crnet::ErrorToString(net_err);
    return net_err;
  }

  net_err = socket->SetReceiveBufferSize(kDefaultSocketReceiveBuffer);
  if (net_err < 0) {
    CR_LOG(ERROR) << 
        "SetReceiveBufferSize() failed: " << crnet::ErrorToString(net_err);
    return net_err;
  }


  net_err = socket->SetSendBufferSize(20 * 1024);
  if (net_err < 0) {
    CR_LOG(ERROR) << 
        "SetSendBufferSize() failed: " << crnet::ErrorToString(net_err);
    return net_err;
  }

  net_err = socket->GetLocalAddress(&server_address_);
  if (net_err < 0) {
    CR_LOG(ERROR) << 
        "GetLocalAddress() failed: " << crnet::ErrorToString(net_err);
    return net_err;
  }

  CR_LOG(INFO) << "Listening on " << server_address_.ToString();

  socket_.swap(socket);
  return crnet::OK;
}

void UDPSimpleServer::StartReading() {
  if (read_pending_)
    return;

  read_pending_ = true;

  int result = socket_->RecvFrom(
      read_buffer_.get(), static_cast<int>(read_buffer_->size()), 
      &client_address_,
      cr::BindOnce(&UDPSimpleServer::OnReadComplete, cr::Unretained(this)));
  if (result == crnet::ERR_IO_PENDING) {
    synchronous_read_count_ = 0;
    return;
  }

  
  if (++synchronous_read_count_ > 32) {
    synchronous_read_count_ = 0;
    // Schedule the processing through the message loop to 1) prevent infinite
    // recursion and 2) avoid blocking the thread for too long.
    cr::ThreadTaskRunnerHandle::Get()->PostTask(
        CR_FROM_HERE, cr::BindOnce(&UDPSimpleServer::OnReadComplete,
                                   weak_factory_.GetWeakPtr(), result));
  } else {
    OnReadComplete(result);
  }
}

void UDPSimpleServer::OnReadComplete(int result) {
  read_pending_ = false;

  if (result == 0)
    result = crnet::ERR_CONNECTION_CLOSED;

  if (result < 0) {
    CR_LOG(ERROR) 
        << "UDPSimpleServer read failed: " << crnet::ErrorToString(result);
    Shutdown();
    return;
  }

  std::string msg;
  msg.assign(read_buffer_->data(), result);
  CR_LOG(INFO) << "GotUDPMessage[:" << client_address_.ToString() << "]:" 
               << msg;

  StartReading();
}

void UDPSimpleServer::Shutdown() {
  socket_->Close();
  socket_.reset();
}

}  // namespace 

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {
#if defined(MINI_CHROMIUM_OS_WIN)
  ::DefWindowProc(NULL, 0, 0, 0);
#endif

  InitLogging();

  cr::AtExitManager at_exit_manager;
  cr::MessageLoop message_loop(cr::MessageLoop::TYPE_IO);

  UDPSimpleServer server;
  if (server.SetUp("127.0.0.1", 3939) < 0)
    return -1;

  server.StartReading();

  cr::RunLoop run_loop;
  run_loop.Run();
  return 0;
}