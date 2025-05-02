#include "shm_common.h"
#include <pthread.h>
#include <unistd.h>

struct Hunter *this_hunter;
struct SystemData *system_data;
int sys_id, hunter_id;
char username[50];

// Notifikasi dungeon setiap 3 detik (Soal K)
void *notifikasi_dungeon(void *arg) {
    while (1) {
        if (system_data->num_dungeons == 0) {
            sleep(3);
            continue;
        }
        int idx = system_data->current_notification_index++ % system_data->num_dungeons;
        struct Dungeon d = system_data->dungeons[idx];
        if (this_hunter->level >= d.min_level) {
            printf("[NOTIF] Dungeon tersedia: %s (Level %d+)\n", d.name, d.min_level);
        }
        sleep(3);
    }
}

void print_menu() {
    printf("\n=== '%s' MENU ===\n", username);
    printf("1. Dungeon List\n");
    printf("2. Dungeon Raid\n");
    printf("3. Hunter Battle\n");
    printf("4. Notification\n");
    printf("5. Exit\n");
    printf("Choice: ");
}

void dungeon_list() {
    printf("=== AVAILABLE DUNGEONS ===\n");
    int found = 0;
    for (int i = 0; i < system_data->num_dungeons; i++) {
        if (this_hunter->level >= system_data->dungeons[i].min_level) {
            printf("%d. %s (Level %d+)\n", i + 1, system_data->dungeons[i].name, system_data->dungeons[i].min_level);
            found = 1;
        }
    }
    if (!found) {
        printf("No available dungeons for your level.\n");
    }
}

void dungeon_raid() {
    dungeon_list();
    printf("Choose Dungeon: ");
    int choice;
    scanf("%d", &choice);
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

    int my_total = this_hunter->atk + this_hunter->hp + this_hunter->def;
    int enemy_total = target->atk + target->hp + target->def;

    if (my_total >= enemy_total) {
        printf("You won the battle!\n");
        this_hunter->atk += target->atk;
        this_hunter->hp += target->hp;
        this_hunter->def += target->def;

        shmctl(shmget(target->shm_key, sizeof(struct Hunter), 0666), IPC_RMID, NULL);
        for (int i = enemy_idx; i < system_data->num_hunters - 1; i++) {
            system_data->hunters[i] = system_data->hunters[i + 1];
        }
        system_data->num_hunters--;
    } else {
        printf("You lost the battle.\n");
    }
}

int main() {
    key_t sys_key = get_system_key();
    sys_id = shmget(sys_key, sizeof(struct SystemData), 0666);
    if (sys_id < 0) {
        printf("System belum aktif.\n");
        return 1;
    }

    system_data = shmat(sys_id, NULL, 0);

    int choice;
    while (1) {
        // Show main menu
        printf("\n=== HUNTER MENU ===\n");
        printf("1. Register\n");
        printf("2. Login\n");
        printf("3. Exit\n");
        printf("Choice: ");
        scanf("%d", &choice);

        // Clear the input buffer
        while (getchar() != '\n');

        if (choice == 1) {
            // Register new hunter
            printf("Masukkan username: ");
            scanf("%s", username);
            while (getchar() != '\n'); // Clear the buffer

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

                key_t hunter_key = ftok("/tmp", username[0]);
                system_data->hunters[idx].shm_key = hunter_key;
                system_data->num_hunters++;

                hunter_id = shmget(hunter_key, sizeof(struct Hunter), IPC_CREAT | 0666);
                this_hunter = shmat(hunter_id, NULL, 0);
                memcpy(this_hunter, &system_data->hunters[idx], sizeof(struct Hunter));
                printf("Registrasi sukses!\n");

                break;  // Exit the loop to proceed with the system menu
            } else {
                printf("Username sudah terdaftar.\n");
            }
        } else if (choice == 2) {
            // Login existing hunter
            printf("Masukkan username: ");
            scanf("%s", username);
            while (getchar() != '\n'); // Clear the buffer

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

                break;  // Exit the loop to proceed with the system menu
            }
        } else if (choice == 3) {
            // Exit the program
            printf("Exiting...\n");
            shmdt(system_data);
            return 0;
        } else {
            printf("Invalid choice. Please try again.\n");
        }
    }

    printf("\n=== HUNTER SYSTEM ===\n");

    pthread_t tid;
    pthread_create(&tid, NULL, notifikasi_dungeon, NULL);

    int cmd;
    while (1) {
        print_menu();
        scanf("%d", &cmd);
        if (cmd == 1) dungeon_list();              // SOAL E
        else if (cmd == 2) dungeon_raid();         // SOAL F
        else if (cmd == 3) hunter_battle();        // SOAL G
        else if (cmd == 5) break;
    }

    shmdt(this_hunter);
    shmdt(system_data);
    return 0;
}