# Sisop-3-2025-IT10

## Member

| No  | Nama                   | NRP        |
| --- | ---------------------- | ---------- |
| 1   | Ardhi Putra Pradana    | 5027241022 |
| 2   | Aslam Ahmad Usman      | 5027241074 |
| 3   | Kanafira Vanesha Putri | 5027241010 |

## Reporting

### Soal 1

**Dikerjakan oleh: Ardhi Putra Pradana (5027241022)**

#### Penjelasan

**Server - image_client.c**

Untuk langkah pertama adalah membuat RPC `server` yang dapat berjalan secara daemon, dengan function daemon berikut ini

```c
void daemonize() {
    pid_t pid = fork();
    int status;

    if (pid < 0) exit(1);
    if (pid > 0) exit(0);
    if (setsid() < 0) exit(1);

    umask(0);
    for (int x = sysconf(_SC_OPEN_MAX); x > 0; x--) close(x);
}
```

Dan karena berjalan secara daemon dan handle dari tiap socket connection dilakukan oleh child process, maka diperlukan signal untuk bisa melakukan kill process setelah connection dari client terputus agar tidak terjadi zombie process, yaitu dengan beberapa hal berikut ini

```c
void reap_zombies(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}
```

Lalu pada bagian atas function `main` diberikan kode berikut untuk melakukan handle daemon dan process signalnya

```c
daemonize();

struct sigaction sa;
sa.sa_handler = reap_zombies;
sigemptyset(&sa.sa_mask);
sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
sigaction(SIGCHLD, &sa, NULL);
```

Kemudian membuat function membuat `listening socket connection`, dengan function berikut ini

```c
int make_socket() {
    struct sockaddr_in address;
    int sockfd;
    socklen_t addrlen = sizeof(address);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if ((bind(sockfd, (struct sockaddr *)&address, addrlen)) < 0) return -1;
    if (listen(sockfd, 5) < 0) return -1;

    return sockfd;
}
```

Setelah itu membuat function `handler` sebagai gate untuk semua action yang dapat dilakukan dan dikirim dari client ke server, yaitu sebagai berikut

```c
void handler(int sock) {
    char action[1024];
    while (1) {
        ssize_t buflen = recv(sock, action, sizeof(action), 0);
        if (buflen <= 0) break;
        action[buflen] = '\0';
        if (action[buflen - 1] == '\n') action[buflen - 1] = '\0';
        if (strcmp(action, "exit") == 0) {
            logger("Client", "EXIT", "Client requested to exit");
            break;
        } else if (strcmp(action, "decode") == 0) {
            decoder_file(sock);
        } else if (strcmp(action, "download") == 0) {
            downloader_file(sock);
        }
    }
}
```

Inti dari function diatas adalah untuk melakukan pengkondisian sebagai acuan kira - kira fitur apa yang perlu dieksekusi dan melakukan respon ke client, jadi handler ini akan membaca buffer dari client, dan mencocokkan actionnya yaitu ada _exit, decode, dan download_, setelah itu akan memanggil handler functionnya masing - masing.

Pertama masuk ke bagian untuk melakukan decode file yang dikirim dari client, yaitu handlernya sebagai berikut

```c
int decoder_file(int sock) {
    logger("Client", "DECRYPT", "Text data");

    char *folder = "database";
    struct stat st;
    if (stat(folder, &st) == -1) {
        if (mkdir(folder, 0700) == -1) return -1;
    };

    time_t timestamp = time(NULL);
    char strtime[32];
    char filename[64];
    char filepath[128];

    snprintf(strtime, sizeof(strtime), "%ld", timestamp);
    snprintf(filename, sizeof(filename), "%s.jpeg", strtime);
    snprintf(filepath, sizeof(filepath), "%s/%s", folder, filename);

    char size_buffer[32];
    ssize_t size_len = recv(sock, size_buffer, sizeof(size_buffer) - 1, 0);
    if (size_len <= 0) return -1;
    size_buffer[size_len] = '\0';
    size_t expected_size = strtoul(size_buffer, NULL, 10);

    if (recvto_file(sock, filepath, expected_size) < 0) return -1;

    long file_size;
    char *file_content = read_file(filepath, &file_size);
    if (!file_content) return -1;

    revstr(file_content);

    unsigned char *decoded;
    size_t decoded_size = hexs2bin(file_content, &decoded);
    if (decoded_size == 0) {
        free(file_content);
        free(decoded);
        return -1;
    }

    write_file(filepath, (char *)decoded, decoded_size);

    free(file_content);
    free(decoded);

    char message[128];
    snprintf(message, sizeof(message), "Text decrypted and saved as %s", filename);
    send(sock, message, strlen(message), 0);

    logger("Client", "SAVE", filename);
    return 0;
}
```

Disini function tersebut akan menyimpan base directory filenya yaitu di `database` kemudian membuat schema filename yaitu menggunakan current timestamp, kemudian mulai membaca content dari client, pertama membaca `size` buffernya terlebih dahulu, setelah itu memanggil function `recvto_file` dan melakukan read kembali isi file tersebut melakukan reversing string dan melakukan decode hex nya dan menyimpannya kembali.

```c
int recvto_file(int sock, char *filename, size_t size) {
    FILE *fp;
    char buffer[1024];
    size_t total_received = 0;
    int threshold = 50;

    fp = fopen(filename, "wb");
    if (!fp) return -1;

    while (total_received < size - threshold) {
        size_t remaining = size - total_received;
        size_t to_recv = sizeof(buffer);
        if (remaining < to_recv) to_recv = remaining;

        ssize_t buflen;
        do {
            buflen = recv(sock, buffer, to_recv, 0);
        } while (buflen == -1 && errno == EINTR);

        if (buflen <= 0) {
            fflush(fp);
            fclose(fp);
            return -1;
        }

        fwrite(buffer, 1, buflen, fp);
        total_received += buflen;
    }

    fflush(fp);
    fclose(fp);
    return 0;
}
```

Diatas adalah function untuk melakukan read dari setiap buffer content dari client sesuai dengan size yang dipassing dari argumentnya, dan lalu melakukan writing ke file targetnya.

```c
char *read_file(char *filename, long *file_size) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    *file_size = ftell(file);
    rewind(file);

    char *buffer = (char *)malloc(*file_size + 1);
    if (buffer == NULL) {
        fflush(file);
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, *file_size, file);
    if (bytes_read != *file_size) {
        free(buffer);
        fflush(file);
        fclose(file);
        return NULL;
    }

    buffer[bytes_read] = '\0';

    fflush(file);
    fclose(file);
    return buffer;
}

int write_file(char *filename, char *content, size_t size) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -1;
    fwrite(content, 1, size, fp);
    fflush(fp);
    fclose(fp);
}
```

Diatas adalah function untuk melakukan handle write dan read file. Dan berikut adalah function yang digunakan untuk melakukan reversing dan melakukan decode hex contentnya

```c
void revstr(char *str) {
    if (!str) return;
    int i = 0;
    int j = strlen(str) - 1;
    while (i < j) {
        char c = str[i];
        str[i] = str[j];
        str[j] = c;
        i++;
        j--;
    }
}

// https://nachtimwald.com/2017/09/24/hex-encode-and-decode-in-c/#hex-to-binary
int hexchr2bin(char hex, char *out) {
    if (out == NULL)
        return 0;

    if (hex >= '0' && hex <= '9') {
        *out = hex - '0';
    } else if (hex >= 'A' && hex <= 'F') {
        *out = hex - 'A' + 10;
    } else if (hex >= 'a' && hex <= 'f') {
        *out = hex - 'a' + 10;
    } else {
        return 0;
    }

    return 1;
}

size_t hexs2bin(char *hex, unsigned char **out) {
    size_t len;
    char b1, b2;
    size_t i;

    if (hex == NULL || *hex == '\0' || out == NULL)
        return 0;

    len = strlen(hex);
    if (len % 2 != 0)
        return 0;
    len /= 2;

    *out = malloc(len + 1);
    if (*out == NULL)
        return 0;

    for (i = 0; i < len; i++) {
        if (!hexchr2bin(hex[i * 2], &b1) || !hexchr2bin(hex[i * 2 + 1], &b2)) {
            free(*out);
            *out = NULL;
            return 0;
        }
        (*out)[i] = (b1 << 4) | b2;
    }

    (*out)[len] = '\0';

    return len;
}
```

Setelah itu masuk ke bagian untuk melakukan handle dari request download client, gate function utamanya adalah sebagai berikut

```c
int downloader_file(int sock) {
    char filename[128];
    char filepath[256];
    char *folder = "database";

    ssize_t buflen = recv(sock, filename, sizeof(filename) - 1, 0);
    if (buflen <= 0) return -1;
    filename[buflen] = '\0';
    if (filename[buflen - 1] == '\n') filename[buflen - 1] = '\0';

    logger("Client", "DOWNLOAD", filename);

    snprintf(filepath, sizeof(filepath), "%s/%s", folder, filename);
    struct stat st;
    if (stat(filepath, &st) == -1) {
        char message[128];
        snprintf(message, sizeof(message), "Gagal menemukan file untuk dikirim ke client");
        send(sock, message, strlen(message), 0);
        return -1;
    }

    send_file(sock, filepath);

    logger("Server", "UPLOAD", filename);
    return 0;
}
```

Handler tersebut akan membaca nama file yang dikirim dari client dan memeriksa apakah benar - benar ada disisi database server, jika ada maka akan mengembalikan atau melakukan send buffer ke client, yaitu dengan melakukan size terlebih dahulu lalu contentnya. Untuk melakukan send tersebut function ini memanggil function lain yaitu `send_file`

```c
int send_file(int sock, const char *filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -1;

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    rewind(fp);

    char size_str[32];
    snprintf(size_str, sizeof(size_str), "%ld\n", file_size);

    if (send(sock, size_str, strlen(size_str), 0) < 0) {
        fflush(fp);
        fclose(fp);
        return -1;
    }
    sleep(0.1);

    char *buffer = malloc(file_size);
    if (!buffer) {
        fflush(fp);
        fclose(fp);
        return -1;
    }

    size_t read_size = fread(buffer, 1, file_size, fp);
    fflush(fp);
    fclose(fp);

    if (read_size != file_size) {
        free(buffer);
        return -1;
    }

    if (send(sock, buffer, file_size, 0) < 0) {
        free(buffer);
        return -1;
    }

    free(buffer);
    return 0;
}
```

Kemudian untuk logic loggernya sebagai berikut ini

```c
void logger(char *from, char *type, char *msg) {
    FILE *fp = fopen("server.log", "a");
    if (fp == NULL) return;
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(fp, "[%s][%s] [%s]: [%s]\n", time_str, from, type, msg);
    fflush(fp);
    fclose(fp);
}
```

Logger function tersebut akan dipanggil dibeberapa function yang ada untuk memberikan info logging yang sesuai dengan aktifitas yang dilakukan oleh server atau dari client ke server.

Lalu semua hal tersebut akan dibungkus menjadi di 1 function `main` sebagai berikut ini

```c
int main(int argc, char *argv[]) {
    daemonize();

    struct sigaction sa;
    sa.sa_handler = reap_zombies;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    int sockfd = make_socket();
    if (sockfd < 0) {
        perror("socket setup failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    while (1) {
        int client_sock = accept(sockfd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_sock < 0) continue;

        pid_t pid = fork();
        if (pid < 0) {
            close(client_sock);
            continue;
        } else if (pid == 0) {
            close(sockfd);
            handler(client_sock);
            close(client_sock);
            exit(0);
        } else {
            close(client_sock);
        }
    }

    close(sockfd);
    return 0;
}
```

**Client - image_client.c**

Sebagai besar hal yang ada diclient sebanarnya adalah sama dengan yang diserver, namun hanya process nya yang sedikit dibalik, namun penggunaan function dan deklarasi functionnya memiliki kemiripan atau bahkan sama.

Pertama membuat function untuk bisa melakukan connection socket ke server, yaitu sebagai berikut

```c
int connect_socket() {
    int sockfd;
    struct sockaddr_in address;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;

    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = inet_addr(HOST);

    if (connect(sockfd, (struct sockaddr *)&address, sizeof(address)) < 0) return -1;

    return sockfd;
}
```

Lalu untuk handler nya sebagai berikut ini

```c
int handler(int sockfd, int opt) {
    char *folder = "secrets";
    char filename[256];

    char target[512];

    switch (opt) {
        case 1:
            printf("Enter the file name: ");
            scanf("%s", filename);
            snprintf(target, sizeof(target), "%s/%s", folder, filename);
            if (send(sockfd, "decode\n", strlen("decode\n"), 0) < 0) return -1;
            if (send_file(sockfd, target) < 0) return -1;

            char res[256];
            ssize_t res_len = recv(sockfd, res, sizeof(res), 0);
            if (res_len <= 0) break;
            res[res_len] = '\0';
            printf("Server: %s\n\n", res);
            break;
        case 2:
            printf("Enter the file name to download: ");
            scanf("%s", filename);
            if (send(sockfd, "download\n", strlen("download\n"), 0) < 0) return -1;
            snprintf(target, sizeof(target), "%s\n", filename);
            sleep(0.1);
            if (send(sockfd, target, strlen(target), 0) < 0) return -1;

            char size_buffer[128];
            ssize_t size_len = recv(sockfd, size_buffer, sizeof(size_buffer) - 1, 0);
            if (size_len <= 0) break;
            size_buffer[size_len] = '\0';

            if (strlen(size_buffer) > 10) {
                printf("Server: %s\n\n", size_buffer);
                break;
            }

            size_t expected_size = strtoul(size_buffer, NULL, 10);

            if (recvto_file(sockfd, filename, expected_size) < 0) return -1;
            break;
        case 3:
            if (send(sockfd, "exit\n", strlen("exit\n"), 0) < 0) return -1;
            close(sockfd);
            exit(EXIT_SUCCESS);
            return 0;
        default:
            printf("Invalid option, try again\n\n");
    }

    return 0;
}
```

Terdapat 3 handler opt disana yaitu **1** untuk meminta request ke server untuk melakukan decode filenya, **2** untuk mendownload file dari server, dan **3** untuk exit.

Untuk bagian **1** yaitu decode, maka handler tersebut akan meminta input nama file dari user, kemudian mengirimkan action **decode** dan lalu memanggil **send_file** sesuai dengan nama file yang diinput oleh user, yaitu sebagai berikut ini

```c
int send_file(int sockfd, const char *filename) {
    struct stat st;
    if (stat(filename, &st) < 0) {
        printf("Salah nama text file input\n\n");
        return -1;
    }

    FILE *fp = fopen(filename, "rb");
    if (!fp) return -1;

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    rewind(fp);

    char size_str[32];
    snprintf(size_str, sizeof(size_str), "%ld\n", file_size);

    if (send(sockfd, size_str, strlen(size_str), 0) < 0) {
        fflush(fp);
        fclose(fp);
        return -1;
    }
    sleep(0.1);

    char *buffer = malloc(file_size);
    if (!buffer) {
        fflush(fp);
        fclose(fp);
        return -1;
    }

    size_t read_size = fread(buffer, 1, file_size, fp);
    fflush(fp);
    fclose(fp);

    if (read_size != file_size) {
        free(buffer);
        return -1;
    }

    if (send(sockfd, buffer, file_size, 0) < 0) {
        free(buffer);
        return -1;
    }

    free(buffer);
    return 0;
}
```

Setelah itu client akan melakukan recv buffer dari server untuk mendapatkan response message dari server itu sendiri.

Lalu untuk **2** yaitu melakukan download, maka client akan meminta input dari user yaitu nama file yang ada pada database server, kemudian mengirimkan action **download** ke server, dan setelah itu lalu mengirimkan nama file yang diinput oleh user ke server, dan kemudian karena server akan memberikan response sebuah `size` jika berhasil, maka dicek terlebih dahulu jika length dari response nya lebih dari 10 maka sebenarnya yang diresponse oleh server bukanlah `size` melainkan `error message`, oleh karena itu akan diprint ke console, namun jika valid maka akan memanggil function `recvto_file`, yaitu sebagai berikut

```c
int recvto_file(int sock, char *filename, size_t size) {
    FILE *fp;
    char buffer[1024];
    size_t total_received = 0;
    int threshold = 50;

    fp = fopen(filename, "wb");
    if (!fp) return -1;

    while (total_received < size - threshold) {
        size_t remaining = size - total_received;
        size_t to_recv = sizeof(buffer);
        if (remaining < to_recv) to_recv = remaining;

        ssize_t buflen;
        do {
            buflen = recv(sock, buffer, to_recv, 0);
        } while (buflen < 0 && errno == EINTR);

        if (buflen <= 0) {
            perror("error receiving file");
            fflush(fp);
            fclose(fp);
            return -1;
        }

        fwrite(buffer, 1, buflen, fp);
        total_received += buflen;
    }

    fflush(fp);
    fclose(fp);
    return 0;
}
```

Logicnya sama persis dengan yang ada diserver.

Semua hal tadi dicombine ke `main` function sebagai berikut

```c
int main(int argc, char *argv[]) {
    int sockfd = connect_socket();

    if (sockfd < 0) {
        printf("Gagal connect ke server\n\n");
        exit(EXIT_FAILURE);
    }
    printf("Connected to address %s:%d\n\n", HOST, PORT);

    int opt;

    while (1) {
        printf("========================\n");
        printf("| Image Decoder Client |\n");
        printf("========================\n");
        printf("1. Send input file to server\n");
        printf("2. Download file from server\n");
        printf("3. Exit\n");
        printf(">>> ");
        scanf("%d", &opt);
        cleanline();
        handler(sockfd, opt);
    }

    close(sockfd);
    return 0;
}
```

Jadi function main akan melakukan while loop untuk menampilkan menu dan meminta input option dari user sesuai dengan menu yang ditampilkan. Untuk error handler bisa dilihat pada deklrasi beberapa function sebelumnya itu sudah memiliki error handlernya masing - masing, lalu untuk memastikan beberapa input adalah number dan tidak memiliki newline, maka disini menggunakan function `cleanline` setelah `scanf` dipanggil

```c
void cleanline() {
    while (getchar() != '\n');
}
```

#### Output

1. Daemonize server

![alt text](assets/soal_1/daemonize_server.png)

2. Client connected

![alt text](assets/soal_1/client_connected.png)

3. Decode file

![alt text](assets/soal_1/decode_file.png)

4. Download file

![alt text](assets/soal_1/download_file.png)

5. Log file

![alt text](assets/soal_1/log_file.png)

6. Error handler

![alt text](assets/soal_1/error_handler.png)

#### Kendala

Tidak ada kendala

### Soal 2

**Dikerjakan oleh: Aslam Ahmad Usman (5027241074)**

#### Penjelasan

#### Output

#### Kendala

Tidak ada kendala

### Soal 3

**Dikerjakan oleh: Ardhi Putra Pradana (5027241022)**

#### Penjelasan

#### Output

1. Client connected ke server dan main menu

![alt text](assets/soal_3/client_connected.png)

2. Stats player

![alt text](assets/soal_3/stats.png)

3. Weapon list dan buy weapon

![alt text](assets/soal_3/weapon_list.png)

4. View inventory and equipped

![alt text](assets/soal_3/inventory_equipped.png)

5. Battle mode

![alt text](assets/soal_3/battle.png)

6. Attacking dan damage

![alt text](assets/soal_3/attack.png)

7. Critical attack

![alt text](assets/soal_3/critical.png)

8. Passive active

![alt text](assets/soal_3/passive.png)

9. Gold reward

![alt text](assets/soal_3/reward.png)

10. Stats setelah melakukan kill dan mendapatkan reward

![alt text](assets/soal_3/after_battle.png)

11. Error handling

![alt text](assets/soal_3/error_1.png)
![alt text](assets/soal_3/error_2.png)
![alt text](assets/soal_3/error_3.png)
![alt text](assets/soal_3/error_4.png)
![alt text](assets/soal_3/error_5.png)

#### Kendala

Tidak ada kendala

### Soal 4

**Dikerjakan oleh: Kanafira Vanesha Putri (5027241010)**

#### Penjelasan

Pada soal ini diminta untuk membantu Sung Jin Woo untuk melakukan modifikasi program.  
A) Membuat file system.c dan hunter.c dan memastikan bahwa hunter hanya bisa dijalankan apabila system sudah jalan. Sehingga code ini memastikan bahwa shared memory telah dibuat.

```c
    signal(SIGINT, sigint_handler);
    key_t key = get_system_key();
    shm_id = shmget(key, sizeof(struct SystemData), IPC_CREAT | 0666);
    system_data = shmat(shm_id, NULL, 0);
    if (system_data->num_hunters == 0 && system_data->num_dungeons == 0) {
        system_data->num_hunters = 0;
        system_data->num_dungeons = 0;
        system_data->current_notification_index = 0;
```

B) Membuat registrasi dan login menu serta hunter menu. Jadi kita membuat function untuk registrasi menu dengan ketentuan yang sudah ada di soal. lalu memasukkan menu tersebut ke main nya agar bisa dipanggil sesuai dengan kondisinya.

```c
void print_menu() {
    printf("\n" BOLD CYAN "=== '%s' MENU ===\n" RESET, username);
    printf(" " GREEN "1. Dungeon List\n");
    printf(" " GREEN "2. Dungeon Raid\n");
    printf(" " GREEN "3. Hunter Battle\n");
    printf(" " GREEN "4. Notification\n");
    printf(" " GREEN "5. Exit\n" RESET);
    printf(" Choice: ");
}

    int choice;
    while (1) {
        printf("\n=== HUNTER MENU ===\n");
        printf("1. Register\n");
        printf("2. Login\n");
        printf("3. Exit\n");
        printf("Choice: ");
        scanf("%d", &choice);
        clear_input_buffer();

        if (choice == 1) {
            printf("Masukkan username: ");
            scanf("%s", username);
            clear_input_buffer();

            int idx = -1;
            for (int i = 0; i < system_data->num_hunters; i++) {
                if (strcmp(system_data->hunters[i].username, username) == 0) {
                    idx = i;
                    break;
                }
            }

            if (idx == -1) {
                idx = system_data->num_hunters;
                strcpy(system_data->hunters[idx].username, username);
                system_data->hunters[idx].level = 1;
                system_data->hunters[idx].exp = 0;
                system_data->hunters[idx].atk = 10;
                system_data->hunters[idx].hp = 100;
                system_data->hunters[idx].def = 5;
                system_data->hunters[idx].banned = 0;

                key_t hunter_key = ftok("/tmp", 'A' + idx);
                system_data->hunters[idx].shm_key = hunter_key;
                system_data->num_hunters++;

                hunter_id = shmget(hunter_key, sizeof(struct Hunter), IPC_CREAT | 0666);
                if (hunter_id == -1) {
                    perror("shmget");
                    exit(EXIT_FAILURE);
                }
                this_hunter = shmat(hunter_id, NULL, 0);
                memcpy(this_hunter, &system_data->hunters[idx], sizeof(struct Hunter));
                printf("Registrasi sukses!\n");

                break;
            } else {
                printf("Username sudah terdaftar.\n");
            }
        } else if (choice == 2) {
            printf("Masukkan username: ");
            scanf("%s", username);
            clear_input_buffer();

            int idx = -1;
            for (int i = 0; i < system_data->num_hunters; i++) {
                if (strcmp(system_data->hunters[i].username, username) == 0) {
                    idx = i;
                    break;
                }
            }

            if (idx == -1) {
                printf("Username tidak ditemukan.\n");
            } else {
                if (system_data->hunters[idx].banned) {
                    printf("Akun Anda dibanned. Tidak bisa login.\n");
                    shmdt(system_data);
                    return 1;
                }
                hunter_id = shmget(system_data->hunters[idx].shm_key, sizeof(struct Hunter), 0666);
                this_hunter = shmat(hunter_id, NULL, 0);
                printf("Login sukses!\n");

                break;
            }
        } else if (choice == 3) {
            shmdt(system_data);
            printf("Exiting without deleting shared memory.\n");
            exit(0);
        } else {
            printf(BOLD RED"Invalid option.\n"RESET);
        }
    }
```

Setelah registrasi lalu terdapat menu hunter.

```c
  printf("\n=== HUNTER SYSTEM ===\n");

int choice;
    while (1) {
        printf("\n=== HUNTER MENU ===\n");
        printf("1. Register\n");
        printf("2. Login\n");
        printf("3. Exit\n");
        printf("Choice: ");
        scanf("%d", &choice);
        clear_input_buffer();
```

C) Membuat system menu yang lalu terdapat fitur untuk menampilkan informasi lengkap hunter. Sehingga pertama tama kita buat dulu function untuk menampilkan hunter yang ada.

```c
void tampilkan_semua_hunter() {
    printf("\n" BOLD CYAN "=== DAFTAR HUNTER ===\n" RESET);
    printf(BOLD MAGENTA "+--------------------+-----+-----+-----+-----+-----+-----------+\n");
    printf("│ Username           │ Lv  │EXP  │ATK  │HP   │DEF  │ Status    │\n");
    printf("+--------------------+-----+-----+-----+-----+-----+-----------+\n" RESET);

    for (int i = 0; i < system_data->num_hunters; i++) {
        struct Hunter h = system_data->hunters[i];
        printf("│ %-18s │ %3d │ %3d │ %3d │ %3d │ %3d │ %-9s │\n",
               h.username, h.level, h.exp, h.atk, h.hp, h.def,
               h.banned ? RED "Banned" RESET : GREEN "Active" RESET);
    }

    printf(BOLD MAGENTA "+--------------------+-----+-----+-----+-----+-----+-----------+\n" RESET);
}
```

Lalu kita panggil function diatas pada kondisi sesuai yang ada di menu yang tertera pada soal.

```c
    int cmd;
while (1) {
    printf("== SYSTEM MENU ==\n");
    printf("1. Hunter Info\n");
    printf("2. Dungeon Info\n");
    printf("3. Generate Dungeon\n");
    printf("4. Duel Hunter\n");
    printf("5. Ban H-unter\n");
    printf("6. Unban Hunter\n");
    printf("7. Reset Hunter\n");
    printf("8. Exit\n");
    printf("Choice: ");

    scanf("%d", &cmd);
    while (getchar() != '\n');
    if (cmd == 1) {
        tampilkan_semua_hunter();
    }
    else if (cmd == 2) {
        tampilkan_semua_dungeon();
    }
    else if (cmd == 3) {
        generate_dungeon();
    }
    else if (cmd == 4) {
        duel();
    }
    else if (cmd == 5) {
        ban_hunter();
    }
    else if (cmd == 6) {
        unban_hunter();
    }
    else if (cmd == 7) {
        reset_hunter();
    }
    else if (cmd == 8) {
        sigint_handler(0);
        break;
    }
    else {
        printf(BOLD RED"Invalid option. \n"RESET);
    }
}
```

D) Membuat funtion untuk fitur generate dungeon dengan ketentuan ketentuan yang telah diberikan pada soal yaitu level, exp, atk, hps, def, dan key lalu memasukkannya ke dalam main menu. dimana setiap dungeon akan disimpan dalam shared memory sendiri yang berbeda dan dapat diakses oleh hunter.

```c
void generate_dungeon() {
    if (system_data->num_dungeons >= MAX_DUNGEONS) {
        printf(RED "Jumlah dungeon maksimal tercapai!\n" RESET);
        return;
    }

    const char* dungeon_names[] = {
        "Double Dungeon", "Demon Castle", "Pyramid Dungeon", "Red Gate Dungeon",
        "Hunters Guild Dungeon", "Busan A-Rank Dungeon", "Insects Dungeon",
        "Goblins Dungeon", "D-Rank Dungeon", "Gwanak Mountain Dungeon",
        "Hapjeong Subway Station Dungeon"
    };

    const int min_levels[] = {1, 2, 3, 1, 2, 4, 1, 1, 2, 3, 4};
    const int exps[] = {150, 200, 250, 180, 220, 300, 160, 170, 190, 210, 250};
    const int atks[] = {130, 150, 140, 120, 130, 150, 140, 125, 130, 135, 145};
    const int hps[] = {60, 80, 70, 65, 75, 90, 70, 65, 70, 85, 80};
    const int defs[] = {30, 40, 35, 30, 35, 45, 40, 30, 35, 40, 45};

    int idx = system_data->num_dungeons;
    struct Dungeon *d = &system_data->dungeons[idx];

    strcpy(d->name, dungeon_names[idx]);
    d->min_level = min_levels[idx];
    d->exp = exps[idx];
    d->atk = atks[idx];
    d->hp = hps[idx];
    d->def = defs[idx];
    d->shm_key = ftok("/tmp", 'A' + idx);
    system_data->num_dungeons++;

    printf(GREEN "\n=== Dungeon berhasil dibuat! ===\n" RESET);
    printf(" " BOLD BLUE "Name           : " RESET "%s\n", d->name);
    printf(" " BOLD BLUE "Minimum Level  : " RESET "%d\n", d->min_level);
    printf(" " BOLD BLUE "EXP Reward     : " RESET "%d\n", d->exp);
    printf(" " BOLD BLUE "ATK            : " RESET "%d\n", d->atk);
    printf(" " BOLD BLUE "HP             : " RESET "%d\n", d->hp);
    printf(" " BOLD BLUE "DEF            : " RESET "%d\n", d->def);
    printf(" " BOLD BLUE "SharedMem Key  : " RESET "%d\n", d->shm_key);
}
```

E) Membuat function untuk fitur yang dapat menampilkan daftar lengkap dungeon. Function tampilkan_semua_dungeon berisi rincian spesifikasi dari dungeon yang telah digenerate tanpa memandang level. Lalu memanggil function ini pada main menu.

```c
void tampilkan_semua_dungeon() {
    printf("\n" BOLD CYAN "╔════════════════════════════════════════════╗\n" RESET);
    printf(        BOLD CYAN "║             DAFTAR DUNGEON                 ║\n" RESET);
    printf(        BOLD CYAN "╚════════════════════════════════════════════╝\n" RESET);

    for (int i = 0; i < system_data->num_dungeons; i++) {
        struct Dungeon d = system_data->dungeons[i];
        printf(BOLD MAGENTA "\n[Dungeon %d]\n" RESET, i + 1);
        printf(" " BOLD "• Nama Dungeon    : " RESET "%s\n", d.name);
        printf(" " BOLD "• Minimum Level   : " RESET "%d\n", d.min_level);
        printf(" " BOLD "• EXP             : " RESET "%d\n", d.exp);
        printf(" " BOLD "• ATK             : " RESET "%d\n", d.atk);
        printf(" " BOLD "• HP              : " RESET "%d\n", d.hp);
        printf(" " BOLD "• DEF             : " RESET "%d\n", d.def);
        printf(" " BOLD "• SharedMem Key   : " RESET "%d\n", d.shm_key);
    }
}
```

F) Menambahkan fitur menampilkan dungeon sesuai level hunter pada menu hunter. Pada function ini dungeon ditampilkan berdasarkan level dari hunternya sehingga berbeda dengan yang ada pada menu system.

```c
void dungeon_list() {
    printf("=== AVAILABLE DUNGEONS ===\n");

    int count = 0;

    for (int i = 0; i < system_data->num_dungeons; i++) {
        if (this_hunter->level >= system_data->dungeons[i].min_level) {
            printf("%d. %s (Level %d+)\n",
                   i + 1,
                   system_data->dungeons[i].name,
                   system_data->dungeons[i].min_level);
            count++;
        }
    }

    if (count == 0) {
        printf("No dungeons available for your level.\n");
    }
}
```

G) Menambahkan function untuk fitur dungeon raid. Yaitu dengan mengecek terlebih dahulu apakah ada dungeon yang tersedia lalu memastikan indexnya dan jika hunter menang maka dungeon akan hilang dan menambahkan stat dari dungeon yang dikalahkan ke stat hunter lalu jika exp hunter mencapai 500 maka hunter akan naik level dan jika level up maka exp level kembali ke 0

```c
void dungeon_raid() {
    dungeon_list();
    printf("Choose Dungeon: ");
    int choice;
    scanf("%d", &choice);
    clear_input_buffer();
    choice -= 1;

    if (choice >= 0 && choice < system_data->num_dungeons &&
        this_hunter->level >= system_data->dungeons[choice].min_level) {

        struct Dungeon d = system_data->dungeons[choice];
        this_hunter->atk += d.atk;
        this_hunter->hp += d.hp;
        this_hunter->def += d.def;
        this_hunter->exp += d.exp;

        if (this_hunter->exp >= 500) {
            this_hunter->level++;
            this_hunter->exp = 0;
        }

        for (int i = choice; i < system_data->num_dungeons - 1; i++) {
            system_data->dungeons[i] = system_data->dungeons[i + 1];
        }
        system_data->num_dungeons--;

        if (system_data->num_dungeons > 0) {
            system_data->current_notification_index %= system_data->num_dungeons;
        } else {
            system_data->current_notification_index = 0;
        }

        for (int i = 0; i < system_data->num_hunters; i++) {
            if (strcmp(system_data->hunters[i].username, this_hunter->username) == 0) {
                memcpy(&system_data->hunters[i], this_hunter, sizeof(struct Hunter));
                break;
            }
        }

        shmctl(shmget(d.shm_key, sizeof(struct Dungeon), 0666), IPC_RMID, NULL);

        printf("\nRaid Success! Gained:\n");
        printf("ATK: +%d\n", d.atk);
        printf("HP: +%d\n", d.hp);
        printf("DEF: +%d\n", d.def);
        printf("EXP: +%d\n", d.exp);
    } else {
        printf("Invalid choice or level too low.\n");
    }
}
```

H) Menambahkan function untuk fitur hunter battle. Pada menu ini hunter dapat memilih hunter lain yang ingin dilawan lalu terdapat kondisi dimana hunter yang menang akan mendapatkan stat tambahan dari stat hunter yang kalah dan akan dihapus dari system.

```c
void duel() {
    char user1[50], user2[50];
    printf("\n" BOLD YELLOW "Masukkan username hunter 1: " RESET);
    scanf("%s", user1);
    printf(BOLD YELLOW "Masukkan username hunter 2: " RESET);
    scanf("%s", user2);

    int idx1 = -1, idx2 = -1;
    for (int i = 0; i < system_data->num_hunters; i++) {
        if (strcmp(system_data->hunters[i].username, user1) == 0) idx1 = i;
        if (strcmp(system_data->hunters[i].username, user2) == 0) idx2 = i;
    }

    if (idx1 == -1 || idx2 == -1) {
        printf(RED "Salah satu atau kedua hunter tidak ditemukan.\n" RESET);
        return;
    }

    struct Hunter *h1 = &system_data->hunters[idx1];
    struct Hunter *h2 = &system_data->hunters[idx2];

    int total1 = h1->atk + h1->hp + h1->def;
    int total2 = h2->atk + h2->hp + h2->def;

    if (total1 == total2) {
        printf(YELLOW "Pertarungan imbang. Tidak ada perubahan.\n" RESET);
        return;
    }

    struct Hunter *winner = (total1 > total2) ? h1 : h2;
    struct Hunter *loser = (total1 > total2) ? h2 : h1;

    key_t loser_key = loser->shm_key;

    winner->atk += loser->atk;
    winner->hp += loser->hp;
    winner->def += loser->def;

    int winner_shmid = shmget(winner->shm_key, sizeof(struct Hunter), 0666);
    struct Hunter *winner_shm = shmat(winner_shmid, NULL, 0);
    if (winner_shm != (void *)-1) {
        winner_shm->atk = winner->atk;
        winner_shm->hp = winner->hp;
        winner_shm->def = winner->def;
        shmdt(winner_shm);
    }

    for (int i = 0; i < system_data->num_hunters; i++) {
        if (strcmp(system_data->hunters[i].username, loser->username) == 0) {
            for (int j = i; j < system_data->num_hunters - 1; j++) {
                system_data->hunters[j] = system_data->hunters[j + 1];
            }
            system_data->num_hunters--;
            break;
        }
    }

    printf(GREEN "\n%s menang duel melawan %s!\n" RESET, winner->username, loser->username);
}
```

I) Menambahkan function untuk fitur ban hunter. Lalu function ini akan dipanggil pada main menu system

```c
void ban_hunter() {
    char user[50];
    printf("Masukkan username yang ingin diban: ");
    scanf("%s", user);
    for (int i = 0; i < system_data->num_hunters; i++) {
        if (strcmp(system_data->hunters[i].username, user) == 0) {
            system_data->hunters[i].banned = 1;

            int shmid = shmget(system_data->hunters[i].shm_key, sizeof(struct Hunter), 0666);
            struct Hunter *shm_hunter = shmat(shmid, NULL, 0);
            if (shm_hunter != (void *)-1) {
                shm_hunter->banned = 1;
                shmdt(shm_hunter);
            }

            printf("%s telah diban.\n", user);
            return;
        }
    }
    printf("Hunter tidak ditemukan.\n");
}
```

<<<<<<< HEAD
J) Menambahkan function untuk fitur unban hunter lalu mereset hunter. Sehingga function ini mengaktifkan kembali hunter yang sebelumnya dilarang (banned), dengan cara mengatur flag banned = 0 pada data hunter di shared memory.
=======
J) Menambahkan function untuk fitur unban hunter. Sehingga function ini mengaktifkan kembali hunter yang sebelumnya dilarang (banned), dengan cara mengatur flag banned = 0 pada data hunter di shared memory.

>>>>>>> refs/remotes/origin/main
```c
void unban_hunter() {
    char user[50];
    printf("Masukkan username yang ingin di-unban: ");
    scanf("%s", user);
    for (int i = 0; i < system_data->num_hunters; i++) {
        if (strcmp(system_data->hunters[i].username, user) == 0) {
            system_data->hunters[i].banned = 0;
            printf("%s telah di-unban.\n", user);
            return;
        }
    }
    printf("Hunter tidak ditemukan.\n");
}

void reset_hunter() {
    char user[50];
    printf("Masukkan username yang ingin direset: ");
    scanf("%s", user);
    for (int i = 0; i < system_data->num_hunters; i++) {
        if (strcmp(system_data->hunters[i].username, user) == 0) {
            system_data->hunters[i].level = 1;
            system_data->hunters[i].exp = 0;
            system_data->hunters[i].atk = 10;
            system_data->hunters[i].hp = 100;
            system_data->hunters[i].def = 5;

            // Update shared memory
            int shmid = shmget(system_data->hunters[i].shm_key, sizeof(struct Hunter), 0666);
            struct Hunter *shm_hunter = shmat(shmid, NULL, 0);
            if (shm_hunter != (void *)-1) {
                shm_hunter->level = 1;
                shm_hunter->exp = 0;
                shm_hunter->atk = 10;
                shm_hunter->hp = 100;
                shm_hunter->def = 5;
                shmdt(shm_hunter);
            }

            printf("%s telah direset.\n", user);
            return;
        }
    }
    printf("Hunter tidak ditemukan.\n");
}
```

K) Menambahkan function untuk fitur notifikasi yang berganti tiap 3 detik. Menampilkan real-time notifikasi dungeon kepada hunter seperti layaknya sistem game, dengan siklus dinamis dan penghentian yang intuitif.

```c
void show_single_notification(int index) {
    if (index >= system_data->num_dungeons || index < 0) return;

    struct Dungeon d = system_data->dungeons[index];
    printf("\n[NOTIF] Dungeon tersedia: %s (Level %d+)\n", d.name, d.min_level);
}

void run_notification_loop() {
    int index = 0;
    int stop = 0;

    printf("Menampilkan notifikasi dungeon...\n");

    while (!stop) {
        if (system_data->num_dungeons == 0) {
            printf("[NOTIF] Tidak ada dungeon tersedia.\n");
            sleep(3);
            continue;
        }

        show_single_notification(index);
        index = (index + 1) % system_data->num_dungeons;

        for (int i = 0; i < 3; i++) {
            sleep(1);
            if (is_enter_pressed()) {
                printf("Keluar dari notifikasi.\n");
                stop = 1;
                break;
            }
        }
    }
}
```

L) Menghapus shared memory setiap kali sistem dimatikan.

```c
void sigint_handler(int sig) {
    printf("\nSystem shutting down...\n");

<<<<<<< HEAD
    for (int i = 0; i < system_data->num_hunters; i++) {
        int shmid = shmget(system_data->hunters[i].shm_key, sizeof(struct Hunter), 0666);
        if (shmid != -1) shmctl(shmid, IPC_RMID, NULL);
    }
    for (int i = 0; i < system_data->num_dungeons; i++) {
        int shmid = shmget(system_data->dungeons[i].shm_key, sizeof(struct Dungeon), 0666);
        if (shmid != -1) shmctl(shmid, IPC_RMID, NULL);
    }

    shmdt(system_data);
    shmctl(shm_id, IPC_RMID, NULL);
    printf("Semua shared memory telah dihapus.\n");
    exit(0);
}
```

#### Output

Pada soal ini diminta untuk membantu Sung Jin Woo untuk melakukan modifikasi program.  
A) Membuat file system.c dan hunter.c dan memastikan bahwa hunter hanya bisa dijalankan apabila system sudah jalan.  
 ![](assets/soal_4/soal_4_a1.png)
![](assets/soal_4/soal_4_a2.png)

B) Membuat registrasi dan login menu serta hunter menu.
![](assets/soal_4/soal_4_b.png)

C) Membuat system menu yang lalu terdapat fitur untuk menampilkan informasi lengkap hunter.  
 ![](assets/soal_4/soal_4_c.png)

D) Membuat fitur generate dungeon.  
 ![](assets/soal_4/soal_4_d.png)

E) Menambahkan fitur yang dapat menampilkan daftar lengkap dungeon.  
 ![](assets/soal_4/soal_4_e.png)

F) Menambahkan fitur menampilkan dungeon sesuai level hunter pada menu hunter.  
 ![](assets/soal_4/soal_4_f.png)

G) Menambahkan fitur dungeon raid.  
 ![](assets/soal_4/soal_4_g.png)

H) Menambahkan fitur hunter battle.  
 ![](assets/soal_4/soal_4_h.png)

I) Menambahkan fitur ban hunter.  
 ![](assets/soal_4/soal_4_i.png)

J) Menambahkan fitur unban hunter.  
 ![](assets/soal_4/soal_4_j.png)

K) Menambahkan fitur notifikasi yang berganti tiap 3 detik.  
 ![](assets/soal_4/soal_4_k.png)

L) Menghapus shared memory setiap kali sistem dimatikan.
![](assets/soal_4/soal_4_l.png)

#### Kendala

Tidak ada kendala
