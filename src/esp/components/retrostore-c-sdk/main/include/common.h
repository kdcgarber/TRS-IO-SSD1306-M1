#pragma once

struct RsData {
  uint8_t* data;
  int len;
  bool delete_on_destruct;

  explicit RsData(bool delete_on_destruct_param = true) {
    data = NULL;
    len = 0;
    delete_on_destruct = delete_on_destruct_param;
  }

  RsData(int size) {
    data = new u_int8_t[size];
    len = size;
  }

  ~RsData() {
    if (delete_on_destruct) {
      delete data;
    }
  }
};
