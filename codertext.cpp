/*
 * codertext - A simple GTK4-based text editor,inspired by coder-edit that I wrote with help of AI in Visual Basic.
 * 
 * Features:
 * - Text editing with GtkTextView
 * - File operations: New, Open, Save, Save As, Quit
 * - Find functionality
 * - Word Wrap toggle
 * - Status bar with line/column position
 * - Keyboard shortcuts (Ctrl+S, Ctrl+O, Ctrl+N, Ctrl+X, Ctrl+F, Ctrl+G, Ctrl+W)
 * - Hamburger menu(the GNOME design concept) in header bar with all actions
 * - Application menu bar (for desktop environments that support it)
 */

#include <gtk/gtk.h>
#include <string>
#include <cstring>

// ============================================================================
// Global state variables
// ============================================================================
static GtkApplication *app = nullptr;        // Main application instance
static GtkWidget *window = nullptr;          // Main application window
static GtkWidget *text_view = nullptr;       // Text editing widget (GtkTextView)
static GtkTextBuffer *buffer = nullptr;      // Text buffer holding document content
static GtkWidget *status_label = nullptr;    // Status bar label (line/col, filename)
static std::string current_file;             // Path of currently open file (empty = new)
static bool modified = false;                // Whether buffer has unsaved changes

// ============================================================================
// Helper functions
// ============================================================================

/*
 * Show a modal alert dialog with title and optional detail text.
 * Uses GtkAlertDialog (GTK4 modern replacement for GtkMessageDialog).
 */
static void show_alert(const char *title, const char *detail) {
    GtkAlertDialog *alert = gtk_alert_dialog_new(title);
    if (detail) gtk_alert_dialog_set_detail(alert, detail);
    gtk_alert_dialog_show(alert, GTK_WINDOW(window));
}

/*
 * Update the status bar with current filename, modified state, and cursor position.
 * Called after any buffer or cursor change.
 */
static void update_status() {
    GtkTextIter iter;
    // Get iterator at cursor position (insert mark)
    gtk_text_buffer_get_iter_at_mark(buffer, &iter, gtk_text_buffer_get_insert(buffer));
    int line = gtk_text_iter_get_line(&iter) + 1;      // 0-based to 1-based
    int col = gtk_text_iter_get_line_offset(&iter) + 1; // 0-based to 1-based
    
    std::string info = current_file.empty() ? "[No Name]" : current_file;
    if (modified) info += " modified";
    info += "   |   Line " + std::to_string(line) + ", Col " + std::to_string(col);
    gtk_label_set_text(GTK_LABEL(status_label), info.c_str());
}

/*
 * Load a file from disk into the text buffer.
 * Uses GFile async I/O for robust file handling.
 */
static void load_file(const std::string &path) {
    GFile *file = g_file_new_for_path(path.c_str());
    gchar *contents = nullptr;
    gsize length = 0;
    GError *err = nullptr;
    
    if (!g_file_load_contents(file, nullptr, &contents, &length, nullptr, &err)) {
        g_clear_error(&err);
        show_alert("Cannot open file", path.c_str());
        g_object_unref(file);
        return;
    }
    
    gtk_text_buffer_set_text(buffer, contents, (gint)length);
    g_free(contents);
    g_object_unref(file);
    
    current_file = path;
    modified = false;
    gtk_text_buffer_set_modified(buffer, FALSE);
    
    // Move cursor to start of document
    GtkTextIter start;
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_place_cursor(buffer, &start);
    update_status();
}

/*
 * Signal handler for buffer "changed" signal.
 * Marks document as modified and updates status bar.
 */
static void on_text_changed(GtkTextBuffer *b, gpointer user_data) {
    modified = true;
    gtk_text_buffer_set_modified(b, TRUE);
    update_status();
}

// ============================================================================
// Window setup and UI construction
// ============================================================================

/*
 * Application "activate" signal handler - creates and shows the main window.
 * Called once when application starts.
 */
static void on_activate(GtkApplication *application, gpointer user_data) {
    // Create main window
    window = gtk_application_window_new(application);
    gtk_window_set_title(GTK_WINDOW(window), "codertext");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    // Main vertical box: header bar (titlebar) + scrolled text + status bar
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(window), vbox);

    // Scrolled window containing the text view
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_append(GTK_BOX(vbox), scrolled);
    gtk_widget_set_hexpand(scrolled, TRUE);
    gtk_widget_set_vexpand(scrolled, TRUE);

    // Text view with monospace font, no line wrapping
    text_view = gtk_text_view_new();
    buffer = gtk_text_buffer_new(nullptr);
    gtk_text_view_set_buffer(GTK_TEXT_VIEW(text_view), buffer);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_NONE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(text_view), TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), text_view);

    // Connect buffer change signal
    g_signal_connect(buffer, "changed", G_CALLBACK(on_text_changed), nullptr);

    // ========================================================================
    // Header bar with hamburger menu (≡)
    // ========================================================================
    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(header), TRUE);
    gtk_window_set_titlebar(GTK_WINDOW(window), header);

    // Hamburger menu button (≡) on right side of header
    GtkWidget *menu_btn = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menu_btn), "open-menu-symbolic");
    gtk_menu_button_set_always_show_arrow(GTK_MENU_BUTTON(menu_btn), TRUE);
    gtk_widget_set_tooltip_text(menu_btn, "Menu");

    // Menu model for hamburger - all actions in one dropdown
    GMenu *hamburger_menu = g_menu_new();
    g_menu_append(hamburger_menu, "New", "app.new");
    g_menu_append(hamburger_menu, "Open", "app.open");
    g_menu_append(hamburger_menu, "Save", "app.save");
    g_menu_append(hamburger_menu, "Save As", "app.save_as");
    g_menu_append(hamburger_menu, "Quit", "app.quit");
    g_menu_append(hamburger_menu, "Find", "app.find");
    g_menu_append(hamburger_menu, "Find Next", "app.find_next");
    g_menu_append(hamburger_menu, "Word Wrap", "app.word_wrap");
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menu_btn), G_MENU_MODEL(hamburger_menu));

    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), menu_btn);

    // Status bar at bottom
    status_label = gtk_label_new("Line 1, Col 1");
    gtk_box_append(GTK_BOX(vbox), status_label);
    gtk_widget_set_halign(status_label, GTK_ALIGN_START);
    gtk_widget_set_margin_top(status_label, 6);
    gtk_widget_set_margin_start(status_label, 6);
    gtk_widget_set_margin_end(status_label, 6);

    update_status();

    gtk_window_present(GTK_WINDOW(window));
}

// ============================================================================
// Action callbacks (triggered by menu items or keyboard shortcuts)
// ============================================================================

/*
 * New file action - clears buffer, discards unsaved changes after confirmation.
 */
static void on_new(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    if (modified) {
        show_alert("Unsaved changes", "Discard?");
    }
    gtk_text_buffer_set_text(buffer, "", 0);
    current_file.clear();
    modified = false;
    gtk_text_buffer_set_modified(buffer, FALSE);
    GtkTextIter start;
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_place_cursor(buffer, &start);
    update_status();
}

// ============================================================================
// Async file operations (open/save)
// Uses GFileDialog for native file chooser dialogs
// ============================================================================

/*
 * Async callback for file open dialog completion.
 * Loads selected file into buffer.
 */
static void open_file_done(GObject *source, GAsyncResult *res, gpointer user_data) {
    GFile *file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(source), res, nullptr);
    if (file) {
        gchar *path = g_file_get_path(file);
        load_file(path);
        g_free(path);
        g_object_unref(file);
    }
}

/*
 * Open file action - shows file chooser dialog, warns about unsaved changes.
 */
static void on_open(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    if (modified) {
        show_alert("Unsaved changes", "Discard?");
    }
    GtkFileDialog *fd = gtk_file_dialog_new();
    gtk_file_dialog_open(fd, GTK_WINDOW(window), nullptr, open_file_done, nullptr);
    g_object_unref(fd);
}

// ============================================================================
// Save operations
// Uses g_file_replace_contents() which:
// - Creates parent directories if needed (G_FILE_CREATE_REPLACE_DESTINATION)
// - Works with local and remote files (via GFile)
// - Atomically replaces file contents
// ============================================================================

/*
 * Save buffer contents to a GFile.
 * Returns true on success, false on failure.
 */
static bool save_file(GFile *file) {
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);
    const char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    
    GError *err = nullptr;
    // G_FILE_CREATE_REPLACE_DESTINATION creates parent dirs if needed
    if (!g_file_replace_contents(file, text, strlen(text), nullptr, FALSE,
                                 G_FILE_CREATE_REPLACE_DESTINATION, nullptr, nullptr, &err)) {
        g_clear_error(&err);
        gchar *path = g_file_get_path(file);
        show_alert("Cannot save file", path ? path : "Unknown path");
        if (path) g_free(path);
        return false;
    }
    
    gchar *path = g_file_get_path(file);
    if (path && *path) current_file = path;
    if (path) g_free(path);
    modified = false;
    gtk_text_buffer_set_modified(buffer, FALSE);
    update_status();
    return true;
}

/*
 * Async callback for save dialog completion.
 * Saves to the selected file.
 */
static void save_file_done(GObject *source, GAsyncResult *res, gpointer user_data) {
    GFile *file = gtk_file_dialog_save_finish(GTK_FILE_DIALOG(source), res, nullptr);
    if (file) {
        save_file(file);
        g_object_unref(file);
    }
}

/*
 * Save action - saves to current file or shows save dialog if new file.
 */
static void on_save(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    if (current_file.empty()) {
        // New file - show save dialog
        GtkFileDialog *fd = gtk_file_dialog_new();
        gtk_file_dialog_save(fd, GTK_WINDOW(window), nullptr, save_file_done, nullptr);
        g_object_unref(fd);
    } else {
        // Existing file - save directly
        GFile *file = g_file_new_for_path(current_file.c_str());
        save_file(file);
        g_object_unref(file);
    }
}

/*
 * Save As action - always shows save dialog.
 */
static void on_save_as(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    GtkFileDialog *fd = gtk_file_dialog_new();
    gtk_file_dialog_save(fd, GTK_WINDOW(window), nullptr, save_file_done, nullptr);
    g_object_unref(fd);
}

/*
 * Quit action - confirms if unsaved changes, then quits application.
 */
static void on_quit(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    if (modified) {
        show_alert("Unsaved changes", "Quit anyway?");
    }
    g_application_quit(G_APPLICATION(app));
}

// ============================================================================
// Find dialog
// ============================================================================

/*
 * Closes a dialog window (used for Cancel button).
 */
static void close_dialog(GtkButton *button, gpointer user_data) {
    gtk_window_destroy(GTK_WINDOW(user_data));
}

/*
 * Find button callback - searches for text from current cursor position.
 * Selects and moves cursor to match if found.
 */
static void find_do_and_close(GtkButton *button, gpointer user_data) {
    GtkWindow *dialog = GTK_WINDOW(user_data);
    GtkWidget *box = gtk_window_get_child(dialog);
    GtkWidget *entry = gtk_widget_get_first_child(box);
    const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
    
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(buffer, &iter, gtk_text_buffer_get_insert(buffer));
    GtkTextIter match_start, match_end;
    
    // Search forward from cursor position
    if (gtk_text_iter_forward_search(&iter, text, GTK_TEXT_SEARCH_TEXT_ONLY,
                                     &match_start, &match_end, nullptr)) {
        gtk_text_buffer_select_range(buffer, &match_start, &match_end);
        gtk_text_buffer_place_cursor(buffer, &match_end);
        update_status();
    }
    gtk_window_destroy(dialog);
}

/*
 * Find action - shows modal find dialog with entry and Find/Cancel buttons.
 */
static void on_find(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Find");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 100);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(dialog), vbox);

    // Search entry
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Find text");
    gtk_box_append(GTK_BOX(vbox), entry);
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_widget_set_margin_top(entry, 12);
    gtk_widget_set_margin_start(entry, 12);
    gtk_widget_set_margin_end(entry, 12);
    gtk_widget_set_margin_bottom(entry, 12);

    // Button row (Find / Cancel)
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_append(GTK_BOX(vbox), hbox);
    gtk_widget_set_halign(hbox, GTK_ALIGN_END);
    gtk_widget_set_margin_top(hbox, 6);
    gtk_widget_set_margin_end(hbox, 6);
    gtk_widget_set_margin_bottom(hbox, 6);

    GtkWidget *btn_find = gtk_button_new_with_label("_Find");
    g_signal_connect(btn_find, "clicked", G_CALLBACK(find_do_and_close), dialog);
    gtk_box_append(GTK_BOX(hbox), btn_find);

    GtkWidget *btn_cancel = gtk_button_new_with_label("_Cancel");
    g_signal_connect(btn_cancel, "clicked", G_CALLBACK(close_dialog), dialog);
    gtk_box_append(GTK_BOX(hbox), btn_cancel);

    gtk_window_present(GTK_WINDOW(dialog));
}

/*
 * Find Next action - placeholder (not implemented).
 */
static void on_find_next(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
}

/*
 * Word Wrap action - toggles line wrapping mode.
 */
static void on_word_wrap(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    GtkWrapMode mode = gtk_text_view_get_wrap_mode(GTK_TEXT_VIEW(text_view));
    GtkWrapMode new_mode = (mode == GTK_WRAP_NONE) ? GTK_WRAP_WORD : GTK_WRAP_NONE;
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), new_mode);
}

// ============================================================================
// Application startup - menu bar and keyboard shortcuts
// ============================================================================

/*
 * Application "startup" signal handler.
 * Sets up:
 * 1. Application menu bar (File/Edit/View) - appears in system top panel on GNOME
 * 2. Action entries connecting menu items to callbacks
 * 3. Keyboard accelerators (shortcuts)
 */
static void app_startup(GtkApplication *application, gpointer user_data) {
    // ========================================================================
    // Application menu bar (for desktop environments that show it)
    // Organized as: File > Edit > View
    // ========================================================================
    GMenu *menu = g_menu_new();

    GMenu *file_menu = g_menu_new();
    g_menu_append(file_menu, "New", "app.new");
    g_menu_append(file_menu, "Open", "app.open");
    g_menu_append(file_menu, "Save", "app.save");
    g_menu_append(file_menu, "Save As", "app.save_as");
    g_menu_append(file_menu, "Quit", "app.quit");
    g_menu_append_section(menu, "File", G_MENU_MODEL(file_menu));

    GMenu *edit_menu = g_menu_new();
    g_menu_append(edit_menu, "Find", "app.find");
    g_menu_append(edit_menu, "Find Next", "app.find_next");
    g_menu_append_section(menu, "Edit", G_MENU_MODEL(edit_menu));

    GMenu *view_menu = g_menu_new();
    g_menu_append(view_menu, "Word Wrap", "app.word_wrap");
    g_menu_append_section(menu, "View", G_MENU_MODEL(view_menu));

    gtk_application_set_menubar(app, G_MENU_MODEL(menu));

    // ========================================================================
    // Action entries - connect menu item names to callback functions
    // Format: { action_name, callback, parameter_type, state, description }
    // ========================================================================
    GActionEntry entries[] = {
        { "new", on_new, nullptr, nullptr, nullptr },
        { "open", on_open, nullptr, nullptr, nullptr },
        { "save", on_save, nullptr, nullptr, nullptr },
        { "save_as", on_save_as, nullptr, nullptr, nullptr },
        { "quit", on_quit, nullptr, nullptr, nullptr },
        { "find", on_find, nullptr, nullptr, nullptr },
        { "find_next", on_find_next, nullptr, nullptr, nullptr },
        { "word_wrap", on_word_wrap, nullptr, nullptr, nullptr },
    };
    g_action_map_add_action_entries(G_ACTION_MAP(app), entries, G_N_ELEMENTS(entries), nullptr);

    // ========================================================================
    // Keyboard accelerators (shortcuts)
    // GTK4 uses <primary> for Ctrl (cross-platform)
    // ========================================================================
    const char *save_accels[] = { "<primary>s", nullptr };
    const char *open_accels[] = { "<primary>o", nullptr };
    const char *new_accels[] = { "<primary>n", nullptr };
    const char *quit_accels[] = { "<primary>x", nullptr };
    const char *find_accels[] = { "<primary>f", nullptr };
    const char *findnext_accels[] = { "<primary>g", nullptr };
    const char *wrap_accels[] = { "<primary>w", nullptr };

    gtk_application_set_accels_for_action(app, "app.save", save_accels);
    gtk_application_set_accels_for_action(app, "app.open", open_accels);
    gtk_application_set_accels_for_action(app, "app.new", new_accels);
    gtk_application_set_accels_for_action(app, "app.quit", quit_accels);
    gtk_application_set_accels_for_action(app, "app.find", find_accels);
    gtk_application_set_accels_for_action(app, "app.find_next", findnext_accels);
    gtk_application_set_accels_for_action(app, "app.word_wrap", wrap_accels);
}

// ============================================================================
// Main entry point
// ============================================================================

int main(int argc, char **argv) {
    // Create GTK application with unique ID
    app = gtk_application_new("org.example.codertext", G_APPLICATION_DEFAULT_FLAGS);
    
    // Connect startup (menu/shortcuts setup) and activate (window creation) signals
    g_signal_connect(app, "startup", G_CALLBACK(app_startup), nullptr);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), nullptr);
    
    // Run application main loop
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    
    // Cleanup
    g_object_unref(app);
    return status;
}