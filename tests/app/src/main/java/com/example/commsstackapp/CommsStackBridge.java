package com.example.commsstackapp;

import android.car.Car;
import android.car.hardware.property.CarPropertyManager;
import android.content.Context;
import android.util.Log;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class CommsStackBridge {

    private static final String TAG = "CommsStackBridge";
    private Car mCar;
    private CarPropertyManager mCarPropertyManager;

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

    // VHAL Integration
    public native void nativeOnVhalPropertyUpdate(int propertyId, int areaId, boolean boolValue, float floatValue, int intValue, String stringValue, byte[] bytesValue, long int64Value);

    private final CarPropertyManager.CarPropertyEventCallback mCarPropertyEventCallback =
            new CarPropertyManager.CarPropertyEventCallback() {
                @Override
                public void onChangeEvent(android.car.hardware.CarPropertyValue<?> value) {
                    int propertyId = value.getPropertyId();
                    int areaId = value.getAreaId();
                    Object propValue = value.getValue();
                    boolean boolValue = false;
                    float floatValue = 0;
                    int intValue = 0;
                    String stringValue = null;
                    byte[] bytesValue = null;
                    long int64Value = 0;

                    if (propValue instanceof Boolean) {
                        boolValue = (Boolean) propValue;
                    } else if (propValue instanceof Float) {
                        floatValue = (Float) propValue;
                    } else if (propValue instanceof Integer) {
                        intValue = (Integer) propValue;
                    } else if (propValue instanceof String) {
                        stringValue = (String) propValue;
                    } else if (propValue instanceof byte[]) {
                        bytesValue = (byte[]) propValue;
                    } else if (propValue instanceof Long) {
                        int64Value = (Long) propValue;
                    }

                    nativeOnVhalPropertyUpdate(propertyId, areaId, boolValue, floatValue, intValue, stringValue, bytesValue, int64Value);
                }

                @Override
                public void onErrorEvent(int propId, int areaId) {
                    Log.w(TAG, "Received error car property event for "
                            + "propId=" + propId + ", areaId=" + areaId);
                }
            };


    public boolean connectToCar(Context context) {
        final CountDownLatch carServiceReady = new CountDownLatch(1);
        mCar = Car.createCar(context, null, Car.CAR_WAIT_TIMEOUT_WAIT_FOREVER, (car, ready) -> {
            if (ready) {
                mCarPropertyManager = (CarPropertyManager) car.getCarManager(Car.PROPERTY_SERVICE);
                carServiceReady.countDown();
            }
        });

        try {
            if (!carServiceReady.await(Car.CAR_WAIT_TIMEOUT_WAIT_FOREVER, TimeUnit.MILLISECONDS)) {
                Log.e(TAG, "Failed to connect to car service");
                return false;
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            Log.e(TAG, "Car service connection interrupted", e);
            return false;
        }
        return true;
    }

    public void disconnectFromCar() {
        if (mCar != null) {
            mCar.disconnect();
            mCar = null;
        }
    }

    public void subscribeToVhalProperty(int propertyId, int areaId, float sampleRate) {
        if (mCarPropertyManager != null) {
            mCarPropertyManager.registerCallback(mCarPropertyEventCallback, propertyId, sampleRate);
        }
    }

    public Object getVhalProperty(int propertyId, int areaId) {
        if (mCarPropertyManager != null) {
            try {
                return mCarPropertyManager.getProperty(propertyId, areaId).getValue();
            } catch (Exception e) {
                Log.e(TAG, "Failed to get VHAL property", e);
            }
        }
        return null;
    }

    public void setVhalProperty(int propertyId, int areaId, Object value) {
        if (mCarPropertyManager != null) {
            try {
                if (value instanceof Boolean) {
                    mCarPropertyManager.setBooleanProperty(propertyId, areaId, (Boolean) value);
                } else if (value instanceof Float) {
                    mCarPropertyManager.setFloatProperty(propertyId, areaId, (Float) value);
                } else if (value instanceof Integer) {
                    mCarPropertyManager.setIntProperty(propertyId, areaId, (Integer) value);
                } else {
                    Log.e(TAG, "Unsupported property type: " + value.getClass().getName());
                }
            } catch (Exception e) {
                Log.e(TAG, "Failed to set VHAL property", e);
            }
        }
    }
}