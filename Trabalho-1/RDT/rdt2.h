#ifndef RDT_PROTOCOL_H
#define RDT_PROTOCOL_H

#include <string.h>
#include <stdint.h>

/* Mensagens de controle do protocolo rdt2.0 */
#define RDT_ACK "ACK"
#define RDT_NAK "NAK"

/**
 * Calcula uma soma de verificacao simples (XOR de todos os bytes).
 * @param data Ponteiro para os dados.
 * @param len Comprimento dos dados.
 * @return A soma de verificacao de 8 bits.
 */
static inline uint8_t calculateChecksum(const char* data, int len) {
    uint8_t checksum = 0;
    for (int i = 0; i < len; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

#endif // RDT_PROTOCOL_H