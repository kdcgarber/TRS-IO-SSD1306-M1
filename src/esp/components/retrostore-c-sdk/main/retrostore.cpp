#include "retrostore.h"

#include "data-fetcher-esp.h"

#include <memory>
#include <string>

#include "esp_log.h"

#include "common.h"
#include "proto/ApiProtos.pb.h"
#include "proto/pb_decode.h"
#include "proto/pb_encode.h"

#define ARRAY_SIZE(arr)     (sizeof(arr) / sizeof((arr)[0]))

namespace {
  static const char *TAG = "rs-api";
};  // namespace

namespace retrostore {

using std::string;

namespace {
int opt_description_max_length = 1024;

// TODO: Secure, HTTPS(443). Use WiFiClientSecure.
const int DEFAULT_PORT = 80;
const string PATH_UPLOAD_STATE = "/api/uploadState";
const string PATH_DOWNLOAD_STATE = "/api/downloadState";
const string PATH_DOWNLOAD_STATE_MEMORY_REGION = "/api/downloadStateMemoryRegion";
const string PATH_GET_APP = "/api/getApp";
const string PATH_FETCH_APPS = "/api/listApps";
const string PATH_FETCH_APPS_NANO = "/api/listAppsNano";
const string PATH_FETCH_MEDIA_IMAGES = "/api/fetchMediaImages";

static RsTrs80Model fromPbModel(Trs80Model pbModel) {
  switch (pbModel) {
    case Trs80Model_MODEL_I:
      return RsTrs80Model_MODEL_I;
    case Trs80Model_MODEL_III:
      return RsTrs80Model_MODEL_III;
    case Trs80Model_MODEL_4:
      return RsTrs80Model_MODEL_4;
    case Trs80Model_MODEL_4P:
      return RsTrs80Model_MODEL_4P;
    default:
      return RsTrs80Model_UNKNOWN_MODEL;
  }
}
static Trs80Model toPbModel(RsTrs80Model model) {
  switch (model) {
    case RsTrs80Model_MODEL_I:
      return Trs80Model_MODEL_I;
    case RsTrs80Model_MODEL_III:
      return Trs80Model_MODEL_III;
    case RsTrs80Model_MODEL_4:
      return Trs80Model_MODEL_4;
    case RsTrs80Model_MODEL_4P:
      return Trs80Model_MODEL_4P;
    case RsTrs80Model_UNKNOWN_MODEL:
      return Trs80Model_UNKNOWN_MODEL;
  }
  return Trs80Model_UNKNOWN_MODEL;
}

static RsMediaType fromPbMediaType(MediaType type) {
  switch (type) {
    case MediaType_UNKNOWN:
      return RsMediaType_UNKNOWN;
    case MediaType_DISK:
      return RsMediaType_DISK;
    case MediaType_CASSETTE:
      return RsMediaType_CASSETTE;
    case MediaType_COMMAND:
      return RsMediaType_COMMAND;
    case MediaType_BASIC:
      return RsMediaType_BASIC;
  }
  return RsMediaType_UNKNOWN;
}

static MediaType toPbMediaType(RsMediaType type) {
  switch (type) {
    case RsMediaType_UNKNOWN:
      return MediaType_UNKNOWN;
    case RsMediaType_DISK:
      return MediaType_DISK;
    case RsMediaType_CASSETTE:
      return MediaType_CASSETTE;
    case RsMediaType_COMMAND:
      return MediaType_COMMAND;
    case RsMediaType_BASIC:
      return MediaType_BASIC;
  }
  return MediaType_UNKNOWN;
}

static bool pb_MemoryRegionData_decode_cb(pb_istream_t* stream,
                                       const pb_field_t* field,
                                       void** arg) {
  std::unique_ptr<uint8_t> bytes(static_cast<uint8_t*>(malloc(sizeof(pb_byte_t) * stream->bytes_left)));
  pb_read(stream, (pb_byte_t*) bytes.get(), stream->bytes_left);
  RsMemoryRegion* newRegion = static_cast<RsMemoryRegion*>(*arg);
  newRegion->data = std::move(bytes);

  return true;  // success
}

struct ConstMemRegionPtr {
  const RsMemoryRegion* region;
};

static bool pb_MemoryRegionData_encode_cb(pb_ostream_t* stream,
                                       const pb_field_t* field,
                                       void * const *arg) {
  if (!pb_encode_tag_for_field(stream, field)) {
    return false;
  }
  const auto* ptr = static_cast<struct ConstMemRegionPtr*>(*arg);
  return pb_encode_string(stream, ptr->region->data.get(), ptr->region->length);
}

// Called from nanopb to parse App of the response.
static bool pb_MemoryRegion_decode_cb(pb_istream_t* stream,
                                     const pb_field_t* field,
                                     void** arg) {
  auto* state = static_cast<RsSystemState*>(*arg);
  RsMemoryRegion rsRegion;

  SystemState_MemoryRegion region = SystemState_MemoryRegion_init_zero;
  region.data.funcs.decode = &pb_MemoryRegionData_decode_cb;
  region.data.arg = &rsRegion;

  if (!pb_decode(stream, SystemState_MemoryRegion_fields, &region)) {
    ESP_LOGE(TAG, "Failed to decode MemoryRegion");
    return false;
  }

  rsRegion.start = region.start;
  rsRegion.length = region.length;
  state->regions.push_back(std::move(rsRegion));
  return true;
}

static bool pb_MemoryRegion_encode_cb(pb_ostream_t* stream,
                                      const pb_field_t* field,
                                      void * const *arg) {
  const auto* rsRegions = static_cast<const std::vector<RsMemoryRegion>*>(*arg);
  for (int i = 0; i < rsRegions->size(); ++i) {
    if (!pb_encode_tag_for_field(stream, field)) {
      return false;
    }

    SystemState_MemoryRegion region = SystemState_MemoryRegion_init_zero;
    region.start = rsRegions->at(i).start;
    region.length = rsRegions->at(i).length;
    region.data.funcs.encode = &pb_MemoryRegionData_encode_cb;
    struct ConstMemRegionPtr arg = {.region = &rsRegions->at(i)};
    region.data.arg = &arg;

    if (!pb_encode_submessage(stream, SystemState_MemoryRegion_fields, &region)) {
      return false;
    }
  }
  return true;
}

static void convertApp(const App pbApp, RsApp* app) {
  app->id = pbApp.id;
  app->name = pbApp.name;
  app->version = pbApp.version;
  ESP_LOGI(TAG, "Description length: %d", app->description.length());
  app->release_year = pbApp.release_year;
  app->author = pbApp.author;
  app->model = fromPbModel(pbApp.ext_trs80.model);

  for (int i = 0; i < pbApp.screenshot_url_count; ++i) {
    app->screenshot_urls.push_back(std::string(pbApp.screenshot_url[i]));
  }
}

static void convertAppNano(const AppNano pbApp, RsAppNano* app) {
  app->id = pbApp.id;
  app->name = pbApp.name;
  app->version = pbApp.version;
  app->release_year = pbApp.release_year;
  app->author = pbApp.author;
  app->model = fromPbModel(pbApp.ext_trs80.model);
}

// Called from nanopb to parse the app description of the response.
static bool pb_App_Description_callback(pb_istream_t* stream,
                                  const pb_field_t* field,
                                  void** arg) {
  auto* rsApp = static_cast<RsApp*>(*arg);

  // We have already read our portion of the string. Discard the rest.
  if (rsApp->description.length() > 0) {
    uint8_t buffer[stream->bytes_left] = {0};
    return pb_read(stream, buffer, stream->bytes_left);
  }

  // Max length is the one we set to, but actual string might be shorter.
  int str_length = opt_description_max_length;
  if (stream->bytes_left < opt_description_max_length) {
    str_length = stream->bytes_left;
  }

  uint8_t buffer[str_length] = {0};
  if (!pb_read(stream, buffer, str_length))
      return false;
  
  rsApp->description = std::string((char*)buffer, str_length);
  return true;
}

// Called from nanopb to parse App of the response.
static bool pb_App_callback(pb_istream_t* stream,
                                  const pb_field_t* field,
                                  void** arg) {
  RsApp rsApp;
  App app;

  app.description.arg = &rsApp;
  app.description.funcs.decode = &pb_App_Description_callback;

  if (!pb_decode(stream, App_fields, &app)) {
    ESP_LOGE(TAG, "Failed to decode App");
    return false;
  }

  convertApp(app, &rsApp);

  auto* apps = static_cast<std::vector<RsApp>*>(*arg);
  apps->push_back(rsApp);
  return true;
}

// Called from nanopb to parse AppNano of the response.
static bool pb_AppNano_callback(pb_istream_t* stream,
                                  const pb_field_t* field,
                                  void** arg) {
  auto* apps = static_cast<std::vector<RsAppNano>*>(*arg);
  AppNano app;
  if (!pb_decode(stream, AppNano_fields, &app)) {
    ESP_LOGE(TAG, "Failed to decode AppNano");
    return false;
  }

  RsAppNano appNano;
  convertAppNano(app, &appNano);
  apps->push_back(appNano);
  return true;
}

static bool pb_MediaImageData_callback(pb_istream_t* stream,
                                       const pb_field_t* field,
                                       void** arg) {
  auto* image = static_cast<RsMediaImage*>(*arg);
  image->data_size = stream->bytes_left;

  std::unique_ptr<uint8_t> bytes(static_cast<uint8_t*>(malloc(sizeof(pb_byte_t) * stream->bytes_left)));
  auto success = pb_read(stream, (pb_byte_t*) bytes.get(), stream->bytes_left);
  if (!success) {
    ESP_LOGE(TAG, "Error while reading media image bytes.");
    return false;
  }

  image->data = std::move(bytes);
  return true;
}

// Called from nanopb to parse AppNano of the response.
static bool pb_MediaImage_callback(pb_istream_t* stream,
                                   const pb_field_t* field,
                                   void** arg) {
  RsMediaImage rsImage;
  MediaImage image;

  image.data.arg = &rsImage;
  image.data.funcs.decode = &pb_MediaImageData_callback;

  if (!pb_decode(stream, MediaImage_fields, &image)) {
    ESP_LOGE(TAG, "Failed to decode MediaImage");
    return false;
  }

  // TODO: Decode data
  rsImage.type = fromPbMediaType(image.type);
  rsImage.uploadTime = image.uploadTime;
  rsImage.filename = image.filename;


  auto* images = static_cast<std::vector<RsMediaImage>*>(*arg);
  images->push_back(std::move(rsImage));
  return true;
}

}

RetroStore::RetroStore()
    : data_fetcher_(new DataFetcherEsp("retrostore.org")) {
}

RetroStore::RetroStore(DataFetcherEsp* data_fetcher)
    : data_fetcher_(data_fetcher) {
}

void RetroStore::PrintVersion() {
  ESP_LOGI(TAG, "RetroStore[ESP] SDK version 2022-11-06");
}

void RetroStore::SetMaxDescriptionLength(int length) {
  opt_description_max_length = length;
}

bool RetroStore::FetchApp(const std::string& appId, RsApp* app) {
  // Create parameter object.
  GetAppParams params = GetAppParams_init_zero;
  strcpy(params.app_id, appId.c_str());

  // Create buffer for params.
  RsData buffer(64);
  pb_ostream_t stream_param = pb_ostream_from_buffer(buffer.data, buffer.len);
  // Encode the object to the buffer stream above.
  if (!pb_encode(&stream_param, GetAppParams_fields, &params)) {
      ESP_LOGE(TAG, "Encoding params failed: %s", PB_GET_ERROR(&stream_param));
      return false;
  }
  buffer.len = stream_param.bytes_written;
  ESP_LOGI(TAG, "GetAppParams created. Size: %d", buffer.len);

  RsData recv_buffer;
  bool success = data_fetcher_->Fetch(PATH_GET_APP, buffer, &recv_buffer);
  if (!success) {
    ESP_LOGE(TAG, "Error fetching data");
    return false;
  }

  ESP_LOGI(TAG, "Received %d bytes response.", recv_buffer.len);
  if (recv_buffer.len == 0) {
    return false;
  }

  // Parse the response.
  std::vector<RsApp> apps;
  ApiResponseApps resp = ApiResponseApps_init_zero;
  resp.app.arg = &apps;
  resp.app.funcs.decode = &pb_App_callback;
  pb_istream_t stream_in = pb_istream_from_buffer(recv_buffer.data, recv_buffer.len);
  if (!pb_decode(&stream_in, ApiResponseApps_fields, &resp)) {
      ESP_LOGE(TAG, "Decoding failed: %s", PB_GET_ERROR(&stream_in));
      return false;
  }
  ESP_LOGI(TAG, "ApiResponseApps decoded successfully.");

  if (!resp.success) {
    ESP_LOGW(TAG, "Bad request. Server responded: %s", resp.message);
    return false;
  }
  if (apps.size() != 1) {
    ESP_LOGE(TAG, "Exactly one app expected, but got: %d", apps.size());
    return false;
  }
  *app = apps[0];
  return true;
}

bool RetroStore::FetchApps(int start, int num, std::vector<RsApp>* apps) {
  return FetchApps(start, num, "", apps);
}

bool RetroStore::FetchApps(int start, int num, const std::string& query, std::vector<RsApp>* apps) {
  ListAppsParams params = ListAppsParams_init_zero;
  params.start = start;
  params.num = num;
  if (!query.empty()) {
    strcpy(params.query, query.c_str());
  }

  // Create buffer for params.
  RsData buffer(200);
  pb_ostream_t stream_param = pb_ostream_from_buffer(buffer.data, buffer.len);
  // Encode the object to the buffer stream above.
  if (!pb_encode(&stream_param, ListAppsParams_fields, &params)) {
      ESP_LOGE(TAG, "Encoding params failed: %s", PB_GET_ERROR(&stream_param));
      return false;
  }
  buffer.len = stream_param.bytes_written;
  ESP_LOGI(TAG, "ListAppsParams created. Size: %d", buffer.len);

  RsData recv_buffer;
  bool success = data_fetcher_->Fetch(PATH_FETCH_APPS, buffer, &recv_buffer);
  if (!success) {
    ESP_LOGE(TAG, "Error fetching data");
    return false;
  }
  ESP_LOGI(TAG, "Received %d bytes response.", recv_buffer.len);
  if (recv_buffer.len == 0) {
    return false;
  }

  // Parse the response.
  ApiResponseApps resp = ApiResponseApps_init_zero;
  resp.app.arg = apps;
  resp.app.funcs.decode = &pb_App_callback;
  pb_istream_t stream_in = pb_istream_from_buffer(recv_buffer.data, recv_buffer.len);
  if (!pb_decode(&stream_in, ApiResponseApps_fields, &resp)) {
      ESP_LOGE(TAG, "Decoding failed: %s", PB_GET_ERROR(&stream_in));
      return false;
  }
  ESP_LOGI(TAG, "ApiResponseApps decoded successfully.");

  if (!resp.success) {
    ESP_LOGW(TAG, "Bad request. Server responded: %s", resp.message);
    return false;
  }
  if (apps->size() > num) {
    ESP_LOGE(TAG, "Max %d apps expected, but got: %d", num, apps->size());
    return false;
  }
  return true;
}

bool RetroStore::FetchAppsNano(int start, int num, std::vector<RsAppNano>* apps) {
  std::vector<RsMediaType> hasType;  // empty
  return FetchAppsNano(start, num, "", hasType, apps);
}

bool RetroStore::FetchAppsNano(int start, int num,
                               const std::string& query,
                               const std::vector<RsMediaType> hasTypes,
                               std::vector<RsAppNano>* apps) {
  ListAppsParams params = ListAppsParams_init_zero;
  params.start = start;
  params.num = num;
  if (!query.empty()) {
    strcpy(params.query, query.c_str());
  }

  for (int i = 0; i < hasTypes.size(); ++i) {
    params.trs80.media_types[i] = toPbMediaType(hasTypes[i]);
  }
  params.has_trs80 = true;
  params.trs80.media_types_count = hasTypes.size();

  // Create buffer for params.
  RsData buffer(200);
  pb_ostream_t stream_param = pb_ostream_from_buffer(buffer.data, buffer.len);
  // Encode the object to the buffer stream above.
  if (!pb_encode(&stream_param, ListAppsParams_fields, &params)) {
      ESP_LOGE(TAG, "Encoding params failed: %s", PB_GET_ERROR(&stream_param));
      return false;
  }
  buffer.len = stream_param.bytes_written;
  ESP_LOGI(TAG, "ListAppsParams created. Size: %d", buffer.len);

  RsData recv_buffer;
  bool success = data_fetcher_->Fetch(PATH_FETCH_APPS_NANO, buffer, &recv_buffer);
  if (!success) {
    ESP_LOGE(TAG, "Error fetching data");
    return false;
  }
  ESP_LOGI(TAG, "Received %d bytes response.", recv_buffer.len);
  if (recv_buffer.len == 0) {
    return false;
  }

  // Parse the response.
  ApiResponseAppsNano resp = ApiResponseAppsNano_init_zero;
  resp.app.arg = apps;
  resp.app.funcs.decode = &pb_AppNano_callback;
  pb_istream_t stream_in = pb_istream_from_buffer(recv_buffer.data, recv_buffer.len);
  if (!pb_decode(&stream_in, ApiResponseAppsNano_fields, &resp)) {
      ESP_LOGE(TAG, "Decoding failed: %s", PB_GET_ERROR(&stream_in));
      return false;
  }
  ESP_LOGI(TAG, "ApiResponseApps decoded successfully.");

  if (!resp.success) {
    ESP_LOGW(TAG, "Bad request. Server responded: %s", resp.message);
    return false;
  }

  if (apps->size() > num) {
    ESP_LOGE(TAG, "Max %d apps expected, but got: %d", num, apps->size());
    return false;
  }

  return true;
}

bool RetroStore::FetchMediaImages(const string& appId,
                                  const std::vector<RsMediaType> types,
                                  std::vector<RsMediaImage>* images) {
  FetchMediaImagesParams params = FetchMediaImagesParams_init_zero;
  strcpy(params.app_id, appId.c_str());

  for (int i = 0; i < types.size(); ++i) {
    params.media_type[i] = toPbMediaType(types[i]);
  }
  params.media_type_count = types.size();

  RsData buffer(64);
  pb_ostream_t stream_param = pb_ostream_from_buffer(buffer.data, buffer.len);
  // Encode the object to the buffer stream above.
  if (!pb_encode(&stream_param, FetchMediaImagesParams_fields, &params)) {
      ESP_LOGE(TAG, "Encoding params failed: %s", PB_GET_ERROR(&stream_param));
      return false;
  }
  buffer.len = stream_param.bytes_written;
  ESP_LOGI(TAG, "FetchMediaImagesParams created. Size: %d", buffer.len);

  RsData recv_buffer;
  bool success = data_fetcher_->Fetch(PATH_FETCH_MEDIA_IMAGES, buffer, &recv_buffer);
  if (!success) {
    ESP_LOGE(TAG, "Error fetching data");
    return false;
  }
  ESP_LOGI(TAG, "Received %d bytes response.", recv_buffer.len);
  if (recv_buffer.len == 0) {
    return false;
  }

  ApiResponseMediaImages resp = ApiResponseMediaImages_init_zero;

  resp.mediaImage.arg = images;
  resp.mediaImage.funcs.decode = &pb_MediaImage_callback;

  pb_istream_t stream_in = pb_istream_from_buffer(recv_buffer.data, recv_buffer.len);
  if (!pb_decode(&stream_in, ApiResponseMediaImages_fields, &resp)) {
      ESP_LOGE(TAG, "Decoding failed: %s", PB_GET_ERROR(&stream_in));
      return false;
  }
  ESP_LOGI(TAG, "ApiResponseMediaImages decoded successfully.");

  if (!resp.success) {
    ESP_LOGW(TAG, "Bad request. Server responded: %s", resp.message);
    return false;
  }
  return true;
}

int RetroStore::UploadState(RsSystemState& state) {
  UploadSystemStateParams params = UploadSystemStateParams_init_zero;
  params.has_state = true;
  params.state.has_registers = true;

  params.state.model = toPbModel(state.model);
  params.state.registers.ix = state.registers.ix;
  params.state.registers.iy = state.registers.iy;
  params.state.registers.pc = state.registers.pc;
  params.state.registers.sp = state.registers.sp;
  params.state.registers.af = state.registers.af;
  params.state.registers.bc = state.registers.bc;
  params.state.registers.de = state.registers.de;
  params.state.registers.hl = state.registers.hl;
  params.state.registers.af_prime = state.registers.af_prime;
  params.state.registers.bc_prime = state.registers.bc_prime;
  params.state.registers.de_prime = state.registers.de_prime;
  params.state.registers.hl_prime = state.registers.hl_prime;
  params.state.registers.i = state.registers.i;
  params.state.registers.r_1 = state.registers.r_1;
  params.state.registers.r_2 = state.registers.r_2;

  params.state.memoryRegions.funcs.encode = &pb_MemoryRegion_encode_cb;
  params.state.memoryRegions.arg = &state.regions;

  // Size of the params depends on the region size.
  int paramSize = 200;
  for (const auto& region : state.regions) {
    paramSize += region.length + 10;
  }

  // Create buffer for params.
  RsData buffer(paramSize);
  pb_ostream_t stream_param = pb_ostream_from_buffer(buffer.data, buffer.len);
  // Encode the object to the buffer stream above.
  if (!pb_encode(&stream_param, UploadSystemStateParams_fields, &params)) {
      ESP_LOGE(TAG, "Encoding failed: %s", PB_GET_ERROR(&stream_param));
      return -1;
  }
  buffer.len = stream_param.bytes_written;
  ESP_LOGI(TAG, "UploadSystemStateParams created. Size: %d", buffer.len);

  RsData recv_buffer;
  bool success = data_fetcher_->Fetch(PATH_UPLOAD_STATE, buffer, &recv_buffer);
  if (!success) {
    ESP_LOGE(TAG, "Error fetching data");
    return -1;
  }
  ESP_LOGI(TAG, "Received %d bytes response.", recv_buffer.len);
  if (recv_buffer.len == 0) {
    return -1;
  }

  ApiResponseUploadSystemState stateResp = ApiResponseUploadSystemState_init_zero;
  pb_istream_t stream_in = pb_istream_from_buffer(recv_buffer.data, recv_buffer.len);
  if (!pb_decode(&stream_in, ApiResponseUploadSystemState_fields, &stateResp)) {
      ESP_LOGE(TAG, "Decoding failed: %s", PB_GET_ERROR(&stream_in));
      return false;
  }
  ESP_LOGI(TAG, "UploadSystemState response decoded successfully.");

  if (!stateResp.success) {
    ESP_LOGW(TAG, "Bad request. Server responded: %s", stateResp.message);
    return false;
  }
  return stateResp.token;
}

bool RetroStore::DownloadState(int token, RsSystemState* state) {
  return DownloadState(token, false, state);
}

bool RetroStore::DownloadState(int token, bool exclude_memory_region_data, RsSystemState* state) {
  // Create params object and set token.
  DownloadSystemStateParams params = DownloadSystemStateParams_init_zero;
  params.token = token;
  params.exclude_memory_region_data = exclude_memory_region_data;

  // Create buffer for params.
  RsData buffer(128);

  pb_ostream_t stream_param = pb_ostream_from_buffer(buffer.data, buffer.len);
  // Encode the object to the buffer stream above.
  if (!pb_encode(&stream_param, DownloadSystemStateParams_fields, &params)) {
      ESP_LOGE(TAG, "Encoding failed: %s", PB_GET_ERROR(&stream_param));
      return false;
  }
  buffer.len = stream_param.bytes_written;
  ESP_LOGI(TAG, "DownloadSystemStateParams created. Size: %d", buffer.len);

  RsData recv_buffer;
  bool success = data_fetcher_->Fetch(PATH_DOWNLOAD_STATE, buffer, &recv_buffer);
  if (!success) {
    ESP_LOGE(TAG, "Error fetching data");
    return false;
  }
  ESP_LOGI(TAG, "Received %d bytes response.", recv_buffer.len);
  if (recv_buffer.len == 0) {
    return false;
  }


  ApiResponseDownloadSystemState stateResp = ApiResponseDownloadSystemState_init_zero;
  stateResp.systemState.memoryRegions.funcs.decode = &pb_MemoryRegion_decode_cb;
  stateResp.systemState.memoryRegions.arg = state;

  pb_istream_t stream_in = pb_istream_from_buffer(recv_buffer.data, recv_buffer.len);
  if (!pb_decode(&stream_in, ApiResponseDownloadSystemState_fields, &stateResp)) {
      ESP_LOGE(TAG, "Decoding failed: %s", PB_GET_ERROR(&stream_in));
      return false;
  }
  ESP_LOGI(TAG, "DownloadSystemState response decoded successfully.");

  if (!stateResp.success) {
    ESP_LOGW(TAG, "Bad request. Server responded: %s", stateResp.message);
    return false;
  }
  if (!stateResp.has_systemState) {
    ESP_LOGE(TAG, "Server did not sent a SystemState despite reporting a success.");
    return false;
  }
  auto& _state = stateResp.systemState;

  state->model = fromPbModel(_state.model);

  // Copy registers
  state->registers.ix = _state.registers.ix;
  state->registers.iy = _state.registers.iy;
  state->registers.pc = _state.registers.pc;
  state->registers.sp = _state.registers.sp;
  state->registers.af = _state.registers.af;
  state->registers.bc = _state.registers.bc;
  state->registers.de = _state.registers.de;
  state->registers.hl = _state.registers.hl;
  state->registers.af_prime = _state.registers.af_prime;
  state->registers.bc_prime = _state.registers.bc_prime;
  state->registers.de_prime = _state.registers.de_prime;
  state->registers.hl_prime = _state.registers.hl_prime;
  state->registers.i = _state.registers.i;
  state->registers.r_1 = _state.registers.r_1;
  state->registers.r_2 = _state.registers.r_2;

  return true;
}

bool RetroStore::DownloadStateMemoryRange(int token, int start, int length, RsMemoryRegion* region) {
  // Create params object and set token.
  DownloadSystemStateMemoryRegionParams params = DownloadSystemStateMemoryRegionParams_init_zero;
  params.token = token;
  params.start = start;
  params.length = length;

  // Create buffer for params.
  RsData buffer(128);

  pb_ostream_t stream_param = pb_ostream_from_buffer(buffer.data, buffer.len);
  // Encode the object to the buffer stream above.
  if (!pb_encode(&stream_param, DownloadSystemStateMemoryRegionParams_fields, &params)) {
      ESP_LOGE(TAG, "Encoding failed: %s", PB_GET_ERROR(&stream_param));
      return false;
  }
  buffer.len = stream_param.bytes_written;
  ESP_LOGI(TAG, "DownloadSystemStateMemoryRegionParams created. Size: %d", buffer.len);

  RsData recv_buffer(false  /* delete_on_destruct */);
  bool success = data_fetcher_->Fetch(PATH_DOWNLOAD_STATE_MEMORY_REGION, buffer, &recv_buffer);
  if (!success) {
    ESP_LOGE(TAG, "Error fetching data");
    return false;
  }
  ESP_LOGI(TAG, "Received %d bytes response.", recv_buffer.len);
  if (recv_buffer.len == 0) {
    return false;
  }

  // Note: We set RsData above to NOT destroy the data on destruction so we can just
  //       assign it here without a copy.
  std::unique_ptr<uint8_t> data(recv_buffer.data);
  region->data = std::move(data);
  region->start = start;
  region->length = recv_buffer.len;

  return true;
}

}  // namespace retrostore