/* Copyright 2025 Ben Mather <bwhmather@bwhmather.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
private void
source_buffer_set_bytes(GtkSource.Buffer buffer, GLib.Bytes bytes) {
    Gtk.TextIter buf_start, buf_end;
    buffer.get_bounds(out buf_start, out buf_end);
    var old_text = buffer.get_text(buf_start, buf_end, true);
    var new_data = bytes.get_data();

    int line_delta = 0;
    int new_pos = 0;
    int new_line = 0;

    buffer.begin_user_action();
    Bedit.line_diff(old_text.data, new_data, (old_start, old_count, new_start, new_count) => {
        while (new_line < new_start && new_pos < new_data.length) {
            if (new_data[new_pos++] == '\n') {
                new_line++;
            }
        }
        int start = new_pos;
        while (new_line < new_start + new_count && new_pos < new_data.length) {
            if (new_data[new_pos++] == '\n') {
                new_line++;
            }
        }
        int end = new_pos;

        int buf_line = old_start + line_delta;
        Gtk.TextIter start_iter;

        if (old_count > 0) {
            Gtk.TextIter end_iter;
            buffer.get_iter_at_line(out start_iter, buf_line);
            if (buf_line + old_count >= buffer.get_line_count()) {
                buffer.get_end_iter(out end_iter);
            } else {
                buffer.get_iter_at_line(out end_iter, buf_line + old_count);
            }
            buffer.delete(ref start_iter, ref end_iter);
        } else {
            if (buf_line >= buffer.get_line_count()) {
                buffer.get_end_iter(out start_iter);
            } else {
                buffer.get_iter_at_line(out start_iter, buf_line);
            }
        }

        if (new_count > 0) {
            buffer.insert(ref start_iter, (string) new_data[start:end], end - start);
        }

        line_delta += new_count - old_count;
    });
    buffer.end_user_action();
}

[GtkTemplate (ui = "/com/bwhmather/Bedit/ui/bedit-document.ui")]
public sealed class Bedit.Document : Gtk.Widget {
    private GLib.Settings settings = new GLib.Settings("com.bwhmather.Bedit");
    private GLib.Settings settings_desktop = new GLib.Settings("org.gnome.desktop.interface");

    private GLib.SimpleActionGroup document_actions = new GLib.SimpleActionGroup();

    [GtkChild]
    private unowned GtkSource.View source_view;
    private unowned GtkSource.Buffer source_buffer;

    /* === File State Tracking ============================================== */

    private GLib.File? _file;
    public GLib.File? file {
        get { return this._file; }
        construct { this._file = value; }
    }

    public GLib.Bytes? file_text { get; private set; }

    private GLib.Cancellable file_text_cancellable = new GLib.Cancellable();
    private GLib.DateTime? file_text_mtime = null;
    private GLib.Error? file_text_reload_error;
    private SourceFunc? file_text_reload_active_cb;
    private SourceFunc? file_text_reload_pending_cb;

    private async void
    file_text_reload_loop_async() {
        while (this.file_text_reload_active_cb != null) {
            var f = this.file;
            if (f != null) {
                GLib.DateTime? mtime = null;
                try {
                    var info = yield f.query_info_async(
                        GLib.FileAttribute.TIME_MODIFIED,
                        GLib.FileQueryInfoFlags.NONE,
                        GLib.Priority.DEFAULT, this.file_text_cancellable
                    );
                    mtime = info.get_modification_date_time();
                } catch (Error err) {
                    warning("Error: %s\n", err.message);
                }

                uint8[] text;
                try {
                    yield f.load_contents_async(this.file_text_cancellable, out text, null);
                    var new_file_text = new GLib.Bytes(text);
                    var old_file_text = this.file_text;
                    this.file_text_mtime = mtime;
                    if (old_file_text == null || old_file_text.compare(new_file_text) != 0) {
                        this.file_text = new_file_text;
                    }
                    this.file_text_reload_error = null;
                } catch (Error e) {
                    this.file_text_mtime = null;
                    this.file_text = null;
                    this.file_text_reload_error = e;
                }
            }

            this.file_text_reload_active_cb();
            this.file_text_reload_active_cb = (owned) this.file_text_reload_pending_cb;
            this.file_text_reload_pending_cb = null;
        }
    }

    private async void
    file_text_force_reload_async() throws Error {
        SourceFunc? prev = null;
        if (this.file_text_reload_active_cb == null) {
            this.file_text_reload_active_cb = this.file_text_force_reload_async.callback;
            this.file_text_reload_loop_async.begin((obj, res) => {
                this.file_text_reload_loop_async.end(res);
            });
        } else {
            prev = this.file_text_reload_pending_cb;
            this.file_text_reload_pending_cb = this.file_text_force_reload_async.callback;
        }

        yield;

        if (prev != null) {
            prev();
        }
        if (this.file_text_reload_error != null) {
            throw this.file_text_reload_error;
        }
    }

    private async void
    file_text_reload_async() throws Error {
        if (this.file == null) {
            return;
        }
        var info = yield this.file.query_info_async(
            GLib.FileAttribute.TIME_MODIFIED,
            GLib.FileQueryInfoFlags.NONE,
            GLib.Priority.DEFAULT, null
        );
        var old_mtime = this.file_text_mtime;
        var new_mtime = info.get_modification_date_time();
        if (new_mtime != null && old_mtime != null && old_mtime.compare(new_mtime) == 0) {
            return;
        }

        yield this.file_text_force_reload_async();
    }

    private async void
    file_text_save_async(GLib.File target, uint8[] bytes) throws Error {
        yield target.replace_contents_async(
            bytes, null, false,
            GLib.FileCreateFlags.NONE,
            null,
            null
        );

        if (this._file != target) {
            this._file = target;
            this.notify_property("file");
        }

        var new_file_text = new GLib.Bytes(bytes);
        var old_file_text = this.file_text;
        if (old_file_text == null || old_file_text.compare(new_file_text) != 0) {
            this.file_text = new_file_text;
        }

        // Queue up a background reload to refresh mtime.
        this.file_text_reload_async.begin((obj, res) => {
            this.file_text_reload_async.end(res);
        });
    }

    private GLib.FileMonitor? file_text_monitor = null;

    private void
    file_text_monitor_on_changed(GLib.File file, GLib.File? other, GLib.FileMonitorEvent event) {
        switch (event) {
        case CHANGED:
        case CREATED:
        case RENAMED:
            this.file_text_reload_async.begin((obj, res) => {
                try {
                    this.file_text_reload_async.end(res);
                } catch (Error err) {
                    warning("Failed to reload file: %s\n", err.message);
                }
            });
            break;
        case DELETED:
            this.file_text = null;
            this.file_text_mtime = null;
            break;
        default:
            break;
        }
    }

    private void
    file_text_monitor_reset() {
        if (this.file_text_monitor != null) {
            this.file_text_monitor.cancel();
            this.file_text_monitor = null;
        }
        if (!this.get_mapped()) {
            return;
        }
        if (this.file == null) {
            return;
        }

        try {
            this.file_text_monitor = this.file.monitor_file(GLib.FileMonitorFlags.NONE, null);
            this.file_text_monitor.changed.connect(this.file_text_monitor_on_changed);
        } catch (Error err) {
            // Monitoring not supported on this backend - silently skip.
        }
    }

    private void
    file_text_dispose() {
        this.file_text_cancellable.cancel();
    }

    private void
    file_text_init() {
        this.map.connect(() => {
            this.file_text_monitor_reset();
            this.file_text_reload_async.begin((obj, res) => {
                try {
                    this.file_text_reload_async.end(res);
                } catch (Error err) {
                    warning("Failed to reload file: %s\n", err.message);
                }
            });
        });
        this.unmap.connect(() => {
            if (this.file_text_monitor != null) {
                this.file_text_monitor.cancel();
                this.file_text_monitor = null;
            }
        });
        var focus_controller = new Gtk.EventControllerFocus();
        focus_controller.enter.connect(() => {
            this.file_text_reload_async.begin((obj, res) => {
                try {
                    this.file_text_reload_async.end(res);
                } catch (Error err) {
                    warning("Failed to reload file: %s\n", err.message);
                }
            });
        });

        this.add_controller(focus_controller);
        this.notify["file"].connect(() => {
            this.file_text_monitor_reset();
        });
    }

    /* === Git State Tracking =============================================== */

    public GLib.Bytes? git_text { get; private set; }

    private GLib.Cancellable git_cancellable = new GLib.Cancellable();
    private bool git_text_reload_pending = false;
    private bool git_text_running = false;
    private Ggit.OId? git_loaded_sha = null;
    private bool git_text_monitor_reset_pending = false;
    private bool git_text_monitor_running = false;
    private GLib.File? git_head_file = null;
    private GLib.File? git_ref_file = null;
    private GLib.FileMonitor? git_head_monitor = null;
    private GLib.FileMonitor? git_ref_monitor = null;

    private async void
    git_text_reload_loop_async() {
        while (this.git_text_reload_pending) {
            this.git_text_reload_pending = false;

            var f = this.file;
            if (f == null) {
                this.git_text = null;
                continue;
            }

            var loaded_sha = this.git_loaded_sha;

            GLib.Bytes? result = null;
            Ggit.OId? result_sha = null;
            bool result_unchanged = false;

            SourceFunc resume = git_text_reload_loop_async.callback;
            new Thread<void>("bedit-git-text", () => {
                try {
                    Ggit.init();
                    var git_dir = Ggit.Repository.discover(f);
                    var repo = Ggit.Repository.open(git_dir);

                    var head_ref = repo.get_head();
                    var resolved = head_ref.resolve();
                    var target = resolved.get_target();

                    if (loaded_sha != null && loaded_sha.equal(target)) {
                        result_unchanged = true;
                        return;
                    }

                    var commit = repo.lookup_commit(target);
                    var tree = commit.get_tree();

                    var workdir = repo.get_workdir();
                    if (workdir == null) {
                        return;
                    }
                    var relative = workdir.get_relative_path(f);
                    if (relative == null) {
                        return;
                    }
                    var entry = tree.get_by_path(relative);
                    var blob = repo.lookup_blob(entry.get_id());
                    result = new GLib.Bytes(blob.get_raw_content());
                    result_sha = target;
                } catch (Error err) {
                    // Just null out git text.
                } finally {
                    Idle.add((owned) resume);
                }
            });
            yield;

            if (this.git_cancellable.is_cancelled()) {
                return;
            }

            if (result_unchanged) {
                continue;
            }

            if (this.git_text != result) {
                this.git_text = result;
            }
            this.git_loaded_sha = result_sha;
        }
    }

    private void
    git_text_reload() {
        this.git_text_reload_pending = true;
        if (!this.git_text_running) {
            this.git_text_running = true;
            this.git_text_reload_loop_async.begin((obj, res) => {
                this.git_text_reload_loop_async.end(res);
                this.git_text_running = false;
            });
        }
    }

    /* --- Monitoring ------------------------------------------------------- */

    private void
    git_head_monitor_on_changed(GLib.File file, GLib.File? other, GLib.FileMonitorEvent event) {
        switch (event) {
        case CHANGED:
        case CREATED:
        case RENAMED:
        case DELETED:
            this.git_text_monitor_reset();
            break;
        default:
            break;
        }
    }

    private void
    git_ref_monitor_on_changed(GLib.File file, GLib.File? other, GLib.FileMonitorEvent event) {
        switch (event) {
        case CHANGED:
        case CREATED:
        case RENAMED:
        case DELETED:
            this.git_text_reload();
            break;
        default:
            break;
        }
    }

    private async void
    git_text_monitor_reset_loop_async() {
        while (this.git_text_monitor_reset_pending) {
            this.git_text_monitor_reset_pending = false;

            var f = this.file;

            if (!this.get_mapped() || f == null) {
                if (this.git_head_file != null) {
                    this.git_head_monitor.cancel();
                    this.git_head_monitor = null;
                    this.git_head_file = null;
                }
                if (this.git_ref_file != null) {
                    this.git_ref_monitor.cancel();
                    this.git_ref_monitor = null;
                    this.git_ref_file = null;
                }
                continue;
            }

            GLib.File? git_dir = null;

            SourceFunc resume = git_text_monitor_reset_loop_async.callback;
            new Thread<void>("bedit-git-text-monitor-reset", () => {
                try {
                    Ggit.init();
                    git_dir = Ggit.Repository.discover(f);
                } catch (Error err) {
                    // Not in a git repo.
                } finally {
                    Idle.add((owned) resume);
                }
            });
            yield;

            if (this.git_cancellable.is_cancelled()) {
                return;
            }

            GLib.File? new_head_file = null;
            GLib.File? new_ref_file = null;

            if (git_dir != null) {
                new_head_file = git_dir.get_child("HEAD");
                try {
                    uint8[] head_contents;
                    yield new_head_file.load_contents_async(this.git_cancellable, out head_contents, null);
                    var head_text = ((string) head_contents).strip();
                    if (head_text.has_prefix("ref: ")) {
                        new_ref_file = git_dir.get_child(head_text.substring(5));
                    }
                } catch (Error err) {
                    new_head_file = null;
                }
            }

            if (this.git_cancellable.is_cancelled()) {
                return;
            }

            bool head_changed = (new_head_file == null) != (this.git_head_file == null);
            if (!head_changed && new_head_file != null && this.git_head_file != null) {
                head_changed = !new_head_file.equal(this.git_head_file);
            }
            if (head_changed) {
                if (this.git_head_file != null) {
                    this.git_head_monitor.cancel();
                    this.git_head_monitor = null;
                }
                this.git_head_file = new_head_file;
                if (new_head_file != null) {
                    try {
                        this.git_head_monitor = new_head_file.monitor_file(GLib.FileMonitorFlags.NONE, null);
                        this.git_head_monitor.changed.connect(this.git_head_monitor_on_changed);
                    } catch (Error err) {
                        this.git_head_file = null;
                    }
                }
            }

            bool ref_changed = (new_ref_file == null) != (this.git_ref_file == null);
            if (!ref_changed && new_ref_file != null && this.git_ref_file != null) {
                ref_changed = !new_ref_file.equal(this.git_ref_file);
            }
            if (ref_changed) {
                if (this.git_ref_file != null) {
                    this.git_ref_monitor.cancel();
                    this.git_ref_monitor = null;
                }
                this.git_ref_file = new_ref_file;
                if (new_ref_file != null) {
                    try {
                        this.git_ref_monitor = new_ref_file.monitor_file(GLib.FileMonitorFlags.NONE, null);
                        this.git_ref_monitor.changed.connect(this.git_ref_monitor_on_changed);
                    } catch (Error err) {
                        this.git_ref_file = null;
                    }
                }
            }

            this.git_text_reload();
        }
    }

    private void
    git_text_monitor_reset() {
        this.git_text_monitor_reset_pending = true;
        if (!this.git_text_monitor_running) {
            this.git_text_monitor_running = true;
            this.git_text_monitor_reset_loop_async.begin((obj, res) => {
                this.git_text_monitor_reset_loop_async.end(res);
                this.git_text_monitor_running = false;
            });
        }
    }

    /* --- Setup ------------------------------------------------------------ */

    private void
    git_text_init() {
        this.map.connect(() => {
            this.git_text_monitor_reset();
        });
        this.unmap.connect(() => {
            this.git_text_monitor_reset();
            this.git_text_reload_pending = false;
        });
        var focus_controller = new Gtk.EventControllerFocus();
        focus_controller.enter.connect(() => {
            this.git_text_reload();
        });
        this.add_controller(focus_controller);
        this.notify["file"].connect(() => {
            this.git_loaded_sha = null;
            this.git_text_monitor_reset();
        });
    }

    private void
    git_text_dispose() {
        this.git_cancellable.cancel();
    }

    /* === Buffer Load / Save =============================================== */

    public bool modified { get; private set; }

    public signal void load();
    public signal void loaded();
    public bool loading { get; private set; }

    public signal void save();
    public signal void saved();
    public bool saving { get; private set; }

    public bool busy { get; private set; }

    private void
    update_busy() {
        this.busy = this.loading || this.saving;
    }

    [GtkChild]
    private unowned Gtk.InfoBar reload_bar;

    [GtkChild]
    private unowned Gtk.Label reload_label;

    [GtkChild]
    private unowned Gtk.Button reload_button;

    private async void
    reload_async() {
        if (!(this.file is GLib.File)) {
            return;
        }
        if (this.loading) {
            return;
        }

        this.reload_bar.revealed = false;
        this.loading = true;
        this.load();

        var initial = true;
        if (this.source_buffer.get_char_count() != 0) {
            initial = false;
        }
        if (this.source_buffer.get_modified()) {
            initial = false;
        }
        if (this.source_buffer.can_undo || this.source_buffer.can_redo) {
            initial = false;
        }

        try {
            yield this.file_text_force_reload_async();

            var bytes = this.file_text;
            if (bytes == null) {
                return;
            }

            if (initial) {
                var text = (string?) bytes.get_data() ?? "";
                this.source_buffer.set_text(text, (int) bytes.length);
                Gtk.TextIter start;
                this.source_buffer.get_start_iter(out start);
                this.source_buffer.place_cursor(start);
            } else {
                source_buffer_set_bytes(this.source_buffer, bytes);
            }
            this.source_buffer.set_modified(false);
            this.scroll_to_cursor();
            this.loaded();
        } catch (Error err) {
            this.reload_label.label = GLib.Markup.printf_escaped("<b>%s</b>", err.message);
            this.reload_button.label = "Try again";
            this.reload_bar.revealed = true;
        } finally {
            this.loading = false;
        }
    }

    public void
    reload() {
        this.reload_async.begin((_, res) => {
            this.reload_async.end(res);
        });
    }

    public async void
    save_async(GLib.File file) throws Error {
        return_val_if_fail(!this.loading, false);
        return_val_if_fail(!this.saving, false);

        this.reload_bar.revealed = false;
        this.saving = true;
        this.save();

        Gtk.TextIter s, e;
        this.source_buffer.get_bounds(out s, out e);
        var text  = this.source_buffer.get_text(s, e, true);
        var bytes = text.data;   // uint8[], shares memory with `text`

        try {
            yield this.file_text_save_async(file, bytes);
            this.source_buffer.set_modified(false);
            this.saved();
        } finally {
            this.saving = false;
        }
    }

    private void
    buffer_init() {
        this.source_buffer.modified_changed.connect((tb) => {
            this.modified = this.source_buffer.get_modified();
        });
        this.notify["loading"].connect((_, pspec) => { this.update_busy(); });
        this.notify["saving"].connect((_, pspec) => { this.update_busy(); });

        var action = new GLib.SimpleAction("reload", null);
        action.activate.connect(this.reload);
        this.document_actions.add_action(action);

        this.reload_bar.close.connect(() => {
            this.reload_bar.revealed = false;
        });

        if (this.file != null) {
            this.reload();
        }

        // React to disk-snapshot changes.
        this.notify["file-text"].connect(() => {
            if (this.loading || this.saving) {
                return;
            }
            var disk = this.file_text;
            if (disk == null) {
                this.reload_bar.revealed = false;
                return;
            }
            Gtk.TextIter s, e;
            this.source_buffer.get_bounds(out s, out e);
            var buffer_bytes = new GLib.Bytes(
                this.source_buffer.get_text(s, e, true).data
            );
            if (disk.compare(buffer_bytes) != 0) {
                if (!this.source_buffer.get_modified() && !this.source_buffer.can_redo) {
                    this.loading = true;
                    this.load();
                    source_buffer_set_bytes(this.source_buffer, disk);
                    this.source_buffer.set_modified(false);
                    this.scroll_to_cursor();
                    this.loaded();
                    this.loading = false;
                } else {
                    this.reload_label.label = "<b>The file has been changed by another program</b>";
                    this.reload_button.label = "Drop Changes and Reload";
                    this.reload_bar.revealed = true;
                }
            } else {
                this.reload_bar.revealed = false;
            }
        });
    }

    /* === Editing ========================================================== */

    public bool can_undo { get; private set; }

    public void
    undo() {
        this.source_buffer.undo();
        this.scroll_to_cursor();
    }

    public bool can_redo { get; private set; }

    public void
    redo() {
        this.source_buffer.redo();
        this.scroll_to_cursor();
    }

    public bool can_cut { get; private set; }

    public void
    cut() {
        var clipboard = this.get_display().get_clipboard();
        this.source_buffer.cut_clipboard(clipboard, this.source_view.editable);
    }

    public bool can_copy { get; private set; }

    public void
    copy() {
        var clipboard = this.get_display().get_clipboard();
        this.source_buffer.copy_clipboard(clipboard);
    }

    public bool can_paste { get; private set; default = true; }

    public void
    paste() {
        var clipboard = this.get_display().get_clipboard();
        this.source_buffer.paste_clipboard(clipboard, null, this.source_view.editable);
    }

    public void
    select_all() {
        Gtk.TextIter start;
        Gtk.TextIter end;
        this.source_buffer.get_bounds(out start, out end);
        this.source_buffer.select_range(start, end);
    }

    public void
    sort_lines() {
        Gtk.TextIter start;
        Gtk.TextIter end;

        if (!this.source_buffer.get_selection_bounds(out start, out end)) {
            this.source_buffer.get_bounds(out start, out end);
        }
        this.source_buffer.sort_lines(start, end, NONE, 0);
    }

    public void
    delete_line() {
        Gtk.TextIter start;
        Gtk.TextIter end;
        if (!this.source_buffer.get_selection_bounds(out start, out end)) {
            var cursor = this.source_buffer.get_insert();
            this.source_buffer.get_iter_at_mark(out start, cursor);
            end = start;
        }
        start.order(ref end);

        start.set_line_offset(0);
        end.forward_lines(1);

        if (end.is_end()) {
            if (start.backward_line() && !start.ends_line()) {
                start.forward_to_line_end();
            }
        }

        if (!start.equal(end)){
            this.source_buffer.begin_user_action();
            this.source_buffer.delete_interactive(ref start, ref end, this.source_view.editable);
            this.source_buffer.end_user_action();

            this.source_view.scroll_mark_onscreen(this.source_buffer.get_insert());
        }
    }

    private void
    editing_init() {
        this.source_buffer.notify["can-undo"].connect((sb, pspec) => {
            this.can_undo = source_buffer.can_undo;
        });
        this.source_buffer.notify["can-redo"].connect((sb, pspec) => {
            this.can_redo = source_buffer.can_redo;
        });
        this.source_buffer.notify["has-selection"].connect((sb, pspec) => {
            bool has_selection = this.source_buffer.has_selection;
            this.can_cut = has_selection;
            this.can_copy = has_selection;
        });
    }

    /* === Metadata ========================================================= */

    /* --- Title ------------------------------------------------------------ */

    public string title { get; private set; }

    private static bool[] title_allocated_draft_numbers = new bool[16];

    private uint title_draft_number;

    private void
    title_acquire_draft_number() {
        if (this.title_draft_number == 0) {
            while (true) {
                this.title_draft_number++;

                if (this.title_draft_number >= title_allocated_draft_numbers.length) {
                    title_allocated_draft_numbers.resize(title_allocated_draft_numbers.length + 1);
                    title_allocated_draft_numbers[this.title_draft_number] = true;
                    break;
                }

                if (!title_allocated_draft_numbers[this.title_draft_number]) {
                    title_allocated_draft_numbers[this.title_draft_number] = true;
                    break;
                }
            }
        }
    }

    private void
    title_release_draft_number() {
        if (this.title_draft_number != 0) {
            title_allocated_draft_numbers[this.title_draft_number] = false;
            this.title_draft_number = 0;
        }
    }

    private void
    title_update() {
        if (this.file == null) {
            this.title_acquire_draft_number();
            this.title = "Untitled Document %u".printf(this.title_draft_number);
        } else {
            this.title_release_draft_number();
            this.title = this.file.get_basename();
        }
    }

    private void
    title_init() {
        this.notify["file"].connect((_, pspec) => { this.title_update(); });
        this.title_update();
    }

    private void
    title_deinit() {
        this.title_release_draft_number();
    }

    /* --- Language --------------------------------------------------------- */

    public GtkSource.Language language { get; set; }

    private void
    language_update() {
        if (this.file == null) {
            return;
        }

        var language_manager = GtkSource.LanguageManager.get_default();
        var language = language_manager.guess_language(this.file.get_path(), null);
        if (language == null) {
            // Don't clear language if already set.
            return;
        }

        this.language = language;
    }

    private void
    language_init() {
        this.bind_property("language", this.source_buffer, "language", SYNC_CREATE);

        this.notify["file"].connect((_, pspec) => { this.language_update(); });
        this.language_update();
    }

    /* --- Source Position -------------------------------------------------- */

    public int line { get; private set; }
    public int column { get; private set; }

    public signal void moved();

    private void
    position_update() {
        Gtk.TextIter start_iter;
        this.source_buffer.get_iter_at_mark(out start_iter, this.source_buffer.get_insert());

        this.freeze_notify();
        this.line = start_iter.get_line() + 1;
        this.column = start_iter.get_line_offset() + 1;
        this.thaw_notify();
        this.moved();
    }

    private void
    position_init() {
        this.source_buffer.end_user_action.connect((tb) => {
            this.position_update();
        });
        this.source_buffer.mark_set.connect((loc, mark) => {
            if (mark == this.source_buffer.get_insert()) {
                this.position_update();
            }
        });
        this.position_update();
    }

    /* === Whitespace ======================================================= */

    /* --- Indentation ------------------------------------------------------ */

    public uint tab_width { get; set; }
    public bool insert_spaces_instead_of_tabs { get; set; }
    public bool auto_indent { get; set; }

    private void
    indentation_init() {
        this.settings.bind("tab-width", this, "tab-width", GET);
        this.bind_property("tab-width", this.source_view, "tab-width", SYNC_CREATE);

        this.settings.bind("insert-spaces-instead-of-tabs", this, "insert-spaces-instead-of-tabs", GET);
        this.bind_property("insert-spaces-instead-of-tabs", this.source_view, "insert-spaces-instead-of-tabs", SYNC_CREATE);

        this.settings.bind("auto-indent", this, "auto-indent", GET);
        this.bind_property("auto-indent", this.source_view, "auto-indent", SYNC_CREATE);
    }

    /* --- Trim trailing whitespace ----------------------------------------- */

    public bool trim_trailing_whitespace { get; set; }

    private void
    trim_trailing() {
        this.source_buffer.begin_user_action();

        Gtk.TextIter iter;
        this.source_buffer.get_start_iter(out iter);
        while (!iter.is_end()) {
            iter.forward_to_line_end();
            var cursor = iter;
            while (!cursor.starts_line()) {
                var lookahead = cursor;
                lookahead.backward_char();

                var ch = lookahead.get_char();
                if (ch != ' ' && ch != '\t') {
                    break;
                }

                cursor = lookahead;
            }

            if (!cursor.equal(iter)) {
                this.source_buffer.@delete(ref cursor, ref iter);
            }
        }

        this.source_buffer.end_user_action();
    }

    private void
    trim_trailing_init() {
        this.settings.bind("trim-trailing-whitespace", this, "trim-trailing-whitespace", GET);

        this.save.connect((d) => {
            if (this.trim_trailing_whitespace) {
                this.trim_trailing();
            }
        });
    }

    /* === Appearance ======================================================= */

    /* --- Font ------------------------------------------------------------- */

    private Gtk.CssProvider font_css_provider;

    public bool use_default_font { get; set; }
    public string editor_font { get; set; }

    private void
    update_font() {
        string font_name;
        if (this.use_default_font) {
            font_name = settings_desktop.get_string("monospace-font-name");
        } else {
            font_name = this.editor_font;
        }
        var font_desc = Pango.FontDescription.from_string(font_name);
        var font_css = Bedit.font_description_to_css(font_desc);
        font_css_provider.load_from_string("textview { %s }".printf(font_css));
    }

    private void
    font_init() {
        this.font_css_provider = new Gtk.CssProvider();
        this.source_view.get_style_context().add_provider(this.font_css_provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION);

        this.settings.bind("use-default-font", this, "use-default-font", GET);
        this.settings.bind("editor-font", this, "editor-font", GET);

        this.notify["use-default-font"].connect((d, pspec) => { this.update_font(); });
        this.notify["editor-font"].connect((d, pspec) => { this.update_font(); });
        this.update_font();
    }

    /* --- Word Wrap -------------------------------------------------------- */

    public bool word_wrap { get; set; }

    private void
    update_word_wrap() {
        if (this.word_wrap) {
            this.source_view.wrap_mode = WORD_CHAR;
        } else {
            this.source_view.wrap_mode = NONE;
        }
    }

    private void
    word_wrap_init() {
        this.settings.bind("word-wrap", this, "word-wrap", GET);
        this.notify["word-wrap"].connect((d, pspec) => { this.update_word_wrap(); });
        this.update_word_wrap();
    }

    /* --- Highlight Current Line ------------------------------------------- */

    public bool highlight_current_line { get; set; }

    private void
    highlight_current_line_init() {
        this.settings.bind("highlight-current-line", this, "highlight-current-line", GET);
        this.bind_property("highlight-current-line", this.source_view, "highlight-current-line", SYNC_CREATE);
    }

    /* --- Highlight Syntax ------------------------------------------------- */

    public bool highlight_syntax { get; set; }

    private void
    highlight_syntax_init() {
        this.settings.bind("highlight-syntax", this, "highlight-syntax", GET);
        this.bind_property("highlight-syntax", this.source_buffer, "highlight-syntax", SYNC_CREATE);
    }

    /* --- Style Scheme ----------------------------------------------------- */

    public string style_scheme { get; set; }

    private void
    update_style_scheme() {
        var scheme_manager = GtkSource.StyleSchemeManager.get_default();
        var scheme = scheme_manager.get_scheme(this.style_scheme);
        if (scheme != null) {
            var gtk_settings = Gtk.Settings.get_for_display(this.get_display());
            if (gtk_settings.gtk_interface_color_scheme == Gtk.InterfaceColorScheme.DARK) {
                var dark_id = scheme.get_metadata("dark-variant");
                if (dark_id != null) {
                    var dark_scheme = scheme_manager.get_scheme(dark_id);
                    if (dark_scheme != null) {
                        scheme = dark_scheme;
                    }
                }
            }
        }
        this.source_buffer.style_scheme = scheme;
    }

    private void
    style_scheme_init() {
        this.settings.bind("style-scheme", this, "style-scheme", GET);
        this.notify["style-scheme"].connect(this.update_style_scheme);
        var gtk_settings = Gtk.Settings.get_for_display(this.get_display());
        gtk_settings.notify["gtk-interface-color-scheme"].connect(this.update_style_scheme);
        this.update_style_scheme();
    }

    /* --- Quick Highlight -------------------------------------------------- */

    public bool highlight_selection { get; set; }

    private GtkSource.SearchContext? highlight_selected_context;
    private uint highlight_selected_timeout_id;

    private void
    highlight_selected_update() {
        if (this.highlight_selected_timeout_id != 0) {
            return;
        }

        this.highlight_selected_timeout_id = GLib.Idle.add_full(GLib.Priority.LOW, () => {
            this.highlight_selected_timeout_id = 0;

            var selection = this.get_selection();
            if (selection == null || selection.length == 0) {
                this.highlight_selected_context = null;
                return GLib.Source.REMOVE;
            }

            if (!this.highlight_selection) {
                this.highlight_selected_context = null;
                return GLib.Source.REMOVE;
            }

            if (this.highlight_selected_context == null) {
                this.highlight_selected_context = new GtkSource.SearchContext(this.source_buffer, null);
            }

            var settings = this.highlight_selected_context.settings;
            settings.search_text = selection;
            settings.regex_enabled = false;
            settings.case_sensitive = true;

            return GLib.Source.REMOVE;
        });
    }

    private void
    highlight_selected_init() {
        this.settings.bind("highlight-selection", this, "highlight-selection", GET);
        this.notify["highlight-selection"].connect(() => {
            this.highlight_selected_update();
        });

        this.source_buffer.mark_set.connect((b, loc, mark) => {
            if (mark == this.source_buffer.get_insert()) {
                this.highlight_selected_update();
            }
        });

        this.source_buffer.delete_range.connect(() => {
            this.highlight_selected_update();
        });
    }

    private void
    highlight_selected_dispose() {
        if (this.highlight_selected_timeout_id != 0) {
            GLib.Source.remove(this.highlight_selected_timeout_id);
            this.highlight_selected_timeout_id = 0;
        }
    }

    /* --- Line Numbers ----------------------------------------------------- */

    public bool show_line_numbers { get; set; }

    private void
    line_numbers_init() {
        this.settings.bind("show-line-numbers", this, "show-line-numbers", GET);
        this.bind_property("show-line-numbers", this.source_view, "show-line-numbers", SYNC_CREATE);
    }

    /* --- Git Diff Gutter -------------------------------------------------- */

    private void
    git_text_gutter_init() {
        var gutter = this.source_view.get_gutter(Gtk.TextWindowType.LEFT);
        var renderer = new Bedit.DiffGutterRenderer();
        gutter.insert(renderer, GtkSource.ViewGutterPosition.LINES - 2);
        this.bind_property("git-text", renderer, "reference", SYNC_CREATE);
    }

    /* --- File Diff Gutter ------------------------------------------------- */

    private void
    file_text_gutter_init() {
        var gutter = this.source_view.get_gutter(Gtk.TextWindowType.LEFT);
        var renderer = new Bedit.DiffGutterRenderer();
        gutter.insert(renderer, GtkSource.ViewGutterPosition.LINES - 1);
        this.bind_property("file-text", renderer, "reference", SYNC_CREATE);
    }

    /* --- Overview Map ----------------------------------------------------------------------------------- */

    [GtkChild]
    private unowned GtkSource.Map overview_map;

    [GtkChild]
    private unowned Gtk.ScrolledWindow scrolled_window;

    public bool show_overview_map { get; set; }

    private void
    update_show_overview_map() {
        if (this.show_overview_map) {
            this.overview_map.visible = true;
            this.scrolled_window.vscrollbar_policy = EXTERNAL;
        } else {
            this.overview_map.visible = false;
            this.scrolled_window.vscrollbar_policy = ALWAYS;
        }
    }

    private void
    overview_map_init() {
        this.settings.bind("show-overview-map", this, "show-overview-map", GET);
        this.notify["show-overview-map"].connect((d, pspec) => { this.update_show_overview_map(); });
        this.update_show_overview_map();
    }

    /* --- Right Margin ----------------------------------------------------- */

    public bool show_right_margin { get; set; }
    public uint right_margin_position { get; set; }

    private void
    right_margin_init() {
        this.settings.bind("right-margin-position", this, "right-margin_position", GET);
        this.bind_property("right-margin-position", this.source_view, "right-margin-position", SYNC_CREATE);

        this.settings.bind("show-right-margin", this, "show-right-margin", GET);
        this.bind_property("show-right-margin", this.source_view, "show-right-margin", SYNC_CREATE);
    }

    /* === Navigation ======================================================= */

    private Gtk.TextMark? start_mark;
    private bool start_mark_reset_blocked;

    private void
    scroll_to_cursor() {
        this.source_view.scroll_to_mark(this.source_buffer.get_insert(), 0.25, false, 0.0, 0.0);
    }

    private void
    clear_start_mark() {
        if (start_mark != null) {
            this.source_buffer.delete_mark(this.start_mark);
            this.start_mark = null;
        }
    }

    private void
    set_start_mark() {
        return_if_fail(this.start_mark == null);

        Gtk.TextIter start_iter;
        this.source_buffer.get_selection_bounds(out start_iter, null);
        this.start_mark = this.source_buffer.create_mark(null, start_iter, false);
    }

    private void
    reset_start_mark() {
        if (!this.start_mark_reset_blocked) {
            this.clear_start_mark();
            this.set_start_mark();
        }
    }

    private void
    start_mark_init() {
        this.set_start_mark();

        this.source_buffer.mark_set.connect((loc, mark) => {
            if (mark == this.source_buffer.get_insert()) {
                this.reset_start_mark();
            }
        });
        this.source_buffer.changed.connect(() => {
            this.reset_start_mark();
        });
    }

    /* --- Search and Replace ----------------------------------------------- */

    private GtkSource.SearchContext? search_context;
    private GLib.Cancellable? search_cancellable;

    public int num_search_occurrences { get; private set; }
    public int selected_search_occurrence { get; private set; }

    private void
    update_search_occurrences() {
        int count = -1;
        int position = -1;
        Gtk.TextIter selection_start;
        Gtk.TextIter selection_end;

        if (this.search_context != null) {
            count = this.search_context.get_occurrences_count();
            if (count == -1) {
                // Search not finished yet.  Leave previous state in place to
                // avoid flashing.
                return;
            }

            this.source_buffer.get_selection_bounds(out selection_start, out selection_end);
            position = this.search_context.get_occurrence_position(selection_start, selection_end);
        }

        this.freeze_notify();
        this.num_search_occurrences = count;
        this.selected_search_occurrence = position;
        this.thaw_notify();
    }

    public void
    find(string query, bool regex, bool case_sensitive) {
        if (this.search_context == null) {
            this.search_context = new GtkSource.SearchContext(this.source_buffer, null);
            this.search_context.notify["occurrences-count"].connect((sc, pspec) => {
                this.update_search_occurrences();
            });
        }

        var settings = this.search_context.settings;
        settings.search_text = query;
        settings.regex_enabled = regex;
        settings.case_sensitive = case_sensitive;
        settings.wrap_around = true;
    }

    private void
    wait_focus_first() {
        Gtk.TextIter start_at;
        Gtk.TextIter match_start;
        Gtk.TextIter match_end;
        bool found;

        if (this.search_cancellable == null) {
            return;
        }

        return_if_fail(this.search_context != null);
        return_if_fail(this.start_mark != null);

        // TODO it would be nice if there was a way to block on the current run
        // of bedit_searchbar_focus_first() instead of killing it and starting
        // again.
        this.search_cancellable.cancel();
        this.search_cancellable = null;

        this.source_buffer.get_iter_at_mark(out start_at, this.start_mark);

        found = this.search_context.forward(start_at, out match_start, out match_end, null);

        if (found) {
            this.start_mark_reset_blocked = true;
            this.source_buffer.select_range(match_start, match_end);
            this.start_mark_reset_blocked = false;

        } else {
            this.source_buffer.select_range(start_at, start_at);
        }

        this.scroll_to_cursor();
    }

    private async void
    focus_first_async() throws Error {
        Gtk.TextIter start_at;
        Gtk.TextIter match_start;
        Gtk.TextIter match_end;
        bool found;

        assert(this.search_context != null);
        assert(this.start_mark != null);

        if (this.search_cancellable!= null) {
            this.search_cancellable.cancel();
        }
        this.search_cancellable = new GLib.Cancellable();

        this.source_buffer.get_iter_at_mark(out start_at, this.start_mark);

        found = yield this.search_context.forward_async(
            start_at, this.search_cancellable, out match_start, out match_end, null
        );

        if (found) {
            this.start_mark_reset_blocked = true;
            this.source_buffer.select_range(match_start, match_end);
            this.start_mark_reset_blocked = false;

        } else {
            this.source_buffer.select_range(start_at, start_at);
        }

        this.scroll_to_cursor();
    }

    public void
    focus_first() {
        this.focus_first_async.begin((_, res) => {
            try {
                this.focus_first_async.end(res);
            } catch(GLib.IOError.CANCELLED err) {
            } catch(Error err) {
                warning("Error: %s\n", err.message);
            }
        });
    }

    public void
    find_next() {
        Gtk.TextIter start_at;
        Gtk.TextIter match_start;
        Gtk.TextIter match_end;
        bool found;

        this.wait_focus_first();

        if (this.search_context == null) {
            return;
        }

        this.source_buffer.get_selection_bounds(null, out start_at);

        found = this.search_context.forward(start_at, out match_start, out match_end, null);
        if (found) {
            this.source_buffer.select_range(match_start, match_end);
            this.reset_start_mark();
            this.scroll_to_cursor();
        }
    }

    public void
    find_prev() {
        Gtk.TextIter start_at;
        Gtk.TextIter match_start;
        Gtk.TextIter match_end;
        bool found;

        this.wait_focus_first();

        if (this.search_context == null) {
            return;
        }

        this.source_buffer.get_selection_bounds(out start_at, null);

        found = this.search_context.backward(start_at, out match_start, out match_end, null);
        if (found) {
            this.source_buffer.select_range(match_start, match_end);
            this.reset_start_mark();
            this.scroll_to_cursor();
        }
    }

    public void
    replace(string replacement) throws Error {
        Gtk.TextIter selection_start;
        Gtk.TextIter selection_end;

        return_if_fail(this.search_context != null);

        this.wait_focus_first();

        this.source_buffer.get_selection_bounds(out selection_start, out selection_end);
        this.search_context.replace(selection_start, selection_end, replacement, -1);

        this.find_next();
    }

    public void
    replace_all(string replacement) throws Error {
        return_if_fail(this.search_context != null);

        this.search_context.replace_all(replacement, -1);
    }

    public void
    clear_search() {
        if (this.search_cancellable != null) {
            this.search_cancellable.cancel();
        }
        this.search_cancellable = null;

        this.search_context = null;

        this.update_search_occurrences();
    }

    public string?
    get_selection() {
        Gtk.TextIter selection_start;
        Gtk.TextIter selection_end;
        bool found;

        found = this.source_buffer.get_selection_bounds(out selection_start, out selection_end);
        if (!found) {
            return null;
        }

        return this.source_buffer.get_slice(selection_start, selection_end, true);
    }

    private void
    search_init() {
        this.source_buffer.mark_set.connect((loc, mark) => {
            if (mark == this.source_buffer.get_insert() || mark == this.source_buffer.get_selection_bound()) {
                this.update_search_occurrences();
            }
        });
    }

    /* --- Go To Line ------------------------------------------------------- */

    [GtkChild]
    private unowned Gtk.Revealer go_to_line_revealer;

    [GtkChild]
    private unowned Gtk.Entry go_to_line_entry;

    private void
    go_to_line_update() {
        string text;
        string line_text;
        string column_text;
        Gtk.TextIter start_at;
        int line;
        int column;

        text = go_to_line_entry.get_text();
        this.source_buffer.get_iter_at_mark(out start_at, this.start_mark);

        if (text[0] == '\0') {
            this.source_buffer.place_cursor(start_at);
            this.scroll_to_cursor();

            // TODO clear error.

            return;
        }

        var components = text.split(":", 2);
        line_text = components[0];
        column_text = components[1];

        line = 0;
        switch (text[0]) {
        case '\0':
            line = start_at.get_line();
            break;

        case '-':
            int curr_line = start_at.get_line();

            int offset = 0;
            if (text[1] != '\0') {
                bool ok = int.try_parse(text[1:], out offset);
                if (!ok) {
                    // Assume overflow.  Snap to first number that will trigger out of bounds and set error
                    // on input.
                    offset = curr_line + 1;
                }
            }
            line = curr_line - offset;
            break;

        case '+':
            int curr_line = start_at.get_line();

            int offset = 0;
            if (text[1] != '\0') {
                bool ok = int.try_parse(text[1:], out offset);
                if (!ok) {
                    // Assume overflow.  Snap to offset that is (almost) guaranteed to be out of bounds to set
                    // error on input..
                    offset = int.MAX - line;
                }
            }

            line = curr_line + offset;
            break;

        default:
            bool ok = int.try_parse(text, out line);
            if (!ok) {
                line = int.MAX;
            }
            if (line != 0) {
                line -= 1;
            }
            break;
        }

        column = 0;
        if (column_text != null && column_text[0] != '\0') {
            column = int.parse(column_text);
        }

        Gtk.TextIter iter;
        this.source_buffer.get_iter_at_line_offset(out iter, line, column);
        this.start_mark_reset_blocked = true;
        this.source_buffer.place_cursor(iter);
        this.start_mark_reset_blocked = false;
        this.scroll_to_cursor();

        if (iter.get_line() != line || iter.get_line_offset() != column) {
            // TODO set error.
        } else {
            // TODO clear error.
        }
    }

    public void
    go_to_line_show() {
        Gtk.TextIter iter;

        this.go_to_line_revealer.reveal_child = true;

        var cursor = this.source_buffer.get_insert();
        this.source_buffer.get_iter_at_mark(out iter, cursor);
        this.go_to_line_entry.text = (iter.get_line() + 1).to_string();
        this.go_to_line_entry.select_region(0, -1);

        this.go_to_line_entry.grab_focus();
    }

    private void
    go_to_line_commit() {
        this.reset_start_mark();
    }

    public void
    go_to_line_hide() {
        Gtk.TextIter start_iter;

        if (!this.go_to_line_revealer.reveal_child) {
            return;
        }
        this.go_to_line_revealer.reveal_child = false;

        this.source_buffer.get_iter_at_mark(out start_iter, this.start_mark);
        this.source_buffer.place_cursor(start_iter);
        this.scroll_to_cursor();
    }

    private void
    go_to_line_init() {
        var action = new GLib.SimpleAction("show-go-to-line", null);
        action.activate.connect(this.go_to_line_show);
        this.document_actions.add_action(action);

        action = new GLib.SimpleAction("hide-go-to-line", null);
        action.activate.connect(this.go_to_line_hide);
        this.document_actions.add_action(action);

        this.go_to_line_entry.changed.connect((e) => { this.go_to_line_update(); });

        var focus_controller = new Gtk.EventControllerFocus();
        focus_controller.leave.connect((ec) => {
            this.go_to_line_commit();
            this.go_to_line_hide();
        });
        this.go_to_line_entry.add_controller(focus_controller);

        var shortcut_controller = new Gtk.ShortcutController();
        shortcut_controller.add_shortcut(new Gtk.Shortcut(
            Gtk.ShortcutTrigger.parse_string("Escape"),
            new Gtk.NamedAction("doc.hide-go-to-line")
        ));
        this.go_to_line_entry.add_controller(shortcut_controller);

        this.go_to_line_entry.activate.connect((e) => {
            this.go_to_line_commit();
            this.go_to_line_hide();
        });
    }

    /* === Lifecyle ========================================================= */

    class construct {
        set_layout_manager_type(typeof (Gtk.BinLayout));
    }

    construct {
        this.source_buffer = source_view.get_buffer() as GtkSource.Buffer;
        this.source_buffer.end_user_action.connect((tb) => {
            this.source_view.scroll_mark_onscreen(this.source_buffer.get_insert());
        });

        file_text_init();
        git_text_init();
        buffer_init();
        editing_init();
        title_init();
        language_init();
        position_init();
        font_init();
        word_wrap_init();
        indentation_init();
        trim_trailing_init();
        overview_map_init();
        right_margin_init();
        highlight_current_line_init();
        highlight_syntax_init();
        style_scheme_init();
        highlight_selected_init();
        git_text_gutter_init();
        file_text_gutter_init();
        line_numbers_init();
        start_mark_init();
        search_init();
        go_to_line_init();

        this.insert_action_group("doc", this.document_actions);
    }

    public override void
    dispose() {
        GLib.SignalHandler.disconnect_by_data(Gtk.Settings.get_for_display(this.get_display()), this);
        this.file_text_dispose();
        this.highlight_selected_dispose();
        this.git_text_dispose();
        this.dispose_template(typeof(Bedit.Document));
        while (this.get_last_child() != null) {
            this.get_last_child().unparent();
        }
        base.dispose();
    }

    ~Document() {
        assert(!this.busy);

        title_deinit();
    }

    public Document.for_file(GLib.File file) {
        Object(file: file);
    }

    public override bool
    grab_focus() {
        return this.source_view.grab_focus();
    }
}
