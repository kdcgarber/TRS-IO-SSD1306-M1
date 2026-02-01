if(ESP_PLATFORM)

idf_component_register(SRCS "main/retrostore.cpp"
                            "main/data-fetcher-esp.cpp"

                            "main/proto/pb_common.c"
                            "main/proto/pb_decode.c"
                            "main/proto/pb_encode.c"
                            "main/proto/ApiProtos.pb.c"
                       INCLUDE_DIRS "main/include" "main/proto"
                       REQUIRES esp_http_client)
endif()
