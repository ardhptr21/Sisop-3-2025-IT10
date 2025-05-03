#include "shm_common.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define BOLD    "\033[1m"

struct Hunter *this_hunter;
struct SystemData *system_data;
int sys_id, hunter_id;
char username[50];

// Fungsi untuk membersihkan buffer input
void clear_input_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// Fungsi untuk mengecek apakah tombol Enter ditekan
int is_enter_pressed() {
    struct timeval tv;
    fd_set fds;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
    return FD_ISSET(STDIN_FILENO, &fds);
}

// Fungsi untuk menampilkan satu dungeon
void show_single_notification(int index) {
    if (index >= system_data->num_dungeons || index < 0) return;

    struct Dungeon d = system_data->dungeons[index];
    printf("\n[NOTIF] Dungeon tersedia: %s (Level %d+)\n", d.name, d.min_level);
}

// Fungsi untuk menjalankan notifikasi secara bergantian setiap 3 detik
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

        // Tunggu 3 detik atau sampai Enter ditekan
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

// Fungsi print_menu tanpa notifikasi otomatis
void print_menu() {
    printf("\n" BOLD CYAN "=== '%s' MENU ===\n" RESET, username);
    printf(" " GREEN "1. Dungeon List\n");
    printf(" " GREEN "2. Dungeon Raid\n");
    printf(" " GREEN "3. Hunter Battle\n");
    printf(" " GREEN "4. Notification\n");
    printf(" " GREEN "5. Exit\n" RESET);
    printf(" Choice: ");
}

void dungeon_list() {
    printf("=== AVAILABLE DUNGEONS ===\n");

    int count = 0; // untuk cek apakah ada dungeon yang memenuhi level

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

        // Pastikan indeks notifikasi tetap valid
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

void hunter_battle() {
    char enemy[50];
    printf("Enter enemy hunter username: ");
    scanf("%s", enemy);
    clear_input_buffer();

    // Cari indeks hunter lawan
    int enemy_idx = -1;
    for (int i = 0; i < system_data->num_hunters; i++) {
        if (strcmp(system_data->hunters[i].username, enemy) == 0) {
            enemy_idx = i;
            break;
        }
    }

    if (enemy_idx == -1) {
        printf("Hunter not found.\n");
        return;
    }

    struct Hunter *target = &system_data->hunters[enemy_idx];

    if (target->banned) {
        printf("Target hunter is banned.\n");
        return;
    }

    // Ambil shared memory target dengan validasi
    int target_shmid = shmget(target->shm_key, 0, 0666);
    if (target_shmid == -1) {
        perror("shmget");
        printf("Target hunter memory not found. Maybe it has been deleted.\n");
        return;
    }

    struct shmid_ds shm_info;
    if (shmctl(target_shmid, IPC_STAT, &shm_info) == -1) {
        perror("shmctl");
        return;
    }

    if (shm_info.shm_segsz != sizeof(struct Hunter)) {
        fprintf(stderr, "Shared memory size mismatch.\n");
        return;
    }

    struct Hunter *target_shm = shmat(target_shmid, NULL, 0);
    if (target_shm == (void *)-1) {
        perror("shmat");
        return;
    }

    // Hitung power
    int my_power = this_hunter->atk + this_hunter->hp + this_hunter->def;
    int enemy_power = target_shm->atk + target_shm->hp + target_shm->def;

    if (my_power >= enemy_power) {
        printf("You won the battle!\n");

        // Tambah stats & exp
        this_hunter->atk += target_shm->atk;
        this_hunter->hp += target_shm->hp;
        this_hunter->def += target_shm->def;
        this_hunter->exp += target_shm->exp;

        if (this_hunter->exp >= 500) {
            this_hunter->level++;
            this_hunter->exp = 0;
        }

        // Update hunter di system_data
        for (int i = 0; i < system_data->num_hunters; i++) {
            if (strcmp(system_data->hunters[i].username, this_hunter->username) == 0) {
                memcpy(&system_data->hunters[i], this_hunter, sizeof(struct Hunter));
                break;
            }
        }

        // Hapus shared memory lawan
        shmdt(target_shm);
        shmctl(target_shmid, IPC_RMID, NULL);

        // Hapus lawan dari array
        for (int i = enemy_idx; i < system_data->num_hunters - 1; i++) {
            system_data->hunters[i] = system_data->hunters[i + 1];
        }
        system_data->num_hunters--;

    } else {
        printf("You lost the battle.\n");

        // Tambah stats & exp ke lawan
        target_shm->atk += this_hunter->atk;
        target_shm->hp += this_hunter->hp;
        target_shm->def += this_hunter->def;
        target_shm->exp += this_hunter->exp;

        if (target_shm->exp >= 500) {
            target_shm->level++;
            target_shm->exp = 0;
        }

        // Update lawan di system_data
        for (int i = 0; i < system_data->num_hunters; i++) {
            if (strcmp(system_data->hunters[i].username, target_shm->username) == 0) {
                memcpy(&system_data->hunters[i], target_shm, sizeof(struct Hunter));
                break;
            }
        }

        shmdt(target_shm);

        // Hapus shared memory hunter sendiri
        shmctl(hunter_id, IPC_RMID, NULL);

        // Hapus hunter sendiri dari system_data
        for (int i = 0; i < system_data->num_hunters; i++) {
            if (strcmp(system_data->hunters[i].username, this_hunter->username) == 0) {
                for (int j = i; j < system_data->num_hunters - 1; j++) {
                    system_data->hunters[j] = system_data->hunters[j + 1];
                }
                system_data->num_hunters--;
                break;
            }
        }

        shmdt(this_hunter);
        exit(0); // Keluar setelah kalah
    }
}

void sigint_handler(int sig) {
    printf("\nExiting hunter process...\n");

    // Detach shared memory yang digunakan hunter
    if (this_hunter != (void *)-1) {
        shmdt(this_hunter);
    }

    if (system_data != (void *)-1) {
        shmdt(system_data);
    }

    // Tidak menghapus shared memory (IPC_RMID) karena hanya system.c yang boleh
    exit(0);
}

int main() {
    signal(SIGINT, sigint_handler);
    key_t sys_key = get_system_key();
    sys_id = shmget(sys_key, sizeof(struct SystemData), 0666);
    if (sys_id < 0) {
        printf("System belum aktif.\n");
        return 1;
    }

    system_data = shmat(sys_id, NULL, 0);

    int choice;
    while (1) {
        printf("\n=== HUNTER MENU ===\n");    ///Soal B
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

                // Perbaikan: Gunakan indeks unik untuk shm_key
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
            // Perbaikan: Jangan hapus shared memory saat exit dari menu awal
            shmdt(system_data);
            printf("Exiting without deleting shared memory.\n");
            exit(0);
        } else {
            printf(BOLD RED"Invalid option.\n"RESET);
        }
    }

    printf("\n=== HUNTER SYSTEM ===\n");

    int cmd;
    while (1) {
        print_menu();
        printf("Choice: ");
        if (scanf("%d", &cmd) != 1) {
            printf(BOLD RED"Invalid option.\n"RESET);
            clear_input_buffer();
            continue;
        }
        clear_input_buffer();

        if (cmd == 1) dungeon_list();              
        else if (cmd == 2) dungeon_raid();         
        else if (cmd == 3) hunter_battle();        
        else if (cmd == 4) run_notification_loop(); 
        else if (cmd == 5) break;
        else printf(BOLD RED"Invalid option.\n"RESET);
    }

    shmdt(this_hunter);
    shmdt(system_data);
    return 0;
}