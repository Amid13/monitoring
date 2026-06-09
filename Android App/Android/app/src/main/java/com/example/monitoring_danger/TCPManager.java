package com.example.monitoring_danger;

import java.io.*;
import java.net.Socket;
import java.util.ArrayList;

public class TCPManager {

    private static TCPManager instance;

    private Socket socket;
    private BufferedReader in;
    private PrintWriter out;

    private final String SERVER_IP = "10.10.108.169";
    private final int SERVER_PORT = 9000;

    private ArrayList<DataListener> listeners = new ArrayList<>();

    // 인터페이스
    public interface DataListener {
        void onReceive(String line);
    }

    // 싱글톤
    public static TCPManager getInstance() {
        if (instance == null) {
            instance = new TCPManager();
        }
        return instance;
    }

    // 연결
    public void connect() {
        new Thread(() -> {
            try {
                socket = new Socket(SERVER_IP, SERVER_PORT);
                in = new BufferedReader(new InputStreamReader(socket.getInputStream()));
                out = new PrintWriter(socket.getOutputStream(), true);

                String line;

                while ((line = in.readLine()) != null) {
                    for (DataListener l : listeners) {
                        l.onReceive(line);
                    }
                }

            } catch (Exception e) {
                e.printStackTrace();
            }
        }).start();
    }

    // 리스너 등록
    public void addListener(DataListener listener) {
        listeners.add(listener);
    }

    public void removeListener(DataListener listener) {
        listeners.remove(listener);
    }

    // 로그 요청
    public void requestLog(int count) {
        if (out != null) {
            if (count == -1) {
                out.println("GET_LOG ALL");
            } else {
                out.println("GET_LOG " + count);
            }
        }
    }
}