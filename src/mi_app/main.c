/*
 * mi_app — Plantilla de aplicación para openfpgaOS
 *
 * Punto de entrada de la aplicación. Edita este archivo para
 * implementar tu juego o demo.
 */

#include "of.h"
#include <stdio.h>

int main(void) {
    of_video_init();

    while (1) {
        of_input_poll();

        /* Borra la pantalla y muestra un mensaje de bienvenida */
        printf("\033[2J\033[H  Hola desde mi_app!\n");
        printf("  Edita src/mi_app/main.c para empezar.\n");

        of_video_flip();
        of_delay_ms(16);
    }

    return 0;
}
