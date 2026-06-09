#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "resource.h"

#pragma comment(lib,"ws2_32.lib")

#define SERVER_IP "10.10.108.160"
#define SERVER_PORT 9000

#define HUMIDITY_LIMIT 600
#define DUST_LIMIT     300

#define WM_UPDATE_SENSOR (WM_USER + 1)

SOCKET sock;
HWND g_hDlg;

#define GRAPH_SIZE 50

int humData[GRAPH_SIZE];
int dustData[GRAPH_SIZE];
int dataCount = 0;

int g_humidity = 0;
int g_dust = 0;

typedef struct
{
    int humidity;
    int dust;
} SensorData;

DWORD WINAPI recvThread(LPVOID arg)
{
    char buffer[512];
    char line[256];
    int linePos = 0;

    while (1)
    {
        int n = recv(sock, buffer, sizeof(buffer) - 1, 0);

        if (n <= 0) continue;

        for (int i = 0; i < n; i++)
        {
            if (buffer[i] == '\n')
            {
                line[linePos] = 0;

                if (strncmp(line, "HUM:", 4) == 0)
                {
                    SensorData* data = (SensorData*)malloc(sizeof(SensorData));

                    if (sscanf(line, "HUM:%d, DUST:%d",
                        &data->humidity,
                        &data->dust) == 2)
                    {
                        PostMessage(g_hDlg, WM_UPDATE_SENSOR, 0, (LPARAM)data);
                    }
                    else
                    {
                        free(data);
                    }
                }

                else if (strncmp(line, "WARN:", 5) == 0)
                {
                    char* log = line + 5;

                    wchar_t wlog[256];
                    MultiByteToWideChar(CP_UTF8, 0, log, -1, wlog, 256);

                    SendDlgItemMessage(g_hDlg, IDC_WARN_LOG, LB_ADDSTRING, 0, (LPARAM)wlog);

                    int count = SendDlgItemMessage(g_hDlg, IDC_WARN_LOG, LB_GETCOUNT, 0, 0);
                    SendDlgItemMessage(g_hDlg, IDC_WARN_LOG, LB_SETTOPINDEX, count - 1, 0);
                }

                else if (strncmp(line, "LOG:", 4) == 0)
                {
                    char* log = line + 4;

                    wchar_t wlog[256];
                    MultiByteToWideChar(CP_UTF8, 0, log, -1, wlog, 256);

                    SendDlgItemMessage(g_hDlg, IDC_QUERY_LOG, LB_ADDSTRING, 0, (LPARAM)wlog);

                    int count = SendDlgItemMessage(g_hDlg, IDC_QUERY_LOG, LB_GETCOUNT, 0, 0);
                    SendDlgItemMessage(g_hDlg, IDC_QUERY_LOG, LB_SETTOPINDEX, count - 1, 0);
                }

                linePos = 0;
            }
            else if (buffer[i] != '\r')
            {
                if (linePos < sizeof(line) - 1)
                    line[linePos++] = buffer[i];
            }
        }
    }
}

void connectServer()
{
    WSADATA wsa;
    struct sockaddr_in server;

    WSAStartup(MAKEWORD(2, 2), &wsa);

    sock = socket(AF_INET, SOCK_STREAM, 0);

    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);
    server.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) == 0)
    {
        CreateThread(NULL, 0, recvThread, NULL, 0, NULL);
    }
}

INT_PTR CALLBACK DialogProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {

    case WM_INITDIALOG:

        g_hDlg = hDlg;

        connectServer();

        return TRUE;


    case WM_UPDATE_SENSOR:
    {
        SensorData* data = (SensorData*)lParam;

        g_humidity = data->humidity;
        g_dust = data->dust;

        WCHAR text[64];

        swprintf(text, 64, L"%d", data->humidity);
        SetDlgItemText(hDlg, IDC_HUMIDITY, text);

        swprintf(text, 64, L"%d", data->dust);
        SetDlgItemText(hDlg, IDC_DUST, text);

        if (dataCount < GRAPH_SIZE)
        {
            humData[dataCount] = data->humidity;
            dustData[dataCount] = data->dust;
            dataCount++;
        }
        else
        {
            for (int i = 1; i < GRAPH_SIZE; i++)
            {
                humData[i - 1] = humData[i];
                dustData[i - 1] = dustData[i];
            }
            humData[GRAPH_SIZE - 1] = data->humidity;
            dustData[GRAPH_SIZE - 1] = data->dust;
        }

        InvalidateRect(hDlg, NULL, TRUE);

        free(data);
        return TRUE;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hDlg, &ps);

        RECT logRect;
        GetWindowRect(GetDlgItem(hDlg, IDC_WARN_LOG), &logRect);

        POINT pt1 = { logRect.left, logRect.top };
        POINT pt2 = { logRect.right, logRect.bottom };

        ScreenToClient(hDlg, &pt1);
        ScreenToClient(hDlg, &pt2);

        logRect.left = pt1.x;
        logRect.top = pt1.y;
        logRect.right = pt2.x;
        logRect.bottom = pt2.y;

        int startX = logRect.left + 30;
        int startY = logRect.top - 120;
        int width = (logRect.right - logRect.left) - 30;
        int height = 80;

        Rectangle(hdc, startX, startY, startX + width, startY + height);

        int maxVal = 1000;

        for (int i = 0; i <= 5; i++)
        {
            int value = i * (maxVal / 5);
            int y = startY + height - (value * height / maxVal);

            MoveToEx(hdc, startX, y, NULL);
            LineTo(hdc, startX + width, y);

            wchar_t buf[16];
            swprintf(buf, 16, L"%d", value);

            TextOut(hdc, startX - 35, y - 8, buf, wcslen(buf));
        }

        // Humidity 기준선 (파랑 점선)
        int yH = startY + height - (HUMIDITY_LIMIT * height / maxVal);
        HPEN humLimitPen = CreatePen(PS_DASH, 1, RGB(0, 0, 255));
        SelectObject(hdc, humLimitPen);
        MoveToEx(hdc, startX, yH, NULL);
        LineTo(hdc, startX + width, yH);
        DeleteObject(humLimitPen);

        // Dust 기준선 (빨강 점선)
        int yD = startY + height - (DUST_LIMIT * height / maxVal);
        HPEN dustLimitPen = CreatePen(PS_DASH, 1, RGB(255, 0, 0));
        SelectObject(hdc, dustLimitPen);
        MoveToEx(hdc, startX, yD, NULL);
        LineTo(hdc, startX + width, yD);
        DeleteObject(dustLimitPen);

        if (dataCount > 1)
        {
            // Humidity (blue)
            HPEN humPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 255));
            SelectObject(hdc, humPen);

            for (int i = 1; i < dataCount; i++)
            {
                int x1 = startX + (i - 1) * width / GRAPH_SIZE;
                int y1 = startY + height - (humData[i - 1] * height / maxVal);

                int x2 = startX + i * width / GRAPH_SIZE;
                int y2 = startY + height - (humData[i] * height / maxVal);

                MoveToEx(hdc, x1, y1, NULL);
                LineTo(hdc, x2, y2);
            }
            DeleteObject(humPen);

            // Dust (red)
            HPEN dustPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
            SelectObject(hdc, dustPen);

            for (int i = 1; i < dataCount; i++)
            {
                int x1 = startX + (i - 1) * width / GRAPH_SIZE;
                int y1 = startY + height - (dustData[i - 1] * height / maxVal);

                int x2 = startX + i * width / GRAPH_SIZE;
                int y2 = startY + height - (dustData[i] * height / maxVal);

                MoveToEx(hdc, x1, y1, NULL);
                LineTo(hdc, x2, y2);
            }
            DeleteObject(dustPen);
        }

        TextOut(hdc, startX, startY - 25, L"Humidity (Blue)", lstrlen(L"Humidity (Blue)"));
        TextOut(hdc, startX + 140, startY - 25, L"Dust (Red)", lstrlen(L"Dust (Red)"));

        EndPaint(hDlg, &ps);
        return TRUE;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
        HWND hCtrl = (HWND)lParam;

        int id = GetDlgCtrlID(hCtrl);

        SetBkMode(hdcStatic, TRANSPARENT);

        if (id == IDC_HUMIDITY)
        {
            if (g_humidity >= HUMIDITY_LIMIT)
                SetTextColor(hdcStatic, RGB(255, 0, 0));
            else
                SetTextColor(hdcStatic, RGB(0, 0, 0));

            return (INT_PTR)GetStockObject(NULL_BRUSH);
        }

        if (id == IDC_DUST)
        {
            if (g_dust >= DUST_LIMIT)
                SetTextColor(hdcStatic, RGB(255, 0, 0));
            else
                SetTextColor(hdcStatic, RGB(0, 0, 0));

            return (INT_PTR)GetStockObject(NULL_BRUSH);
        }

        return FALSE;
    }

    case WM_COMMAND:

        if (LOWORD(wParam) == IDC_LOG_10)
        {
            SendDlgItemMessage(hDlg, IDC_QUERY_LOG, LB_RESETCONTENT, 0, 0);
            send(sock, "GET_LOG 10", 10, 0);
        }

        else if (LOWORD(wParam) == IDC_LOG_50)
        {
            SendDlgItemMessage(hDlg, IDC_QUERY_LOG, LB_RESETCONTENT, 0, 0);
            send(sock, "GET_LOG 50", 10, 0);
        }

        else if (LOWORD(wParam) == IDC_LOG_ALL)
        {
            SendDlgItemMessage(hDlg, IDC_QUERY_LOG, LB_RESETCONTENT, 0, 0);
            send(sock, "GET_LOG ALL", 11, 0);
        }

        return TRUE;


    case WM_CLOSE:

        closesocket(sock);
        WSACleanup();

        EndDialog(hDlg, 0);

        return TRUE;

    }

    return FALSE;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
    return DialogBox(hInst, MAKEINTRESOURCE(IDD_MAIN_DIALOG), NULL, DialogProc);
}