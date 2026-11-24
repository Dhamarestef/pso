#include <stdio.h> // Entrada y salida estándar
#include "InterfazNcurses.h" // Modulo gráfico de ncurses

int main() {
    iniciar_consola_ncurses(); // Llama a la interfaz interactiva
    printf("Programa terminado.\n"); // Imprime mensaje al cerrar ncurses
    return 0; // Finaliza la ejecución con retorno exitoso
}
