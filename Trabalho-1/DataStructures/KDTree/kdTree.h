#ifndef KDTREE_H
#define KDTREE_H

#define EARTH_RADIUS 6378.1

typedef struct Node {
    double coordinates[2];
    int fuelPrices[3]; /* 0- Diesel, 1- Etanol, 2- Gasolina */
    struct Node* left;
    struct Node* right;
} Node;

Node* createNewNode(double lat_, double lon_, int idFuel, int price);
Node* insertTree(Node* root, double lat_, double lon_, int idFuel, int price);
Node* search(Node* root, double lat_, double lon_);
double haversine(double lat1, double lon1, double lat2, double lon2);
Node* findBestStationInRange(Node* root, double centerLat, double centerLon, double radius, int idFuel);
Node* loadTree(char* fileName);
void saveTree(Node* root, char* fileName);
void freeTree(Node* root);

#endif