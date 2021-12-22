#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define IN  0
#define OUT 1
#define LOW  0
#define HIGH 1

#define PRESSURE_IN  18
#define MOTION_IN 26

static int GPIOExport(int pin) {
#define BUFFER_MAX 3
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
   }

   bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
   write(fd, buffer, bytes_written);
   close(fd);
   return(0);
}

static int GPIODirection(int pin, int dir) {
   static const char s_directions_str[]  = "in\0out";

#define DIRECTION_MAX 35
   char path[DIRECTION_MAX]="/sys/class/gpio/gpio%d/direction";
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
#define VALUE_MAX 30
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

void error_handling(char* message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
/*
int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in serv_addr;
    char msg[10];
    char on[2] = "1";
    int str_len;
    int light = 0;

   int state = 1;
   int prev_state = 1;

    if (argc != 3) {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

   //Enable GPIO pins
   if (-1 == GPIOExport(POUT2) || -1 == GPIOExport(PIN))
      return(1);

   //Set GPIO directions
   if (-1 == GPIODirection(POUT2, OUT) || -1 == GPIODirection(PIN, IN))
      return(2);

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))==-1)
        error_handling("connect() error");


   while(1){
      if ( -1 == GPIOWrite(POUT2,1)) {
            return(3);
        }
        if(GPIORead(PIN)==0){ // Button  pushed
            snprintf(msg,10,"request");
            write(sock,msg,sizeof(msg));
            str_len = read(sock, msg, sizeof(msg));
            if (str_len == -1)
               error_handling("read() error");

            printf("Receive message from Server : %s\n", msg);
            usleep(1000000);
        }

        usleep(25000);

   }
    close(sock);
   
   //Disable GPIO pins
   if (-1 == GPIOUnexport(POUT2) || -1 == GPIOUnexport(PIN))
      return(4);

   return(0);
}*/

int sock;
struct sockaddr_in serv_addr;
char msg[10];
char state[2];
int str_len;

void *sensor_thd(){
   //Enable GPIO pins
   if ( -1 == GPIOExport(PRESSURE_IN) || -1 == GPIOExport(MOTION_IN)) return;

   //Set GPIO directions
   if (-1 == GPIODirection(PRESSURE_IN, IN) || -1 == GPIODirection(MOTION_IN, IN)) return;

   while(1){
      int pressure_state=GPIORead(PRESSURE_IN);
      int motion_state=GPIORead(MOTION_IN);

      if(pressure_state) state[0]='1';
      else state[0]='0';
      if(motion_state) state[1]='1';
      usleep(100000);
   }

   //Disable GPIO pins
   if (-1 == GPIOUnexport(PRESSURE_IN) || -1 == GPIOUnexport(MOTION_IN)) return;
}

void *send_thd(){
   while(1){
        str_len = read(sock, msg, sizeof(msg));
        if (str_len == -1)
            error_handling("read() error");
        if(strcmp("request",msg)==0){
            snprintf(msg,10,"%s",state);
            printf("Sending message to server: %s\n",msg);
            write(sock,msg,sizeof(msg));
         state[1]='0';
        }
    }
}

int main(int argc, char *argv[]) {

   /// socket communication part ///
   if (argc != 3) {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

   sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))==-1)
        error_handling("connect() error");


   /// thread part ///
   pthread_t p_thread[2];
    int thr_id; int status;
    char p1[]="thread_1";
    char p2[]="thread_2";

    thr_id=pthread_create(&p_thread[0],NULL,sensor_thd,(void*)p1);
    if(thr_id<0){
        perror("thread create error: "); exit(0);
    }
    
    thr_id=pthread_create(&p_thread[1],NULL,send_thd,(void*)p2);
    if(thr_id<0){
        perror("thread create error: "); exit(0);
    }
    
    pthread_join(p_thread[0], (void **)&status);
    pthread_join(p_thread[1], (void **)&status);

   close(sock);
   return 0;
}
