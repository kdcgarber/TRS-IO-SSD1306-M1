#ifndef retro_store_default_data_fetcher_h
#define retro_store_default_data_fetcher_h

#include <string>

#include "common.h"

namespace retrostore {

// API to fetch URLs. Depending on the environment, feel free to implement
// in a way that works for you, e.g. using ESP32's IDF HTTP fetch API.
class DataFetcherEsp {
  public:
    DataFetcherEsp(const std::string& host) : host_(host) {}
    bool Fetch(const std::string& path,
               const RsData& params,
               RsData* data) const;
  private:
     const std::string host_;
};

}  // namespace retrostore

#endif