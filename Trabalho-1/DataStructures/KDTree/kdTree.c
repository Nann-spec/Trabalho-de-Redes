#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "kdTree.h"

/**
 * @brief Cria um novo no para ser inserido na arvore.
 * @param lat_ Latitude a ser armazenada.
 * @param lon_ Longitude a ser armazenada.
 * @param idFuel Indice do combustivel (0-Diesel, 1-Etanol, 2-Gasolina).
 * @param price Preco do combustivel.
 * @return Retorna o novo no.
 */ 
Node* createNewNode(double lat_, double lon_, int idFuel, int price) {
    Node* temp = malloc(sizeof(Node));
    temp->coordinates[0] = lat_;
    temp->coordinates[1] = lon_;
    
    // Inicializa todos os precos como 0 (sem valor)
    temp->fuelPrices[0] = 0;
    temp->fuelPrices[1] = 0;
    temp->fuelPrices[2] = 0;

    // Atribui o preco para o combustivel especifico
    if (idFuel >= 0 && idFuel < 3) {
        temp->fuelPrices[idFuel] = price;
    }
    
    temp->left = NULL;
    temp->right = NULL; 
    return temp;
}

/**
 * @brief Insercao recursiva de um novo no ou atualizacao de um no existente.
 * @param root No atual da arvore.
 * @param lat_ Latitude a ser inserida/atualizada.
 * @param lon_ Longitude a ser inserida/atualizada.
 * @param idFuel Indice do combustivel.
 * @param price Preco a ser inserido/atualizado.
 * @param depth Profundidade utilizada para auxilio do particionamento.
 * @returns No atual ou No a ser inserido.
 */
Node* insertTreeRec(Node* root, double lat_, double lon_, int idFuel, int price, int depth) {
    if (!root) { /* Chegou no local a ser inserido, cria um novo no */
        return createNewNode(lat_, lon_, idFuel, price);
    }
    
    /* Se o no com as coordenadas ja existe, apenas atualiza o preco */
    if (root->coordinates[0] == lat_ && root->coordinates[1] == lon_) {
        if (idFuel >= 0 && idFuel < 3) {
            root->fuelPrices[idFuel] = price;
        }
        return root; /* Retorna o no existente e atualizado */
    }

    int cd = depth % 2; /* Dimensao atual */
    if ((cd == 0 && lat_ < root->coordinates[0]) || (cd == 1 && lon_ < root->coordinates[1])) {
        root->left = insertTreeRec(root->left, lat_, lon_, idFuel, price, depth + 1); /* Insercao a esquerda */
    } else {
        root->right = insertTreeRec(root->right, lat_, lon_, idFuel, price, depth + 1); /* Insercao a direita */
    }
    
    return root; /* Retorna o no atual */
}

/**
 * @brief Interface para insercao de um no na arvore.
 * @param root Raiz da arvore.
 * @param lat_ Latitude a ser inserida.
 * @param lon_ Longitude a ser inserida.
 * @param idFuel Indice do combustivel.
 * @param price Preco a ser inserido.
 * @return Raiz para a nova arvore.
 */
Node* insertTree(Node* root, double lat_, double lon_, int idFuel, int price) {
    return insertTreeRec(root, lat_, lon_, idFuel, price, 0);
}

/**
 * @brief Calculo de distancia pela formula inversa de haversine
 * @param lat1 Latitude 1
 * @param lon1  Longitude 1
 * @param lat2 Latitude 2
 * @param lon2 Longitude 2
 * @return A distancia entre os dois pontos na esfera
 */
double haversine(double lat1, double lon1, double lat2, double lon2) {
    const double DEG_TO_RAD = M_PI / 180.0;

    lat1 *= DEG_TO_RAD;
    lon1 *= DEG_TO_RAD;
    lat2 *= DEG_TO_RAD;
    lon2 *= DEG_TO_RAD;

    double dlat = lat2 - lat1;
    double dlon = lon2 - lon1;

    double a = pow(sin(dlat / 2.0), 2.0) +
               cos(lat1) * cos(lat2) * pow(sin(dlon / 2.0), 2.0);

    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    return EARTH_RADIUS * c;
}

/**
 * @brief Busca recursiva de um no na arvore
 * @param root No atual da arvore
 * @param lat_ Latitude a ser inserida na arvore
 * @param lon_ Longitude a ser inserida na arvore
 * @param depth Profundidade utilizada para auxilio do particionamento
 * @returns No alvo ou nulo
 */
Node* searchRec(Node* root, double lat_, double lon_, int depth) {
    /* Casos bases */
    if (!root)  /* Nao encontrou o no */
        return NULL;
    if (root->coordinates[0] == lat_ && root->coordinates[1] == lon_) /* Encontrou o no */
        return root;

    int cd = depth % 2; /* Dimensao atual */
    if((cd == 0 && lat_ < root->coordinates[0]) || (cd == 1 && lon_ < root->coordinates[1]))
        return searchRec(root->left, lat_, lon_, depth + 1);    /* Busca pela subarvore esquerda */
    return searchRec(root->right, lat_, lon_, depth + 1);   /* Busca pela subarvore direita */
}

/**
 * @brief Busca de um no na arvore
 * @param root Raiz da arvore
 * @param lat_ Latitude a ser inserida na arvore
 * @param lon_ Longitude a ser inserida na arvore
 * @return O resultado da busca recursiva
 */
Node* search(Node* root, double lat_, double lon_) {
    return searchRec(root, lat_, lon_, 0);
}

/**
 * @brief Funcao recursiva para encontrar o no com o menor preco em um raio.
 * @param root No atual na arvore.
 * @param centerLat Latitude central.
 * @param centerLon Longitude central.
 * @param radius Raio em quilometros.
 * @param idFuel Tipo do combustivel.
 * @param bestStation Ponteiro para o ponteiro do no que armazena o melhor candidato.
 * @param depth Profundidade atual da recursao.
 */
void findBestStationInRangeRec(Node* root, double centerLat, double centerLon, double radius, int idFuel, Node** bestStation, int depth) {
    if (root == NULL) {
        return;
    }

    /* Calcula a distancia do no atual ao centro da busca */
    double distance = haversine(centerLat, centerLon, root->coordinates[0], root->coordinates[1]);

    /* Se o no esta dentro do raio, verifica se ele e um candidato melhor */
    if (distance <= radius) {
        int currentPrice = root->fuelPrices[idFuel];
        /* Considera apenas precos validos (> 0) */
        if (currentPrice > 0) {
            /* Se ainda nao encontrou nenhum posto, este e o melhor por padrao. */
            /* Ou, se o preco deste posto for menor que o do melhor posto atual, ele se torna o novo melhor. */
            if (*bestStation == NULL || currentPrice < (*bestStation)->fuelPrices[idFuel]) {
                *bestStation = root;
            }
        }
    }

    /* Otimizacao de poda da arvore KD */
    int cd = depth % 2;
    double diff_coord = (cd == 0) ? centerLat - root->coordinates[0] : centerLon - root->coordinates[1];
    
    /* Converte o raio em graus (aproximacao) para a dimensao atual */
    double radius_in_deg;
    if (cd == 0) { /* Latitude */
        radius_in_deg = radius / 111.0;
    } else { /* Longitude */
        radius_in_deg = radius / (111.0 * cos(centerLat * M_PI / 180.0));
    }

    Node *first_child = (diff_coord < 0) ? root->left : root->right;
    Node *second_child = (diff_coord < 0) ? root->right : root->left;
    
    findBestStationInRangeRec(first_child, centerLat, centerLon, radius, idFuel, bestStation, depth + 1);

    if (fabs(diff_coord) < radius_in_deg) {
        findBestStationInRangeRec(second_child, centerLat, centerLon, radius, idFuel, bestStation, depth + 1);
    }
}

/**
 * @brief Interface para buscar o no (posto) com o menor preço de um combustivel em um raio.
 * @param root Raiz da arvore KD.
 * @param centerLat Latitude do ponto central.
 * @param centerLon Longitude do ponto central.
 * @param radius Raio da busca em quilometros.
 * @param idFuel O tipo de combustivel a ser pesquisado (0-Diesel, 1-Alcool, 2-Gasolina).
 * @return Um ponteiro para o no com o menor preço, ou NULL se nenhum for encontrado.
 */
Node* findBestStationInRange(Node* root, double centerLat, double centerLon, double radius, int idFuel) {
    Node* bestStation = NULL;

    if (idFuel < 0 || idFuel > 2)   /* Range invalido */
        return bestStation;

    /* A funcao recursiva ira atualizar bestStation diretamente atraves de seu endereço */
    findBestStationInRangeRec(root, centerLat, centerLon, radius, idFuel, &bestStation, 0);

    return bestStation;
}

/**
 * @brief Funcao para carregamento da arvore de dados a partir de um arquivo.
 * @return A raiz para a arvore criada.
 */
Node* loadTree(char* fileName) {
    FILE* file = fopen(fileName, "a");  /* Criar o arquivo se nao existir */
    if (!file) {
        perror("Erro ao abrir o arquivo");
        exit(1);
    } 

    fclose(file);

    file = fopen(fileName, "r");
    if (!file) {
        perror("Erro ao abrir o arquivo");
        exit(1);
    }

    Node* _root = NULL;
    char lineBuffer[256];
    double lat, lon;
    int fuels[3];

    while (fgets(lineBuffer, sizeof(lineBuffer), file)) {
        int itemsRead = sscanf(lineBuffer, "%lf %lf %d %d %d", &lat, &lon, &fuels[0], &fuels[1], &fuels[2]);

        if (itemsRead == 5) {
            /* Para cada linha, insere/atualiza os precos para os tres combustiveis. */
            if (fuels[0] > 0) {
                _root = insertTree(_root, lat, lon, 0, fuels[0]); /* Diesel */
            }
            if (fuels[1] > 0) {
                _root = insertTree(_root, lat, lon, 1, fuels[1]); /* Etanol */
            }
            if (fuels[2] > 0) {
                _root = insertTree(_root, lat, lon, 2, fuels[2]); /* Gasolina */
            }
        }
    }

    fclose(file);
    return _root;
}

/**
 * @brief Procedimento de salvamento recursivo
 * @param node No atual da recursao
 * @param outFile FILE pointer para o arquivo de salvamento
 */
void saveTreeRec(Node* node, FILE* outFile) {
    if (node == NULL)
        return;

    /* Guardo as informacoes no arquivo */
    fprintf(outFile, "%.15f %.15f %d %d %d\n", 
            node->coordinates[0],
            node->coordinates[1],
            node->fuelPrices[0],
            node->fuelPrices[1],
            node->fuelPrices[2]);

    /* Chama recursivamente a sub-arvore esquerda */
    saveTreeRec(node->left, outFile);

    /* Chama recursivamente a sub-arvore direita */
    saveTreeRec(node->right, outFile);
}

/** 
 * @brief Procedimento que armazena a arvore do programa em Pre Ordem no arquivo
*/
void saveTree(Node* root, char* fileName) {
    FILE* file = fopen(fileName, "w");
    if (!file) {
        perror("Erro ao abrir arquivo");
        exit(1);
    }

    /* Chama a funcao de salvamento recursiva */
    saveTreeRec(root, file);

    fclose(file);
}

/**
 * @brief Libera a arvore da memoria
 * @param root Raiz da arvore
 */
void freeTree(Node* root) {
    if (root == NULL)
        return;
    freeTree(root->left);
    freeTree(root->right);
    free(root);
}