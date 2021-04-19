#include <pc/manager.hpp>
#include <pc/log.hpp>
#include <unistd.h>
#include <signal.h>
#include <iostream>

// pyth daemon service

using namespace pc;

std::string get_rpc_host()
{
  return "localhost";
}

std::string get_key_store()
{
  std::string dir = getenv("HOME");
  return dir + "/.pythd/";
}

int get_port()
{
  return 8910;
}

int usage()
{
  std::cerr << "usage: pythd [options]" << std::endl << std::endl;
  std::cerr << "options include:" << std::endl;
  std::cerr << "  -r <rpc_host (default " << get_rpc_host() << ")>"
            << std::endl;
  std::cerr << "  -k <key_store_directory (default " << get_key_store()
            << ")>]" << std::endl;
  std::cerr << "  -p <listening_port (default " << get_port() << ">"
            << std::endl;
  std::cerr << "  -w <web content directory>" << std::endl;
  std::cerr << "  -c <capture file>" << std::endl;
  std::cerr << "  -l <log_file>" << std::endl;
  std::cerr << "  -v <version>" << std::endl;
  std::cerr << "  -n" << std::endl;
  std::cerr << "  -d" << std::endl;
  return 1;
}

bool do_run = true;

void sig_handle( int )
{
  do_run = false;
}

void sig_toggle( int )
{
  // toggle between debug and info logging
  if ( log::has_level( PC_LOG_DBG_LVL ) ) {
    log::set_level( PC_LOG_INF_LVL );
  } else {
    log::set_level( PC_LOG_DBG_LVL );
  }
}

int main(int argc, char **argv)
{
  // command-line parsing
  std::string cnt_dir, cap_file, log_file;
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  uint32_t version = PC_VERSION;
  int pyth_port = get_port();
  int opt = 0;
  bool do_wait = true, do_debug = false;
  while( (opt = ::getopt(argc,argv, "r:p:k:w:c:l:v:dnh" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'p': pyth_port = ::atoi(optarg); break;
      case 'k': key_dir = optarg; break;
      case 'c': cap_file = optarg; break;
      case 'w': cnt_dir = optarg; break;
      case 'l': log_file = optarg; break;
      case 'v': version = ::atoi( optarg ); break;
      case 'n': do_wait = false; break;
      case 'd': do_debug = true; break;
      default: return usage();
    }
  }

  // set up logging and disable SIGPIPE
  signal( SIGPIPE, SIG_IGN );
  if ( !log_file.empty() && !log::set_log_file( log_file ) ) {
    std::cerr << "pythd: failed to create log_file="
              << log_file << std::endl;
    return 1;
  }
  log::set_level( do_debug ? PC_LOG_DBG_LVL : PC_LOG_INF_LVL );

  // construct and initialize pyth-client manager
  manager mgr;
  mgr.set_dir( key_dir );
  mgr.set_rpc_host( rpc_host );
  mgr.set_listen_port( pyth_port );
  mgr.set_version( version );
  mgr.set_content_dir( cnt_dir );
  mgr.set_capture_file( cap_file );
  mgr.set_do_capture( !cap_file.empty() );
  if ( !mgr.init() ) {
    std::cerr << "pythd: " << mgr.get_err_msg() << std::endl;
    return 1;
  }
  if ( !mgr.get_mapping_pub_key() ) {
    std::cerr << "pythd: missing mapping account key ["
      << mgr.get_mapping_pub_key_file() << "]" << std::endl;
    return 1;
  }
  // set up signal handing
  signal( SIGINT, sig_handle );
  signal( SIGHUP, sig_handle );
  signal( SIGTERM, sig_handle );
  signal( SIGUSR1, sig_toggle );
  while( do_run && !mgr.get_is_err() ) {
    mgr.poll( do_wait );
  }
  int retcode = 0;
  if ( mgr.get_is_err() ) {
    std::cerr << "pythd: " << mgr.get_err_msg() << std::endl;
    retcode = 1;
  }
  return retcode;
}