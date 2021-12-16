#include "../include/vm.h"
#include "../include/display.h"
#include "../include/debug.h"
#include <stdbool.h>
#include <gtk/gtk.h>


static void app_activated(GApplication* app, gpointer* user_data) {
#ifdef DEBUG_LOGGING
    g_print("[GTK Application Activated]\n");
#endif
    GtkWidget* window = gtk_application_window_new(GTK_APPLICATION(app));
    gtk_window_set_title(GTK_WINDOW(window), "MegaGBC");
    gtk_window_set_default_size(GTK_WINDOW(window), MIN_RES_WIDTH_PX * 5, MIN_RES_HEIGHT_PX * 5);
    gtk_widget_show(window);
}

static void displayCleanupHandler(void* arg) {
    /* Mark display thread as dead */
#ifdef DEBUG_LOGGING
    printf("Cleaning Up Display Worker Thread\n");
#endif
    
    VM* vm = (VM*)arg;
    vm->displayThreadStatus = THREAD_DEAD;
    vm->displayThreadID = 0;

    /* Free the GUI */

    if (vm->gtkApp != NULL) {
        g_object_unref((gpointer)vm->gtkApp);
        vm->gtkApp = NULL;
    } 
}

void* startDisplay(void* arg) {
    VM* vm = (VM*)arg;
    vm->displayThreadStatus = THREAD_RUNNING;
    pthread_cleanup_push(displayCleanupHandler, arg);

    GtkApplication* app = gtk_application_new("com.megagbc.display", G_APPLICATION_FLAGS_NONE);
    vm->gtkApp = app;

    g_signal_connect(app, "activate", G_CALLBACK(app_activated), NULL);
    int status = g_application_run(G_APPLICATION(app), 0, NULL);
    
#ifdef DEBUG_LOGGING
    printf("[Exited GTK Application with status code %d]\n", status);
#endif

    /* We execute the cleanup handler now */
    pthread_cleanup_pop(1);
    return NULL;
}

