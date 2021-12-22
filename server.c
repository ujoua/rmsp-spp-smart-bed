#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <wiringPi.h>
#include <softTone.h>

#define IN 0
#define OUT 1
#define LOW 0
#define HIGH 1

#define VALUE_MAX 256
#define BUFFER_MAX 32
#define DIRECTION_MAX 64

#define MAX_MESSAGE_LEN 10

#define POUT_WIRING_SPEAKER 4//1
#define POUT_LED 18//23
#define PWM 0
#define PIN_BUTTON 20
#define POUT_BUTTON 21

static int GPIOExport(int pin) {
	char buffer[BUFFER_MAX];
	ssize_t bytes_written;
	int fd;
	fd = open("/sys/class/gpio/export", O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open export for writing!\n");
		return(-1);
	}
	bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
	write(fd, buffer, bytes_written);
	close(fd);
	return(0);
}
static int GPIOUnexport(int pin) {
	char buffer[BUFFER_MAX];
	ssize_t bytes_written;
	int fd;
	fd = open("/sys/class/gpio/unexport", O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open unexport for writing!\n");
		return(-1);
	} bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
	write(fd, buffer, bytes_written);
	close(fd);
	return(0);
}
static int GPIODirection(int pin, int dir) {
	static const char s_directions_str[] = "in\0out";
	char path[DIRECTION_MAX] = "/sys/class/gpio/gpio%d/direction";
	int fd;
	snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);
	fd = open(path, O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio direction for writing!\n");
		return(-1);
	}
	if (-1 == write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2 : 3)) {
		fprintf(stderr, "Failed to set direction!\n");
		return(-1);
	}
	close(fd);
	return(0);
}
static int GPIORead(int pin) {
	char path[VALUE_MAX];
	char value_str[3];
	int fd;
	snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
	fd = open(path, O_RDONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio value for reading!\n");
		return(-1);
	}
	if (-1 == read(fd, value_str, 3)) {
		fprintf(stderr, "Failed to read value!\n");
		return(-1);
	}
	close(fd);
	return(atoi(value_str));
}
static int GPIOWrite(int pin, int value) {
	static const char s_values_str[] = "01";
	char path[VALUE_MAX];
	int fd;
	snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
	fd = open(path, O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio value for writing!\n");
		return(-1);
	}
	if (1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1)) {
		fprintf(stderr, "Failed to write value!\n");
		return(-1);
		close(fd);
		return(0);
	}
}

static int PWMExport(int pwmnum)
{
	char buffer[BUFFER_MAX];
	ssize_t bytes_written;
	int fd;
	fd = open("/sys/class/pwm/pwmchip0/export", O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open in export!!\n");
		return(-1);
	}
	bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pwmnum);
	write(fd, buffer, bytes_written);
	close(fd);
	sleep(1);
	return(0);
}
static int PWMUnexport(int pwmnum)
{
	char buffer[BUFFER_MAX];
	ssize_t bytes_written;
	int fd;
	fd = open("/sys/class/pwm/pwmchip0/unexport", O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open in unexport!!\n");
		return(-1);
	}
	bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pwmnum);
	write(fd, buffer, bytes_written);
	close(fd);
	sleep(1);
	return(0);
}
static int PWMEnable(int pwmnum)
{
	static const char s_unenable_str[] = "0";
	static const char s_enable_str[] = "1";
	char path[DIRECTION_MAX];
	int fd;
	snprintf(path, DIRECTION_MAX, "/sys/class/pwm/pwmchip0/pwm%d/enable", pwmnum); //?
	fd = open(path, O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open in enable! \n");
		return(-1);
	}
	write(fd, s_unenable_str, strlen(s_unenable_str));
	close(fd);
	fd = open(path, O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open in enable! \n");
		return(-1);
	}
	write(fd, s_enable_str, strlen(s_enable_str));
	close(fd);
	return(0);
}
static int PWMWritePeriod(int pwmnum, int value)
{
	char s_values_str[VALUE_MAX] = "01";
	char path[VALUE_MAX];
	int fd, byte;
	snprintf(path, VALUE_MAX, "/sys/class/pwm/pwmchip0/pwm%d/period", pwmnum); //?
	fd = open(path, O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open in period! \n");
		return(-1);
	}
	byte = snprintf(s_values_str, VALUE_MAX, "%d", value);
	if (1 == write(fd, s_values_str, byte)) {
		fprintf(stderr, "Failed to write value in period!\n");
		close(fd);
		return(-1);
	}
	close(fd);
	return(0);
}
static int PWMWriteDutyCycle(int pwmnum, int value)
{
	char path[VALUE_MAX];
	char s_values_str[VALUE_MAX];
	int fd, byte;
	snprintf(path, VALUE_MAX, "/sys/class/pwm/pwmchip0/pwm%d/duty_cycle", pwmnum); //
	fd = open(path, O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open in duty_cycle! \n");
		return(-1);
	}
	value = (value >= 10) ? value : 10;
	byte = snprintf(s_values_str, VALUE_MAX, "%d", value);
	if (1 == write(fd, s_values_str, byte)) {
		fprintf(stderr, "Failed to write value in duty_cycle!\n");
		printf("[DEBUG] vlaue: %d\n", value);
		close(fd);
		return(-1);
	}
	close(fd);
	return(0);
}

pthread_t thr[5];
int thr_id[5];
int thr_rst[5];

int end_time;
int start_time;

int state_led = 0;

void* TurnOffAlarm_thd() {
	if (wiringPiSetup() == -1) exit(1);

	softToneCreate(POUT_WIRING_SPEAKER);

	softToneWrite(POUT_WIRING_SPEAKER, 0);

	pthread_exit((void*)& thr_rst[0]);

	pthread_join(thr[0], (void*)& thr_rst[0]);
}

void* TurnOnAlarm_thd() {
	if (wiringPiSetup() == -1) exit(1);

	softToneCreate(POUT_WIRING_SPEAKER);

	softToneWrite(POUT_WIRING_SPEAKER, 523);

	pthread_exit((void*)& thr_rst[0]);

	pthread_join(thr[0], (void*)& thr_rst[0]);
}

void* TurnOffLED_thd() {
	printf("Here is TurnOffLED\n");
	state_led = 0;

	//PWMUnexport(0);
	for (int i = 100; i >= 1; i--) {
		PWMWriteDutyCycle(0, i * 10000);
		usleep(1000);
	}

	pthread_exit((void*)& thr_rst[1]);

	pthread_join(thr[1], (void*)& thr_rst[1]);
}

void* TurnOnLED_thd() {
	printf("Here is TurnOnLED\n");
	state_led = 1;

	PWMExport(0); // pwm0 is gpio18

	PWMWritePeriod(0, 20000000);

	PWMWriteDutyCycle(0, 0);

	PWMEnable(0);
	for (int i = 1; i < 100; i++) {
		PWMWriteDutyCycle(0, i * 10000);
		usleep(1000);
	}

	end_time = (unsigned)time(NULL) + 8; //20 * 60; // 20 min
	start_time = (unsigned)time(NULL);

	while (end_time - start_time > 0) {
		start_time = (unsigned)time(NULL);
	}

	TurnOffLED_thd();
}

void* RestartLED_thd() {
	printf("Here is Restart\n");
	end_time = (unsigned)time(NULL) + 8;

	pthread_exit((void*)& thr_rst[2]);

	pthread_join(thr[2], (void*)& thr_rst[2]);
}

void error_handling(char* message) {
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}

int main(int argc, char* argv[]) {
	sleep(5);

	// 1 is alarm, 2 is sensor
	struct sockaddr_in serv_addr, clnt_addr;
	int serv_sock = -1, clnt_sock = -1;
	socklen_t clnt_addr_size;
	char msg[MAX_MESSAGE_LEN]; int msg_len;

	struct sockaddr_in serv_addr2, clnt_addr2;
	int serv_sock2 = -1, clnt_sock2 = -1;
	socklen_t clnt_addr_size2;
	char msg2[MAX_MESSAGE_LEN]; int msg_len2;

	if (argc != 2) printf("Usage: %s <port>\n", argv[0]);

	//
	serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (serv_sock == -1) error_handling("socket() error");

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(atoi(argv[1]));

	serv_sock2 = socket(PF_INET, SOCK_STREAM, 0);
	if (serv_sock2 == -1) error_handling("socket() error");

	memset(&serv_addr2, 0, sizeof(serv_addr2));
	serv_addr2.sin_family = AF_INET;
	serv_addr2.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr2.sin_port = htons(atoi(argv[1]) + 1);

	if (bind(serv_sock, (struct sockaddr*) & serv_addr, sizeof(serv_addr)) == -1 || bind(serv_sock2, (struct sockaddr*) & serv_addr2, sizeof(serv_addr2)) == -1) error_handling("bind() error");

	if (listen(serv_sock, 5) == -1 || listen(serv_sock2, 5) == -1) error_handling("listen() error");

	if (clnt_sock < 0) {
		clnt_addr_size = sizeof(clnt_addr);
		clnt_sock = accept(serv_sock, (struct sockaddr*) & clnt_addr, &clnt_addr_size);
		if (clnt_sock == -1)
			error_handling("accept() error");
		else
			printf("Client successfuly accepted!\n");
	}
	if (clnt_sock2 < 0) {
		clnt_addr_size2 = sizeof(clnt_addr2);
		clnt_sock2 = accept(serv_sock2, (struct sockaddr*) & clnt_addr2, &clnt_addr_size2);
		if (clnt_sock2 == -1)
			error_handling("accept() error");
		else
			printf("Client successfuly accepted!\n");
	}

	//int sensor_state;
	if (GPIOExport(PIN_BUTTON) == -1) exit(1);

	usleep(100000); // To prevent "Failed to open gpio direction for writing!" wating for device file creation

	if (GPIODirection(PIN_BUTTON, IN) == -1) exit(2);

	//EOF != read(clnt_sock, msg, sizeof(msg))
	while (1) {
		usleep(1000000);
		snprintf(msg, 10, "request");
		write(clnt_sock, msg, sizeof(msg));
		msg_len = read(clnt_sock, msg, sizeof(msg));

		snprintf(msg2, 10, "request");
		write(clnt_sock2, msg2, sizeof(msg2));
		msg_len2 = read(clnt_sock2, msg2, sizeof(msg2));

		if (msg_len == -1)
			error_handling("read() error");
		else
			printf("Receive message from Client : %s\n", msg);
		if (msg_len2 == -1)
			error_handling("read() error");
		else
			printf("Receive message from Client : %s\n", msg2);

		if (msg2[0] == '0') {
			printf("pressure\n");
			thr_id[0] = pthread_create(&thr[0], NULL, TurnOffAlarm_thd, NULL);
		}
		if (msg2[1] == '1' && state_led) {
			printf("motion\n");
			thr_id[2] = pthread_create(&thr[2], NULL, RestartLED_thd, NULL);
		}
		if (msg[0] == '4') {
			printf("alarm\n");
			thr_id[3] = pthread_create(&thr[3], NULL, TurnOnAlarm_thd, NULL);
		}
		if (LOW == GPIORead(PIN_BUTTON) && !state_led) {
			printf("button\n");
			thr_id[1] = pthread_create(&thr[1], NULL, TurnOnLED_thd, NULL);

			while (LOW == GPIORead(PIN_BUTTON));
		}
	}

	close(clnt_sock);
	close(clnt_sock2);
	close(serv_sock);
	close(serv_sock2);

	return 0;
}
