package com.example.monitoring_danger;

import androidx.appcompat.app.AppCompatActivity;
import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.ListView;
import android.widget.TextView;

import java.io.*;
import java.util.ArrayList;

import com.github.mikephil.charting.charts.LineChart;
import com.github.mikephil.charting.data.*;

public class MainActivity extends AppCompatActivity {

    // 텍스트
    TextView txtHumidity, txtDust;
    ListView listWarn;
    ArrayList<String> warnList = new ArrayList<>();
    ArrayAdapter<String> warnAdapter;

    // 버튼
    Button btn10;
    Button btn50;
    Button btnAll;


    // 그래프
    LineChart lineChart;
    ArrayList<Entry> humidityEntries = new ArrayList<>();
    ArrayList<Entry> dustEntries = new ArrayList<>();

    LineDataSet humidityDataSet;
    LineDataSet dustDataSet;
    LineData lineData;
    int xIndex = 0;

    TCPManager.DataListener listener;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // 연결
        txtHumidity = findViewById(R.id.txtHumidity);
        txtDust = findViewById(R.id.txtDust);
        btn10 = findViewById(R.id.btn10);
        btn50 = findViewById(R.id.btn50);
        btnAll = findViewById(R.id.btnAll);
        lineChart = findViewById(R.id.lineChart);
        listWarn = findViewById(R.id.listWarn);

        warnAdapter = new ArrayAdapter<>(
                this,
                android.R.layout.simple_list_item_1,
                warnList
        );

        listWarn.setAdapter(warnAdapter);

        // 그래프 초기화
        humidityDataSet = new LineDataSet(humidityEntries, "Humidity");
        dustDataSet = new LineDataSet(dustEntries, "Dust");
        humidityDataSet.setColor(Color.BLUE);
        dustDataSet.setColor(Color.RED);
        lineData = new LineData(humidityDataSet, dustDataSet);
        lineChart.setData(lineData);
        lineChart.getDescription().setText("습도 / 미세먼지");
        lineChart.invalidate();

        // 버튼 → 로그 화면
        btn10.setOnClickListener(v -> {
            Intent intent = new Intent(MainActivity.this, LogActivity.class);
            intent.putExtra("count", 10);
            startActivity(intent);
        });

        btn50.setOnClickListener(v -> {
            Intent intent = new Intent(MainActivity.this, LogActivity.class);
            intent.putExtra("count", 50);
            startActivity(intent);
        });

        btnAll.setOnClickListener(v -> {
            Intent intent = new Intent(MainActivity.this, LogActivity.class);
            intent.putExtra("count",-1);
            startActivity(intent);
        });


        TCPManager tcp = TCPManager.getInstance();

        tcp.connect();

        listener = line -> {

            runOnUiThread(() -> {

                if (line.startsWith("HUM:")) {

                    String[] parts = line.split(",");

                    int humidity = Integer.parseInt(parts[0].split(":")[1].trim());
                    int dust = Integer.parseInt(parts[1].split(":")[1].trim());

                    txtHumidity.setText("습도 : " + humidity);
                    txtDust.setText("미세먼지 : " + dust);

                    //습도 임계치 - 600
                    if (humidity > 600) {
                        txtHumidity.setTextColor(getResources().getColor(android.R.color.holo_red_dark));
                    } else {
                        txtHumidity.setTextColor(getResources().getColor(android.R.color.black));
                    }

                    //미세먼지 임계치 - 300
                    if (dust > 300) {
                        txtDust.setTextColor(getResources().getColor(android.R.color.holo_red_dark));
                    } else {
                        txtDust.setTextColor(getResources().getColor(android.R.color.black));
                    }

                    humidityEntries.add(new Entry(xIndex, humidity));
                    dustEntries.add(new Entry(xIndex, dust));

                    xIndex++;

                    if (humidityEntries.size() > 20) {
                        humidityEntries.remove(0);
                        dustEntries.remove(0);
                    }

                    humidityDataSet.notifyDataSetChanged();
                    dustDataSet.notifyDataSetChanged();
                    lineChart.getData().notifyDataChanged();
                    lineChart.notifyDataSetChanged();
                    lineChart.invalidate();
                }

                else if (line.startsWith("WARN:")) {
                    String msg = line.substring(5);

                    warnList.add(msg);

                    if (warnList.size() > 50) {
                        warnList.remove(0);
                    }

                    warnAdapter.notifyDataSetChanged();
                    listWarn.setSelection(warnAdapter.getCount() - 1);
                }

            });
        };
        tcp.addListener(listener);
    }

    protected void onDestroy() {
        super.onDestroy();
        TCPManager.getInstance().removeListener(listener);
    }
}