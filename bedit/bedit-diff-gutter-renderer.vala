/* Copyright 2026 Ben Mather <bwhmather@bwhmather.com>
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

private enum Bedit.DiffMark {
    NONE,
    ADDED,
    CHANGED,
    REMOVED,
}

private bool
get_foreground_color(GtkSource.StyleScheme scheme, string id, ref Gdk.RGBA color) {
    var style = scheme.get_style(id);
    if (style == null) {
        return false;
    }
    if (!style.foreground_set) {
        return false;
    }
    return color.parse(style.foreground);
}

public sealed class Bedit.DiffGutterRenderer : GtkSource.GutterRenderer {

    public GLib.Bytes? reference { get; set; }

    private DiffMark[] marks = {};
    private bool has_baseline = false;

    /* --- Diff computation ------------------------------------------------- */

    private GLib.Cancellable? cancellable = null;
    private uint timeout_id = 0;
    private bool running = false;

    private async void
    do_update() {
        var cancel = this.cancellable;

        var buffer = this.get_buffer();
        var reference = this.reference;
        if (buffer == null || reference == null) {
            this.marks = {};
            this.has_baseline = false;
            this.queue_draw();
            return;
        }

        this.running = true;

        unowned uint8[] ref_bytes = reference.get_data();

        Gtk.TextIter start, end;
        buffer.get_bounds(out start, out end);
        var text = buffer.get_text(start, end, true);
        var buf_bytes = text.data;

        DiffMark[] new_marks = new DiffMark[buffer.get_line_count()];

        SourceFunc resume = do_update.callback;
        new Thread<void>("bedit-diff", () => {
            Bedit.line_diff(ref_bytes, buf_bytes, (old_start, old_count, new_start, new_count) => {
                DiffMark mark;
                int first, last;

                if (old_count == 0) {
                    mark = ADDED;
                    first = new_start;
                    last = new_start + new_count;
                } else if (new_count == 0) {
                    mark = REMOVED;
                    first = int.max(0, new_start - 1);
                    last = first + 1;
                } else {
                    mark = CHANGED;
                    first = new_start;
                    last = new_start + new_count;
                }

                for (int i = first; i < last; i++) {
                    new_marks[i] = mark;
                }
            });

            Idle.add((owned) resume);
        });
        yield;

        this.running = false;

        if (cancel.is_cancelled()) {
            /* A new update was requested while the thread was running.  If the
             * timeout already fired and skipped the launch (because we were
             * running), restart now so the result doesn't get lost. */
            if (this.timeout_id == 0) {
                Idle.add(() => {
                    this.do_update.begin();
                    return GLib.Source.REMOVE;
                });
            }
            return;
        }

        this.marks = new_marks;
        this.has_baseline = true;
        this.queue_draw();
    }

    private void
    schedule_update() {
        this.cancellable.cancel();
        this.cancellable = new GLib.Cancellable();

        if (this.timeout_id != 0) {
            GLib.Source.remove(this.timeout_id);
        }
        this.timeout_id = GLib.Timeout.add(300, () => {
            this.timeout_id = 0;
            if (!this.running) {
                this.do_update.begin();
            }
            return GLib.Source.REMOVE;
        });
    }

    /* --- Incremental mark updates ----------------------------------------- */

    private void
    on_insert_text(Gtk.TextIter pos, string text, int len) {
        if (!this.has_baseline) {
            return;
        }

        int line = pos.get_line();
        bool at_start = pos.starts_line();
        bool at_end = pos.ends_line();

        int added = 0;
        for (int i = 0; i < len; i++) {
            if (text[i] == '\n') {
                added++;
            }
        }

        // Make room.
        int old_len = this.marks.length;
        this.marks.resize(old_len + added);
        for (int i = old_len - 1; i >= line + 1; i--) {
            this.marks[i + added] = this.marks[i];
        }

        DiffMark mark_line = this.marks[line];

        // Set first line mark.
        if (at_start) {
            this.marks[line] = ADDED;
        } else if (at_end && text[0] == '\n') {
            this.marks[line] = mark_line;
        } else if (mark_line == ADDED) {
            this.marks[line] = ADDED;
        } else {
            this.marks[line] = CHANGED;
        }

        // Set last line mark.
        if (at_end && added > 0) {
            this.marks[line + added] = ADDED;
        } else if (text[len - 1] == '\n' && at_start) {
            this.marks[line + added] = mark_line;
        } else if (mark_line == ADDED) {
            this.marks[line + added] = ADDED;
        } else {
            this.marks[line + added] = CHANGED;
        }

        // Fill gap.
        DiffMark gap_mark = ADDED;
        if (marks[line] == CHANGED || marks[line + added] == CHANGED) {
            gap_mark = CHANGED;
        }
        for (int i = 1; i < added; i++) {
            this.marks[line + i] = gap_mark;
        }

        this.queue_draw();
    }

    private void
    on_delete_range(Gtk.TextIter start, Gtk.TextIter end) {
        if (!this.has_baseline) {
            return;
        }

        int line = start.get_line();
        int removed = end.get_line() - line;

        DiffMark mark_line = this.marks[line];
        DiffMark mark_end = this.marks[line + removed];

        // Collapse removed lines.
        int old_len = this.marks.length;
        int new_len = old_len - removed;
        for (int i = line + 1; i < new_len; i++) {
            this.marks[i] = this.marks[i + removed];
        }
        this.marks.resize(new_len);

        // Set merged line mark.
        if (start.starts_line() && end.starts_line()) {
            this.marks[line] = mark_end;
            int prev = int.max(0, line - 1);
            if (this.marks[prev] == NONE) {
                this.marks[prev] = REMOVED;
            }
        } else if (mark_line == ADDED && mark_end == ADDED) {
            this.marks[line] = ADDED;
        } else {
            this.marks[line] = CHANGED;
        }

        this.queue_draw();
    }

    /* --- Buffer tracking -------------------------------------------------- */

    public override void
    change_buffer(GtkSource.Buffer? old_buffer) {
        if (old_buffer != null) {
            GLib.SignalHandler.disconnect_by_data(old_buffer, this);
        }
        this.has_baseline = false;
        var buf = this.get_buffer();
        if (buf != null) {
            this.marks = new DiffMark[buf.get_line_count()];
            buf.insert_text.connect(this.on_insert_text);
            buf.delete_range.connect(this.on_delete_range);
            buf.changed.connect(this.schedule_update);
            buf.notify["style-scheme"].connect(this.update_style);
            this.schedule_update();
            this.update_style();
            this.queue_draw();
        } else {
            this.marks = {};
            this.queue_draw();
        }
    }

    /* --- Rendering -------------------------------------------------------- */

    Gdk.RGBA color_added;
    Gdk.RGBA color_changed;
    Gdk.RGBA color_removed;

    private void
    update_style() {
        Gdk.RGBA c = {};

        var buffer = this.get_buffer();
        if (buffer == null) {
            return;
        }
        var scheme = buffer.get_style_scheme();
        if (scheme == null) {
            return;
        }

        this.color_added = { 0.35f, 0.74f, 0.31f, 1.0f };
        foreach (var id in new string[] { "diff:added-line", "green_4", "green" }) {
            if (!get_foreground_color(scheme, id, ref c)) {
                continue;
            }
            if (c.green > c.red && c.green > c.blue) {
                this.color_added = c;
                break;
            }
        }

        this.color_changed = { 0.87f, 0.73f, 0.26f, 1.0f };
        foreach (var id in new string[] { "diff:changed-line", "yellow_5", "orange" }) {
            if (!get_foreground_color(scheme, id, ref c)) {
                continue;
            }
            if (c.red > c.blue && c.green > c.blue) {
                this.color_changed = c;
                break;
            }
        }

        this.color_removed = { 0.88f, 0.20f, 0.20f, 1.0f };
        foreach (var id in new string[] { "diff:removed-line", "red_3", "red" }) {
            if (!get_foreground_color(scheme, id, ref c)) {
                continue;
            }
            if (c.red > c.green && c.red > c.blue) {
                this.color_removed = c;
                break;
            }
        }

        this.queue_draw();
    }

    public override void
    snapshot_line(Gtk.Snapshot snapshot, GtkSource.GutterLines lines, uint line) {
        if (!this.has_baseline) {
            return;
        }

        DiffMark mark = this.marks[line];
        if (mark == NONE) {
            return;
        }


        int width = this.get_width();
        int y, height;
        lines.get_line_yrange(line, CELL, out y, out height);

        if (mark == ADDED) {
            var rect = Graphene.Rect();
            rect.init(2.0f, (float) y, (float) width - 2, (float) height);
            snapshot.append_color(this.color_added, rect);
            return;
         }

        if (mark == CHANGED) {
            var rect = Graphene.Rect();
            rect.init(2.0f, (float) y, (float) width - 2, (float) height);
            snapshot.append_color(this.color_changed, rect);
            return;
        }

        if (mark == REMOVED) {
            var rect = Graphene.Rect();
            rect.init(1.0f, (float) (y + height - 2), (float) width - 1, 2.0f);
            snapshot.append_color(this.color_removed, rect);
            return;
        }
    }

    /* === Lifecycle ======================================================== */

    construct {
        this.width_request = 5;
        this.cancellable = new GLib.Cancellable();
        this.notify["reference"].connect(this.schedule_update);
    }

    public override void
    dispose() {
        if (this.timeout_id != 0) {
            GLib.Source.remove(this.timeout_id);
            this.timeout_id = 0;
        }
        if (this.cancellable != null) {
            this.cancellable.cancel();
            this.cancellable = null;
        }
        base.dispose();
    }
}
