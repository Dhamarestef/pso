#ifndef OPERACIONES_H
#define OPERACIONES_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>

// Definición del PCB (Process Control Block)
typedef struct PCB {
    int id;                      // Contador / ID del proceso
    int ax, bx, cx, dx;          // Registros generales
    int pc;                      // Program Counter
    char ir[16];                 // Instruction Register (ej. MOV AX, 17)
    char nombreProceso[30];      // Nombre del archivo (ej. a.asm)
    FILE *archivo;               // Puntero al archivo abierto
    char status[40];             // Estado ("OK", "END", "ERROR de sintaxis", etc.)
    struct PCB *siguiente;       // Puntero para la lista enlazada
} PCB;

int es_mayuscula(const char *cadena) {
    for (int i = 0; cadena[i] != '\0'; i++) {
        if (isalpha((unsigned char)cadena[i]) && islower((unsigned char)cadena[i])) {
            return 0;
        }
    }
    return 1;
}

int es_registro(const char *operando) {
    return (strcmp(operando, "AX") == 0 || strcmp(operando, "BX") == 0 ||
            strcmp(operando, "CX") == 0 || strcmp(operando, "DX") == 0);
}

int es_numero_valido(const char *operando, long *valor_out) {
    if (operando[0] == '\0') return 0;

    int i = 0;
    if (operando[0] == '-' || operando[0] == '+') i = 1;
    if (operando[i] == '\0') return 0;

    for (; operando[i] != '\0'; i++) {
        if (!isdigit((unsigned char)operando[i])) return 0;
    }

    errno = 0;
    char *fin;
    long valor = strtol(operando, &fin, 10);
    if (errno == ERANGE || valor > INT_MAX || valor < INT_MIN) return 0;

    *valor_out = valor;
    return 1;
}

int obtener_valor(PCB *pcb, char *operando, int *es_valido) {
    *es_valido = 1;
    if (strcmp(operando, "AX") == 0) return pcb->ax;
    if (strcmp(operando, "BX") == 0) return pcb->bx;
    if (strcmp(operando, "CX") == 0) return pcb->cx;
    if (strcmp(operando, "DX") == 0) return pcb->dx;

    long valor;
    if (!es_numero_valido(operando, &valor)) {
        *es_valido = 0;
        return 0;
    }
    return (int)valor;
}

void guardar_valor(PCB *pcb, char *registro, int valor) {
    if (strcmp(registro, "AX") == 0) pcb->ax = valor;
    else if (strcmp(registro, "BX") == 0) pcb->bx = valor;
    else if (strcmp(registro, "CX") == 0) pcb->cx = valor;
    else if (strcmp(registro, "DX") == 0) pcb->dx = valor;
}

void procesar_instruccion(PCB *pcb, char *instruccion_original) {
    char copia[100];
    strncpy(copia, instruccion_original, sizeof(copia) - 1);
    copia[sizeof(copia) - 1] = '\0';
    copia[strcspn(copia, "\r\n")] = 0;

    strncpy(pcb->ir, copia, sizeof(pcb->ir) - 1);
    pcb->ir[sizeof(pcb->ir) - 1] = '\0';
    strcpy(pcb->status, "OK");

    if (!es_mayuscula(copia)) {
        strcpy(pcb->status, "ERROR de sintaxis");
        return;
    }

    char *comando = strtok(copia, " ,");
    if (comando == NULL) return;

    char *op1 = strtok(NULL, " ,");
    char *op2 = strtok(NULL, " ,");
    char *extra = strtok(NULL, " ,");

    if (extra != NULL) {
        strcpy(pcb->status, "ERROR de sintaxis");
        return;
    }

    if (strcmp(comando, "END") == 0) {
        if (op1 != NULL) {
            strcpy(pcb->status, "ERROR Sintaxis");
        } else {
            strcpy(pcb->status, "END");
        }
        return;
    }

    int valido1 = 1, valido2 = 1;

    if (strcmp(comando, "INC") == 0 || strcmp(comando, "DEC") == 0) {
        if (op1 == NULL || op2 != NULL || !es_registro(op1)) {
            strcpy(pcb->status, "ERROR Sintaxis");
            return;
        }
        int val = obtener_valor(pcb, op1, &valido1);
        if (strcmp(comando, "INC") == 0) guardar_valor(pcb, op1, val + 1);
        if (strcmp(comando, "DEC") == 0) guardar_valor(pcb, op1, val - 1);
    }
    else if (strcmp(comando, "MOV") == 0 || strcmp(comando, "ADD") == 0 ||
             strcmp(comando, "SUB") == 0 || strcmp(comando, "MUL") == 0 ||
             strcmp(comando, "DIV") == 0) {

        if (op1 == NULL || op2 == NULL || !es_registro(op1)) {
            strcpy(pcb->status, "ERROR Sintaxis");
            return;
        }

        int val2 = obtener_valor(pcb, op2, &valido2);
        int val1 = obtener_valor(pcb, op1, &valido1);

        if (!valido1 || !valido2) {
            strcpy(pcb->status, "ERROR OP invalido");
            return;
        }

        if (strcmp(comando, "MOV") == 0) guardar_valor(pcb, op1, val2);
        else if (strcmp(comando, "ADD") == 0) guardar_valor(pcb, op1, val1 + val2);
        else if (strcmp(comando, "SUB") == 0) guardar_valor(pcb, op1, val1 - val2);
        else if (strcmp(comando, "MUL") == 0) guardar_valor(pcb, op1, val1 * val2);
        else if (strcmp(comando, "DIV") == 0) {
            if (val2 == 0) {
                strcpy(pcb->status, "ERROR Division por cero");
            } else {
                guardar_valor(pcb, op1, val1 / val2);
            }
        }
    } else {
        strcpy(pcb->status, "ERROR Sintaxis");
    }
}

#endif
