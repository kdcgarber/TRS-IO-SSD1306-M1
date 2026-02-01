# retrostore-c-sdk
The RetroStore C/C++ SDK

## Updating/generating protocol buffer files
First, make sure you have the Protocol Buffer Compiler installed:
https://github.com/protocolbuffers/protobuf#protocol-compiler-installation

Second, checkout the nanopb project: `https://github.com/nanopb/nanopb`.

The source of truth Protocol Buffer is in the JVM project, [here](https://github.com/shaeberling/retrostore-jvm-sdk/blob/main/src/main/proto/org/retrostore/client/common/proto/ApiProtos.proto). Make sure to check out the project as well, or
at least get that file.

1. Generate the binary pb file from the original proto:
`protoc -o ApiProtos.pb ApiProtos.proto`

2. Then install the C and header file:
`python [your path to]/generator/nanopb_generator.py ApiProtos.pb`
Which will write two output files: `ApiProtos.pb.h` and `ApiProtos.pb.c`

3. Copy these files into the `proto` source subdirectory in this project. We
   will not need the binary `ApiProtos.pb` file anymore.

4. You might have to change the import of `pb.h` in the `ApiProtos.pb.c` file.


## C++ RetroStore API
The C++ RetroStore API has the following features:
 - Uses nanopb to be extremely memory efficient so it can easily be used in
   projects that run on microprocessors, such as the ESP32.
 - Has a `DataFetcher` API so that projects can easily provide their own way
   of fetching the data from RetroStore, depending on which APIs are available.

## Example
See https://github.com/shaeberling/retrostore-sdk-esp32-example for an example
on how to use this in your ESP32 project.
