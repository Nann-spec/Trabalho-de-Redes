#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include "DataStructures/KDTree/kdTree.h"
#include "RDT/rdt2.h"

#define BUFFER_SIZE 1024
#define FILE_NAME "Data/database.txt"

#define RESET "\033[0m"
#define GREEN "\033[32m"
#define CYAN "\033[36m"
#define YELLOW "\033[33m"
#define WHITE "\033[37m"

/* Raiz da arvore */
Node* root = NULL;

/* Semáforos e variáveis globais */
sem_t mutex_readcount;
sem_t queue_mutex;
sem_t rw_mutex;

int read_count = 0;

/* Estrutura para passar dados a thread */
typedef struct {
    int sockfd;
    struct sockaddr_in cliente_addr;
    socklen_t cliente_len;
    char message[BUFFER_SIZE];
} ClienteArgs;

void* periodicSavior(void* arg);

void* reader(void* arg);

void* writer(void* arg);

int main(int argc, char* argv[]) {
    if (argc != 2) {    /* Quantidade invalida de argumentos */
        perror("Uso: ./servidor <porta>");
        exit(1);
    }

    root = loadTree(FILE_NAME);

    int porta = atoi(argv[1]);      /* Porta do servidor*/
    int sockfd;                     /* Socket */
    struct sockaddr_in servidor_addr, cliente_addr; /* Estrutura do servidor e cliente */
    socklen_t cliente_len;          /* Tamanho do cliente */
    char buffer[BUFFER_SIZE];       /* Buffer */

    /* Cria socket UDP */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Erro ao criar socket");
        exit(1);
    }

    /* Preenche estrutura do servidor */
    memset(&servidor_addr, 0, sizeof(servidor_addr));
    servidor_addr.sin_family = AF_INET;
    servidor_addr.sin_addr.s_addr = INADDR_ANY;
    servidor_addr.sin_port = htons(porta);

    /* Associa socket a porta */
    if (bind(sockfd, (struct sockaddr*)&servidor_addr, sizeof(servidor_addr)) < 0) {
        perror("Erro no bind");
        close(sockfd);
        exit(1);
    }

    printf("Servidor UDP/RDT 2.0 escutando na porta %d...\n", porta);

    sem_init(&mutex_readcount, 0, 1); /* Mutex para controlar read_count */
    sem_init(&queue_mutex, 0, 1);     /* Inicializa com 1 */
    sem_init(&rw_mutex, 0, 1);        /* Semáforo para controle de leitura/escrita */

    pthread_t saveThread;
    pthread_create(&saveThread, NULL, periodicSavior, NULL);

    /* Loop principal */
    while (1) {
        cliente_len = sizeof(cliente_addr);
        memset(buffer, 0, BUFFER_SIZE);

        /* Recebe mensagem do cliente */
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0,
                         (struct sockaddr*)&cliente_addr, &cliente_len);
        if (n < 0) {
            perror("Erro ao receber dados");
            continue;
        }
        buffer[n] = '\0'; /* garante fim da string */
        printf("%s[SERVIDOR] Dados recebidos: %s%s\n", WHITE, buffer, RESET);

        /* RDT 2.0 */
        char* dataPart = strchr(buffer, ':');
        if (!dataPart) {
            /* Pacote mal formatado (provavelmente corrompido) */
            printf("%s[RDT] Pacote corrompido recebido (sem checksum). Enviando NAK%s\n", CYAN, RESET);
            sendto(sockfd, RDT_NAK, strlen(RDT_NAK), 0, (struct sockaddr*)&cliente_addr, cliente_len);
            continue;
        }

        *dataPart = '\0';   /* Separa o checksum */
        uint8_t receivedChecksum = (uint8_t)atoi(buffer);
        dataPart++;         /* Aponta para os dados */

        /* Verifica integridade do pacote */
        uint8_t calculatedChecksum = calculateChecksum(dataPart, strlen(dataPart));
        if (calculatedChecksum != receivedChecksum) {
            /* Pacote corrompido. Envia NAK e descarta */
            printf("%s[RDT] Pacote corrompido!\nChecksum Recebido: %d, Calculado: %d. Envia NAK.\n%s", CYAN, receivedChecksum, calculatedChecksum, RESET);
            sendto(sockfd, RDT_NAK, strlen(RDT_NAK), 0, (struct sockaddr*)&cliente_addr, cliente_len);
            continue;
        }

        /* Pacote nao corrompido */
        printf("%s[RDT] Pacote recebido e validado. Enviando ACK e processando...\n%s", CYAN, RESET);

        /* Envia ACK para confirmar recepcao dos dados */
        sendto(sockfd, RDT_ACK, strlen(RDT_ACK), 0,
           (struct sockaddr*)&cliente_addr, cliente_len);

        /* Cria estrutura para thread */
        ClienteArgs* args = malloc(sizeof(ClienteArgs));
        if (!args) {
            perror("Erro ao alocar memória");
            continue;
        }

        /* Armazena as informacoes do cliente */
        args->sockfd = sockfd;
        args->cliente_addr = cliente_addr;
        args->cliente_len = cliente_len;
        strncpy(args->message, dataPart, BUFFER_SIZE);

        /* ID para as threads */
        pthread_t tid;

        /* Verifica se e escrita ou leitura */
        if (dataPart[0] == 'D' || dataPart[0] == 'd') { /* Dados (Escrita) */
            /* Cria thread para processar a mensagem */
            if (pthread_create(&tid, NULL, writer, args) != 0) {
                perror("Erro ao criar thread");
                free(args);
            } else {
                pthread_detach(tid); /* libera recursos da thread apos termino */
            }
        } else {
            if(dataPart[0] == 'P' || dataPart[0] == 'p') {  /* Pesquisa (Leitura) */
                /* Cria thread para processar a mensagem */
                if (pthread_create(&tid, NULL, reader, args) != 0) {
                    perror("Erro ao criar thread");
                    free(args);
                } else {
                    pthread_detach(tid); /* libera recursos da thread apos termino */
                }
            } else {    /* Tipo invalido */
                char* errorMessage = "[SERVIDOR] Tipo de mensagem invalido.";
                sendto(sockfd, errorMessage, strlen(errorMessage), 0,
                       (struct sockaddr*)&cliente_addr, cliente_len);
                printf("%s[SERVIDOR] Tipo de mensagem invalido.\n%s", YELLOW, RESET);
                //free(args);
            }
            pthread_detach(tid);
        }
    }
    close(sockfd);
    return 0;
}

/* Procedimento que salva os dados a cada 5 segundos */
void* periodicSavior(void* arg) {
    while (1) {
        /* Intervalo de tempo entre os salvamentos */
        sleep(5);

        printf("%s[SAVIOR] Iniciando salvamento periodico...\n%s", GREEN, RESET);

        /* Adquirir o lock de leitura */
        sem_wait(&mutex_readcount);
        read_count++;
        if (read_count == 1) { /* Primeiro leitor a entrar */
            sem_wait(&rw_mutex); /* Bloqueia escritores */
        }
        sem_post(&mutex_readcount);

        if (root) {
            saveTree(root, FILE_NAME); /* Salva a arvore */
            printf("%s[SAVIOR] Arvore de dados salva com sucesso.\n%s", GREEN, RESET);
        } else {
            printf("%s[SAVIOR] Arvore vazia, nada para salvar.\n%s", GREEN, RESET);
        }

        /* Sair do lock de leitura */
        sem_wait(&mutex_readcount);
        read_count--;
        if (read_count == 0) { /* Ultimo leitor a sair */
            sem_post(&rw_mutex); /* Libera escritores */
        }
        sem_post(&mutex_readcount);
    }
}

/* Funcao de escrita */
void* writer(void* arg) {
    ClienteArgs args = *(ClienteArgs*)arg;
    free(arg);

    /* Obtencao das informacoes */
    char* token = strtok(args.message, " ");   /* Recebe o primeiro token */
    int count = 1;                              /* Contador para a quantidade de dados */

    /* Infos */
    int idFuel, price;  /* Identificador do cliente e do combustivel e raio de busca */
    double lat, lon;        /* Latitude e Longitude do centro */

    while (token != NULL && count <= 6) {
        switch (count) {
            case 2: /* Identificador do combustivel */
                idFuel = atoi(token);
                break;
            case 3: /* Preco (inteiro) */
                price = atoi(token);
                break;
            case 4: /* Latitude */
                lat = atof(token);
                break;
            case 5: /* Longitude */
                lon = atof(token);
                break;
        }
        count++;
        token = strtok(NULL, " ");         
    }

    if (idFuel < 0 || idFuel > 2) { /* Validacao dos dados enviados */
        /* Resposta para o cliente */
        char* messageReply = "[SERVIDOR] ID para tipo de combustivel invalido.";
        sendto(args.sockfd, messageReply, strlen(messageReply), 0,
            (struct sockaddr*)&args.cliente_addr, args.cliente_len);
        printf("%s[WRITER] Dados nao inseridos.\n%s", YELLOW, RESET);
    } else {    /* idFuel valido */

        /* Entrar na fila (garante ordem de chegada) */
        sem_wait(&queue_mutex);

        /* Adquirir o lock de escrita (espera leitores terminarem) */
        sem_wait(&rw_mutex);
        /* Escritor agora tem acesso exclusivo. */
        sem_post(&queue_mutex); /* Liberar a fila para o proximo da linha */

        /* Inicio escrita */
        root = insertTree(root, lat, lon, idFuel, price);
        /* Fim escrita */

        /* Liberar o lock de escrita */
        sem_post(&rw_mutex);

        /* Resposta para o cliente */
        char* messageReply = "[SERVIDOR] Dados inseridos.";
        sendto(args.sockfd, messageReply, strlen(messageReply), 0,
            (struct sockaddr*)&args.cliente_addr, args.cliente_len);
        printf("%s[WRITER] Dados inseridos.\n%s", YELLOW, RESET);
    }

    return NULL;
}

/* Funcao de leitura */
void* reader(void* arg) {
    ClienteArgs args = *(ClienteArgs*)arg;
    free(arg);

    /* Obtencao das informacoes */
    char* token = strtok(args.message, " ");   /* Recebe o primeiro token */
    int count = 1;                              /* Contador para a quantidade de dados */

    /* Infos */
    int idFuel, range;  /* Identificador do cliente e do combustivel e raio de busca */
    double lat, lon;        /* Latitude e Longitude do centro */

    while (token != NULL && count <= 5) {
        switch (count) {
            case 2: /* Identificador do combustivel */
                idFuel = atoi(token);
                break;
            case 3: /* Raio (inteiro) */
                range = atoi(token);
                break;
            case 4: /* Latitude */
                lat = atof(token);
                break;
            case 5: /* Longitude */
                lon = atof(token);
                break;
        }
        count++;
        token = strtok(NULL, " ");         
    }

    if (idFuel < 0 || idFuel > 2) { /* Validacao dos dados enviados */
        /* Resposta para o cliente */
        char* messageReply = "[SERVIDOR] ID para tipo de combustivel invalido.";
        sendto(args.sockfd, messageReply, strlen(messageReply), 0,
            (struct sockaddr*)&args.cliente_addr, args.cliente_len);
        printf("%s[READER] Busca nao realizada.\n%s", YELLOW, RESET);
    } else {
        /* Entrar na fila (garante ordem de chegada) */
        sem_wait(&queue_mutex);

        /* Adquirir o lock de leitura */
        sem_wait(&mutex_readcount);
        read_count++;
        if (read_count == 1) { /* Primeiro leitor a entrar */
            sem_wait(&rw_mutex); /* Bloqueia escritores */
        }
        sem_post(&mutex_readcount);
        sem_post(&queue_mutex); /* Liberar a fila para o proximo da linha */

        /* Leitura */
        Node* lowestPrice = findBestStationInRange(root, lat, lon, range, idFuel);
        /* Fim leitura */

        /* Sair do lock de leitura */
        sem_wait(&mutex_readcount);
        read_count--;
        if (read_count == 0) { /* Ultimo leitor a sair */
            sem_post(&rw_mutex); /* Libera escritores */
        }
        sem_post(&mutex_readcount);

        /* Resposta */
        char messageReply[BUFFER_SIZE]; /* Mensagem de resposta */
        if (lowestPrice) {
            if (lowestPrice->fuelPrices[idFuel] != 0) { /* Encontrou o menor preco dentro do raio */
                double price = (double)lowestPrice->fuelPrices[idFuel] / 1000;
                snprintf(messageReply, BUFFER_SIZE, "[SERVIDOR] Melhor preco encontrado: %.3f", price);   /* Converte o valor do combustivel em str */
            } else {    /* Nenhum posto encontrado com valor para o combustivel dentro do raio */
                snprintf(messageReply, BUFFER_SIZE, "[SERVIDOR] Nenhum posto encontrado dentro do raio");
            }
        } else {
            snprintf(messageReply, BUFFER_SIZE, "[SERVIDOR] Nenhum posto encontrado dentro do raio");
        }
        sendto(args.sockfd, messageReply, strlen(messageReply), 0,
            (struct sockaddr*)&args.cliente_addr, args.cliente_len);
    }

    return NULL;
}