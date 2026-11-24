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
    int id;                      // PID del proceso
    int ax, bx, cx, dx;          // Registros guardados (contexto)
    int pc;                      // Program Counter guardado
    char ir[32];                 // Instruction Register guardado
    char nombreProceso[30];      // Nombre del archivo (imagen)
    FILE *archivo;               // Archivo abierto (conserva la posición de lectura)
    char status[40];             // Resultado de la última instrucción (OK, END, ERROR...)
    char estado[12];             // Estado del proceso: Listo, Ejecucion, Salida
    struct PCB *siguiente;       // Puntero para la lista enlazada
} PCB;

// Registros de simulación del CPU (uno solo, compartido por todos los procesos)
typedef struct {
    int ax, bx, cx, dx;
    int pc;
    char ir[32];
    char status[40];
} CPU;

// PCB -> CPU (el proceso entra a ejecución)
void cargar_contexto(CPU *cpu, const PCB *pcb) {
    cpu->ax = pcb->ax;  cpu->bx = pcb->bx;
    cpu->cx = pcb->cx;  cpu->dx = pcb->dx;
    cpu->pc = pcb->pc;
    strcpy(cpu->ir, pcb->ir);
    strcpy(cpu->status, pcb->status);
}

// CPU -> PCB (el proceso sale de ejecución)
void guardar_contexto(PCB *pcb, const CPU *cpu) {
    pcb->ax = cpu->ax;  pcb->bx = cpu->bx;
    pcb->cx = cpu->cx;  pcb->dx = cpu->dx;
    pcb->pc = cpu->pc;
    strcpy(pcb->ir, cpu->ir);
    strcpy(pcb->status, cpu->status);
}

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

int obtener_valor(CPU *cpu, char *operando, int *es_valido) {
    *es_valido = 1;
    if (strcmp(operando, "AX") == 0) return cpu->ax;
    if (strcmp(operando, "BX") == 0) return cpu->bx;
    if (strcmp(operando, "CX") == 0) return cpu->cx;
    if (strcmp(operando, "DX") == 0) return cpu->dx;

    long valor;
    if (!es_numero_valido(operando, &valor)) {
        *es_valido = 0;
        return 0;
    }
    return (int)valor;
}

void guardar_valor(CPU *cpu, char *registro, int valor) {
    if (strcmp(registro, "AX") == 0) cpu->ax = valor;
    else if (strcmp(registro, "BX") == 0) cpu->bx = valor;
    else if (strcmp(registro, "CX") == 0) cpu->cx = valor;
    else if (strcmp(registro, "DX") == 0) cpu->dx = valor;
}

void procesar_instruccion(CPU *cpu, char *instruccion_original) {
    char copia[100];
    strncpy(copia, instruccion_original, sizeof(copia) - 1);
    copia[sizeof(copia) - 1] = '\0';
    copia[strcspn(copia, "\r\n")] = 0;

    strncpy(cpu->ir, copia, sizeof(cpu->ir) - 1);
    cpu->ir[sizeof(cpu->ir) - 1] = '\0';
    strcpy(cpu->status, "OK");

    if (!es_mayuscula(copia)) {
        strcpy(cpu->status, "ERROR de sintaxis");
        return;
    }

    char *comando = strtok(copia, " ,");
    if (comando == NULL) return;

    char *op1 = strtok(NULL, " ,");
    char *op2 = strtok(NULL, " ,");
    char *extra = strtok(NULL, " ,");

    if (extra != NULL) {
        strcpy(cpu->status, "ERROR de sintaxis");
        return;
    }

    if (strcmp(comando, "END") == 0) {
        if (op1 != NULL) {
            strcpy(cpu->status, "ERROR Sintaxis");
        } else {
            strcpy(cpu->status, "END");
        }
        return;
    }

    int valido1 = 1, valido2 = 1;

    if (strcmp(comando, "INC") == 0 || strcmp(comando, "DEC") == 0) {
        if (op1 == NULL || op2 != NULL || !es_registro(op1)) {
            strcpy(cpu->status, "ERROR Sintaxis");
            return;
        }
        int val = obtener_valor(cpu, op1, &valido1);
        if (strcmp(comando, "INC") == 0) guardar_valor(cpu, op1, val + 1);
        if (strcmp(comando, "DEC") == 0) guardar_valor(cpu, op1, val - 1);
    }
    else if (strcmp(comando, "MOV") == 0 || strcmp(comando, "ADD") == 0 ||
             strcmp(comando, "SUB") == 0 || strcmp(comando, "MUL") == 0 ||
             strcmp(comando, "DIV") == 0) {

        if (op1 == NULL || op2 == NULL || !es_registro(op1)) {
            strcpy(cpu->status, "ERROR Sintaxis");
            return;
        }

        int val2 = obtener_valor(cpu, op2, &valido2);
        int val1 = obtener_valor(cpu, op1, &valido1);

        if (!valido1 || !valido2) {
            strcpy(cpu->status, "ERROR OP invalido");
            return;
        }

        if (strcmp(comando, "MOV") == 0) guardar_valor(cpu, op1, val2);
        else if (strcmp(comando, "ADD") == 0) guardar_valor(cpu, op1, val1 + val2);
        else if (strcmp(comando, "SUB") == 0) guardar_valor(cpu, op1, val1 - val2);
        else if (strcmp(comando, "MUL") == 0) guardar_valor(cpu, op1, val1 * val2);
        else if (strcmp(comando, "DIV") == 0) {
            if (val2 == 0) {
                strcpy(cpu->status, "ERROR Division por cero");
            } else {
                guardar_valor(cpu, op1, val1 / val2);
            }
        }
    } else {
        strcpy(cpu->status, "ERROR Sintaxis");
    }
}

#endif
