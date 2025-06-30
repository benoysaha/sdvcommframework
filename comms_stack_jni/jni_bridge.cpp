// comms_stack_jni/jni_bridge.cpp
#include <jni.h>
#include <string>
#include <vector>
#include <iostream>
#include <map>
#include <mutex>
#include <thread> // For detaching native threads if callbacks are on them
#include <future> // For std::future
#include <stdexcept> // For std::runtime_error

#include "communication_manager.h" // From comms_stack_lib
#include "publisher.h"
#include "subscriber.h"
#include "rpc_client.h"
#include "common_messages.pb.h"    // For SimpleNotification
#include "sample_rpc_service.pb.h" // For EchoRequest/Response, AddRequest/Response

// JNI OnLoad / OnUnload (optional but good practice)
JavaVM* g_java_vm = nullptr;
static std::mutex g_vm_mutex; // Mutex for g_java_vm initialization

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    std::lock_guard<std::mutex> lock(g_vm_mutex);
    g_java_vm = vm;
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    std::cout << "JNI_OnLoad called successfully" << std::endl;
    return JNI_VERSION_1_6;
}

void JNI_OnUnload(JavaVM* vm, void* reserved) {
    std::cout << "JNI_OnUnload called" << std::endl;
    std::lock_guard<std::mutex> lock(g_vm_mutex);
    g_java_vm = nullptr;
}

// Helper to get JNIEnv (and attach if necessary)
// Returns a struct containing the env and a flag if attachment occurred
struct JniEnvContext {
    JNIEnv* env = nullptr;
    bool attached = false;
};

JniEnvContext getJniEnv() {
    JniEnvContext context;
    std::lock_guard<std::mutex> lock(g_vm_mutex); // Protect g_java_vm
    if (!g_java_vm) return context;

    int status = g_java_vm->GetEnv(reinterpret_cast<void**>(&context.env), JNI_VERSION_1_6);
    if (status == JNI_EDETACHED) {
        std::cout << "JNI: Thread not attached, attempting to attach..." << std::endl;
        if (g_java_vm->AttachCurrentThread(&context.env, nullptr) != JNI_OK) {
            std::cerr << "JNI: Failed to attach current thread" << std::endl;
            context.env = nullptr; // Ensure env is null on failure
        } else {
            context.attached = true;
            std::cout << "JNI: Thread attached successfully." << std::endl;
        }
    } else if (status != JNI_OK) { // JNI_EVERSION or other error
        std::cerr << "JNI: GetEnv failed with status: " << status << std::endl;
        context.env = nullptr; // Ensure env is null on failure
    }
    return context;
}

void detachCurrentThreadIfNeeded(bool attached) {
    std::lock_guard<std::mutex> lock(g_vm_mutex); // Protect g_java_vm
    if (attached && g_java_vm) {
        g_java_vm->DetachCurrentThread();
        std::cout << "JNI: Detached current thread." << std::endl;
    }
}


// --- Helper to convert jstring to std::string ---
std::string jstringToStdString(JNIEnv* env, jstring jStr) {
    if (!env || !jStr) return "";
    const char* chars = env->GetStringUTFChars(jStr, nullptr);
    if (!chars) return ""; // Check for null if GetStringUTFChars fails
    std::string stdStr = chars;
    env->ReleaseStringUTFChars(jStr, chars);
    return stdStr;
}

// --- Global state for JNI (callbacks, etc.) ---
struct SubscriptionContext {
    jobject listener_global_ref;
    std::shared_ptr<comms_stack::Subscriber> subscriber;
    jmethodID on_notification_mid;
    jmethodID on_error_mid;
};
std::map<long, SubscriptionContext> g_subscriptions;
long g_next_subscription_id = 1;
std::mutex g_subscriptions_mutex;


// --- JNI Method Implementations ---
extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_example_commsstack_CommsStackBridge_nativeInit(JNIEnv* env, jobject /* this */, jstring jAppName, jstring jConfigPath) {
    std::string appName = jstringToStdString(env, jAppName);
    std::string configPath = jstringToStdString(env, jConfigPath);
    std::cout << "JNI: nativeInit called. AppName: " << appName << ", ConfigPath: " << configPath << std::endl;
    return comms_stack::CommunicationManager::getInstance().init(appName, configPath);
}

JNIEXPORT void JNICALL
Java_com_example_commsstack_CommsStackBridge_nativeShutdown(JNIEnv* env, jobject /* this */) {
    std::cout << "JNI: nativeShutdown called." << std::endl;
    comms_stack::CommunicationManager::getInstance().shutdown();

    std::lock_guard<std::mutex> lock(g_subscriptions_mutex);
    JniEnvContext ctx = getJniEnv(); // Need env for DeleteGlobalRef
    if (ctx.env) {
        for (auto const& [id, context] : g_subscriptions) {
            if (context.listener_global_ref) {
                ctx.env->DeleteGlobalRef(context.listener_global_ref);
            }
        }
        detachCurrentThreadIfNeeded(ctx.attached);
    }
    g_subscriptions.clear();
    std::cout << "JNI: Cleared global subscription references." << std::endl;
}

JNIEXPORT jboolean JNICALL
Java_com_example_commsstack_CommsStackBridge_nativePublishSimpleNotification(
    JNIEnv* env, jobject /* this */, jstring jTopicName, jint id, jstring jMessageContent, jlong timestamp) {

    std::string topicName = jstringToStdString(env, jTopicName);
    std::string messageContent = jstringToStdString(env, jMessageContent);

    auto& comm_mgr = comms_stack::CommunicationManager::getInstance();
    auto vsomeip_app = comm_mgr.getVsomeipApplication();
    if (!vsomeip_app) return false;

    const uint16_t service_id = 0x1111;
    const uint16_t instance_id = 0x0001;
    const uint16_t eventgroup_id = 0x9100;

    // This should use CommunicationManager::getPublisher which handles config lookup and caching.
    // For now, direct construction for test.
    comms_stack::Publisher publisher(topicName, vsomeip_app, service_id, instance_id, eventgroup_id, true);
     if(!publisher.isOffered()){
         std::cout << "JNI: Publisher for " << topicName << " trying to offer event..." << std::endl;
         // Allow to proceed, publish might work if vsomeip is just slow to confirm offer
     }

    comms_stack::protos::SimpleNotification msg;
    msg.set_id(static_cast<uint32_t>(id));
    msg.set_message_content(messageContent);
    msg.set_timestamp(static_cast<uint64_t>(timestamp));

    std::cout << "JNI: Publishing to topic: " << topicName << ", ID: " << id << std::endl;
    return publisher.publish(msg);
}


JNIEXPORT jlong JNICALL
Java_com_example_commsstack_CommsStackBridge_nativeSubscribeSimpleNotification(
    JNIEnv* env, jobject /* this */, jstring jTopicName, jobject jListener) {

    std::string topicName = jstringToStdString(env, jTopicName);
    if (!jListener) {
        std::cerr << "JNI: Listener cannot be null for subscription to " << topicName << std::endl;
        return -1;
    }
    std::cout << "JNI: nativeSubscribeSimpleNotification for topic: " << topicName << std::endl;

    auto& comm_mgr = comms_stack::CommunicationManager::getInstance();
    auto vsomeip_app = comm_mgr.getVsomeipApplication();
    if (!vsomeip_app) {
        std::cerr << "JNI: vsomeip_app not available for subscription." << std::endl;
        return -2;
    }

    const uint16_t service_id = 0x1111;
    const uint16_t instance_id = 0x0001;
    const uint16_t eventgroup_id = 0x9100;

    // This should use CommunicationManager::getSubscriber which handles config lookup and caching.
    // For now, direct construction for test.
    auto subscriber = std::make_shared<comms_stack::Subscriber>(
        topicName, vsomeip_app, service_id, instance_id, eventgroup_id, true);

    jobject listener_global_ref = env->NewGlobalRef(jListener);
    if (!listener_global_ref) { /* error handling */ return -3; }

    jclass listener_class = env->GetObjectClass(listener_global_ref);
    if (!listener_class) { /* error handling */ env->DeleteGlobalRef(listener_global_ref); return -4; }
    jmethodID on_notification_mid = env->GetMethodID(listener_class, "onNotificationReceived", "(ILjava/lang/String;J)V");
    jmethodID on_error_mid = env->GetMethodID(listener_class, "onError", "(Ljava/lang/String;)V");
    env->DeleteLocalRef(listener_class);

    if (!on_notification_mid || !on_error_mid) { /* error handling */ env->DeleteGlobalRef(listener_global_ref); return -5; }

    bool success = subscriber->subscribe(
        [listener_global_ref, on_notification_mid, on_error_mid, topicName](const comms_stack::protos::SimpleNotification& msg) {
            JniEnvContext ctx = getJniEnv();
            if (!ctx.env) { std::cerr << "JNI CB: Failed to get JNIEnv for " << topicName << std::endl; return; }

            jstring java_content_str = ctx.env->NewStringUTF(msg.message_content().c_str());
            ctx.env->CallVoidMethod(listener_global_ref, on_notification_mid,
                                static_cast<jint>(msg.id()), java_content_str, static_cast<jlong>(msg.timestamp()));
            if (ctx.env->ExceptionCheck()) { ctx.env->ExceptionDescribe(); ctx.env->ExceptionClear(); }
            if (java_content_str) ctx.env->DeleteLocalRef(java_content_str);
            detachCurrentThreadIfNeeded(ctx.attached);
        }
    );

    if (!success) { env->DeleteGlobalRef(listener_global_ref); return -6; }

    std::lock_guard<std::mutex> lock(g_subscriptions_mutex);
    long current_id = g_next_subscription_id++;
    g_subscriptions[current_id] = {listener_global_ref, subscriber, on_notification_mid, on_error_mid};
    std::cout << "JNI: Subscribed to " << topicName << " with sub ID: " << current_id << std::endl;
    return current_id;
}

JNIEXPORT void JNICALL
Java_com_example_commsstack_CommsStackBridge_nativeUnsubscribe(JNIEnv* env, jobject /* this */, jlong subscriptionId) {
    std::cout << "JNI: nativeUnsubscribe called for ID: " << subscriptionId << std::endl;
    std::lock_guard<std::mutex> lock(g_subscriptions_mutex);
    auto it = g_subscriptions.find(subscriptionId);
    if (it != g_subscriptions.end()) {
        if (it->second.subscriber) { it->second.subscriber->unsubscribe(); }
        JniEnvContext ctx = getJniEnv(); // Env needed for DeleteGlobalRef
        if (it->second.listener_global_ref && ctx.env) {
             ctx.env->DeleteGlobalRef(it->second.listener_global_ref);
        }
        if(ctx.env) detachCurrentThreadIfNeeded(ctx.attached); // Detach if getJniEnv attached
        g_subscriptions.erase(it);
        std::cout << "JNI: Unsubscribed and cleaned up for ID: " << subscriptionId << std::endl;
    } else { /* error handling */ }
}

JNIEXPORT void JNICALL
Java_com_example_commsstack_CommsStackBridge_nativeCallEcho(
    JNIEnv* env, jobject /* this */, jstring jServiceName, jstring jRequestMessage, jobject jListener) {

    std::string serviceName = jstringToStdString(env, jServiceName);
    std::string requestMessage = jstringToStdString(env, jRequestMessage);
    if (!jListener) { /* error handling */ return; }
    std::cout << "JNI: nativeCallEcho for service: " << serviceName << " msg: " << requestMessage << std::endl;

    auto& comm_mgr = comms_stack::CommunicationManager::getInstance();
    auto vsomeip_app = comm_mgr.getVsomeipApplication();
    if (!vsomeip_app) { /* error handling with callback */ return; }

    const uint16_t service_id = 0x2222;
    const uint16_t instance_id = 0x0001;

    // This should use CommunicationManager::getRpcClient which handles config lookup and caching.
    // For now, direct construction for test.
    auto rpc_client = std::make_shared<comms_stack::RpcClient>(serviceName, vsomeip_app, service_id, instance_id);

    jobject listener_global_ref = env->NewGlobalRef(jListener); // Must manage this ref!
    jclass listener_class = env->GetObjectClass(listener_global_ref);
    jmethodID on_response_mid = env->GetMethodID(listener_class, "onEchoResponse", "(Ljava/lang/String;)V");
    jmethodID on_error_mid = env->GetMethodID(listener_class, "onError", "(Ljava/lang/String;)V");
    env->DeleteLocalRef(listener_class);

    if (!on_response_mid || !on_error_mid) { /* error handling */ if(listener_global_ref) env->DeleteGlobalRef(listener_global_ref); return;}

    if (!rpc_client->isServiceAvailable()) { // Basic check
        std::cerr << "JNI: RPC service " << serviceName << " not immediately available." << std::endl;
        jstring error_msg_jstr = env->NewStringUTF("Service not available");
        env->CallVoidMethod(listener_global_ref, on_error_mid, error_msg_jstr);
        if(error_msg_jstr) env->DeleteLocalRef(error_msg_jstr);
        env->DeleteGlobalRef(listener_global_ref);
        return;
    }

    comms_stack::protos::EchoRequest req;
    req.set_request_message(requestMessage);
    std::future<comms_stack::protos::EchoResponse> future_res = rpc_client->Echo(req);

    std::thread([future_res = std::move(future_res), listener_global_ref, on_response_mid, on_error_mid, serviceName, rpc_client /*keep client alive*/]() mutable {
        JniEnvContext ctx = getJniEnv();
        if (!ctx.env) {
            std::cerr << "JNI RPC CB: Failed to get JNIEnv for Echo on " << serviceName << std::endl;
            std::lock_guard<std::mutex> lock(g_vm_mutex); // Protect g_java_vm
            if(listener_global_ref && g_java_vm) g_java_vm->DeleteGlobalRef(listener_global_ref);
            return;
        }
        try {
            comms_stack::protos::EchoResponse res = future_res.get();
            jstring java_res_msg = ctx.env->NewStringUTF(res.response_message().c_str());
            ctx.env->CallVoidMethod(listener_global_ref, on_response_mid, java_res_msg);
            if(java_res_msg) ctx.env->DeleteLocalRef(java_res_msg);
        } catch (const std::exception& e) {
            jstring error_msg_jstr = ctx.env->NewStringUTF(e.what());
            ctx.env->CallVoidMethod(listener_global_ref, on_error_mid, error_msg_jstr);
            if(error_msg_jstr) ctx.env->DeleteLocalRef(error_msg_jstr);
        }
        if (ctx.env->ExceptionCheck()) { ctx.env->ExceptionDescribe(); ctx.env->ExceptionClear(); }
        ctx.env->DeleteGlobalRef(listener_global_ref);
        detachCurrentThreadIfNeeded(ctx.attached);
    }).detach();
}

JNIEXPORT void JNICALL Java_com_example_commsstack_CommsStackBridge_nativeCallAdd(
    JNIEnv* env, jobject thiz, jstring jServiceName, jint a, jint b, jobject jListener) {
    // Implementation would be very similar to nativeCallEcho,
    // using AddRequest, AddResponse, and the corresponding AddResponseListener methods.
    // For brevity, not fully implemented here but structure is the same.
    std::string serviceName = jstringToStdString(env, jServiceName);
    std::cout << "JNI: nativeCallAdd for " << serviceName << " (" << a << ", " << b << ") - NOT FULLY IMPLEMENTED IN THIS EXAMPLE" << std::endl;

    if (!jListener) return;
    jobject listener_global_ref = env->NewGlobalRef(jListener);
    jclass listener_class = env->GetObjectClass(listener_global_ref);
    jmethodID on_error_mid = env->GetMethodID(listener_class, "onError", "(Ljava/lang/String;)V");
    env->DeleteLocalRef(listener_class);

    if(on_error_mid){
        jstring error_msg_jstr = env->NewStringUTF("Add RPC not fully implemented in JNI bridge example.");
        env->CallVoidMethod(listener_global_ref, on_error_mid, error_msg_jstr);
        if(error_msg_jstr) env->DeleteLocalRef(error_msg_jstr);
    }
    if(listener_global_ref) env->DeleteGlobalRef(listener_global_ref);
}


} // extern "C"
