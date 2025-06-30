package com.example.commsstackapp;

import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MainActivity extends AppCompatActivity {

    private static final String TAG = "CommsStackDemoApp";
    private CommsStackBridge commsBridge;
    private TextView outputTextView;
    private ScrollView outputScrollView;
    private EditText topicEditText, messageEditText, serviceNameEditText, echoMessageEditText, addAEditText, addBEditText;
    private Button initButton, publishButton, subscribeButton, unsubscribeButton, rpcEchoButton, rpcAddButton, clearLogButton;

    private long currentSubscriptionId = -1;
    private final Handler uiHandler = new Handler(Looper.getMainLooper());
    private final ExecutorService backgroundExecutor = Executors.newSingleThreadExecutor();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main); 

        commsBridge = new CommsStackBridge();

        outputTextView = findViewById(R.id.outputTextView);
        outputScrollView = findViewById(R.id.outputScrollView);
        topicEditText = findViewById(R.id.topicEditText);
        messageEditText = findViewById(R.id.messageEditText);
        serviceNameEditText = findViewById(R.id.serviceNameEditText);
        echoMessageEditText = findViewById(R.id.echoMessageEditText);
        addAEditText = findViewById(R.id.addAEditText);
        addBEditText = findViewById(R.id.addBEditText);

        initButton = findViewById(R.id.initButton);
        publishButton = findViewById(R.id.publishButton);
        subscribeButton = findViewById(R.id.subscribeButton);
        unsubscribeButton = findViewById(R.id.unsubscribeButton);
        rpcEchoButton = findViewById(R.id.rpcEchoButton);
        rpcAddButton = findViewById(R.id.rpcAddButton);
        clearLogButton = findViewById(R.id.clearLogButton);

        topicEditText.setText("TestTopic");
        serviceNameEditText.setText("SampleRpc");
        echoMessageEditText.setText("Hello from Android!");
        addAEditText.setText("123");
        addBEditText.setText("456");

        initButton.setOnClickListener(v -> backgroundExecutor.execute(this::initializeCommsStack));
        publishButton.setOnClickListener(v -> backgroundExecutor.execute(this::publishMessage));
        subscribeButton.setOnClickListener(v -> backgroundExecutor.execute(this::subscribeToTopic));
        unsubscribeButton.setOnClickListener(v -> backgroundExecutor.execute(this::unsubscribeFromTopic));
        rpcEchoButton.setOnClickListener(v -> backgroundExecutor.execute(this::callRpcEcho));
        rpcAddButton.setOnClickListener(v -> backgroundExecutor.execute(this::callRpcAdd));
        clearLogButton.setOnClickListener(v -> outputTextView.setText(""));

        logToUi("App started. Press INIT.");
    }

    private String copyAssetToInternalStorage(String assetNameInAssetsFolder, String targetFileName) {
        File file = new File(getFilesDir(), targetFileName);
        try (InputStream in = getAssets().open(assetNameInAssetsFolder);
             OutputStream out = new FileOutputStream(file)) {
            byte[] buffer = new byte[1024];
            int read;
            while ((read = in.read(buffer)) != -1) {
                out.write(buffer, 0, read);
            }
            logToUi("Copied asset '" + assetNameInAssetsFolder + "' to: " + file.getAbsolutePath());
            return file.getAbsolutePath();
        } catch (IOException e) {
            Log.e(TAG, "Failed to copy asset: " + assetNameInAssetsFolder, e);
            logToUi("Error copying config: " + e.getMessage());
            return null;
        }
    }

    private void initializeCommsStack() {
        String configPath = copyAssetToInternalStorage("vsomeip_config/vsomeip_host.json", "app_vsomeip.json");
        if (configPath == null) {
            logToUi("CRITICAL: Failed to prepare vsomeip config file.");
            return;
        }
        boolean success = commsBridge.nativeInit("CommsStackApp_RpcClient", configPath); 
        logToUi("Native Init success: " + success);
        runOnUiThread(() -> {
            initButton.setEnabled(!success);
            publishButton.setEnabled(success);
            subscribeButton.setEnabled(success);
            rpcEchoButton.setEnabled(success);
            rpcAddButton.setEnabled(success);
        });
    }

    private void publishMessage() {
        String topic = topicEditText.getText().toString();
        String message = messageEditText.getText().toString();
        if (topic.isEmpty()) {
            logToUi("Publish Error: Topic is empty.");
            return;
        }
        int id = (int) (System.currentTimeMillis() % 10000);
        long timestamp = System.currentTimeMillis() / 1000;
        logToUi("Attempting to publish to " + topic + ": '" + message + "'");
        boolean success = commsBridge.nativePublishSimpleNotification(topic, id, message, timestamp);
        logToUi("Publish to " + topic + " success: " + success);
    }

    private void subscribeToTopic() {
        if (currentSubscriptionId != -1) {
            logToUi("Already subscribed. Unsubscribe first.");
            return;
        }
        String topic = topicEditText.getText().toString();
        if (topic.isEmpty()) {
            logToUi("Subscribe Error: Topic is empty.");
            return;
        }
        logToUi("Attempting to subscribe to: " + topic);
        currentSubscriptionId = commsBridge.nativeSubscribeSimpleNotification(topic, new CommsStackBridge.SimpleNotificationListener() {
            @Override
            public void onNotificationReceived(int id, String messageContent, long timestamp) {
                logToUi("SUB CB [Topic: " + topic + "]: ID=" + id + ", Msg='" + messageContent + "', TS=" + timestamp);
            }
            @Override
            public void onError(String errorMessage) {
                logToUi("SUB CB ERROR [Topic: " + topic + "]: " + errorMessage);
            }
        });
        logToUi("Subscription call for " + topic + " returned ID: " + currentSubscriptionId);
        runOnUiThread(() -> unsubscribeButton.setEnabled(currentSubscriptionId >= 0));
    }

    private void unsubscribeFromTopic() {
        if (currentSubscriptionId == -1) {
            logToUi("Not currently subscribed.");
            return;
        }
        logToUi("Attempting to unsubscribe from ID: " + currentSubscriptionId);
        commsBridge.nativeUnsubscribe(currentSubscriptionId);
        logToUi("Unsubscribed from ID: " + currentSubscriptionId);
        currentSubscriptionId = -1;
        runOnUiThread(() -> unsubscribeButton.setEnabled(false));
    }

    private void callRpcEcho() {
        String service = serviceNameEditText.getText().toString();
        String message = echoMessageEditText.getText().toString();
        if (service.isEmpty()) {
            logToUi("RPC Echo Error: Service name is empty.");
            return;
        }
        logToUi("Attempting RPC Echo to " + service + " with msg: '" + message + "'");
        commsBridge.nativeCallEcho(service, message, new CommsStackBridge.EchoResponseListener() {
            @Override
            public void onEchoResponse(String responseMessage) {
                logToUi("RPC Echo CB: Response='" + responseMessage + "'");
            }
            @Override
            public void onError(String errorMessage) {
                 logToUi("RPC Echo CB ERROR: " + errorMessage);
            }
        });
    }
    
    private void callRpcAdd() {
        String service = serviceNameEditText.getText().toString();
        if (service.isEmpty()) {
            logToUi("RPC Add Error: Service name is empty.");
            return;
        }
        try {
            int a = Integer.parseInt(addAEditText.getText().toString());
            int b = Integer.parseInt(addBEditText.getText().toString());
            logToUi("Attempting RPC Add to " + service + " with a=" + a + ", b=" + b);
            commsBridge.nativeCallAdd(service, a, b, new CommsStackBridge.AddResponseListener() {
                @Override
                public void onAddResponse(int sum) {
                     logToUi("RPC Add CB: Sum=" + sum);
                }
                @Override
                public void onError(String errorMessage) {
                    logToUi("RPC Add CB ERROR: " + errorMessage);
                }
            });
        } catch (NumberFormatException e) {
            logToUi("RPC Add Error: Invalid numbers.");
        }
    }

    private void logToUi(String message) {
        Log.d(TAG, message); 
        uiHandler.post(() -> {
            String currentText = outputTextView.getText().toString();
            if (outputTextView.getLineCount() > 100) { 
                int lineEnd = currentText.indexOf('\n', currentText.length() / 2);
                if (lineEnd != -1) currentText = currentText.substring(lineEnd + 1);
            }
            outputTextView.append("\n" + message);
            outputScrollView.post(() -> outputScrollView.fullScroll(View.FOCUS_DOWN));
        });
    }

    @Override
    protected void onDestroy() {
        logToUi("onDestroy: Shutting down native stack.");
        backgroundExecutor.execute(() -> {
            if (currentSubscriptionId != -1) {
                commsBridge.nativeUnsubscribe(currentSubscriptionId);
                currentSubscriptionId = -1;
                 logToUi("Unsubscribed during onDestroy.");
            }
            commsBridge.nativeShutdown();
            logToUi("Native shutdown completed.");
        });
        backgroundExecutor.shutdown();
        super.onDestroy();
    }
}