// Copyright (C) 2026  Alexey Kutepov <reximkut@gmail.com>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, see <https://www.gnu.org/licenses/>.
#include "task.h"
#include "ht.h"

bool tags_contains(Tags tags, String_View tag)
{
    for (size_t i = 0; i < tags.count; ++i) {
        if (sv_eq(tags.items[i], tag)) return true;
    }
    return false;
}

void append_task_md_content(String_Builder *sb, Task task)
{
    sb_appendf(sb, "# "SV_Fmt"\n", SV_Arg(task.title));
    sb_appendf(sb, "\n");
    sb_appendf(sb, "- STATUS: "SV_Fmt"\n", SV_Arg(task.status));
    sb_appendf(sb, "- PRIORITY: %d\n", task.priority);
    sb_appendf(sb, "- TAGS:");
    if (task.tags.count) {
        sb_appendf(sb, " ");
        for (size_t i = 0; i < task.tags.count; ++i) {
            if (i > 0) sb_appendf(sb, ",");
            sb_append_buf(sb, task.tags.items[i].data, task.tags.items[i].count);
        }
    }
    sb_appendf(sb, "\n");
    sb_appendf(sb, "\n");
    sb_appendf(sb, "No description.\n");
}

void print_task(const char *rel_path, Task *task)
{
    printf("%s/%s/TASK.md:1:", rel_path, task->id); // Location
    printf(" "SV_Fmt, SV_Arg(task->status));        // Status
    printf(" [PRIORITY: %3d]", task->priority);     // Priority
    if (task->tags.count) {                         // Tags
        printf(" [");
        for (size_t i = 0; i < task->tags.count; ++i) {
            if (i > 0) printf(",");
            printf(SV_Fmt, SV_Arg(task->tags.items[i]));
        }
        printf("]");
    }
    printf(" "SV_Fmt, SV_Arg(task->title));         // Title
    printf("\n");
}

bool load_tasks(Tasks *tasks, const char *dir_path)
{
    bool result = true;
    File_Paths children = {0};

    if (!read_entire_dir(dir_path, &children)) return_defer(false);
    size_t checkpoint = temp_save();
    for (size_t i = 0; i < children.count; ++i) {
        temp_rewind(checkpoint);
        const char *id = children.items[i];
        if (*id == '.') continue;
        if (!is_valid_huid(id)) continue;
        const char *task_path = temp_sprintf("%s/%s", dir_path, id);
        File_Type type = get_file_type(task_path);
        if (type < 0) return_defer(false);
        if (type != FILE_DIRECTORY) {
            nob_log(ERROR, "%s is not a directory", id);
            return_defer(false);
        }
        // This String_Builder becomes owned by Task.task_md_content.
        // So no memory deallocation is needed for it in here.
        String_Builder sb_content = {0};
        const char *task_md_path = temp_sprintf("%s/%s/TASK.md", dir_path, id);
        // TASK(20260308-171346): there should be a command that reports all the skipped weird folders and files found in the tasks/ folder
        if (!file_exists(task_md_path)) continue;
        if (!read_entire_file(task_md_path, &sb_content)) return_defer(false);
        sb_append_null(&sb_content);

        Task task = {
            .id = strdup(id),
            .task_md_content = sb_to_sv(sb_content),
        };

        properties_put(&task.properties, SVLIT("STATUS"),   SVLIT("OPEN"));   // Task is open until explicitly stated otherwise
        properties_put(&task.properties, SVLIT("PRIORITY"), SVLIT("999999")); // Unset priority is high so you don't forget to set it
        properties_put(&task.properties, SVLIT("TAGS"),     SVLIT(""));       // No tags by default

        task_md_parse(sb_content.items, &task.title, &task.properties, &task.body);
        task.status   = properties_get(&task.properties, SVLIT("STATUS"));
        task.priority = atoi(temp_sv_to_cstr(properties_get(&task.properties, SVLIT("PRIORITY"))));
        parse_tags(&task.tags, properties_get(&task.properties, SVLIT("TAGS")));

        da_append(tasks, task);
    }

defer:
    free(children.items);
    return result;
}

int task_compare_id_reverse(const void *a, const void *b)
{
    const Task *ta = a;
    const Task *tb = b;
    return strcmp(tb->id, ta->id);
}

int task_compare_priority_reverse(const void *a, const void *b)
{
    const Task *ta = a;
    const Task *tb = b;
    return tb->priority - ta->priority;
}

Task_Compare task_sorter(bool by_id)
{
    if (by_id) {
        return task_compare_id_reverse;
    } else {
        return task_compare_priority_reverse;
    }
    UNREACHABLE("task_sorter");
    return NULL;
}

bool task_matches_tags(const Task *task, const char **tags, size_t tags_count)
{
    for (size_t j = 0; j < tags_count; ++j) {
        if (!tags_contains(task->tags, sv_from_cstr(tags[j]))) {
            return false;
        }
    }
    return true;
}

void parse_tags(Tags *tags, String_View sv)
{
    while (sv.count && (isspace(*sv.data) || *sv.data == ',')) {
        sv_chop_left(&sv, 1);
    }
    while (sv.count > 0) {
        const char *start = sv.data;
        while (sv.count && !(isspace(*sv.data) || *sv.data == ',')) {
            sv_chop_left(&sv, 1);
        }
        String_View tag = sv_from_parts(start, sv.data - start);
        da_append(tags, tag);
        while (sv.count && (isspace(*sv.data) || *sv.data == ',')) {
            sv_chop_left(&sv, 1);
        }
    }
}
