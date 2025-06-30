# SOME/IP Communication Stack for SDV

## 1. Overview

This project implements a C++ communication stack designed for Software-Defined Vehicle (SDV) environments. It facilitates inter-component communication using a topic-based publish/subscribe mechanism and supports Remote Procedure Calls (RPC). Serialization for RPC is handled by Protocol Buffers (Protobuf), and the transport layer leverages SOME/IP (Scalable service-Oriented MiddlewarE over IP).

**Key Features:**
*   Topic-based Publish/Subscribe for event-driven communication.
*   RPC for request/response interactions.
*   Protocol Buffers for defining service interfaces and message structures, and for payload serialization.
*   SOME/IP for network transport, service discovery, and event multicasting.
*   Designed for Android native layer integration.
*   Host-based development and testing support.

## 2. Architecture

The stack is composed of several core C++ components:

*   **`CommunicationManager`**: A singleton class that orchestrates the entire stack. It initializes and shuts down the underlying `vsomeip` application, manages service configurations, and provides access to `Publisher`, `Subscriber`, and `RpcClient` instances.
*   **`Publisher`**: Allows components to publish messages (defined as Protobuf messages) to a specific topic. It handles message serialization and uses `vsomeip` to send SOME/IP events/eventgroups.
*   **`Subscriber`**: Allows components to subscribe to topics. It receives SOME/IP events/eventgroups, deserializes the payload into Protobuf messages, and invokes user-registered callbacks.
*   **`RpcClient`**: Enables components to make RPC calls to remote services. It serializes Protobuf request messages, sends them via SOME/IP, and handles asynchronous responses (deserializing Protobuf response messages) using `std::future`.
*   **`RpcService` (Implemented by User)**: Applications implement service interfaces defined in `.proto` files (e.g., `MySampleRpcImpl` implementing `protos::SampleRpc`). These implementations are registered with the `CommunicationManager`.
*   **`vsomeip` Library**: The underlying library responsible for SOME/IP protocol handling, including service discovery, message routing, serialization (of SOME/IP headers, not payload), and network communication (UDP/TCP).
*   **Protocol Buffers Library**: Used for defining data structures (`message`) and service interfaces (`service`) in `.proto` files. It also provides the tools (`protoc`) and runtime libraries for serializing and deserializing message payloads.

*(Conceptual Diagram would be included here if this were a rich document)*

## 3. Dependencies

### 3.1. Host System (for development & testing)
*   **CMake (>= 3.16)**: Build system generator.
*   **C++17 Compiler**: (e.g., GCC, Clang).
*   **Boost (>= 1.66.0)**: Specifically `system`, `thread`, and `log` (including `log_setup`, `date_time`, `chrono`) components are required by `vsomeip`. Install development libraries.
*   **Protocol Buffers (>= 3.x.x)**:
    *   `protoc` compiler.
    *   C++ runtime libraries and headers (e.g., `libprotobuf-dev`).
*   **vsomeip (Ideally >= 3.x)**:
    *   Core library (`libvsomeip3.so` or `libvsomeip.so`).
    *   Configuration module (`libvsomeip3-cfg.so` or `libvsomeip-cfg.so`).
    *   Service Discovery module (`libvsomeip3-sd.so` or `libvsomeip-sd.so`).
    *   Typically built from source from [COVESA/vsomeip](https://github.com/COVESA/vsomeip).

### 3.2. Android (NDK)
*   **Android NDK (e.g., r21+ recommended)**: For C++ compilation.
*   **Prebuilt Dependencies for Android ABIs** (e.g., `armeabi-v7a`, `arm64-v8a`, `x86`, `x86_64`):
    *   **Boost**: `libboost_system`, `libboost_thread`, `libboost_log` (as `.so` or `.a`).
    *   **Protocol Buffers C++ Runtime**: `libprotobuf.so` or `libprotobuf.a`.
    *   **vsomeip**: `libvsomeip3.so`, `libvsomeip3-cfg.so`, `libvsomeip3-sd.so`.
    *   These prebuilt libraries and their corresponding headers should be organized in a `third_party` directory (see "Building for Android" section).

## 4. Building

### 4.1. Host System

1.  **Install Dependencies**: Ensure all host dependencies listed above are installed. For `vsomeip`, you may need to build it from source and install it to a location discoverable by CMake (e.g., `/usr/local`).
2.  **Set `VSOMEIP_CONFIGURATION`**: Before running any application using this stack, set the `VSOMEIP_CONFIGURATION` environment variable to point to your `vsomeip.json` configuration file.
    ```bash
    export VSOMEIP_CONFIGURATION=/path/to/your/vsomeip_config/vsomeip_host.json
    ```
3.  **Configure with CMake**:
    ```bash
    mkdir build && cd build
    cmake .. 
    ```
4.  **Compile**:
    ```bash
    make -j$(nproc)
    ```
    This will build the `libcomms_stack.so` (or `.a`) library and the host test applications in `build/tests/`.

### 4.2. Android (NDK)

1.  **Prepare Prebuilt Dependencies**:
    *   Compile or obtain prebuilt versions of Boost, Protocol Buffers C++ runtime, and vsomeip for your target Android ABIs.
    *   Organize them in a `third_party` directory within the project root, as outlined in the conceptual structure:
        ```
        YourProjectRoot/
        |-- third_party/
            |-- android_includes/
            |   |-- boost/
            |   |-- protobuf/
            |   |-- vsomeip/
            |-- android_libs/
                |-- armeabi-v7a/ (contains .so files)
                |-- arm64-v8a/   (contains .so files)
                |-- ... (other ABIs) 
        ```
2.  **Integrate with Android Studio Project**:
    *   Create an Android Studio project.
    *   In your app module's `build.gradle` file, configure `externalNativeBuild` to use CMake:
        ```gradle
        android {
            // ...
            defaultConfig {
                // ...
                externalNativeBuild {
                    cmake {
                        cppFlags "" 
                        // arguments "-DANDROID_STL=c++_shared" // If needed
                    }
                }
            }
            externalNativeBuild {
                cmake {
                    // Path to the app's intermediate CMakeLists.txt
                    path "src/main/cpp/CMakeLists.txt" 
                    version "3.18.1" // Or your NDK's CMake version
                }
            }
        }
        ```
    *   Create `app/src/main/cpp/CMakeLists.txt`. This file should primarily point to the root `CMakeLists.txt` of this communication stack project:
        ```cmake
        cmake_minimum_required(VERSION 3.18)
        project(YourAppNativeBridge CXX)

        # Adjust path to the root of the comms_stack project
        add_subdirectory(
            ../../../../ # Example: if comms_stack is 4 levels up
            ${CMAKE_CURRENT_BINARY_DIR}/comms_stack_build 
        )
        ```
    *   The root `CMakeLists.txt` of this project is already configured to detect Android builds and link against the prebuilt libraries in `third_party/`.
3.  **Build via Android Studio**: Gradle will invoke the NDK build process using CMake, compiling `libcomms_stack.so` and `libcomms_stack_jni.so`.

## 5. Configuration (`vsomeip.json`)

The stack relies on `vsomeip`'s JSON configuration file for defining network parameters, services, events, and their mappings. An example (`vsomeip_config/vsomeip_host.json`) is provided.

Key sections:
*   `unicast`: Local IP address.
*   `applications`: Defines application names and IDs used by `vsomeip`.
*   `service-discovery`: Settings for vsomeip's service discovery mechanism (ports, multicast address, timings).
*   `services`: Array defining each SOME/IP service:
    *   `service`: Service ID (hex).
    *   `instance`: Instance ID (hex).
    *   `reliable`/`unreliable`: Port configuration for TCP/UDP.
    *   `methods`: Array of RPC methods within the service (method ID, name).
    *   `eventgroups`: Array of eventgroups offered by the service.
    *   `events`: Array of specific events, potentially belonging to eventgroups.

**For Android**: The `vsomeip.json` file should be packaged in the app's `assets` directory. At runtime, it must be copied to a file system path accessible by the native code (e.g., app's internal storage). This path should then be provided to `vsomeip` (e.g., by setting the `VSOMEIP_CONFIGURATION` environment variable programmatically before initializing `CommunicationManager`, or if `vsomeip` is built to check a specific path passed to it).

## 6. API Usage (C++)

Refer to header files in `comms_stack/include/` for detailed API.

### 6.1. Initialization
```cpp
#include "communication_manager.h"

// App name should match one defined in vsomeip.json
// Config path is optional if VSOMEIP_CONFIGURATION env var is set.
comms_stack::CommunicationManager::getInstance().init("CommsStackApp_PubSub" /*, "/path/to/vsomeip.json"*/);
6.2. Publishing
#include "publisher.h"
#include "common_messages.pb.h" // Your protobuf message

auto pub_app = comms_stack::CommunicationManager::getInstance().getVsomeipApplication();
// IDs would typically come from a config lookup via CommunicationManager based on "MyTopic"
auto publisher = std::make_shared<comms_stack::Publisher>(
    "MyTopic", pub_app, 0x1111, 0x0001, 0x9100, true);

comms_stack::protos::SimpleNotification msg;
msg.set_id(1);
msg.set_message_content("Hello World");
publisher->publish(msg); 
6.3. Subscribing
#include "subscriber.h"
#include "common_messages.pb.h"

void my_message_handler(const comms_stack::protos::SimpleNotification& message) {
    // Process message
}

auto sub_app = comms_stack::CommunicationManager::getInstance().getVsomeipApplication();
// IDs would typically come from a config lookup
auto subscriber = std::make_shared<comms_stack::Subscriber>(
    "MyTopic", sub_app, 0x1111, 0x0001, 0x9100, true);

subscriber->subscribe(my_message_handler);
// ...
// subscriber->unsubscribe(); 
6.4. RPC Client
#include "rpc_client.h"
#include "sample_rpc_service.pb.h" // Your generated RPC protos

auto rpc_app = comms_stack::CommunicationManager::getInstance().getVsomeipApplication();
// IDs would typically come from config lookup for "SampleRpc"
auto rpc_client = std::make_shared<comms_stack::RpcClient>(
    "SampleRpc", rpc_app, 0x2222, 0x0001);

comms_stack::protos::EchoRequest req;
req.set_request_message("Hello RPC");

std::future<comms_stack::protos::EchoResponse> future_resp = rpc_client->Echo(req);
// ... wait for future_resp and get result ...
6.5. RPC Server
#include "my_sample_rpc_impl.h" // Your implementation of protos::SampleRpc
#include "communication_manager.h"

auto service_impl = std::make_shared<comms_stack::MySampleRpcImpl>();
// IDs would typically come from config lookup for "SampleRpc"
comms_stack::CommunicationManager::getInstance().registerRpcService(
    "SampleRpc", 0x2222, 0x0001, service_impl);
6.6. Shutdown
comms_stack::CommunicationManager::getInstance().shutdown();
7. API Usage (Java - via JNI CommsStackBridge.java)
The CommsStackBridge.java class (located conceptually in app/src/main/java/com/example/commsstack/) provides the JNI interface.

// In your Android Activity or Service:
CommsStackBridge bridge = new CommsStackBridge();

// Initialization (copy config from assets first)
String configPath = copyAssetToInternalStorage("vsomeip_config/vsomeip_host.json", "vsomeip.json");
boolean initOk = bridge.nativeInit("CommsStackApp_RpcClient", configPath);

// Publish
bridge.nativePublishSimpleNotification("TestTopic", 123, "Hello from Java", System.currentTimeMillis()/1000);

// Subscribe
long subId = bridge.nativeSubscribeSimpleNotification("TestTopic", new CommsStackBridge.SimpleNotificationListener() {
    @Override
    public void onNotificationReceived(int id, String messageContent, long timestamp) {
        // Handle message on UI thread if needed
    }
    @Override
    public void onError(String errorMessage) { /* ... */ }
});
// ...
// bridge.nativeUnsubscribe(subId);

// RPC Call
bridge.nativeCallEcho("SampleRpc", "Java RPC Call", new CommsStackBridge.EchoResponseListener() {
    @Override
    public void onEchoResponse(String responseMessage) { /* ... */ }
    @Override
    public void onError(String errorMessage) { /* ... */ }
});

// Shutdown (e.g., in onDestroy)
// bridge.nativeShutdown();
8. Test Applications
The tests/ directory contains host-based C++ test applications:

publisher_test: Publishes messages on "TestTopic".
subscriber_test: Subscribes to "TestTopic" and prints received messages.
rpc_server_test: Registers and runs an instance of MySampleRpcImpl.
rpc_client_test: Calls methods on the SampleRpc service.
Running Host Tests:

Build the tests (see "Building for Host").
Set the VSOMEIP_CONFIGURATION environment variable.
Run the executables from the build output directory (e.g., build/tests/).
Start rpc_server_test first, then rpc_client_test.
Start publisher_test, then subscriber_test.
A conceptual sample Android application is also outlined, demonstrating JNI usage.

9. Troubleshooting
vsomeip Initialization Errors:
Ensure VSOMEIP_CONFIGURATION is set correctly and the JSON file is valid and accessible.
Check unicast IP in JSON; it should be a local IP address of your host/device.
Verify application names/IDs in init() match those in the JSON.
No Communication:
Check vsomeip logs for service discovery messages (offers, finds).
Ensure firewalls are not blocking UDP ports (for SD and unreliable traffic) or TCP ports (for reliable traffic). Default SD port is 30490.
Verify service/instance/method/event IDs match exactly between client/server and the JSON config.
JNI Issues (UnsatisfiedLinkError, crashes):
Ensure libcomms_stack_jni.so and its dependencies (libcomms_stack.so, etc.) are correctly packaged in the APK for the target ABI.
Verify JNI function signatures in C++ match Java native declarations precisely.
Check for mishandling of JNIEnv* or Java object references (local vs. global).
10. Future Enhancements & TODOs
Centralized Configuration Management: CommunicationManager should parse configuration and map string names to SOME/IP IDs internally.
Generic Subscriber Deserialization: Implement a type registry or factory mechanism for deserializing into arbitrary Protobuf types for generic subscribers.
RPC Method ID Management: Load/map method IDs from configuration rather than hardcoding.
Robust Error Handling: Implement more specific C++ exceptions and improve error propagation to JNI/Java.
JNI Callback Threading: Optimize threading for JNI callbacks (e.g., dedicated callback thread pool).
Automated Testing: Add unit tests (GoogleTest) and automated integration tests.
SOME/IP Reliability: Make TCP/UDP usage configurable per topic/method.
Security: Investigate and implement SOME/IP security features (e.g., TLS/DTLS) if required.
