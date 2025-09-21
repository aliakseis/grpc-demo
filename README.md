# gRPC Demo (C++)

A minimal but extendable **gRPC demo project in C++** showcasing a client–server architecture, message exchange via `.proto` definitions, and integration with supporting libraries.

---

## 🚀 Overview

This project demonstrates how to set up and use **gRPC** with modern **C++17**, using a clean modular structure.  
It includes:

- A **gRPC server** (`server/`, `serverlib/`)
- A **gRPC client** (`client/`, `clientlib/`)
- A **shared protocol definition** (`proto/Fov.proto`)
- **Utility libraries** (`common/`) and **transformer app** (`transformer/`)
- Examples of **publish/subscribe client patterns**

---

## 📂 Project Structure
```
grpc-demo-master/
├── client/ # Demo client executable
│ └── main.cpp
├── clientlib/ # Client library implementation
│ ├── ClientImpl.h
│ ├── FovClient.cpp/h
│ └── IPublishSubscribeClient.h
├── common/ # Shared utilities
│ ├── Delegate.h
│ ├── fqueue.h
│ └── notifications.hpp
├── server/ # Demo server executable
│ └── main.cpp
├── serverlib/ # Server library implementation
│ ├── FovServer.cpp/h
│ └── ServerImpl.h
├── transformer/ # Data transformer app
│ └── main.cpp
├── ultimateclient/ # Additional example client
│ └── main.cpp
├── proto/ # gRPC service definitions
│ └── Fov.proto
├── cmake/ # CMake helper scripts
│ └── doxygenHelper.cmake
├── docs/ # Doxygen configuration
│ └── Doxyfile.in
├── CMakeLists.txt # Root build configuration
├── LICENSE # License information
├── .gitignore
└── .gitattributes
```


---

## 🛠️ Dependencies

- **C++17 or newer**
- [gRPC](https://grpc.io/) (C++ implementation)
- [Protobuf](https://developers.google.com/protocol-buffers)
- [CMake ≥ 3.15](https://cmake.org/)
- Standard C++ build toolchain (GCC, Clang, or MSVC)

Optional:
- [Doxygen](https://www.doxygen.nl/) for documentation generation

---

## ⚙️ Building

1. **Clone the repository**:
```
   git clone https://github.com/yourusername/grpc-demo.git
   cd grpc-demo-master
```

2. **Create a build directory**:

```
   mkdir build && cd build
```

3. **Run CMake**:

```
cmake ..
```
4. **Build the project**:

```
cmake --build .
```
▶️ Running
Start the server:

```
./server/server
```

Run the client:

```
./client/client
```

By default, the client connects to the server using the configuration defined in proto/Fov.proto.

🔧 Extending
Add new services to proto/*.proto

Run protoc with the gRPC C++ plugin to regenerate stubs

Implement server handlers in serverlib/

Update client logic in clientlib/

Example protoc invocation:

```
protoc -I=proto --grpc_out=. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` proto/Fov.proto
protoc -I=proto --cpp_out=. proto/Fov.proto
```
📖 Documentation
You can generate API documentation with Doxygen:

```
cd build
make docs
```
Output will be placed under docs/.

The code is structured to be modular and reusable.

You can integrate the client and server libraries into larger applications.

The transformer and ultimateclient are example applications showing flexibility of the architecture.

* [Server implementation](serverlib/ServerImpl.h), [usage example](serverlib/FovServer.cpp)
* [Client implementation](clientlib/ClientImpl.h), [usage example](clientlib/FovClient.cpp)

https://user-images.githubusercontent.com/11851670/127492863-ae7e13a9-babe-46a9-8cd2-a118cda92448.mp4
