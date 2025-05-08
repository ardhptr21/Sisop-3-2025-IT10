#include <stdio.h>
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>

#define MAX_ORDERS 100

typedef struct {
 char nama[50];
 char alamat[50];
 char jenis[20];
 char delivered_by[50];
 bool status;
} Order;

//membaca file csv ke shared memory
int read_csv(char *filename, Order *orders) {
  if (access("Soal_2/delivery_order.csv", F_OK) != 0) {
    pid_t pid = fork();
    if (pid == 0) {
      char *args[] = {"sh", "-c", "wget --quiet --no-cache --no-cookies --no-check-certificate \"https://docs.google.com/uc?export=download&confirm=$(wget --quiet --no-cache --no-cookies --no-check-certificate 'https://docs.google.com/uc?export=download&id=BU299JKGENW28R' -O- | sed -rn 's/.*confirm=([0-9A-Za-z_]+).*/\\1/p')&id=1OJfRuLgsBnIBWtdRXbRsD2sG6NhMKOg9\" -O Soal_2/delivery_order.csv", NULL};
      execvp("sh", args);
      exit(EXIT_FAILURE);
    } else {
      wait(NULL);
   }
 }

  FILE *fp = fopen("Soal_2/delivery_order.csv", "r");
  if (!fp) {
    perror("fopen");
    return 0;
  }

   char line[256];
   int i = 0;

   fgets(line, sizeof(line), fp); 

   while (fgets(line, sizeof(line), fp) && i < MAX_ORDERS) {
      char *nama = strtok(line, ",");
      char *alamat = strtok(NULL, ",");
      char *jenis = strtok(NULL, "\n");

      if (nama && alamat && jenis) {
        strncpy(orders[i].nama, nama, sizeof(orders[i].nama));
        strncpy(orders[i].alamat, alamat, sizeof(orders[i].alamat));
        strncpy(orders[i].jenis, jenis, sizeof(orders[i].jenis));
        i++;
     }
    }
  fclose(fp);
  return i;
}

//mencatat orderan reguler
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

  fprintf(log, "[%s %s] [AGENT %s] Reguler package delivered to %s in %s\n", date, time, agents, nama, alamat);
  fclose(log);
}

int main(int argc, char *argv[]) {
    key_t key = 1234;
    int shmid = shmget(key, sizeof(Order) * MAX_ORDERS, IPC_CREAT | 0666);
    if (shmid == -1) {
      return 1;
    }

    Order *orders = (Order *)shmat(shmid, NULL, 0);
    if (orders == (void *) -1) {
      perror("shmat");
      return 1;
    }
   
    int found = 0;
    int total_order = read_csv("Soal_2/delivery_order.csv", orders);

  if (argc < 2) {
      printf("Usage: %s [-status|-deliver|-list] [nama]\n", argv[0]);
      return 1;
  }

  if (argc == 3 && strcmp(argv[1], "-status") == 0) {
    char *target = argv[2];
    for (int i = 0; i < MAX_ORDERS; i++) {
      if (strcmp(orders[i].nama, target) == 0) {
          found = true;
          if (orders[i].status == true) {
              printf("Status for %s: Delivered by %s\n", orders[i].nama, orders[i].delivered_by);
          } else {
              printf("Status for %s: Pending\n", orders[i].nama);
          }
          break;
      }    
  } if (!found) {
    printf("Order in the name of %s not found.\n", target);
  } 
  } else if (argc == 2 && strcmp(argv[1], "-list") == 0) {
    printf("Order List:\n");
    for (int i = 0; i < MAX_ORDERS; i++) {
        if (strlen(orders[i].nama) == 0) continue; 
        printf("- %s: %s (%s)\n", orders[i].nama, orders[i].status == 1 ? "Delivered" : "Pending", orders[i].jenis);
    }    
  } else if (strcmp(argv[1], "-deliver") == 0) {
    char *target_name = argv[2];
    char *delivered_by = getenv("USER");

  for (int i = 0; i < total_order; i++) {
     if (strcmp(orders[i].nama, target_name) == 0 && strcmp(orders[i].jenis, "Reguler") == 0) {
        orders[i].status = true;
        if (delivered_by != NULL) {
        strcpy(orders[i].delivered_by, delivered_by);
        }
        printf("Delivering %s by %s\n", orders[i].nama, orders[i].delivered_by);
        write_log(orders[i].delivered_by, orders[i].nama, orders[i].alamat);
        break;
     }  
     else if (strcmp(orders[i].nama, target_name) == 0 && strcmp(orders[i].jenis, "Express") == 0) {
        printf("Command can't deliver an Express orders.\n");
        break;
   }
 }
  shmdt(orders);
  return 0;
}
}


