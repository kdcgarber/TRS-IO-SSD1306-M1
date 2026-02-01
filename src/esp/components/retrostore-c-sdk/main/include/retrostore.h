#ifndef retro_store_h
#define retro_store_h

#include <memory>
#include <string>
#include <vector>

#include "data-fetcher-esp.h"

namespace retrostore {

typedef enum {
    RsTrs80Model_UNKNOWN_MODEL = 0,
    RsTrs80Model_MODEL_I = 1,
    RsTrs80Model_MODEL_III = 2,
    RsTrs80Model_MODEL_4 = 3,
    RsTrs80Model_MODEL_4P = 4
} RsTrs80Model;

typedef enum {
    RsMediaType_UNKNOWN = 0,
    RsMediaType_DISK = 1,
    RsMediaType_CASSETTE = 2,
    RsMediaType_COMMAND = 3,
    RsMediaType_BASIC = 4,
} RsMediaType;

typedef struct {
  int ix = 0;
  int iy = 0;
  int pc = 0;
  int sp = 0;
  int af = 0;
  int bc = 0;
  int de = 0;
  int hl = 0;
  int af_prime = 0;
  int bc_prime = 0;
  int de_prime = 0;
  int hl_prime = 0;
  int i = 0;
  int r_1 = 0;
  int r_2 = 0;
} RsSystemStateRegisters;

typedef struct _RsMemoryRegion {
  int start = 0;
  int length = 0;
  std::unique_ptr<uint8_t> data;
} RsMemoryRegion;

typedef struct {
  RsTrs80Model model = RsTrs80Model_UNKNOWN_MODEL;
  RsSystemStateRegisters registers;
  std::vector<RsMemoryRegion> regions;
} RsSystemState;

typedef struct {
  std::string id;
  std::string name;
  std::string version;
  std::string description;
  int release_year = 0;
  std::vector<std::string> screenshot_urls;
  std::string author;
  RsTrs80Model model;
} RsApp;

// Nano version does not have description or screenshot URLs,
// which would take up a lot of memory for an embedded system.
typedef struct {
  std::string id;
  std::string name;
  std::string version;
  int release_year = 0;
  std::string author;
  RsTrs80Model model;
} RsAppNano;

typedef struct {
  RsMediaType type;
  std::string filename;
  std::unique_ptr<uint8_t> data;
  int data_size = 0;
  int64_t uploadTime = 0;
} RsMediaImage;

// API to communicate with the RetroStore server.
class RetroStore
{
  public:
    // Construct with default settings such as server.
    explicit RetroStore();
    // Use custom fetcher, will transfer ownrship of the object.
    RetroStore(DataFetcherEsp* data_fetcher);
    // Print version of the library.
    void PrintVersion();
    // Change the max number of characters to fetch for the description.
    void SetMaxDescriptionLength(int length);

    // Fetches data about a single app with the given ID.
    bool FetchApp(const std::string& appId, RsApp* app);
    // Fetch metadata of retrostore apps from catalog.
    bool FetchApps(int start, int num, std::vector<RsApp>* apps);
    // Query apps based on a search term.
    bool FetchApps(int start, int num, const std::string& query, std::vector<RsApp>* apps);
    // Fetch metadata of retrostore apps from catalog (Nanot API uses less memory)
    bool FetchAppsNano(int start, int num, std::vector<RsAppNano>* apps);
    // Query apps based on a search term.
    bool FetchAppsNano(int start, int num,
                       const std::string& query,
                       const std::vector<RsMediaType> hasTypes,
                       std::vector<RsAppNano>* apps);
    // Fetch media images for the app with the given ID.
    bool FetchMediaImages(const std::string& appId,
                          const std::vector<RsMediaType> types,
                          std::vector<RsMediaImage>* images);
    // Upload system state and return the token. Token is -1 on error.
    int UploadState(RsSystemState& state);
    // Download system state with the given token.
    bool DownloadState(int token, RsSystemState* state);
    // Download system state with the given token.
    bool DownloadState(int token, bool exclude_memory_region_data, RsSystemState* state);
    // Download a custom memory region for the state with the given token.
    bool DownloadStateMemoryRange(int token, int start, int length, RsMemoryRegion* region);
  private:
    const DataFetcherEsp* data_fetcher_;
};

}  // namespace retrostore

#endif