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

#define MAX_PROCS 20 //define o numero maximo de processos exibidos
#define REFRESH_INTERVAL 1 //define o intervalo de atualizacao em segundos

typedef struct {
    int pid;
    char user[32];
    char name[256];
    char state;
} ProcessInfo;

ProcessInfo processes[MAX_PROCS];
int should_exit = 0; //indica quando o programa deve encerrar

//funcao para obter o nome do usuario pelo UID
void get_username(uid_t uid, char *user) {
    struct passwd *pw = getpwuid(uid);
    if (pw) {
        strncpy(user, pw->pw_name, 31);
        user[31] = '\0';
    } else {
        snprintf(user, 32, "%d", uid); //usa o UID como nome caso nao encontre o usuario
    }
}

//funcao para obter as informacoes do processo
void fetch_process_info() {
    //limpa a lista de processos para atualizar com dados recentes
    memset(processes, 0, sizeof(processes));
    
    DIR *proc_dir = opendir("/proc"); //abre o diretorio /proc
    struct dirent *entry;
    int count = 0;

    while ((entry = readdir(proc_dir)) != NULL && count < MAX_PROCS) {
        if (entry->d_type == DT_DIR && isdigit(entry->d_name[0])) {
            int pid = atoi(entry->d_name);
            char path[256], comm[256], state;
            snprintf(path, sizeof(path), "/proc/%d/stat", pid); //cria caminho para o arquivo stat

            FILE *stat_file = fopen(path, "r"); //abre o arquivo stat do processo
            if (!stat_file) continue;

            //obtem o nome e o estado do processo
            fscanf(stat_file, "%*d (%[^)]) %c", comm, &state);
            fclose(stat_file);

            //obtem o UID do processo
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

//funcao para exibir os processos em formato de tabela
void display_processes() {
    system("clear"); //limpa a tela para atualizar a exibicao
    printf("PID    | User            | PROCNAME          | Estado\n");
    printf("-------|-----------------|-------------------|--------\n");
    for (int i = 0; i < MAX_PROCS && processes[i].pid != 0; i++) {
        printf("%-7d| %-16s| %-18s| %c\n",
               processes[i].pid, processes[i].user, processes[i].name, processes[i].state);
    }
}

//thread que atualiza a tabela de processos a cada 1 segundo
void *update_display(void *arg) {
    while (!should_exit) {
        fetch_process_info();
        display_processes();
        sleep(REFRESH_INTERVAL); //espera 1 segundo antes da proxima atualizacao
    }
    return NULL;
}

//thread que gerencia a entrada do usuario para envio de sinais
void *handle_input(void *arg) {
    char input[256];
    int pid, signal;

    while (!should_exit) {
        fflush(stdout);
        if (fgets(input, sizeof(input), stdin) == NULL) continue;

        if (input[0] == 'q') { //sai do programa se o usuario digitar 'q'
            should_exit = 1;
            break;
        } else if (sscanf(input, "%d %d", &pid, &signal) == 2) {
            if (kill(pid, signal) == 0) {
                printf("Sinal %d enviado ao PID %d.\n", signal, pid);
            } else {
                printf("Falha ao enviar sinal ao PID %d: %s\n", pid, strerror(errno));
            }
        } else {
            printf("Entrada invalida. Digite '<PID> <SIGNAL>' ou 'q' para sair.\n");
        }
    }
    return NULL;
}

int main() {
    pthread_t display_thread, input_thread;

    //cria as threads para atualizar a tabela e capturar a entrada do usuario
    pthread_create(&display_thread, NULL, update_display, NULL);
    pthread_create(&input_thread, NULL, handle_input, NULL);

    //aguarda ate que ambas as threads terminem
    pthread_join(display_thread, NULL);
    pthread_join(input_thread, NULL);

    return 0;
}
