package com.example.monitoring_danger;

import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;
import android.widget.*;

import java.io.*;
import java.util.ArrayList;

public class LogActivity extends AppCompatActivity {

    // 리스트
    ListView listLog;
    TCPManager.DataListener listener;
    ArrayList<String> logList = new ArrayList<>();
    ArrayAdapter<String> logAdapter;


    int count;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_log);

        count = getIntent().getIntExtra("count", 10);

        // 리스트 연결
        listLog = findViewById(R.id.listLog);

        logAdapter = new ArrayAdapter<>(this,
                android.R.layout.simple_list_item_1, logList);

        listLog.setAdapter(logAdapter);

        TCPManager tcp = TCPManager.getInstance();

        logList.clear();

        listener = line -> {
            runOnUiThread(() -> {
                if (line.startsWith("LOG:")) {
                    String msg = line.substring(4);
                    logList.add(msg);
                    logAdapter.notifyDataSetChanged();
                }
            });
        };

        tcp.addListener(listener);

        new Thread(() -> {
            tcp.requestLog(count);
        }).start();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        TCPManager.getInstance().removeListener(listener);
    }
}