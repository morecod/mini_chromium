#include <iostream>

#include "crbase/at_exit.h"
#include "crbase/codes/punycode.h"
#include "crbase/import_libs.cc"
#include "crurl/gurl.h"

using namespace std;

int main(int argc, char* argv[]) {
  crbase::AtExitManager atexit_manager;

  GURL url(u8"https://www.旺旺google中国.旺旺:888/1.txt");
  cout << url.scheme() << endl; // "http"
  cout << url.host() << endl;   // "www.xn--google-gw7is51dpz5aa.xn--ihva"
  cout << url.path() << endl;   // "/1.txt"
  cout << url.port() << endl;   // "888"
  std::system("pause");
  return 0;
}