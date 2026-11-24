#ifndef INTERFAZ_NCURSES_H
#define INTERFAZ_NCURSES_H

#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include "Operaciones.h"
#include "kbhit.h"

#define FILA_TITULO       0
#define FILA_PROMPT       1
#define FILA_EJECUCION    3
#define FILA_LISTOS       6
#define FILA_FINALIZADOS 14
#define FILA_MENSAJE     23

#define INDENT "  "
#define ANCHO_STATUS 24
#define MS_VISIBLE  700

static int contador_procesos = 0;

// Listas enlazadas para la gestión de estados del SO
static PCB *lista_listos = NULL;
static PCB *proceso_ejecucion = NULL;
static PCB *lista_finalizados = NULL;

// Registros del CPU simulado y quantum (máx. de instrucciones por turno)
static CPU cpu;
static int quantum = 3;

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

// Selección del siguiente proceso (Round Robin = el primero de la cola).
// Para la parte 3.1 (otro algoritmo) solo se cambia esta función.
PCB* seleccionar_siguiente() {
    return extraer_primero(&lista_listos);
}

// Saca de una lista el nodo con ese id (NULL si no está)
PCB* extraer_por_id(PCB **cabeza, int id) {
    PCB *actual = *cabeza, *anterior = NULL;
    while (actual != NULL) {
        if (actual->id == id) {
            if (anterior) anterior->siguiente = actual->siguiente;
            else *cabeza = actual->siguiente;
            actual->siguiente = NULL;
            return actual;
        }
        anterior = actual;
        actual = actual->siguiente;
    }
    return NULL;
}

// Estado Salida: limpieza del proceso (cierra su archivo)
void limpiar_proceso(PCB *p) {
    strcpy(p->estado, "Salida");
    if (p->archivo != NULL) {
        fclose(p->archivo);
        p->archivo = NULL;
    }
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
    mvprintw(fila + 1, 0, INDENT "%-4s%-10s%-5s%-5s%-5s%-5s%-5s%-20s%-*s%s",
             "ID", "Estado", "PC", "AX", "BX", "CX", "DX", "IR",
             ANCHO_STATUS, "Status", "Proceso");
}

// Una fila de la tabla (id <= 0 se muestra como "-")
void imprimir_fila(int fila, int id, const char *estado, int pc,
                   int ax, int bx, int cx, int dx,
                   const char *ir, const char *status, const char *nombre) {
    char id_txt[12];
    if (id > 0) snprintf(id_txt, sizeof(id_txt), "%d", id);
    else strcpy(id_txt, "-");
    mvprintw(fila, 0, INDENT "%-4s%-10s%-5d%-5d%-5d%-5d%-5d%-20.20s%-*.*s%s",
             id_txt, estado, pc, ax, bx, cx, dx, ir,
             ANCHO_STATUS, ANCHO_STATUS - 1, status, nombre);
}

void redibujar_pantalla() {
    for (int f = FILA_EJECUCION; f < FILA_MENSAJE; f++) {
        move(f, 0);
        clrtoeol();
    }

    // 1. ESTADO DEL CPU (siempre visible, aunque esté libre)
    imprimir_cabecera_seccion(FILA_EJECUCION, "CPU");
    if (proceso_ejecucion != NULL) {
        imprimir_fila(FILA_EJECUCION + 2, proceso_ejecucion->id, "Ejecucion",
                      cpu.pc, cpu.ax, cpu.bx, cpu.cx, cpu.dx,
                      cpu.ir, cpu.status, proceso_ejecucion->nombreProceso);
    } else {
        imprimir_fila(FILA_EJECUCION + 2, 0, "Libre",
                      cpu.pc, cpu.ax, cpu.bx, cpu.cx, cpu.dx,
                      cpu.ir, cpu.status, "-");
    }

    // 2. MONITOR: procesos en Listo
    imprimir_cabecera_seccion(FILA_LISTOS, "Listos");
    PCB *temp = lista_listos;
    int fila = FILA_LISTOS + 2;
    while (temp != NULL && fila < FILA_FINALIZADOS) {
        imprimir_fila(fila++, temp->id, temp->estado, temp->pc,
                      temp->ax, temp->bx, temp->cx, temp->dx,
                      temp->ir, temp->status, temp->nombreProceso);
        temp = temp->siguiente;
    }

    // 3. MONITOR: procesos en Salida
    imprimir_cabecera_seccion(FILA_FINALIZADOS, "Terminadas");
    temp = lista_finalizados;
    fila = FILA_FINALIZADOS + 2;
    while (temp != NULL && fila < FILA_MENSAJE) {
        imprimir_fila(fila++, temp->id, temp->estado, temp->pc,
                      temp->ax, temp->bx, temp->cx, temp->dx,
                      temp->ir, temp->status, temp->nombreProceso);
        temp = temp->siguiente;
    }

    refresh();
}

// Bucle de simulación: Round Robin con quantum, interrupción con kbhit()
void simular_procesos() {
    int interrumpido = 0;

    while (lista_listos != NULL && !interrumpido) {
        // 1. Seleccionar proceso y cargar su contexto en el CPU
        proceso_ejecucion = seleccionar_siguiente();
        strcpy(proceso_ejecucion->estado, "Ejecucion");
        cargar_contexto(&cpu, proceso_ejecucion);

        char linea[100];
        int ejecutadas = 0;
        int termino = 0;

        // 2. Ejecutar hasta agotar el quantum
        while (ejecutadas < quantum) {
            if (fgets(linea, sizeof(linea), proceso_ejecucion->archivo) == NULL) {
                termino = 1;                       // fin de archivo sin END
                break;
            }

            procesar_instruccion(&cpu, linea);     // opera sobre el CPU
            ejecutadas++;
            redibujar_pantalla();
            napms(MS_VISIBLE);

            if (strcmp(cpu.status, "END") == 0) {
                termino = 1;
            } else {
                cpu.pc++;                          // siguiente instrucción
            }

            if (kbhit()) {                         // pausa por teclado
                getch();
                interrumpido = 1;
            }

            if (termino || interrumpido) break;
        }

        // 3. Guardar contexto CPU -> PCB
        guardar_contexto(proceso_ejecucion, &cpu);

        // 4. Decidir a dónde va el proceso
        if (termino) {
            limpiar_proceso(proceso_ejecucion);                       // Salida
            agregar_al_final(&lista_finalizados, proceso_ejecucion);
        } else {
            strcpy(proceso_ejecucion->estado, "Listo");
            if (interrumpido)
                agregar_al_inicio(&lista_listos, proceso_ejecucion);  // reanuda el mismo
            else
                agregar_al_final(&lista_listos, proceso_ejecucion);   // Round Robin
        }
        proceso_ejecucion = NULL;
        redibujar_pantalla();
    }

    if (interrumpido && lista_listos != NULL) {
        mostrar_mensaje("Ejecucion pausada. Presione ENTER para reanudar o ingrese comandos...");
    } else {
        mostrar_mensaje("Procesos finalizados. Presione Enter para continuar...");
        nodelay(stdscr, FALSE);
        int ch;
        do {
            ch = getch();
        } while (ch != '\n' && ch != '\r' && ch != KEY_ENTER);
        mostrar_mensaje("");
    }
}

// Comando mata: devuelve 0 = ok, 1 = ya habia terminado, 2 = no existe
int mata_pid(int pid) {
    PCB *p = extraer_por_id(&lista_listos, pid);
    if (p != NULL) {
        strcpy(p->status, "Terminado por mata");
        limpiar_proceso(p);                          // estado Salida + fclose
        agregar_al_final(&lista_finalizados, p);
        return 0;
    }
    PCB *t = lista_finalizados;
    while (t != NULL && t->id != pid) t = t->siguiente;
    return (t != NULL) ? 1 : 2;
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

    strcpy(cpu.ir, "-");
    strcpy(cpu.status, "-");

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
                    PCB *nuevo = (PCB*) calloc(1, sizeof(PCB));
                    nuevo->id = ++contador_procesos;
                    nuevo->ax = nuevo->bx = nuevo->cx = nuevo->dx = 0;
                    nuevo->pc = 1;
                    strcpy(nuevo->ir, "-");
                    strcpy(nuevo->status, "OK");
                    strcpy(nuevo->estado, "Listo");
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
        }
        else if (strcmp(token, "mata") == 0) {
            char *arg = strtok(NULL, " ");
            if (arg == NULL) {
                mostrar_mensaje("Uso: mata <pid>");
            } else {
                int resultado = 2;
                int uso_incorrecto = 0;
                while (arg != NULL) {
                    char *fin;
                    long pid = strtol(arg, &fin, 10);
                    if (*fin != '\0' || pid <= 0) { uso_incorrecto = 1; break; }
                    resultado = mata_pid((int)pid);
                    arg = strtok(NULL, " ");
                }
                redibujar_pantalla();
                if (uso_incorrecto)         mostrar_mensaje("Uso: mata <pid>");
                else if (resultado == 0)    mostrar_mensaje("Proceso movido a Salida");
                else if (resultado == 1)    mostrar_mensaje("ERROR el proceso ya termino");
                else                        mostrar_mensaje("ERROR pid no existe");
            }
        }
        else if (strcmp(token, "quantum") == 0) {
            char *arg = strtok(NULL, " ");
            int n = (arg != NULL) ? atoi(arg) : 0;
            if (n > 0) {
                quantum = n;
                mostrar_mensaje("Quantum actualizado");
            } else {
                mostrar_mensaje("Uso: quantum <N> (N > 0)");
            }
        } else {
            mostrar_mensaje("Comando invalido");
        }
    }

    endwin();
}

#endif
