// Simple tool for manipulating tasks database
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
#include "nob.h"
#include "flag.h"
#include "ht.h"

#include "path.h"
#include "huid.h"
#include "md.h"
#include "query.h"
#include "task.h"
#include "build.h"

#define DEFAULT_TASK_TITLE "New Task"
#define DEFAULT_PRIORITY 100

bool get_current_dir_path(Path *cwd_path)
{
    const char *cwd_path_cstr = get_current_dir_temp();
    if (!cwd_path_cstr) return false;
    path_parse(cwd_path, sv_from_cstr(get_current_dir_temp()));
    return true;
}

bool find_tasks_database(Path *dir_path)
{
    bool result = false;
    String_Builder sb_path = {0};

    if (!get_current_dir_path(dir_path)) return_defer(false);
    assert(dir_path->count > 0 && sv_eq(dir_path->items[0], SVLIT("")) && "CWD must be absolute");
    while (dir_path->count > 1) {
        da_append(dir_path, SVLIT("tasks"));
        const char *path = path_render_cstr(&sb_path, *dir_path);
        if (file_exists(path)) {
            File_Type type = get_file_type(path);
            if (type < 0) return_defer(false);
            if (type == FILE_DIRECTORY) return_defer(true);
        }
        dir_path->count -= 2;
    }
    nob_log(ERROR, "Could not find tasks/ folder");
    return_defer(false);
defer:
    free(sb_path.items);
    return result;
}

char *find_relative_tasks_directory(void)
{
    char *result = NULL;

    Path dir_path = {0};
    Path cwd_path = {0};
    Path rel_path = {0};
    String_Builder sb_path = {0};

    if (!find_tasks_database(&dir_path))  return_defer(NULL);
    if (!get_current_dir_path(&cwd_path)) return_defer(NULL);
    path_relative(&rel_path, cwd_path, dir_path);

    return_defer(path_render_cstr(&sb_path, rel_path));

defer:
    free(dir_path.items);
    free(cwd_path.items);
    free(rel_path.items);
    return result;
}

typedef struct Command Command;

struct Command {
    const char *name;
    const char *description;
    const char *signature;
    bool (*run)(Command *self, const char *program_name, int argc, char **argv);
};

void print_command_usage(Command *command, const char *program_name, void *c)
{
    if (command->signature) {
        fprintf(stderr, "Usage: %s %s %s\n", program_name, command->name, command->signature);
    } else {
        fprintf(stderr, "Usage: %s %s\n", program_name, command->name);
    }
    fprintf(stderr, "OPTIONS:\n");
    flag_c_print_options(c, stderr);
}

bool init_run(Command *self, const char *program_name, int argc, char **argv)
{
    bool help = false;
    void *c = flag_c_new(program_name);
    flag_c_bool_var(c, &help, "help", false, "Print this help message");

    if (!flag_c_parse(c, argc, argv)) {
        print_command_usage(self, program_name, c);
        flag_c_print_error(c, stderr);
        return false;
    }

    if (help) {
        print_command_usage(self, program_name, c);
        return true;
    }

    if (!mkdir_if_not_exists("./tasks/")) return false;

    return true;
}

bool ls_run(Command *self, const char *program_name, int argc, char **argv)
{
    bool closed = false;
    bool ascending = false;
    bool help = false;
    bool debug = false;
    bool by_id = false;
    String_Builder query_src = {0};

    void *c = flag_c_new(program_name);
    flag_c_bool_var(c, &closed, "c", false, "List closed tasks");
    flag_c_bool_var(c, &ascending, "a", false, "List tasks in ascending order");
    flag_c_bool_var(c, &by_id, "id", false, "Sort tasks by id");
    flag_c_bool_var(c, &debug, "debug", false, "Output opcodes of the query for debug purpose");
    flag_c_bool_var(c, &help, "help", false, "Print this help message");

    if (!flag_c_parse(c, argc, argv)) {
        print_command_usage(self, program_name, c);
        flag_c_print_error(c, stderr);
        return false;
    }

    argc = flag_c_rest_argc(c);
    argv = flag_c_rest_argv(c);

    while (argc > 0) {
        if (query_src.count > 0) sb_append(&query_src, ' ');
        sb_append_cstr(&query_src, shift(argv, argc));
    }

    if (help) {
        print_command_usage(self, program_name, c);
        return true;
    }

    String_View src = sv_trim(sb_to_sv(query_src));
    if (src.count == 0) src = sv_from_cstr("any");
    String_View original_src = src;
    Query query = {0};
    if (!compile_query(original_src, &src, &query)) return false;

    if (debug) {
        printf("TOKENS:\n");
        src = sv_trim(sb_to_sv(query_src));
        for (;;) {
            String_View token = chop_next_query_token(&src);
            if (token.count == 0) break;
            printf("    "SV_Fmt"\n", SV_Arg(token));
        }
        printf("\n");
        printf("OPS:\n");
        da_foreach(Op, op, &query) {
            printf("    ");
            print_op(*op);
        }
        return true;
    }

    char *dir_path = find_relative_tasks_directory();
    if (!dir_path) return false;

    Tasks tasks = {0};
    if (!load_tasks(&tasks, dir_path)) return false;
    qsort(tasks.items, tasks.count, sizeof(*tasks.items), task_sorter(by_id, ascending));

    Stack stack = {0};

    size_t tasks_matched = 0;
    da_foreach(Task, task, &tasks) {
        if (closed) {
            if (!sv_eq(task->status, SVLIT("CLOSED"))) continue;
        } else {
            if (sv_eq(task->status, SVLIT("CLOSED"))) continue;
        }
        switch (task_matches_query(original_src, task, query, &stack)) {
        case TMR_MATCHED:    break;
        case TMR_MISMATCHED: continue;
        case TMR_ERROR:      return false;
        default:             UNREACHABLE("Task_Match_Result");
        }
        print_task(dir_path, task);
        tasks_matched += 1;
    }

    if (tasks_matched == 0) {
        printf("No tasks were found\n");
        return true;
    }

    return true;
}

bool new_run(Command *self, const char *program_name, int argc, char **argv)
{
    Flag_List tags = {0};
    bool help = false;
    uint64_t priority = 0;
    char *suffix = NULL;

    void *c = flag_c_new(program_name);
    flag_c_list_var(c, &tags, "t", "Tags to add to the new task");
    flag_c_uint64_var(c, &priority, "p", DEFAULT_PRIORITY, "Priority of the new task");
    flag_c_str_var(c, &suffix, "s", NULL, "Task ID optional suffix");
    flag_c_bool_var(c, &help, "help", false, "Print this help message");
    String_Builder sb_title = {0};

    if (!flag_c_parse(c, argc, argv)) {
        print_command_usage(self, program_name, c);
        flag_c_print_error(c, stderr);
        return false;
    }

    argc = flag_c_rest_argc(c);
    argv = flag_c_rest_argv(c);

    while (argc > 0) {
        if (sb_title.count > 0) sb_append(&sb_title, ' ');
        sb_append_cstr(&sb_title, shift(argv, argc));
    }

    if (help) {
        print_command_usage(self, program_name, c);
        return true;
    }

    char *dir_path = find_relative_tasks_directory();
    if (!dir_path) return false;

    char *id = temp_new_huid(suffix);
    const char *task_path = temp_sprintf("%s/%s", dir_path, id);
    int exists = file_exists(task_path);
    if (exists < 0) return false;
    if (exists) {
        nob_log(ERROR, "%s already exists. You are probably creating tasks too fast, or your time is broken", id);
        return false;
    }
    if (!mkdir_if_not_exists(task_path)) return false;
    String_View title = SVLIT(DEFAULT_TASK_TITLE);
    if (sb_title.count > 0) {
        title = sb_to_sv(sb_title);
    }

    Task task = {
        .id       = id,
        .title    = title,
        .status   = SVLIT("OPEN"),
        .priority = priority,
    };

    da_foreach(const char *, tag, &tags) {
        parse_tags(&task.tags, sv_from_cstr(*tag));
    }

    String_Builder sb_md_content = {0};

    append_task_md_content(&sb_md_content, task);
    const char *task_md_path = temp_sprintf("%s/%s/TASK.md", dir_path, id);
    if (!write_entire_file(task_md_path, sb_md_content.items, sb_md_content.count)) return false;

    print_task(dir_path, &task);
    return true;
}

bool find_run(Command *self, const char *program_name, int argc, char **argv)
{
    bool help = false;
    bool path_only = false;

    void *c = flag_c_new(program_name);
    flag_c_bool_var(c, &path_only, "path-only", false, "Print only path to TASK.md");
    flag_c_bool_var(c, &help, "help", false, "Print this help message");

    const char *huid = NULL;

    while (argc > 0) {
        if (!flag_c_parse(c, argc, argv)) {
            print_command_usage(self, program_name, c);
            flag_c_print_error(c, stderr);
            return false;
        }

        argc = flag_c_rest_argc(c);
        argv = flag_c_rest_argv(c);

        if (argc > 0) {
            if (huid != NULL) {
                print_command_usage(self, program_name, c);
                nob_log(ERROR, "Several HUIDs is not supported");
                return false;
            }

            huid = shift(argv, argc);
            if (!is_valid_huid(huid)) {
                print_command_usage(self, program_name, c);
                nob_log(ERROR, "`%s` is not a valid HUID. Valid HUID matches regexp %s", huid, HUID_REGEXP_FOR_USER_REPORT_PURPOSES);
                return false;
            }
        }
    }

    if (help) {
        print_command_usage(self, program_name, c);
        return true;
    }

    if (huid == NULL) {
        print_command_usage(self, program_name, c);
        nob_log(ERROR, "No HUID was provided");
        return false;
    }

    char *dir_path = find_relative_tasks_directory();
    if (!dir_path) return false;

    Tasks tasks = {0};
    if (!load_tasks(&tasks, dir_path)) return false;

    bool found = false;
    da_foreach(Task, task, &tasks) {
        if (strcmp(task->id, huid) == 0) {
            if (path_only) {
                printf("%s/%s/TASK.md\n", dir_path, task->id);
            } else {
                print_task(dir_path, task);
            }
            found = true;
        }
    }
    if (!found) {
        nob_log(ERROR, "No task with with HUID `%s` was found", huid);
        return false;
    }
    return true;
}

bool ref_run(Command *self, const char *program_name, int argc, char **argv)
{
    Path dir_path = {0};
    Path cwd_path = {0};
    Path rel_path = {0};
    String_Builder sb_path = {0};
    Cmd cmd = {0};

    bool help = false;

    void *c = flag_c_new(program_name);
    flag_c_bool_var(c, &help, "help", false, "Print this help message");

    if (!flag_c_parse(c, argc, argv)) {
        print_command_usage(self, program_name, c);
        flag_c_print_error(c, stderr);
        return false;
    }

    const char *huid = NULL;

    while (argc > 0) {
        if (!flag_c_parse(c, argc, argv)) {
            print_command_usage(self, program_name, c);
            flag_c_print_error(c, stderr);
            return false;
        }

        argc = flag_c_rest_argc(c);
        argv = flag_c_rest_argv(c);

        if (argc > 0) {
            if (huid != NULL) {
                print_command_usage(self, program_name, c);
                nob_log(ERROR, "Several HUIDs is not supported");
                return false;
            }

            huid = shift(argv, argc);
            if (!is_valid_huid(huid)) {
                print_command_usage(self, program_name, c);
                nob_log(ERROR, "`%s` is not a valid HUID. Valid HUID matches regexp %s", huid, HUID_REGEXP_FOR_USER_REPORT_PURPOSES);
                return false;
            }
        }
    }

    if (help) {
        print_command_usage(self, program_name, c);
        return true;
    }

    if (!find_tasks_database(&dir_path))  return false;
    if (!get_current_dir_path(&cwd_path)) return false;
    path_relative(&rel_path, cwd_path, dir_path);

    if (huid == NULL) {
        if (!(rel_path.count == 1 && sv_eq(da_last(&rel_path), SVLIT("..")))) {
            nob_log(ERROR, "You are not inside any task folder. Pass the ID of a task as an argument to search for referers of that task.");
            return false;
        }
        huid = temp_sv_to_cstr(da_last(&cwd_path));
    }

    da_append(&rel_path, SVLIT(".."));

    // TASK(20260901-051338): Make the grep command configurable for `tatr-ref`
    cmd_append(&cmd, "grep");
    cmd_append(&cmd, "--exclude-dir=.git");
    cmd_append(&cmd, "-Irn");
    cmd_append(&cmd, huid);
    cmd_append(&cmd, path_render_cstr(&sb_path, rel_path));
    if (!cmd_run(&cmd)) return false;

    return true;
}

bool graph_run(Command *self, const char *program_name, int argc, char **argv)
{
    bool help = false;
    void *c = flag_c_new(program_name);
    flag_c_bool_var(c, &help, "help", false, "Print this help message");

    if (!flag_c_parse(c, argc, argv)) {
        print_command_usage(self, program_name, c);
        flag_c_print_error(c, stderr);
        return false;
    }

    if (help) {
        print_command_usage(self, program_name, c);
        return true;
    }

    char *dir_path = find_relative_tasks_directory();
    if (!dir_path) return false;

    Tasks tasks = {0};
    if (!load_tasks(&tasks, dir_path)) return false;

    typedef Ht(String_View, bool) HUIDs;
    Ht(const char *, HUIDs) graph = {
        .hasheq = ht_cstr_hasheq,
        .default_value = {
            .hasheq = ht_sv_hasheq,
        }
    };
    size_t checkpoint = temp_save();
    da_foreach(Task, task, &tasks) {
        temp_rewind(checkpoint);
        HUIDs *ref = ht_put(&graph, task->id);
        String_View content = task->task_md_content;
        while (content.count > 0) {
            String_View huid = {0};
            if (chop_huid(&content, &huid)) {
                if (file_exists(temp_sprintf("%s/"SV_Fmt"/TASK.md", dir_path, SV_Arg(huid)))) {
                    *ht_put(ref, huid) = true;
                }
            } else {
                sv_chop_left(&content, 1);
            }
        }
    }

    String_Builder sb = {0};

    sb_appendf(&sb, "digraph {\n");
    ht_foreach(ref, &graph) {
        const char *orig = ht_key(&graph, ref);
        if (ref->count == 0) continue;
        ht_foreach(huid, ref) {
            String_View nbor = ht_key(ref, huid);
            sb_appendf(&sb, "    \"%s\" -> \""SV_Fmt"\";\n", orig, SV_Arg(nbor));
        }
    }
    sb_appendf(&sb, "}\n");

    const char *format = "svg";
    const char *dot_path = "graph.dot";
    const char *out_path = temp_sprintf("graph.%s", format);
    if (!write_entire_file(dot_path, sb.items, sb.count)) return false;
    nob_log(INFO, "Generated %s", dot_path);

    Cmd cmd = {0};
    // cmd_append(&cmd, "dot");
    cmd_append(&cmd, "neato");
    // cmd_append(&cmd, "twopi");
    cmd_append(&cmd, "-Goverlap=scale");
    cmd_append(&cmd, temp_sprintf("-T%s", format));
    cmd_append(&cmd, dot_path);
    cmd_append(&cmd, temp_sprintf("-o%s", out_path));
    if (!cmd_run(&cmd)) return false;

    return true;
}

typedef struct {
    String_View tag;
    size_t count;
} Tag_Count;

int tag_count_compare_by_count_desc(const void *a, const void *b)
{
    const Tag_Count *tca = a;
    const Tag_Count *tcb = b;
    if (tca->count < tcb->count) return 1;
    if (tca->count > tcb->count) return -1;
    return 0;
}

bool summary_run(Command *self, const char *program_name, int argc, char **argv)
{
    bool closed = false;
    bool help = false;
    void *c = flag_c_new(program_name);
    flag_c_bool_var(c, &closed, "c", false, "List closed tasks");
    flag_c_bool_var(c, &help, "help", false, "Print this help message");

    if (!flag_c_parse(c, argc, argv)) {
        print_command_usage(self, program_name, c);
        flag_c_print_error(c, stderr);
        return false;
    }

    if (help) {
        print_command_usage(self, program_name, c);
        return true;
    }

    char *dir_path = find_relative_tasks_directory();
    if (!dir_path) return false;

    Tasks tasks = {0};
    if (!load_tasks(&tasks, dir_path)) return false;

    Ht(String_View, String_View) tags_desc = {
        .hasheq = ht_sv_hasheq,
    };
    const char *tags_desc_path = temp_sprintf("%s/tags", dir_path);
    if (file_exists(tags_desc_path)) {
        String_Builder sb = {0};
        if (!read_entire_file(tags_desc_path, &sb)) return false;
        String_View sv = sb_to_sv(sb);
        for (size_t line_number = 0; sv.count > 0; ++line_number) {
            String_View line = sv_trim(sv_chop_by_delim(&sv, '\n'));
            if (line.count == 0) continue;
            const char *start = line.data;
            while (line.count && !(isspace(*line.data) || *line.data == ',')) {
                sv_chop_left(&line, 1);
            }
            String_View tag  = sv_from_parts(start, line.data - start);
            while (line.count && (isspace(*line.data) || *line.data == ',')) {
                sv_chop_left(&line, 1);
            }
            String_View desc = sv_trim_right(line);
            String_View *slot = ht_find(&tags_desc, tag);
            if (slot) {
                fprintf(stderr, "%s:%zu: WARNING: redefinition of tag description '"SV_Fmt"'\n", tags_desc_path, line_number, SV_Arg(tag));
            } else {
                slot = ht_put(&tags_desc, tag);
            }
            *slot = desc;
        }
    }

    size_t total_count = 0;
    size_t untagged_count = 0;
    Ht(String_View, size_t) tags_count = {
        .hasheq = ht_sv_hasheq,
    };
    for (size_t i = 0; i < tasks.count; ++i) {
        Task *task = &tasks.items[i];
        bool task_is_closed = sv_eq(task->status, SVLIT("CLOSED"));
        if (closed == task_is_closed) {
            total_count += 1;
            for (size_t j = 0; j < task->tags.count; ++j) {
                *ht_find_or_put(&tags_count, task->tags.items[j]) += 1;
            }
            if (task->tags.count == 0) {
                untagged_count += 1;
            }
        }
    }

    struct {
        Tag_Count *items;
        size_t count;
        size_t capacity;
    } sorted_tags_count = {0};

    size_t max_width = 0;
    ht_foreach(value, &tags_count) {
        String_View key = ht_key(&tags_count, value);
        if (max_width < key.count) {
            max_width = key.count;
        }
        da_append(&sorted_tags_count, ((Tag_Count) {
            .tag = key,
            .count = *value,
        }));
    }

    qsort(sorted_tags_count.items, sorted_tags_count.count, sizeof(*sorted_tags_count.items), tag_count_compare_by_count_desc);

    const char *status = closed ? "CLOSED" : "OPEN";
    printf("STATUS:   %s\n",  status);
    printf("TOTAL:    %zu\n", total_count);
    if (untagged_count > 0) {
        printf("UNTAGGED: %zu\n", untagged_count);
    }
    if (sorted_tags_count.count > 0) {
        printf("TAGGED:\n");
        size_t mark = temp_save();
        da_foreach(Tag_Count, tag_count, &sorted_tags_count) {
            temp_rewind(mark);
            String_View *tag_desc = ht_find(&tags_desc, tag_count->tag);
            if (tag_desc) {
                printf("    %*s => %3zu - "SV_Fmt"\n", (int)max_width, temp_sv_to_cstr(tag_count->tag), tag_count->count, SV_Arg(*tag_desc));
            } else {
                printf("    %*s => %3zu\n", (int)max_width, temp_sv_to_cstr(tag_count->tag), tag_count->count);
            }
        }
    }

    return true;
}

bool help_run(Command *self, const char *program_name, int argc, char **argv);

bool version_run(Command *self, const char *program_name, int argc, char **argv)
{
    UNUSED(self);
    UNUSED(program_name);
    UNUSED(argc);
    UNUSED(argv);
    printf("tatr - Task Tracker\n");
    printf("Built at %s\n", BUILD_TIME);
    printf("GIT HASH: "GIT_HASH"\n");
    return true;
}

Command commands[] = {
    {
        .name = "init",
        .description = "Create tasks/ directory in the current working directory if it doesn't exist yet",
        .run = init_run,
    },
    {
        .name = "ls",
        .signature = "[OPTIONS] [QUERY...]",
        .description = "List the tasks",
        .run = ls_run,
    },
    {
        .name = "new",
        .signature = "[OPTIONS] [TITLE...]",
        .description = "Create a new task",
        .run = new_run,
    },
    {
        .name = "summary",
        .signature = "[OPTIONS]",
        .description = "Print the summary of the tasks",
        .run = summary_run,
    },
    {
        .name = "find",
        .signature = "<HUID> [OPTIONS]",
        .description = "Find the task with a given HUID",
        .run = find_run,
    },
    {
        .name = "ref",
        .signature = "[HUID]",
        .description = "Finds referers of the task. Basically greps the Task ID over entirety of the repo. If ran inside of a task folder automatically picks up the ID of that task.",
        .run = ref_run,
    },
    {
        .name = "graph",
        .description = "Generate graph of tasks cross-referring to each other. This command is largely useless right now.",
        .run = graph_run,
    },
    {
        .name = "help",
        .signature = "[OPTIONS]",
        .description = "Print this help message",
        .run = help_run,
    },
    {
        .name = "version",
        .description = "Print the current version and date of the build",
        .run =  version_run,
    },
};

void print_available_commands(Log_Level log_level)
{
    nob_log(log_level, "Available commands:");
    int max_width = 0;
    for (size_t i = 0; i < ARRAY_LEN(commands); ++i) {
        int width = strlen(commands[i].name);
        if (width > max_width) max_width = width;
    }
    for (size_t i = 0; i < ARRAY_LEN(commands); ++i) {
        nob_log(log_level, "    %-*s - %s", max_width, commands[i].name, commands[i].description);
    }
}

bool help_run(Command *self, const char *program_name, int argc, char **argv)
{
    UNUSED(self);
    UNUSED(program_name);
    UNUSED(argc);
    UNUSED(argv);
    print_available_commands(INFO);
    return true;
}

int main(int argc, char **argv)
{
#ifndef TASKS_TEST
    const char *program_name = shift(argv, argc);

    if (argc <= 0) {
        print_available_commands(ERROR);
        nob_log(ERROR, "No command is provided");
        return 0;
    }

    const char *command_name = shift(argv, argc);

    for (size_t i = 0; i < ARRAY_LEN(commands); ++i) {
        if (strcmp(command_name, commands[i].name) == 0) {
            if (!commands[i].run(&commands[i], program_name, argc, argv)) return 1;
            return 0;
        }
    }

    print_available_commands(ERROR);
    nob_log(ERROR, "Unknown command `%s`", command_name);
    return 1;
#else // TASKS_TEST
    UNUSED(argc);
    UNUSED(argv);
    if (!test_path_normalize())        return 1;
    if (!test_path_parse_and_render()) return 1;
    if (!test_path_relative())         return 1;
    return 0;
#endif // TASKS_TEST
}

#include "path.c"
#include "huid.c"
#include "md.c"
#include "query.c"
#include "task.c"

#define NOB_IMPLEMENTATION
#define NOB_OVERWRITE_TEMP_ON_REWIND
#include "nob.h"
#define FLAG_IMPLEMENTATION
#define FLAG_PUSH_DASH_DASH_BACK
#include "flag.h"
#define HT_IMPLEMENTATION
#include "ht.h"
