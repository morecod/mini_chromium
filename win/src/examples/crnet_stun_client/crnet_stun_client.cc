#include <iostream>

#include "crbase/at_exit.h"
#include "crbase/pickle.h"
#include "crbase/digest/sha1.h"
#include "crbase/digest/md5.h"
#include "crbase/import_libs.cc"

using namespace std;

int main(int argc, char* argv) {
  cr::AtExitManager at_exit;
  
  cr::SHA1Digest sha1_digest;
  cr::SHA1Hmac("1111", "1111", &sha1_digest);
  std::string sha1_hmac = cr::SHA1DigestToBase16(sha1_digest);
  cout << "sha1 hmac=" << sha1_hmac << endl;
  
  cr::MD5Digest md5_digest;
  cr::MD5Hmac("1111", "1111", &md5_digest);
  std::string md5_hmac = cr::MD5DigestToBase16(md5_digest);
  cout << "md5 hmac=" << md5_hmac << endl;

  system("pause");

  static_assert(2 >= 3);
  return 0;
}