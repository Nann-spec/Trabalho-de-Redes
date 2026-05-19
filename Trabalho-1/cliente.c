#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include "RDT/rdt2.h"

#define BUFFER_SIZE 1024
#define CORRUPT_PROB 0.5
#define RESET "\033[0m"
#define GREEN "\033[32m"
#define RED "\033[31m"
#define CYAN "\033[36m"

/* Validar formato da mensagem */
int validarFormato(const char *mensagem) {
    char c;
    int i1, i2;
    double d1, d2;
    char extra;

    /* Tenta ler exatamente no formato desejado */
    int qtd = sscanf(mensagem, " %c %d %d %lf %lf %c", &c, &i1, &i2, &d1, &d2, &extra);

    /* Se conseguiu ler exatamente 5 itens e nao ha nada extra */
    return (qtd == 5);
}

void rdtSend(int sockfd, const char* message, const struct sockaddr* addr, socklen_t addr_len) {
    /* Laco */
    while (1) {
        char buffer_send[BUFFER_SIZE], buffer_recv[BUFFER_SIZE];        /* Buffers */
        uint8_t checksum = calculateChecksum(message, strlen(message)); /* Obtem o checksum */

        snprintf(buffer_send, BUFFER_SIZE, "%d:%s", checksum, message); /* Empacota a mensagem junto com a checksum */

        double prob = (double)(rand() % 100) / 100;
        if (prob < CORRUPT_PROB) {    /* Probabilidade do pacote sair corrompido */
            buffer_send[strlen(buffer_send) - 1] = 'X'; /* Substitui um valor por X (simula erro de bit) */
        }

        printf("%s[RDT_SEND] Enviando pacote...\n%s", CYAN, RESET);
        sendto(sockfd, buffer_send, strlen(buffer_send), 0, addr, addr_len);

        memset(buffer_recv, 0, BUFFER_SIZE);
        int n = recvfrom(sockfd, buffer_recv, BUFFER_SIZE - 1, 0, NULL, NULL);
        if (n > 0) {
            buffer_recv[n] = '\0';
            if (strncmp(buffer_recv, RDT_ACK, strlen(RDT_ACK)) == 0) {  /* Verifica se recebeu um ACK */
                printf("%s[SUCESSO] Operacao confirmada pelo servidor.%s\n", GREEN, RESET);
                char data[BUFFER_SIZE];
                n = recvfrom(sockfd, data, BUFFER_SIZE - 1, 0, NULL, NULL); /* Aguarda os dados seguintes que o servidor ira mandar */
                data[n] = '\0';
                if (n > 0) {
                    printf("  |--> Resposta: %s\n\n", data);    /* Imprime o conteudo que o servidor encaminhou */
                    break;
                } else {
                    perror("Erro fatal no recvfrom");
                }
            } else {    /* Recebeu algo que nao e um ACK */
                 printf("%s[ERRO] NAK ou resposta corrompida. Retransmitindo...%s\n", RED, RESET);
            }
        } else {
            perror("Erro fatal no recvfrom");
            break;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {    /* Quantidade invalida de argumentos */
        perror("Uso: ./servidor <ip_servidor> <porta>");
        exit(1);
    }

    srand(time(NULL));

    const char* ipServer = argv[1]; /* IP do servidor */
    int porta = atoi(argv[2]);      /* Porta do servidor */
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);    /* Socket */
    struct sockaddr_in servidor_addr;   /* Estrutura para servidor */
    char line[BUFFER_SIZE];             /* Buffer de linha */

    /* Preenche a estrutura do cliente */
    memset(&servidor_addr, 0, sizeof(servidor_addr));
    servidor_addr.sin_family = AF_INET;
    servidor_addr.sin_port = htons(porta);
    inet_pton(AF_INET, ipServer, &servidor_addr.sin_addr);

    printf("Cliente RDT 2.0.\n");
    printf("Digite 'exit' para sair.\n\n");

    while (1) {
        printf("> ");
        if (fgets(line, sizeof(line), stdin) == NULL)
            break;

        line[strcspn(line, "\n")] = 0;  /* Retira a quebra de linha */

        if (strcmp(line, "exit") == 0)
            break;

        if(strlen(line) > 0) {    /* Ha mensagem para enviar */
            /* Verifica se mensagem tem formato valido */
            if (validarFormato(line)) {
                rdtSend(sockfd, line, (struct sockaddr*)&servidor_addr, sizeof(servidor_addr));
            } else {
                printf("%s[ERRO] Formato inválido. Use: <char> <int> <int> <double> <double>%s\n", RED, RESET);
            }
        }
    }
    close(sockfd);
    return 0;
}