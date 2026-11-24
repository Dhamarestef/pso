#ifndef INTERFAZ_NCURSES_H
#define INTERFAZ_NCURSES_H

#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>
#include "Operaciones.h"
#include "kbhit.h"

#define FILA_TITULO       0
#define FILA_PROMPT       1
#define FILA_EJECUCION    3  
#define FILA_LISTOS       6  
#define FILA_FINALIZADOS 14  
#define FILA_MENSAJE     23

#define INDENT "  "
#define ANCHO_STATUS 20
#define MS_VISIBLE  700

static int contador_procesos = 0;

// Listas enlazadas para la gestión de estados del SO
static PCB *lista_listos = NULL;
static PCB *proceso_ejecucion = NULL;
static PCB *lista_finalizados = NULL;

void agregar_al_final(PCB **cabeza, PCB *nuevo) {
    nuevo->siguiente = NULL;
    if (*cabeza == NULL) {
        *cabeza = nuevo;
    } else {
        PCB *temp = *cabeza;
        while (temp->siguiente != NULL) {
            temp = temp->siguiente;
        }
        temp->siguiente = nuevo;
    }
}

void agregar_al_inicio(PCB **cabeza, PCB *nuevo) {
    nuevo->siguiente = *cabeza;
    *cabeza = nuevo;
}

PCB* extraer_primero(PCB **cabeza) {
    if (*cabeza == NULL) return NULL;
    PCB *temp = *cabeza;
    *cabeza = (*cabeza)->siguiente;
    temp->siguiente = NULL;
    return temp;
}

// Libera dinámicamente toda la memoria ocupada por una lista enlazada (free)
void liberar_lista(PCB **cabeza) {
    PCB *actual = *cabeza;
    PCB *siguiente = NULL;
    while (actual != NULL) {
        siguiente = actual->siguiente;
        if (actual->archivo != NULL) {
            fclose(actual->archivo);
        }
        free(actual);
        actual = siguiente;
    }
    *cabeza = NULL;
}

void mostrar_mensaje(const char *mensaje) {
    move(FILA_MENSAJE, 0);
    clrtoeol();
    mvprintw(FILA_MENSAJE, 0, INDENT "%s", mensaje);
    refresh();
}

void imprimir_cabecera_seccion(int fila, const char *titulo) {
    mvprintw(fila, 0, "%s:", titulo);
    mvprintw(fila + 1, 0, INDENT "%-4s%-5s%-5s%-5s%-5s%-5s%-16s%-*s%s",
             "ID", "PC", "AX", "BX", "CX", "DX", "IR", ANCHO_STATUS, "Status", "Proceso");
}

void redibujar_pantalla() {
    for (int f = FILA_EJECUCION; f < FILA_MENSAJE; f++) {
        move(f, 0);
        clrtoeol();
    }

    // 1. Sección de Ejecución (Máximo 1 nodo a la vez)
    imprimir_cabecera_seccion(FILA_EJECUCION, "Ejecucion");
    if (proceso_ejecucion != NULL) {
        mvprintw(FILA_EJECUCION + 2, 0, INDENT "%-4d%-5d%-5d%-5d%-5d%-5d%-16s%-*s%s",
                 proceso_ejecucion->id, proceso_ejecucion->pc, proceso_ejecucion->ax,
                 proceso_ejecucion->bx, proceso_ejecucion->cx, proceso_ejecucion->dx,
                 proceso_ejecucion->ir, ANCHO_STATUS, proceso_ejecucion->status, proceso_ejecucion->nombreProceso);
    }

    // 2. Sección de Listos
    imprimir_cabecera_seccion(FILA_LISTOS, "Listos");
    PCB *temp = lista_listos;
    int fila = FILA_LISTOS + 2;
    while (temp != NULL && fila < FILA_FINALIZADOS) {
        int pc_disp = temp->pc;
        int ax_disp = (temp->pc > 1) ? temp->ax : 0;
        int bx_disp = (temp->pc > 1) ? temp->bx : 0;
        int cx_disp = (temp->pc > 1) ? temp->cx : 0;
        int dx_disp = (temp->pc > 1) ? temp->dx : 0;
        const char *ir_disp = (temp->pc > 1) ? temp->ir : "-";

        mvprintw(fila++, 0, INDENT "%-4d%-5d%-5d%-5d%-5d%-5d%-16s%-*s%s",
                 temp->id, pc_disp, ax_disp, bx_disp, cx_disp, dx_disp,
                 ir_disp, ANCHO_STATUS, temp->status, temp->nombreProceso);
        temp = temp->siguiente;
    }

    // 3. Sección de Finalizados / Terminadas
    imprimir_cabecera_seccion(FILA_FINALIZADOS, "Terminadas");
    temp = lista_finalizados;
    fila = FILA_FINALIZADOS + 2;
    while (temp != NULL && fila < FILA_MENSAJE) {
        mvprintw(fila++, 0, INDENT "%-4d%-5d%-5d%-5d%-5d%-5d%-16s%-*s%s",
                 temp->id, temp->pc, temp->ax, temp->bx, temp->cx, temp->dx,
                 temp->ir, ANCHO_STATUS, temp->status, temp->nombreProceso);
        temp = temp->siguiente;
    }

    refresh();
}

// Bucle de simulación con interrupción mediante kbhit()
void simular_procesos() {
    int interrumpido = 0;

    while (lista_listos != NULL && !interrumpido) {
        proceso_ejecucion = extraer_primero(&lista_listos);
        char linea[100];

        while (fgets(linea, sizeof(linea), proceso_ejecucion->archivo)) {
            procesar_instruccion(proceso_ejecucion, linea);
            redibujar_pantalla();
            napms(MS_VISIBLE);

            // Detección de interrupción por teclado
            if (kbhit()) {
                getch(); // Limpia la tecla pulsada del buffer
                
                if (strcmp(proceso_ejecucion->status, "END") != 0) {
                    proceso_ejecucion->pc++;
                }

                // Pausa y regresa el proceso al frente de la cola de listos
                agregar_al_inicio(&lista_listos, proceso_ejecucion);
                proceso_ejecucion = NULL;
                
                interrumpido = 1;
                mostrar_mensaje("Ejecucion pausada. Presione ENTER para reanudar o ingrese comandos...");
                break;
            }

            if (strcmp(proceso_ejecucion->status, "END") == 0) {
                break;
            }
            proceso_ejecucion->pc++;
        }

        if (!interrumpido && proceso_ejecucion != NULL) {
            if (proceso_ejecucion->archivo) {
                fclose(proceso_ejecucion->archivo);
                proceso_ejecucion->archivo = NULL;
            }
            agregar_al_final(&lista_finalizados, proceso_ejecucion);
            proceso_ejecucion = NULL;
            redibujar_pantalla();
        }
    }

    // Espera la pulsación exclusiva de ENTER al finalizar la tanda
    if (!interrumpido) {
        mostrar_mensaje("Procesos finalizados. Presione Enter para continuar...");
        nodelay(stdscr, FALSE);
        int ch;
        do {
            ch = getch();
        } while (ch != '\n' && ch != '\r' && ch != KEY_ENTER);
        mostrar_mensaje("");
    }
}

void leer_comando(char *destino, int tam) {
    move(FILA_PROMPT, 0);
    clrtoeol();
    mvprintw(FILA_PROMPT, 0, "> ");
    refresh();

    echo();
    curs_set(1);
    getnstr(destino, tam - 1);
    noecho();
    curs_set(0);
}

void iniciar_consola_ncurses() {
    initscr();
    cbreak();
    keypad(stdscr, TRUE);
    curs_set(0);
    noecho();

    mvprintw(FILA_TITULO, 0, "Simulador de CPU - Laboratorio SO");
    redibujar_pantalla();

    char entrada[300];

    while (1) {
        leer_comando(entrada, sizeof(entrada));

        char *token = strtok(entrada, " ");
        
        // Si el usuario solo presiona ENTER sin comando
        if (token == NULL) {
            if (lista_listos != NULL) {
                mostrar_mensaje("");
                redibujar_pantalla();
                simular_procesos();
            }
            continue;
        }

        // Salida y liberación completa de memoria (free)
        if (strcasecmp(token, "salir") == 0) {
            if (proceso_ejecucion != NULL) {
                if (proceso_ejecucion->archivo) fclose(proceso_ejecucion->archivo);
                free(proceso_ejecucion);
                proceso_ejecucion = NULL;
            }
            liberar_lista(&lista_listos);
            liberar_lista(&lista_finalizados);
            break;
        }
        else if (strcmp(token, "ejecutar") == 0) {
            char *archivo = strtok(NULL, " ");
            int creados = 0;

            while (archivo != NULL) {
                FILE *f = fopen(archivo, "r");
                if (f == NULL) {
                    char err[150];
                    snprintf(err, sizeof(err), "ERROR proceso %s no existe", archivo);
                    mostrar_mensaje(err);
                    napms(1500);
                } else {
                    PCB *nuevo = (PCB*) malloc(sizeof(PCB));
                    nuevo->id = ++contador_procesos;
                    nuevo->ax = nuevo->bx = nuevo->cx = nuevo->dx = 0;
                    nuevo->pc = 1;
                    strcpy(nuevo->ir, "-");
                    strcpy(nuevo->status, "OK");
                    strncpy(nuevo->nombreProceso, archivo, sizeof(nuevo->nombreProceso) - 1);
                    nuevo->archivo = f;
                    nuevo->siguiente = NULL;

                    agregar_al_final(&lista_listos, nuevo);
                    creados++;
                }
                archivo = strtok(NULL, " ");
            }

            if (creados > 0 || lista_listos != NULL) {
                mostrar_mensaje("");
                redibujar_pantalla();
                simular_procesos();
            }
        } else {
            mostrar_mensaje("Comando invalido");
        }
    }

    endwin();
}

#endif
