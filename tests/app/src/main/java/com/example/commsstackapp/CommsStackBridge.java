ackage com.example.commsstackapp; 

public class CommsStackBridge {

    static {
        System.loadLibrary("comms_stack_jni"); 
    }

    public native boolean nativeInit(String appName, String configPath);
    public native void nativeShutdown();

    public native boolean nativePublishSimpleNotification(String topicName, int id, String messageContent, long timestamp);

    public interface SimpleNotificationListener {
        void onNotificationReceived(int id, String messageContent, long timestamp);
        void onError(String errorMessage);
    }
    public native long nativeSubscribeSimpleNotification(String topicName, SimpleNotificationListener listener);
    public native void nativeUnsubscribe(long subscriptionId);

    public interface EchoResponseListener {
        void onEchoResponse(String responseMessage);
        void onError(String errorMessage);
    }
    public native void nativeCallEcho(String serviceName, String requestMessage, EchoResponseListener listener);

    public interface AddResponseListener {
        void onAddResponse(int sum);
        void onError(String errorMessage);
    }
    public native void nativeCallAdd(String serviceName, int a, int b, AddResponseListener listener);
}