#include <stdio.h>
#include <stdlib.h> 
#include <unistd.h> 
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define MAX_ORDERS 100
void write_log(char *agents, char *nama, char *alamat);

typedef struct {
  char nama[50];
  char alamat[50];
  char jenis[20];
  char delivered_by[50];
  bool status;
 } Order;

 pthread_mutex_t lock;
 Order *orders;

//melakukan order express
void *express_order(void *arg) {
  printf("orders: %p\n", orders);
  
 if (!orders) {
    printf("orders is NULL!\n");
    pthread_exit(NULL);
 }

  char *agent_name = (char *)arg;
  while (1) {
  pthread_mutex_lock(&lock);
  bool found = false;

  for (int i = 0; i < MAX_ORDERS; i++) {
   if (strcmp(orders[i].jenis, "Express") == 0 && orders[i].status == false) {
      orders[i].status = true;
      strcpy(orders[i].delivered_by, agent_name);
      write_log(agent_name, orders[i].nama, orders[i].alamat);
      found = true;
      break;
     }
    }
    pthread_mutex_unlock(&lock); 
    if (!found) break;
    sleep(1);
  }
 return NULL;
}

//mencatat orderan express
void write_log(char *agents, char *nama, char *alamat) {
  FILE *log = fopen("delivery.log", "a");
  if (!log) {
    return;
}

time_t now = time(NULL);
struct tm *t = localtime(&now);
char date[32];
char time[32];

strftime(date, sizeof(date), "%d/%m/%Y", t);
strftime(time, sizeof(time), "%H:%M:%S", t); 

fprintf(log, "[%s %s] [%s] Express package delivered to %s in %s\n", date, time, agents, nama, alamat);
fclose(log);
}

int main() {
  key_t key = 1234;
  int shmid = shmget(key, sizeof(Order) * MAX_ORDERS, 0666);
  if (shmid == -1) {
     return 1;
  }

    orders = (Order *)shmat(shmid, NULL, 0);
    if (orders == (void *)-1) {
      perror("shmat");
      return 1;
    }

    pthread_mutex_init(&lock, NULL); 

    pthread_t express_agents[3];
    char *agent_names[] = {"AGENT A", "AGENT B", "AGENT C"};

    for (int i = 0; i < 3; i++) {
      pthread_create(&express_agents[i], NULL, express_order, agent_names[i]);
    }
    for (int i = 0; i < 3; i++) {
      pthread_join(express_agents[i], NULL);
    }

    pthread_mutex_destroy(&lock);
    shmdt(orders);
    return 0;
 }
