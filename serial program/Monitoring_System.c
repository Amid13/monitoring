#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#define SERIAL_PORT "/dev/ttyACM2"
#define MAX_SIZE 20

//추후 변경
#define HUMIDITY_LIMIT 950
#define DUST_LIMIT     920

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
        
	fd = open(SERIAL_PORT, O_RDWR | O_NOCTTY );
	if (fd == -1) { 
		perror("Serial open error"); 
		return 1; 
	}

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
		int n = read(fd, buffer, sizeof(buffer));

		for (int i = 0; i < n; i++) {
			if (buffer[i] == '\n') {
				line[linePos] = '\0';
				
				int h, d;
				if (sscanf(line, "H:%d D:%d", &h, &d) == 2) {
					printf("Parsed -> 습도: %d, 공기 오염도: %d\n", h, d);
					enqueue(&queue, h, d);

					printAverage(&queue);
				}

				linePos = 0;
			} else if (buffer[i] != '\r') {
				line[linePos++] = buffer[i];
			}
		}
	}

	close(fd);
	return 0;
}
