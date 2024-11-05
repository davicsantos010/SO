#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

#define MAX_PROCS 20
#define REFRESH_INTERVAL 1

typedef struct {
    int pid;
    char user[32];
    char name[256];
    char state;
} ProcessInfo;

ProcessInfo processes[MAX_PROCS];
int should_exit = 0;

// Função para obter o nome do usuário pelo UID
void get_username(uid_t uid, char *user) {
    struct passwd *pw = getpwuid(uid);
    if (pw) {
        strncpy(user, pw->pw_name, 31);
        user[31] = '\0';
    } else {
        snprintf(user, 32, "%d", uid);
    }
}

// Função para obter as informações do processo
void fetch_process_info() {
    // Limpa a lista de processos para atualizar com dados recentes
    memset(processes, 0, sizeof(processes));
    
    DIR *proc_dir = opendir("/proc");
    struct dirent *entry;
    int count = 0;

    while ((entry = readdir(proc_dir)) != NULL && count < MAX_PROCS) {
        if (entry->d_type == DT_DIR && isdigit(entry->d_name[0])) {
            int pid = atoi(entry->d_name);
            char path[256], comm[256], state;
            snprintf(path, sizeof(path), "/proc/%d/stat", pid);

            FILE *stat_file = fopen(path, "r");
            if (!stat_file) continue;

            // Obtém o nome e o estado do processo
            fscanf(stat_file, "%*d (%[^)]) %c", comm, &state);
            fclose(stat_file);

            // Obtém o UID do processo
            snprintf(path, sizeof(path), "/proc/%d", pid);
            struct stat info;
            if (stat(path, &info) == -1) continue;

            processes[count].pid = pid;
            get_username(info.st_uid, processes[count].user);
            strncpy(processes[count].name, comm, sizeof(processes[count].name) - 1);
            processes[count].name[sizeof(processes[count].name) - 1] = '\0';
            processes[count].state = state;

            count++;
        }
    }
    closedir(proc_dir);
}

// Função para exibir os processos em formato de tabela
void display_processes() {
    system("clear");
    printf("PID    | User            | PROCNAME          | Estado\n");
    printf("-------|-----------------|-------------------|--------\n");
    for (int i = 0; i < MAX_PROCS && processes[i].pid != 0; i++) {
        printf("%-7d| %-16s| %-18s| %c\n",
               processes[i].pid, processes[i].user, processes[i].name, processes[i].state);
    }
}

// Thread que atualiza a tabela de processos a cada 1 segundo
void *update_display(void *arg) {
    while (!should_exit) {
        fetch_process_info();
        display_processes();
        sleep(REFRESH_INTERVAL);
    }
    return NULL;
}

// Thread que gerencia a entrada do usuário para envio de sinais
void *handle_input(void *arg) {
    char input[256];
    int pid, signal;

    while (!should_exit) {
        fflush(stdout);
        if (fgets(input, sizeof(input), stdin) == NULL) continue;

        if (input[0] == 'q') {
            should_exit = 1;
            break;
        } else if (sscanf(input, "%d %d", &pid, &signal) == 2) {
            if (kill(pid, signal) == 0) {
                printf("Signal %d sent to PID %d.\n", signal, pid);
            } else {
                printf("Failed to send signal to PID %d: %s\n", pid, strerror(errno));
            }
        } else {
            printf("Invalid input. Please enter '<PID> <SIGNAL>' or 'q' to quit.\n");
        }
    }
    return NULL;
}

int main() {
    pthread_t display_thread, input_thread;

    // Cria as threads para atualizar a tabela e capturar a entrada do usuário
    pthread_create(&display_thread, NULL, update_display, NULL);
    pthread_create(&input_thread, NULL, handle_input, NULL);

    // Aguarda até que ambas as threads terminem
    pthread_join(display_thread, NULL);
    pthread_join(input_thread, NULL);

    printf("Exiting meutop.\n");
    return 0;
}
