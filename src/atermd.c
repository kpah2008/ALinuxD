#include <gtk/gtk.h>
#include <vte/vte.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <time.h>
#include <dirent.h>
#include <glib/gstdio.h>
#include <glib/gkeyfile.h>

#define TAB_COUNT 10
#define CONFIG_FILE "/.config/alinuxd/conf.ini"
#define FONT_SIZE_STEP 1
#define MIN_FONT_SIZE 8
#define MAX_FONT_SIZE 32

gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data);

GtkWidget *window;
GtkWidget *notebook;
GtkWidget *clock_label;
GtkWidget *header_bar;
GtkWidget *tab_info_label;
GtkWidget *terminal_tabs[TAB_COUNT];
GtkWidget *about_dialog;
GtkWidget *help_dialog;
GtkWidget *amenu_window;
GtkWidget *amenu_entry;
GtkWidget *amenu_list;
GtkClipboard *clipboard;
GKeyFile *config;
int current_font_size = 16;

typedef struct {
    GdkRGBA background;
    GdkRGBA foreground;
    GdkRGBA cursor;
    GdkRGBA highlight;
} ColorScheme;

const ColorScheme retro_terminal = {
    {0.0, 0.0, 0.0, 1.0},
    {0.0, 1.0, 1.0, 1.0},
    {1.0, 1.0, 1.0, 1.0},
    {0.0, 1.0, 0.0, 1.0}
};

void update_clock() {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%H:%M:%S | %a %d %b %Y", t);
    gtk_label_set_text(GTK_LABEL(clock_label), time_str);
}

gboolean on_clock_tick(gpointer data) {
    update_clock();
    return G_SOURCE_CONTINUE;
}

void set_window_properties(GtkWidget *window) {
    GdkWindow *gdk_window = gtk_widget_get_window(window);
    if (gdk_window) {
        Display *xdisplay = GDK_WINDOW_XDISPLAY(gdk_window);
        Window xwindow = GDK_WINDOW_XID(gdk_window);
        
        Atom net_wm_state = XInternAtom(xdisplay, "_NET_WM_STATE", False);
        Atom net_wm_state_below = XInternAtom(xdisplay, "_NET_WM_STATE_BELOW", False);
        Atom net_wm_state_skip_taskbar = XInternAtom(xdisplay, "_NET_WM_STATE_SKIP_TASKBAR", False);
        Atom net_wm_state_skip_pager = XInternAtom(xdisplay, "_NET_WM_STATE_SKIP_PAGER", False);
        Atom net_wm_state_sticky = XInternAtom(xdisplay, "_NET_WM_STATE_STICKY", False);
        Atom net_wm_name = XInternAtom(xdisplay, "_NET_WM_NAME", False);
        Atom utf8_string = XInternAtom(xdisplay, "UTF8_STRING", False);
        
        XClientMessageEvent xclient;
        memset(&xclient, 0, sizeof(xclient));
        xclient.type = ClientMessage;
        xclient.window = xwindow;
        xclient.message_type = net_wm_state;
        xclient.format = 32;
        xclient.data.l[0] = 1;
        xclient.data.l[1] = net_wm_state_below;
        xclient.data.l[2] = net_wm_state_skip_taskbar;
        xclient.data.l[3] = net_wm_state_skip_pager;
        xclient.data.l[4] = net_wm_state_sticky;
        
        XSendEvent(xdisplay, DefaultRootWindow(xdisplay), False,
                   SubstructureRedirectMask | SubstructureNotifyMask,
                   (XEvent *)&xclient);
        
        const char *wm_name = "ATermD - Retro Terminal with Tabs";
        XChangeProperty(xdisplay, xwindow, net_wm_name, utf8_string, 8,
                        PropModeReplace, (unsigned char *)wm_name, strlen(wm_name));
    }
}

const char *get_shell() {
    const char *shell = g_getenv("SHELL");
    if (!shell || access(shell, X_OK) != 0) {
        shell = "/bin/bash";
        if (access(shell, X_OK) != 0) {
            shell = "/bin/sh";
        }
    }
    return shell;
}

void update_terminal_font() {
    char font_desc[64];
    snprintf(font_desc, sizeof(font_desc), "Courier New Bold %d", current_font_size);
    
    for (int i = 0; i < TAB_COUNT; i++) {
        PangoFontDescription *font = pango_font_description_from_string(font_desc);
        vte_terminal_set_font(VTE_TERMINAL(terminal_tabs[i]), font);
        pango_font_description_free(font);
    }
}

void change_font_size(int delta) {
    current_font_size += delta;
    
    if (current_font_size < MIN_FONT_SIZE) current_font_size = MIN_FONT_SIZE;
    if (current_font_size > MAX_FONT_SIZE) current_font_size = MAX_FONT_SIZE;
    
    update_terminal_font();
    g_key_file_set_integer(config, "Settings", "font_size", current_font_size);
    
    char *config_dir = g_build_filename(g_get_home_dir(), CONFIG_FILE, NULL);
    g_key_file_save_to_file(config, config_dir, NULL);
    g_free(config_dir);
}

GtkWidget *create_terminal_tab(int tab_num) {
    GtkWidget *terminal = vte_terminal_new();
    VteTerminal *vte_term = VTE_TERMINAL(terminal);
    
    vte_terminal_set_color_background(vte_term, &retro_terminal.background);
    vte_terminal_set_color_foreground(vte_term, &retro_terminal.foreground);
    vte_terminal_set_color_cursor(vte_term, &retro_terminal.cursor);
    
    GdkRGBA palette[16] = {
        {0.0, 0.0, 0.0, 1.0},
        {0.8, 0.0, 0.0, 1.0},
        {0.0, 0.8, 0.0, 1.0},
        {0.8, 0.8, 0.0, 1.0},
        {0.0, 0.0, 0.8, 1.0},
        {0.8, 0.0, 0.8, 1.0},
        {0.0, 0.8, 0.8, 1.0},
        {0.8, 0.8, 0.8, 1.0},
        {0.4, 0.4, 0.4, 1.0},
        {1.0, 0.4, 0.4, 1.0},
        {0.4, 1.0, 0.4, 1.0},
        {1.0, 1.0, 0.4, 1.0},
        {0.4, 0.4, 1.0, 1.0},
        {1.0, 0.4, 1.0, 1.0},
        {0.4, 1.0, 1.0, 1.0},
        {1.0, 1.0, 1.0, 1.0}
    };
    vte_terminal_set_colors(vte_term, NULL, NULL, palette, 16);
    
    update_terminal_font();
    
    const char *shell = get_shell();
    char *shell_argv[] = { (char *)shell, NULL };
    vte_terminal_spawn_async(vte_term,
                             VTE_PTY_DEFAULT,
                             NULL,
                             shell_argv,
                             NULL,
                             G_SPAWN_SEARCH_PATH,
                             NULL, NULL, NULL, -1, NULL, NULL, NULL);
    
    return terminal;
}

void update_tab_info(int tab_num) {
    char info_text[32];
    snprintf(info_text, sizeof(info_text), "ATermD - %d/%d | ALT+1..0", tab_num + 1, TAB_COUNT);
    gtk_label_set_text(GTK_LABEL(tab_info_label), info_text);
}

void show_about_dialog(GtkWidget *widget, gpointer data) {
    about_dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                      GTK_DIALOG_MODAL,
                                      GTK_MESSAGE_INFO,
                                      GTK_BUTTONS_OK,
                                      "About ALinuxD");
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(about_dialog),
                                           "This is a retro-style Desktop Environment\n"
                                           "created for nostalgia feeling.\n\n"
                                           "ATermD is the desktop for this DE.");
    gtk_window_set_title(GTK_WINDOW(about_dialog), "About ALinuxD");
    gtk_dialog_run(GTK_DIALOG(about_dialog));
    gtk_widget_destroy(about_dialog);
}

void show_help_dialog(GtkWidget *widget, gpointer data) {
    help_dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                      GTK_DIALOG_MODAL,
                                      GTK_MESSAGE_INFO,
                                      GTK_BUTTONS_OK,
                                      "ATermD Controls");
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(help_dialog),
                                           "ALT+1..0 - Switch tabs\n"
                                           "ALT+D - Open AmenuD\n"
                                           "ALT+H - Hide/show window\n"
                                           "ALT+L - Clear terminal\n"
                                           "CTRL++/- - Change font size\n"
                                           "Super/Win - Open obconf\n"
                                           "Right click - Context menu");
    gtk_window_set_title(GTK_WINDOW(help_dialog), "ATermD Controls");
    gtk_dialog_run(GTK_DIALOG(help_dialog));
    gtk_widget_destroy(help_dialog);
}

void copy_text(GtkWidget *widget, gpointer data) {
    int current_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
    vte_terminal_copy_clipboard_format(VTE_TERMINAL(terminal_tabs[current_page]),
                                     VTE_FORMAT_TEXT);
}

void paste_text(GtkWidget *widget, gpointer data) {
    int current_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
    vte_terminal_paste_clipboard(VTE_TERMINAL(terminal_tabs[current_page]));
}

void launch_app(GtkWidget *widget, gpointer data) {
    const char *command = (const char *)data;
    char *argv[] = { (char *)command, NULL };
    g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
}

void launch_obconf(GtkWidget *widget, gpointer data) {
    char *argv[] = { "obconf", NULL };
    g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
}

GtkWidget *create_apps_menu() {
    GtkWidget *menu = gtk_menu_new();
    DIR *dir;
    struct dirent *ent;
    const char *applications_dir = "/usr/share/applications/";
    
    if ((dir = opendir(applications_dir)) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (g_str_has_suffix(ent->d_name, ".desktop")) {
                char *path = g_build_filename(applications_dir, ent->d_name, NULL);
                GKeyFile *key_file = g_key_file_new();
                if (g_key_file_load_from_file(key_file, path, G_KEY_FILE_NONE, NULL)) {
                    char *name = g_key_file_get_string(key_file, "Desktop Entry", "Name", NULL);
                    char *exec = g_key_file_get_string(key_file, "Desktop Entry", "Exec", NULL);
                    
                    if (name && exec) {
                        GtkWidget *item = gtk_menu_item_new_with_label(name);
                        g_signal_connect(item, "activate", G_CALLBACK(launch_app), g_strdup(exec));
                        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
                    }
                    
                    g_free(name);
                    g_free(exec);
                }
                g_key_file_free(key_file);
                g_free(path);
            }
        }
        closedir(dir);
    }
    
    return menu;
}

void update_amenu_list(const char *text) {
    GList *children, *iter;
    children = gtk_container_get_children(GTK_CONTAINER(amenu_list));
    for(iter = children; iter != NULL; iter = g_list_next(iter))
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    g_list_free(children);
    
    if (strlen(text) == 0) return;
    
    DIR *dir;
    struct dirent *ent;
    const char *applications_dir = "/usr/share/applications/";
    int count = 0;
    
    if ((dir = opendir(applications_dir)) != NULL) {
        while ((ent = readdir(dir)) != NULL && count < 10) {
            if (g_str_has_suffix(ent->d_name, ".desktop")) {
                char *path = g_build_filename(applications_dir, ent->d_name, NULL);
                GKeyFile *key_file = g_key_file_new();
                if (g_key_file_load_from_file(key_file, path, G_KEY_FILE_NONE, NULL)) {
                    char *name = g_key_file_get_string(key_file, "Desktop Entry", "Name", NULL);
                    char *exec = g_key_file_get_string(key_file, "Desktop Entry", "Exec", NULL);
                    
                    if (name && exec && g_strrstr(name, text)) {
                        GtkWidget *item = gtk_button_new_with_label(name);
                        g_signal_connect(item, "clicked", G_CALLBACK(launch_app), g_strdup(exec));
                        gtk_container_add(GTK_CONTAINER(amenu_list), item);
                        count++;
                    }
                    
                    g_free(name);
                    g_free(exec);
                }
                g_key_file_free(key_file);
                g_free(path);
            }
        }
        closedir(dir);
    }
    
    gtk_widget_show_all(amenu_list);
}

void show_amenu() {
    if (amenu_window && gtk_widget_get_visible(amenu_window)) {
        gtk_widget_hide(amenu_window);
        return;
    }
    
    if (!amenu_window) {
        amenu_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(amenu_window), "AmenuD");
        gtk_window_set_default_size(GTK_WINDOW(amenu_window), 400, 300);
        gtk_window_set_position(GTK_WINDOW(amenu_window), GTK_WIN_POS_CENTER);
        gtk_container_set_border_width(GTK_CONTAINER(amenu_window), 10);
        
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        gtk_container_add(GTK_CONTAINER(amenu_window), box);
        
        amenu_entry = gtk_entry_new();
        gtk_entry_set_placeholder_text(GTK_ENTRY(amenu_entry), "Type program name...");
        g_signal_connect(amenu_entry, "changed", G_CALLBACK(update_amenu_list), NULL);
        gtk_box_pack_start(GTK_BOX(box), amenu_entry, FALSE, FALSE, 0);
        
        amenu_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        gtk_box_pack_start(GTK_BOX(box), amenu_list, TRUE, TRUE, 0);
        
        g_signal_connect(amenu_window, "key-press-event", G_CALLBACK(on_key_press), NULL);
    }
    
    gtk_entry_set_text(GTK_ENTRY(amenu_entry), "");
    gtk_widget_show_all(amenu_window);
    gtk_widget_grab_focus(amenu_entry);
}

void show_context_menu(GtkWidget *widget) {
    GtkWidget *menu = gtk_menu_new();
    
    GtkWidget *apps_item = gtk_menu_item_new_with_label("Apps");
    GtkWidget *apps_menu = create_apps_menu();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(apps_item), apps_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), apps_item);
    
    GtkWidget *about_item = gtk_menu_item_new_with_label("About");
    g_signal_connect(about_item, "activate", G_CALLBACK(show_about_dialog), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), about_item);
    
    GtkWidget *help_item = gtk_menu_item_new_with_label("Comb");
    g_signal_connect(help_item, "activate", G_CALLBACK(show_help_dialog), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), help_item);
    
    GtkWidget *copy_item = gtk_menu_item_new_with_label("Copy");
    g_signal_connect(copy_item, "activate", G_CALLBACK(copy_text), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), copy_item);
    
    GtkWidget *paste_item = gtk_menu_item_new_with_label("Paste");
    g_signal_connect(paste_item, "activate", G_CALLBACK(paste_text), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), paste_item);
    
    GtkWidget *obconf_item = gtk_menu_item_new_with_label("Openbox Config");
    g_signal_connect(obconf_item, "activate", G_CALLBACK(launch_obconf), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), obconf_item);
    
    gtk_widget_show_all(menu);
    
    GdkEvent *event = gtk_get_current_event();
    if (event) {
        gtk_menu_popup_at_pointer(GTK_MENU(menu), event);
        gdk_event_free(event);
    } else {
        gtk_menu_popup_at_widget(GTK_MENU(menu), widget, GDK_GRAVITY_CENTER, GDK_GRAVITY_CENTER, NULL);
    }
}

gboolean on_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    if (event->button == GDK_BUTTON_SECONDARY) {
        show_context_menu(widget);
        return TRUE;
    }
    return FALSE;
}

GtkWidget *create_header_bar() {
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_size_request(header, -1, 32);
    
    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css_provider,
        "box {"
        "  background: #000033;"
        "  border-bottom: 2px solid #00FFFF;"
        "  color: #00FFFF;"
        "  padding: 0 12px;"
        "  font-family: 'Courier New', monospace;"
        "  font-weight: bold;"
        "}", -1, NULL);
    gtk_style_context_add_provider(gtk_widget_get_style_context(header),
                                  GTK_STYLE_PROVIDER(css_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    tab_info_label = gtk_label_new("ATermD - 1/10 | ALT+1..0");
    gtk_widget_set_halign(tab_info_label, GTK_ALIGN_START);
    gtk_widget_set_valign(tab_info_label, GTK_ALIGN_CENTER);
    
    clock_label = gtk_label_new("");
    gtk_widget_set_halign(clock_label, GTK_ALIGN_END);
    gtk_widget_set_valign(clock_label, GTK_ALIGN_CENTER);
    update_clock();
    g_timeout_add_seconds(1, on_clock_tick, NULL);
    
    gtk_box_pack_start(GTK_BOX(header), tab_info_label, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(header), clock_label, FALSE, FALSE, 0);
    
    return header;
}

void switch_to_tab(int tab_num) {
    if (tab_num >= 0 && tab_num < TAB_COUNT) {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), tab_num);
        update_tab_info(tab_num);
    }
}

gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    if (event->state & GDK_MOD1_MASK) {
        if (event->keyval >= GDK_KEY_1 && event->keyval <= GDK_KEY_9) {
            switch_to_tab(event->keyval - GDK_KEY_1);
            return TRUE;
        } else if (event->keyval == GDK_KEY_0) {
            switch_to_tab(9);
            return TRUE;
        } else if (event->keyval == GDK_KEY_d || event->keyval == GDK_KEY_D) {
            show_amenu();
            return TRUE;
        } else if (event->keyval == GDK_KEY_h || event->keyval == GDK_KEY_H) {
            if (gtk_widget_get_visible(window)) {
                gtk_widget_hide(window);
            } else {
                gtk_widget_show_all(window);
            }
            return TRUE;
        } else if (event->keyval == GDK_KEY_l || event->keyval == GDK_KEY_L) {
            int current_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
            vte_terminal_reset(VTE_TERMINAL(terminal_tabs[current_page]), TRUE, TRUE);
            return TRUE;
        }
    }
    
    if (event->state & GDK_CONTROL_MASK) {
        if (event->keyval == GDK_KEY_plus || event->keyval == GDK_KEY_KP_Add) {
            change_font_size(FONT_SIZE_STEP);
            return TRUE;
        } else if (event->keyval == GDK_KEY_minus || event->keyval == GDK_KEY_KP_Subtract) {
            change_font_size(-FONT_SIZE_STEP);
            return TRUE;
        }
    }
    
    if (event->keyval == GDK_KEY_Super_L || event->keyval == GDK_KEY_Super_R) {
        launch_obconf(NULL, NULL);
        return TRUE;
    }
    
    if (event->keyval == GDK_KEY_p || event->keyval == GDK_KEY_P) {
        show_context_menu(widget);
        return TRUE;
    }
    
    return FALSE;
}

void load_config() {
    char *config_dir = g_build_filename(g_get_home_dir(), CONFIG_FILE, NULL);
    char *dir_path = g_path_get_dirname(config_dir);
    
    if (g_mkdir_with_parents(dir_path, 0755) == -1) {
        g_free(dir_path);
        g_free(config_dir);
        return;
    }
    g_free(dir_path);
    
    config = g_key_file_new();
    g_key_file_load_from_file(config, config_dir, G_KEY_FILE_NONE, NULL);
    
    current_font_size = g_key_file_get_integer(config, "Settings", "font_size", NULL);
    if (current_font_size < MIN_FONT_SIZE || current_font_size > MAX_FONT_SIZE) {
        current_font_size = 16;
    }
    
    g_free(config_dir);
}

void setup_main_window() {
    load_config();
    
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "ATermD - Retro Terminal with Tabs");
    gtk_window_maximize(GTK_WINDOW(window));
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
    gtk_window_set_keep_below(GTK_WINDOW(window), TRUE);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    set_window_properties(window);
    
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), main_box);
    
    header_bar = create_header_bar();
    gtk_box_pack_start(GTK_BOX(main_box), header_bar, FALSE, FALSE, 0);
    
    notebook = gtk_notebook_new();
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), FALSE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(notebook), FALSE);
    gtk_box_pack_end(GTK_BOX(main_box), notebook, TRUE, TRUE, 0);
    
    for (int i = 0; i < TAB_COUNT; i++) {
        terminal_tabs[i] = create_terminal_tab(i);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), terminal_tabs[i], NULL);
        
        g_signal_connect(terminal_tabs[i], "button-press-event", G_CALLBACK(on_button_press), NULL);
    }
    
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press), NULL);
    
    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css_provider,
        "window {"
        "  background-color: #000000;"
        "  border: 2px solid #00FFFF;"
        "}"
        "notebook {"
        "  padding: 0;"
        "}"
        "entry {"
        "  font-family: 'Courier New', monospace;"
        "  font-size: 16px;"
        "  color: #00FFFF;"
        "  background: #000033;"
        "  border: 1px solid #00FFFF;"
        "}", -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                            GTK_STYLE_PROVIDER(css_provider),
                                            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    
    setup_main_window();
    
    gtk_widget_show_all(window);
    gtk_main();
    
    if (config) {
        g_key_file_free(config);
    }
    
    return 0;
}
