load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
  name = "api-protos",
  srcs = [ "ApiProtos.pb.c"],
  hdrs = [ "ApiProtos.pb.h"],
  deps = [ ":nano-pb" ],
  visibility = ["//visibility:public"],
)

cc_library(
  name = "nano-pb",
  srcs = [
           "pb_common.c",
           "pb_decode.c",
           "pb_encode.c"
         ],
  hdrs = [
           "pb.h",
           "pb_common.h",
           "pb_decode.h",
           "pb_encode.h"
         ],
)
