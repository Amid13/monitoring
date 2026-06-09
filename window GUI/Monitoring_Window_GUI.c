#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <mysql/mysql.h>
#include <time.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> 

#define SERIAL_PORT "/dev/ttyACM0"
#define MAX_SIZE 20
#define TCP_PORT 9000

#define HUMIDITY_LIMIT 600
#define DUST_LIMIT     300

#define DB_HOST "localhost"
#define DB_USER "root"
#define DB_PASS "password"
#define DB_NAME "iot_db"

MYSQL *conn;

int server_fd;
int client_fd;

typedef struct {
	int humidity;
	int dust;
} SensorData;

typedef struct {
	SensorData data[MAX_SIZE];
	int front;
	int rear;
	int count;
} CircularQueue;

void initQueue(CircularQueue *q) {
	q->front = 0;
	q->rear = 0;
	q->count = 0;
}

void enqueue(CircularQueue *q, int h, int d) {
	q->data[q->rear].humidity = h;
	q->data[q->rear].dust = d;

	q->rear = (q->rear + 1) % MAX_SIZE;

	if (q->count < MAX_SIZE) {
		q->count++;
	} else {
		q->front = (q->front + 1) % MAX_SIZE;
	}
}

void saveToDB(int h, int d) {
	char query[256];

	sprintf(query,
		"INSERT INTO sensor_data (humidity, dust) VALUES (%d, %d)",
		h, d);

	if(mysql_query(conn, query)) {
		printf("MySQL Insert Error : %s\n", mysql_error(conn));
	}
}

void saveDangerEvent(const char *type, int value, const char *message)
{
	char query[256];

	sprintf(query,
		"INSERT INTO danger_event (type, value, message) VALUES ('%s', %d, '%s')",
		type, value, message);

	if(mysql_query(conn, query)) {
		printf("Danger Event Insert Error : %s\n", mysql_error(conn));
	}
}

void detectDanger(int humidity, int dust) {
	time_t now = time(NULL);
	struct tm *t = localtime(&now);

	char timeStr[64];
	char msg[256];
	sprintf(timeStr, "%04d-%02d-%02d %02d:%02d:%02d", t->tm_year+1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);

	if (humidity >= HUMIDITY_LIMIT) {
		printf("⚠️  WARNING: 습도가 너무 높습니다!\n");
		saveDangerEvent("humidity", humidity, "습도가 너무 높음");
		sprintf(msg, "WARN:%s Humidity High (%d)\n", timeStr, humidity);
		if(client_fd > 0)
			send(client_fd, msg, strlen(msg), 0);
	}

	if (dust >= DUST_LIMIT) {
		printf("⚠️  WARNING: 공기 오염도가 너무 높습니다!\n");
		saveDangerEvent("dust", dust, "공기 오염도가 너무 높음");
		sprintf(msg, "WARN:%s Dust High (%d)\n", timeStr, dust);
		if(client_fd > 0)
			send(client_fd, msg, strlen(msg), 0);
	}
}

void sendtoGUI(int humidity, int dust) {
	char msg[128];
	
	sprintf(msg, "HUM:%d, DUST:%d\n", humidity, dust);

	if(client_fd > 0)
		send(client_fd, msg, strlen(msg), 0);
}

void sendLogsToGUI(int limit) {
	MYSQL_RES *res;
	MYSQL_ROW row;
	char query[256];

       	if(limit > 0)
		sprintf(query, "SELECT timestamp, message FROM danger_event ORDER BY id DESC LIMIT %d", limit);
	else
		sprintf(query, "SELECT timestamp, message FROM danger_event ORDER BY id DESC");

	if(mysql_query(conn, query))
	{
		printf("Log Query Error: %s\n", mysql_error(conn));
		return;
	}

	res = mysql_store_result(conn);

	char msg[256];

	while((row = mysql_fetch_row(res)))
	{
		sprintf(msg, "LOG: %s %s\n", row[0], row[1]);

		if(client_fd > 0)
			send(client_fd, msg, strlen(msg), 0);
	}

	mysql_free_result(res);
}

void checkGUIRequest() {
	char buf[128];

	int n = recv(client_fd, buf, sizeof(buf)-1, MSG_DONTWAIT);

	if(n <= 0) return;

	buf[n] = 0;

	if(strncmp(buf,"GET_LOG", 7) == 0)
	{
		if(strstr(buf, "10")) sendLogsToGUI(10);
		else if(strstr(buf, "50")) sendLogsToGUI(50);
		else sendLogsToGUI(0);
	}
}

void initTCP() {
	struct sockaddr_in server_addr;

	server_fd = socket(AF_INET, SOCK_STREAM, 0);

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(TCP_PORT);

	bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));

	listen(server_fd, 5);

	printf("TCP Server waiting...\n");

	client_fd = accept(server_fd, NULL, NULL);

	printf("GUI connected\n");
}

void printAverage(CircularQueue *q) {
	if (q->count == 0) return;

	int sumH = 0;
	int sumD = 0;

	for (int i = 0; i < q->count; i++) {
		int index = (q->front + i) % MAX_SIZE;

		sumH += q->data[index].humidity;
		sumD += q->data[index].dust;
	}

	float avgH = sumH / q->count;
	float avgD = sumD / q->count;

	printf("\n----- 최근 %d개 데이터 평균 -----\n", q->count);
	printf("평균 습도: %.2f\n", avgH);
	printf("평균 공기 오염도: %.2f\n", avgD);
	printf("----------------------------------\n\n");

	if (avgH >= HUMIDITY_LIMIT) {
		printf("⚠️  WARNING: 습도가 너무 높습니다!\n");
	}

	if (avgD >= DUST_LIMIT) {
		printf("⚠️  WARNING: 공기 오염도가 너무 높습니다!\n");
	}

	printf("\n");
}

int main()
{
	int fd;
	struct termios options;
	char buffer[256];
	char line[256];
	int linePos = 0;
	
	CircularQueue queue;
	initQueue(&queue);
        
	conn = mysql_init(NULL);

	if(mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME, 0, NULL, 0) == NULL) {
		printf("DB error : %s\n", mysql_error(conn));
		return 1;
	}

	printf("MySQL Connected!\n");

	initTCP();

	fd = open(SERIAL_PORT, O_RDWR | O_NOCTTY );

	tcgetattr(fd, &options); 
	cfsetispeed(&options, B9600);
	cfsetospeed(&options, B9600);

	options.c_cflag |= (CLOCAL | CREAD);
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;

	tcsetattr(fd, TCSANOW, &options);

	while (1) {
		checkGUIRequest();

		int n = read(fd, buffer, sizeof(buffer));

		if(n<=0) continue;

		for (int i = 0; i < n; i++) {
			if (buffer[i] == '\n') {
				line[linePos] = '\0';
				
				int h, d;
				if (sscanf(line, "H:%d D:%d", &h, &d) == 2) {
					printf("Parsed -> 습도: %d, 공기 오염도: %d\n", h, d);

					enqueue(&queue, h, d);

					saveToDB(h, d);

					detectDanger(h, d);

					sendtoGUI(h, d);

					printAverage(&queue);
				}

				linePos = 0;
			} else if (buffer[i] != '\r') {
				if(linePos < sizeof(line)-1)
					line[linePos++] = buffer[i];
			}
		}
	}
	
	mysql_close(conn);
	close(fd);
	return 0;
}
