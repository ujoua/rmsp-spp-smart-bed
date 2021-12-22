#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <wiringPiI2C.h>
#include <wiringPi.h>

#define IN  0
#define OUT 1
#define LOW  0
#define HIGH 1

#define SIN 20
#define SOUT 21
#define MIN 12
#define MOUT 13
#define HIN 16
#define HOUT 19

#define TEIN 5
#define TEOUT 6
#define VALUE_MAX 40

int sec = 0;
int min = 0;
int hour = 0;
int date = 0;
int month = 0;
int year = 0;

int set_sec = 0;
int set_min = 0;
int set_hour = 0;
int set_date = 0;
int set_month = 0;
int set_year = 0;


#define I2C_ADDR   0x27 

#define LCD_CHR  1 // send date
#define LCD_CMD  0 // send command

#define LINE1  0x80
#define LINE2  0xC0

#define LCD_BACKLIGHT   0x08 

#define ENABLE  0b00000100

void lcd_init(void);
void lcd_byte(int bits, int mode);
void lcd_toggle_enable(int bits);

void typeInt(int i);
void typeFloat(float myFloat);
void lcdLoc(int line);
void ClrLcd(void); 
void typeln(const char *s);
void typeChar(char val);
int fd;


void typeFloat(float myFloat)   {
  char buffer[20];
  sprintf(buffer, "%4.2f",  myFloat);
  typeln(buffer);
}


void typeInt(int i)   {
  char array1[20];
  sprintf(array1, "%d",  i);
  typeln(array1);
}


void ClrLcd(void)   {
  lcd_byte(0x01, LCD_CMD);
  lcd_byte(0x02, LCD_CMD);
}


void lcdLoc(int line)   {
  lcd_byte(line, LCD_CMD);
}


void typeChar(char val)   {

  lcd_byte(val, LCD_CHR);
}



void typeln(const char *s)   {

  while ( *s ) lcd_byte(*(s++), LCD_CHR);

}

void lcd_byte(int bits, int mode)   {

  int bits_high;
  int bits_low;

  bits_high = mode | (bits & 0xF0) | LCD_BACKLIGHT ;
  bits_low = mode | ((bits << 4) & 0xF0) | LCD_BACKLIGHT ;

  wiringPiI2CReadReg8(fd, bits_high);
  lcd_toggle_enable(bits_high);

  wiringPiI2CReadReg8(fd, bits_low);
  lcd_toggle_enable(bits_low);
}

void lcd_toggle_enable(int bits)   {
  delayMicroseconds(500);
  wiringPiI2CReadReg8(fd, (bits | ENABLE));
  delayMicroseconds(500);
  wiringPiI2CReadReg8(fd, (bits & ~ENABLE));
  delayMicroseconds(500);
}


void lcd_init()   {
  lcd_byte(0x33, LCD_CMD);
  lcd_byte(0x32, LCD_CMD); 
  lcd_byte(0x06, LCD_CMD); 
  lcd_byte(0x0C, LCD_CMD); 
  lcd_byte(0x28, LCD_CMD); 
  lcd_byte(0x01, LCD_CMD); 
  delayMicroseconds(500);
}

static int GPIOExport(int pin){
#define BUFFER_MAX 3
  char buffer[BUFFER_MAX];
  ssize_t bytes_written;
  int fd;

  fd = open("/sys/class/gpio/export",O_WRONLY);
  if(-1 == fd){
    fprintf(stderr, "Failed to open export for writing!\n");
    return (-1);
  }

  bytes_written = snprintf(buffer, BUFFER_MAX, "%d",pin);
  write(fd, buffer, bytes_written);
  close(fd);
  return(0);
}

static int GPIODirection(int pin, int dir){
  static const char s_directions_str[] = "in\0out";

#define DIRECTION_MAX 35
  char path[DIRECTION_MAX] = "/sys/class/gpio/gpio%d/direction";
  int fd;

  snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction" , pin);

  fd = open(path, O_WRONLY);
  if(-1 == fd){
    fprintf(stderr, "Failed to open gpio direction for writing!\n");
    return (-1);
  }

  if(-1 == write(fd, &s_directions_str[IN ==dir ? 0:3], IN == dir ? 2:3)){
    fprintf(stderr, "Failed to set direction!\n");
    close(fd);
    return(-1);
  }

  close(fd);
  return(0);
  
}


static int GPIORead(int pin){
  char path[VALUE_MAX];
  char value_str[3];
  int fd;

  snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
  
  fd = open(path, O_RDONLY);
  if(-1==fd){
    fprintf(stderr, "Failed to open gpio value for reading!\n");
    return(-1);
  }

  if(-1 == read(fd, value_str, 3)){
    fprintf(stderr, "Failed to read value!\n");
    close(fd);
    return(-1);
  }

  close(fd);

  return(atoi(value_str));
}

static int GPIOWrite(int pin, int value){
  static const char s_values_str[] = "01";

  char path[VALUE_MAX];
  int fd;

  snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
  fd = open(path, O_WRONLY);
  if(-1==fd){
    fprintf(stderr, "Failed to open gpio value for writing!\n");
    return(-1);
  }
  if(1!=write(fd, &s_values_str[LOW==value?0:1],1)){
    fprintf(stderr, "Failed to write value!\n");
    close(fd);
    return(-1);
  }

  close(fd);
  return(0);
}

static int GPIOUnexport(int pin){
  char buffer[BUFFER_MAX];
  ssize_t bytes_written;
  int fd;

  fd = open("/sys/class/gpio/unexport",O_WRONLY);
  if(-1 == fd){
    fprintf(stderr, "Failed to open unexport for writing!\n");
    return(-1);
  }

  bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
  write(fd, buffer, bytes_written);
  close(fd);
  return(0);

}

void error_handling(char* message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}


int sock;
struct sockaddr_in serv_addr;
char msg[10];
char state[1];
int str_len;

void *alarm_thd(){

  time_t t = time(NULL);
  struct tm tm = *localtime(&t);

  int repeat = 10000;

  int s_state = 0;
  int s_prev_state = 0;

  int m_state = 0;
  int m_prev_state = 0;

  int h_state = 0;
  int h_prev_state = 0;

  int te_state = 0;
  int te_prev_state = 0;

  int wake_message = 0;

  state[0] = '0';

  if (wiringPiSetup () == -1) exit (1);

  fd = wiringPiI2CSetup(I2C_ADDR);

  lcd_init(); // setup LCD



  
  if(-1==GPIOExport(SOUT)||-1==GPIOExport(SIN)||-1==GPIOExport(MOUT)||-1==GPIOExport(MIN)||-1==GPIOExport(HOUT)||-1==GPIOExport(HIN)||-1==GPIOExport(TEOUT)||-1==GPIOExport(TEIN)){
    return;
  }

  usleep(10000);

  if(-1 == GPIODirection(SIN,IN) || -1 == GPIODirection(SOUT, OUT) || -1 == GPIODirection(MIN,IN) || -1 == GPIODirection(MOUT, OUT) ||  -1 == GPIODirection(HIN,IN) || -1 == GPIODirection(HOUT, OUT) || -1 == GPIODirection(TEIN,IN) || -1 == GPIODirection(TEOUT, OUT) ){
    return;
  }

  char msg1[500];
  char msg2[500];

  // 현재 시간 받아오는 코드
  t = time(NULL);
  tm = *localtime(&t);

  snprintf(msg1,16,"%02d/%02d %02d:%02d:%02d",
       tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  printf("%s\n", msg1);
  //ClrLcd();
  lcdLoc(LINE1);
  typeln(msg1);

  snprintf(msg2,16,"+ 00H 00M 00S");
  printf("%s\n", msg2);
  //ClrLcd();
  lcdLoc(LINE2);
  typeln(msg2);

  while(1){
    
    // 현재 시간 받아오는 코드
    t = time(NULL);
    tm = *localtime(&t);

  
    snprintf(msg1,16,"%02d/%02d %02d:%02d:%02d",
       tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    printf("%s\n", msg1);
    //ClrLcd();
    lcdLoc(LINE1);
    typeln(msg1);

    // 타이머 울리는 코드
    if (tm.tm_sec == set_sec && tm.tm_min == set_min && tm.tm_hour == set_hour && tm.tm_mday == set_date && tm.tm_mon+1 == set_month && tm.tm_year+1900 == set_year){
      printf("ring!\n");
      printf("now: %d-%d-%d %d:%d:%d\n",
       tm.tm_year+900, tm.tm_mon+1, tm.tm_mday,
       tm.tm_hour, tm.tm_min, tm.tm_sec);
      sec = 0;
      min = 0;
      hour = 0;
      date = 0;
      month = 0;
      year = 0;

      set_sec = 0;
      set_min = 0;
      set_hour = 0;
      set_date = 0;
      set_month = 0;
      set_year = 0;
      state[0]='4';

      snprintf(msg2,16,"WAKE UP!!");
      printf("%s\n", msg2);
      ClrLcd();
      lcdLoc(LINE2);
      typeln(msg2);
      
    }

    // 초 증가시키는 코드
    s_prev_state = s_state;
    s_state = GPIORead(SIN);
    if(s_state==0){
      if(s_prev_state == 1) {
        sec += 1;
        if (sec >= 60 ){
          sec = 0;
        }
        printf("%d : %d : %d\n",hour, min, sec);

        snprintf(msg2,16,"+ %02dH %02dM %02dS",
        hour, min, sec);
        printf("%s\n", msg2);
        ClrLcd();
        lcdLoc(LINE2);
        typeln(msg2);
      }
    }

    // 분 증가시키는 코드
    m_prev_state = m_state;
    m_state = GPIORead(MIN);
    if(m_state==0){
      if(m_prev_state == 1) {
        min += 1;
        if (min >= 60 ){
          min = 0;
        }
        printf("%d : %d : %d\n",hour, min, sec);
        snprintf(msg2,16,"+ %02dH %02dM %02dS",
        hour, min, sec);
        printf("%s\n", msg2);
        ClrLcd();
        lcdLoc(LINE2);
        typeln(msg2);
      }
    }

    // 시 증가시키는 코드
    h_prev_state = m_state;
    h_state = GPIORead(HIN);
    if(h_state==0){
      if(h_prev_state == 1) {
        hour += 1;
        if (hour >= 24 ){
          hour = 0;
        }
        printf("%d : %d : %d\n",hour, min, sec);
        snprintf(msg2,16,"+ %02dH %02dM %02dS",
        hour, min, sec);
        printf("%s\n", msg2);
        ClrLcd();
        lcdLoc(LINE2);
        typeln(msg2);
      }
    }

    // 시간 입력에 대한 코드
    te_prev_state = te_state;
    te_state = GPIORead(TEIN);
    if(te_state==0){
      if(te_prev_state == 1) {
        set_sec = tm.tm_sec + sec;
        if(set_sec >= 60){
          min += set_sec/60;
          set_sec = set_sec%60;
        }
        set_min = tm.tm_min + min;
        if(set_min >= 60){
          hour += set_min/60;
          set_min = set_min%60;
        }
        set_hour = tm.tm_hour + hour;
        if(set_hour >= 24){
          date += set_hour/24;
          set_hour = set_hour%24;
        }
        set_date = tm.tm_mday + date;        
        if (set_date >= 32 && (tm.tm_mon+1 == 1 || tm.tm_mon+1 == 3 || tm.tm_mon+1 == 5 ||tm.tm_mon+1 == 7 || tm.tm_mon+1 == 8 || tm.tm_mon+1 == 10 ||tm.tm_mon+1 == 12)){
          month += 1;
          set_date = 1;
        }
        else if (set_date >= 31 &&  (tm.tm_mon+1 == 4 || tm.tm_mon+1 == 6 ||tm.tm_mon+1 == 9 || tm.tm_mon+1 == 11 )){
          month += 1;
          set_date = 1;
        }
        else if (set_date >= 29 && tm.tm_mon+1 == 2){
          month += 1;
          set_date = 1;
        }
        set_month = tm.tm_mon+1 + month;
        if (set_month >= 13){
          set_month = 1;
          year += 1;
        }
        set_year = tm.tm_year+1900 + year;
          
        printf("set time!\n");
        printf("now: %d-%d-%d %d:%d:%d\n",
        tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec);
        printf("timer: %d-%d-%d %d:%d:%d\n",
        set_year, set_month, set_date,
        set_hour, set_min, set_sec);
        snprintf(msg2,16,"ALARM %02d:%02d:%02d",
        set_hour, set_min, set_sec);
        printf("%s\n", msg2);
        ClrLcd();
        lcdLoc(LINE2);
        typeln(msg2);
      }
    }

    printf("==debug== ");
    printf(" %d-%d-%d %d:%d:%d\n",
          tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
          tm.tm_hour, tm.tm_min, tm.tm_sec);
    if(-1 == GPIOWrite(SOUT, 1)||-1 == GPIOWrite(MOUT, 1)||-1 == GPIOWrite(HOUT, 1)||-1 == GPIOWrite(TEOUT, 1) ){
      return;
    }

    usleep(100000);
  }

  if(-1 == GPIOUnexport(SOUT)||-1==GPIOUnexport(SIN)||-1 == GPIOUnexport(MOUT)||-1==GPIOUnexport(MIN)||-1 == GPIOUnexport(HOUT)||-1==GPIOUnexport(HIN)||-1 == GPIOUnexport(TEOUT)||-1==GPIOUnexport(TEIN)){
    return;
  }
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
            state[0]='3';
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

    thr_id=pthread_create(&p_thread[0],NULL,alarm_thd,(void*)p1);
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