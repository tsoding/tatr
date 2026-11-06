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
#include "update.h"
#include "task.h"
#include "build.h"

#include <inttypes.h>

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
    fprintf(stderr, "ERROR: Could not find tasks/ folder\n");
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
    const char *short_description;
    const char *extra_description;
    const char *signature;
    bool deprecated;
    bool (*run)(Command *self, const char *program_name, int argc, char **argv);
};

void print_command_usage(FILE *sink, Command *command, const char *program_name, void *c)
{
    fprintf(sink, "Usage:\n");
    fprintf(sink, "    %s %s", program_name, command->name);
    if (command->signature) {
        fprintf(sink, " %s", command->signature);
    }
    fprintf(sink, "\n");
    fprintf(sink, "\n");

    if (command->short_description) {
        fprintf(sink, "Description:\n");
        fprintf(sink, "    %s", command->short_description);
        if (command->extra_description) {
            fprintf(sink, " %s", command->extra_description);
        }
        fprintf(sink, "\n");
        fprintf(sink, "\n");
    }

    if (c) {
        fprintf(sink, "OPTIONS:\n");
        flag_c_print_options(c, sink);
        fprintf(sink, "\n");
    }
}

bool init_run(Command *self, const char *program_name, int argc, char **argv)
{
    bool help = false;
    bool no_readme = false;
    void *c = flag_c_new(program_name);
    flag_c_bool_var(c, &help, "help", false, "Print this help message");
#ifdef TASKS_README_MD
    flag_c_bool_var(c, &no_readme, "no-readme", false, "Do not create default README.md in the ./tasks/ folder");
#else
    flag_c_bool_var(c, &no_readme, "no-readme", false, "Doesn't do anything");
#endif // TASKS_README_MD

    if (!flag_c_parse(c, argc, argv)) {
        print_command_usage(stderr, self, program_name, c);
        flag_c_print_error(c, stderr);
        return false;
    }

    if (help) {
        print_command_usage(stdout, self, program_name, c);
        return true;
    }

    const char *tasks_dir = "./tasks/";
    if (!file_exists(tasks_dir)) {
        if (!mkdir_if_not_exists(tasks_dir)) return false;
    }

#ifdef TASKS_README_MD
    const char *tasks_readme_md_file = "./tasks/README.md";
    if (!file_exists(tasks_readme_md_file) && !no_readme) {
        if (!write_entire_file(tasks_readme_md_file, TASKS_README_MD, ARRAY_LEN(TASKS_README_MD))) return false;
        printf("Created %s\n", tasks_readme_md_file);
    }
#endif // TASKS_README_MD

    return true;
}

bool tag_run(Command *self, const char *program_name, int argc, char **argv)
{
    String_Builder query_src = {0};
    Flag_List tags_to_add = {0};
    bool help = false;
    bool closed = false;

    void *c = flag_c_new(program_name);
    flag_c_list_var(c, &tags_to_add, "t", "Tags to add to the tasks");
    flag_c_bool_var(c, &closed, "c", false, "Process closed tasks");
    flag_c_bool_var(c, &help, "help", false, "Print this help message");

    if (!flag_c_parse(c, argc, argv)) {
        print_command_usage(stderr, self, program_name, c);
        flag_c_print_error(c, stderr);
        return false;
    }

    argc = flag_c_rest_argc(c);
    argv = flag_c_rest_argv(c);

    if (help) {
        print_command_usage(stdout, self, program_name, c);
        return true;
    }

    if (tags_to_add.count == 0) {
        fprintf(stderr, "ERROR: no tags are provided\n");
        return false;
    }

    if (argc <= 0) {
        fprintf(stderr, "ERROR: no query is provided\n");
        return false;
    }

    while (argc > 0) {
        if (query_src.count > 0) sb_append(&query_src, ' ');
        sb_append_cstr(&query_src, shift(argv, argc));
    }

    String_Builder alternative_command = {0};
    sb_append_cstr(&alternative_command, program_name);
    sb_append_cstr(&alternative_command, " ls -update ");
    da_foreach(const char *, tag, &tags_to_add) {
        if (tag != tags_to_add.items) {
            sb_append(&alternative_command, ',');
        }
        sb_appendf(&alternative_command, "+%s", *tag);
    }
    sb_append_cstr(&alternative_command, " ");
    sb_append_sb(&alternative_command, query_src);
    sb_append_null(&alternative_command);
    fprintf(stderr, "ERROR: This command is deprecated! Please use `%s` instead!\n", alternative_command.items);
    return false;
}

bool untag_run(Command *self, const char *program_name, int argc, char **argv)
{
    String_Builder query_src = {0};
    Flag_List tags_to_remove = {0};
    bool help = false;
    bool closed = false;

    void *c = flag_c_new(program_name);
    flag_c_list_var(c, &tags_to_remove, "t", "Tags to remove from the tasks");
    flag_c_bool_var(c, &closed, "c", false, "Process closed tasks");
    flag_c_bool_var(c, &help, "help", false, "Print this help message");

    if (!flag_c_parse(c, argc, argv)) {
        print_command_usage(stderr, self, program_name, c);
        flag_c_print_error(c, stderr);
        return false;
    }

    argc = flag_c_rest_argc(c);
    argv = flag_c_rest_argv(c);

    if (help) {
        print_command_usage(stdout, self, program_name, c);
        return true;
    }

    if (argc <= 0) {
        fprintf(stderr, "ERROR: no query is provided\n");
        return false;
    }

    if (tags_to_remove.count == 0) {
        fprintf(stderr, "ERROR: no tags are provided\n");
        return false;
    }

    while (argc > 0) {
        if (query_src.count > 0) sb_append(&query_src, ' ');
        sb_append_cstr(&query_src, shift(argv, argc));
    }

    String_Builder alternative_command = {0};
    sb_append_cstr(&alternative_command, program_name);
    sb_append_cstr(&alternative_command, " ls -update ");
    da_foreach(const char *, tag, &tags_to_remove) {
        if (tag != tags_to_remove.items) {
            sb_append(&alternative_command, ',');
        }
        sb_appendf(&alternative_command, "-%s", *tag);
    }
    sb_append_cstr(&alternative_command, " ");
    sb_append_sb(&alternative_command, query_src);
    sb_append_null(&alternative_command);
    fprintf(stderr, "ERROR: This command is deprecated! Please use `%s` instead!\n", alternative_command.items);
    return false;
}

bool ls_run(Command *self, const char *program_name, int argc, char **argv)
{
    bool closed = false;
    bool ascending = false;
    bool help = false;
    bool debug = false;
    bool by_id = false;
    Flag_List updates_from_flags = {0};

    void *c = flag_c_new(program_name);
    flag_c_bool_var(c, &closed, "c", false, "List closed tasks");
    flag_c_bool_var(c, &ascending, "a", false, "List tasks in ascending order");
    flag_c_bool_var(c, &by_id, "id", false, "Sort tasks by id");
    flag_c_list_var(c, &updates_from_flags, "update", "Apply update operations on the resulting list");
    flag_c_bool_var(c, &debug, "debug", false, "Output opcodes of the query for debug purpose");
    flag_c_bool_var(c, &help, "help", false, "Print this help message");

    if (!flag_c_parse(c, argc, argv)) {
        print_command_usage(stderr, self, program_name, c);
        flag_c_print_error(c, stderr);
        return false;
    }

    argc = flag_c_rest_argc(c);
    argv = flag_c_rest_argv(c);

    if (help) {
        print_command_usage(stdout, self, program_name, c);
        return true;
    }

    bool query_is_empty = false;

    // Compiling query
    String_Builder sb_query_src = {0};
    while (argc > 0) {
        if (sb_query_src.count > 0) sb_append(&sb_query_src, ' ');
        sb_append_cstr(&sb_query_src, shift(argv, argc));
    }
    String_View query_src = sv_trim(sb_to_sv(sb_query_src));
    if (query_src.count == 0) {
        query_src = sv_from_cstr("any");
        query_is_empty = true;
    }
    String_View original_query_src = query_src;
    Query query = {0};
    if (!compile_query(original_query_src, &query_src, &query)) return false;

    // Compiling update
    String_Builder sb_updates_src = {0};
    da_foreach(const char *, updates_cstr, &updates_from_flags) {
        sb_append_cstr(&sb_updates_src, *updates_cstr);
        sb_append(&sb_updates_src, ' ');
    }
    String_View update_src = sv_trim(sb_to_sv(sb_updates_src));
    String_View original_update_src = update_src;
    Update update = {0};
    if (!compile_update(original_update_src, &update_src, &update)) return false;

    // TASK(20260910-181239): `tatr ls -debug` should be a separate command
    if (debug) {
        printf("QUERY TOKENS:\n");
        query_src = sv_trim(sb_to_sv(sb_query_src));
        String_View token = {0};
        while (chop_next_query_token(&query_src, &token)) {
            printf("    "SV_Fmt"\n", SV_Arg(token));
        }
        printf("\n");
        printf("QUERY OPS:\n");
        da_foreach(QOp, op, &query) {
            printf("    ");
            print_qop(*op);
        }
        if (update.count) {
            printf("\n");
            printf("UPDATE TOKENS:\n");
            query_src = sv_trim(sb_to_sv(sb_updates_src));
            String_View token = {0};
            while (chop_next_update_token(&query_src, &token)) {
                printf("    "SV_Fmt"\n", SV_Arg(token));
            }
            printf("\n");
            printf("UPDATE OPS:\n");
            da_foreach(UOp, op, &update) {
                printf("    ");
                print_uop(*op);
            }
        }
        return true;
    }

    if (update.count > 0 && query_is_empty) {
        fprintf(stderr, "ERROR: -%s with empty query is not allowed!\n", flag_c_name(c, &updates_from_flags));
        fprintf(stderr, "NOTE: if you do want to update all the tasks use \"any\" query\n");
        return false;
    }

    char *dir_path = find_relative_tasks_directory();
    if (!dir_path) return false;

    Tasks tasks = {0};
    if (!load_tasks(&tasks, dir_path)) return false;
    qsort(tasks.items, tasks.count, sizeof(*tasks.items), task_sorter(by_id));
    if (ascending) {
        for (size_t i = 0; i < tasks.count/2; ++i) {
            size_t j = tasks.count - 1 - i;
            Task t = tasks.items[i];
            tasks.items[i] = tasks.items[j];
            tasks.items[j] = t;
        }
    }

    Stack stack = {0};

    size_t tasks_matched = 0;
    size_t tasks_updated = 0;
    da_foreach(Task, task, &tasks) {
        String_View status = task->status;
        if (closed) {
            if (!sv_eq(status, SVLIT("CLOSED"))) continue;
        } else {
            if (sv_eq(status, SVLIT("CLOSED"))) continue;
        }
        switch (task_matches_query(original_query_src, *task, query, &stack)) {
        case QUERY_MATCHED:    break;
        case QUERY_MISMATCHED: continue;
        case QUERY_ERROR:      return false;
        default:             UNREACHABLE("Task_Match_Result");
        }

        if (update_task(task, update)) {
            String_View update_src = sv_trim(sb_to_sv(sb_updates_src));
            String_Builder task_md_content = {0};
            render_task_md(*task, &task_md_content, NULL);
            const char *task_md_path = temp_sprintf("%s/%s/TASK.md", dir_path, task->id);
            if (!write_entire_file(task_md_path, task_md_content.items, task_md_content.count)) return false;
            print_task_report(dir_path, task, update_src);
            tasks_updated += 1;
        } else {
            print_task_report(dir_path, task, SVNULL);
        }

        tasks_matched += 1;
    }

    if (tasks_matched == 0) {
        printf("No tasks were found\n");
        return true;
    }

    if (tasks_updated > 0) {
        printf("%zu tasks where updated\n", tasks_updated);
        return true;
    }

    return true;
}

char *create_new_task(const char *dir_path, const char *id_suffix, String_View title, int priority, Parsed_Tags parsed_tags)
{
    char *id = temp_new_huid(id_suffix);
    char *task_path = temp_sprintf("%s/%s", dir_path, id);
    if (file_exists(task_path)) {
        // We hit an already existing HUID. Let's try to wait 1 second to acquire the next one.
        sleep(1);
        id = temp_new_huid(id_suffix);
        task_path = temp_sprintf("%s/%s", dir_path, id);
        if (file_exists(task_path)) {
            // We still hit an existing HUID even afte waiting for 1 second.
            // Something is wrong with the environment. Better report an error.
            fprintf(stderr, "ERROR: %s already exists. You are probably creating tasks too fast, or your time is broken\n", id);
            return NULL;
        }
    }
    if (!mkdir_if_not_exists(task_path)) return NULL;

    Task task = {
        .id          = id,
        .title       = title,
        .status      = SVLIT("OPEN"),
        .priority    = priority,
        .parsed_tags = parsed_tags
    };

    properties_put(&task.properties, SVLIT("STATUS"), SVNULL);
    properties_put(&task.properties, SVLIT("PRIORITY"), SVNULL);
    properties_put(&task.properties, SVLIT("TAGS"), SVNULL);

    String_Builder sb_md_content = {0};

    render_task_md(task, &sb_md_content, NULL);
    const char *task_md_path = temp_sprintf("%s/%s/TASK.md", dir_path, id);
    if (!write_entire_file(task_md_path, sb_md_content.items, sb_md_content.count)) return NULL;
    print_task_report(dir_path, &task, SVNULL);
    return id;
}

typedef struct {
    char c;
    const char *description;
} Prompt_Option;

char user_prompt(const char *prompt, Prompt_Option *options, size_t options_count)
{
    char buffer[256];
    for (;;) {
        printf("%s [", prompt);
        for (size_t i = 0; i < options_count; ++i) {
            printf("%c,", options[i].c);
        }
        printf("?] ");
        fflush(stdout);
        if (!fgets(buffer, sizeof(buffer), stdin)) return false;
        for (char *it = buffer; *it; it += 1) *it = tolower(*it);
        String_View response = sv_trim(sv_from_cstr(buffer));
        if (sv_eq(response, SVLIT("?"))) {
            for (size_t i = 0; i < options_count; ++i) {
                printf("%c - %s\n", options[i].c, options[i].description);
            }
            continue;
        }
        for (size_t i = 0; i < options_count; ++i) {
            if (sv_eq(response, sv_from_parts(&options[i].c, 1))) return options[i].c;
        }
    }
}

typedef struct {
    String_View *items;
    size_t count;
    size_t capacity;
} Lines;

void lines_concat(Lines lines, String_Builder *sb, String_View sep)
{
    da_foreach(String_View, line, &lines) {
        sb_append_sv(sb, *line);
        sb_append_sv(sb, sep);
    }
}

bool todo_run(Command *self, const char *program_name, int argc, char **argv)
{
    Flag_List tags_from_flags = {0};
    char *id_suffix = NULL;
    bool help = false;

    void *c = flag_c_new(program_name);
    flag_c_list_var(c, &tags_from_flags, "t", "Tags to add to the new task");
    flag_c_str_var(c, &id_suffix, "s", NULL, "Task ID optional suffix");
    flag_c_bool_var(c, &help, "help", false, "Print this help message");

    bool use_color = isatty(STDOUT_FILENO);

    if (!flag_c_parse(c, argc, argv)) {
        print_command_usage(stderr, self, program_name, c);
        flag_c_print_error(c, stderr);
        return false;
    }

    argc = flag_c_rest_argc(c);
    argv = flag_c_rest_argv(c);

    if (help) {
        print_command_usage(stdout, self, program_name, c);
        return true;
    }

    if (argc <= 0) {
        print_command_usage(stderr, self, program_name, c);
        fprintf(stderr, "ERROR: no files were provided\n");
        return false;
    }

    Parsed_Tags parsed_tags = {0};
    da_foreach(const char *, tags_cstr, &tags_from_flags) {
        String_View unparsed_tags = sv_from_cstr(*tags_cstr);
        String_View tag = {0};
        while (unparsed_tags_next(&unparsed_tags, &tag)) {
            da_append(&parsed_tags, tag);
        }
    }

    char *dir_path = find_relative_tasks_directory();
    if (!dir_path) return false;

    String_Builder sb_input = {0};
    String_Builder sb_output = {0};
    Lines lines = {0};

    String_View tag = SVLIT("TO""DO:");
    size_t checkpoint = temp_save();
    while (argc > 0) {
        temp_rewind(checkpoint);

        const char *file_path = shift(argv, argc);

        sb_input.count = 0;
        if (!read_entire_file(file_path, &sb_input)) return false;

        lines.count = 0;
        String_View input = sb_to_sv(sb_input);
        while (input.count > 0) {
            String_View line = sv_chop_by_delim(&input, '\n');
            da_append(&lines, line);
        }

        bool yes_entire_file = false;
        for (size_t i = 0; i < lines.count; ++i) {
            String_View line = lines.items[i];
            size_t line_number = i + 1;
            int todo_index = sv_find(line, tag);
            if (todo_index < 0) {
                continue;
            }
            String_View prefix = sv_from_parts(line.data, todo_index);
            String_View suffix = sv_from_parts(line.data + prefix.count + tag.count, line.count - prefix.count - tag.count);
            String_View title  = sv_trim(suffix);
            printf("%s:%zu: Found a TO""DO: "SV_Fmt"\n", file_path, line_number, SV_Arg(title));
            for (long j = (long)i - 2; j <= (long)i + 2; ++j) {
                if (j < 0) continue;
                if (j >= (long)lines.count) continue;
                if (j == (long)i) {
                    if (use_color) {
                        printf(SV_Fmt "\033[1;31mTODO\033[0m:\033[36m" SV_Fmt "\033[0m\n", SV_Arg(prefix), SV_Arg(suffix));
                    } else {
                        printf(SV_Fmt "TO""DO:" SV_Fmt "\n", SV_Arg(prefix), SV_Arg(suffix));
                    }
                } else {
                    printf(SV_Fmt"\n", SV_Arg(lines.items[j]));
                }
            }

            if (!yes_entire_file) {
                static Prompt_Option options[] = {
                    { .c = 'y', .description = "yes"  },
                    { .c = 'n', .description = "no"   },
                    { .c = 'q', .description = "quit" },
                    { .c = 'a', .description = "yes until the end of file" },
                    { .c = 'c', .description = "no until the end of file" },
                };
                switch (user_prompt("Create a task from it?", options, ARRAY_LEN(options))) {
                case 'y': break;
                case 'n': continue;
                case 'q': return true;
                case 'a': yes_entire_file = true; break;
                case 'c': goto no_entire_file;
                default: UNREACHABLE("user_prompt");
                }
            }

            char *id = create_new_task(dir_path, id_suffix, title, DEFAULT_PRIORITY, parsed_tags);
            if (id == NULL) {
                return false;
            }
            lines.items[i] = sv_from_cstr(temp_sprintf(SV_Fmt "TASK(%s):" SV_Fmt, SV_Arg(prefix), id, SV_Arg(suffix)));

            sb_output.count = 0;
            lines_concat(lines, &sb_output, SVLIT("\n"));
            if (!write_entire_file(file_path, sb_output.items, sb_output.count)) {
                return false;
            }
        }
no_entire_file: {}
    }
    return true;
}

bool new_run(Command *self, const char *program_name, int argc, char **argv)
{
    Flag_List tags_from_flags = {0};
    bool help = false;
    uint64_t priority = 0;
    char *id_suffix = NULL;

    void *c = flag_c_new(program_name);
    flag_c_list_var(c, &tags_from_flags, "t", "Tags to add to the new task");
    flag_c_uint64_var(c, &priority, "p", DEFAULT_PRIORITY, "Priority of the new task");
    flag_c_str_var(c, &id_suffix, "s", NULL, "Task ID optional suffix");
    flag_c_bool_var(c, &help, "help", false, "Print this help message");
    String_Builder sb_title = {0};

    if (!flag_c_parse(c, argc, argv)) {
        print_command_usage(stderr, self, program_name, c);
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
        print_command_usage(stdout, self, program_name, c);
        return true;
    }

    char *dir_path = find_relative_tasks_directory();
    if (!dir_path) return false;

    String_View title = SVLIT(DEFAULT_TASK_TITLE);
    if (sb_title.count > 0) {
        title = sb_to_sv(sb_title);
    }

    Parsed_Tags parsed_tags = {0};
    da_foreach(const char *, tags_cstr, &tags_from_flags) {
        String_View unparsed_tags = sv_from_cstr(*tags_cstr);
        String_View tag = {0};
        while (unparsed_tags_next(&unparsed_tags, &tag)) {
            da_append(&parsed_tags, tag);
        }
    }

    if (!create_new_task(dir_path, id_suffix, title, priority, parsed_tags)) return false;

    return true;
}

bool id_run(Command *self, const char *program_name, int argc, char **argv)
{
    bool help = false;
    void *c = flag_c_new(program_name);
    flag_c_bool_var(c, &help, "help", false, "Print this help message");

    if (!flag_c_parse(c, argc, argv)) {
        print_command_usage(stderr, self, program_name, c);
        flag_c_print_error(c, stderr);
        return false;
    }

    argc = flag_c_rest_argc(c);
    argv = flag_c_rest_argv(c);

    if (help) {
        print_command_usage(stdout, self, program_name, c);
        return true;
    }

    Path dir_path = {0};
    Path cwd_path = {0};
    Path rel_path = {0};
    String_Builder sb_path = {0};
    if (!find_tasks_database(&dir_path))  return false;
    if (!get_current_dir_path(&cwd_path)) return false;
    path_relative(&rel_path, cwd_path, dir_path);
    if (!(rel_path.count == 1 && sv_eq(da_last(&rel_path), SVLIT("..")))) {
        fprintf(stderr, "ERROR: You must be inside of a specific task folder like %s/<HUID>/\n", path_render_cstr(&sb_path, rel_path));
        return false;
    }
    const char *huid = temp_sv_to_cstr(da_last(&cwd_path));
    printf("%s", huid);
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
            print_command_usage(stderr, self, program_name, c);
            flag_c_print_error(c, stderr);
            return false;
        }

        argc = flag_c_rest_argc(c);
        argv = flag_c_rest_argv(c);

        if (argc > 0) {
            if (huid != NULL) {
                print_command_usage(stderr, self, program_name, c);
                fprintf(stderr, "ERROR: Several HUIDs is not supported\n");
                return false;
            }

            huid = shift(argv, argc);
            if (!is_valid_huid(huid)) {
                print_command_usage(stderr, self, program_name, c);
                fprintf(stderr, "ERROR: `%s` is not a valid HUID. Valid HUID matches regexp %s\n", huid, HUID_REGEXP_FOR_USER_REPORT_PURPOSES);
                return false;
            }
        }
    }

    if (help) {
        print_command_usage(stdout, self, program_name, c);
        return true;
    }

    if (huid == NULL) {
        print_command_usage(stderr, self, program_name, c);
        fprintf(stderr, "ERROR: No HUID was provided\n");
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
                print_task_report(dir_path, task, SVNULL);
            }
            found = true;
        }
    }
    if (!found) {
        fprintf(stderr, "ERROR: No task with with HUID `%s` was found\n", huid);
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
        print_command_usage(stderr, self, program_name, c);
        flag_c_print_error(c, stderr);
        return false;
    }

    const char *huid = NULL;

    while (argc > 0) {
        if (!flag_c_parse(c, argc, argv)) {
            print_command_usage(stderr, self, program_name, c);
            flag_c_print_error(c, stderr);
            return false;
        }

        argc = flag_c_rest_argc(c);
        argv = flag_c_rest_argv(c);

        if (argc > 0) {
            if (huid != NULL) {
                print_command_usage(stderr, self, program_name, c);
                fprintf(stderr, "ERROR: Several HUIDs is not supported\n");
                return false;
            }

            huid = shift(argv, argc);
            if (!is_valid_huid(huid)) {
                print_command_usage(stderr, self, program_name, c);
                fprintf(stderr, "ERROR: `%s` is not a valid HUID. Valid HUID matches regexp %s\n", huid, HUID_REGEXP_FOR_USER_REPORT_PURPOSES);
                return false;
            }
        }
    }

    if (help) {
        print_command_usage(stdout, self, program_name, c);
        return true;
    }

    if (!find_tasks_database(&dir_path))  return false;
    if (!get_current_dir_path(&cwd_path)) return false;
    path_relative(&rel_path, cwd_path, dir_path);

    if (huid == NULL) {
        if (!(rel_path.count == 1 && sv_eq(da_last(&rel_path), SVLIT("..")))) {
            fprintf(stderr, "ERROR: You should be inside of a specific task folder like %s/<HUID>. Alternatively you can pass the <HUID> of the task as an argument to search for referers of that task.\n", path_render_cstr(&sb_path, rel_path));
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
        print_command_usage(stderr, self, program_name, c);
        flag_c_print_error(c, stderr);
        return false;
    }

    if (help) {
        print_command_usage(stdout, self, program_name, c);
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
    printf("Generated %s\n", dot_path);

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
        print_command_usage(stderr, self, program_name, c);
        flag_c_print_error(c, stderr);
        return false;
    }

    if (help) {
        print_command_usage(stdout, self, program_name, c);
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
        String_View status = task->status;
        bool task_is_closed = sv_eq(status, SVLIT("CLOSED"));
        if (closed == task_is_closed) {
            total_count += 1;
            bool untagged = true;
            for (size_t i = 0; i < task->parsed_tags.count; ++i) {
                String_View tag = task->parsed_tags.items[i];
                *ht_find_or_put(&tags_count, tag) += 1;
                untagged = false;
            }
            if (untagged) untagged_count += 1;
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
    bool help = false;
    void *c = flag_c_new(program_name);
    flag_c_bool_var(c, &help, "help", false, "Print this help message");

    if (!flag_c_parse(c, argc, argv)) {
        print_command_usage(stderr, self, program_name, c);
        flag_c_print_error(c, stderr);
        return false;
    }

    argc = flag_c_rest_argc(c);
    argv = flag_c_rest_argv(c);

    if (help) {
        print_command_usage(stdout, self, program_name, c);
        return true;
    }

#ifdef GIT_HASH
    printf("tatr commit %s\n", GIT_HASH);
#else
    printf("tatr\n");
#endif // GIT_HASH
#ifdef BUILD_TIME
    printf("Build time: %s\n", BUILD_TIME);
#endif // BUILD_TIME
#ifdef COMPILER_VERSION
    printf("Compiler: %s\n", COMPILER_VERSION);
#endif // COMPILER_VERSION
    return true;
}

// IMPORTANT! Every commands must follow a convention where if you provide `-help` it should call print_command_usage().
// This is important because `tatr help <COMMAND>` is a syntax sugar for `tatr <COMMAND> -help` and if the commands
// don't follow this convention `tatr-help` will not work properly.
Command commands[] = {
    {
        .name = "init",
        .signature = "[OPTIONS]",
        .short_description = "Create tasks/ directory in the current working directory if it doesn't exist yet.",
        .run = init_run,
    },
    {
        .name = "ls",
        .signature = "[OPTIONS] [QUERY...]",
        .short_description = "List and update the tasks.",
        .run = ls_run,
    },
    {
        .name = "new",
        .signature = "[OPTIONS] [TITLE...]",
        .short_description = "Create a new task.",
        .run = new_run,
    },
    {
        .name = "todo",
        .signature = "[OPTIONS] <FILE...>",
        .short_description = "Create tasks from all the TODOs in the provided files.",
        .run = todo_run,
    },
    {
        .name = "summary",
        .signature = "[OPTIONS]",
        .short_description = "Print the summary of the tasks.",
        .run = summary_run,
    },
    {
        .name = "find",
        .signature = "[OPTIONS] <HUID>",
        .short_description = "Find the task with a given HUID.",
        .extra_description =
            "Unlike `tatr ls <HUID>` this command finds the task regardless of whether it's closed or not.",
        .run = find_run,
    },
    {
        .name = "id",
        .signature = "[OPTIONS]",
        .short_description = "Print the id of the current task folder to stdout.",
        .extra_description = "On Linux you can do something like `tatr id | xclip` with this.",
        .run = id_run,
    },
    {
        .name = "ref",
        .signature = "[OPTIONS] [HUID]",
        .short_description = "Finds referers of the task.",
        .extra_description = "Basically greps the Task ID over entirety of the repo. If ran inside of a task folder automatically picks up the ID of that task.",
        .run = ref_run,
    },
    {
        .name = "graph",
        .signature = "[OPTIONS]",
        .short_description = "Generate graph of tasks cross-referring to each other.",
        .extra_description = "This command is largely useless right now.",
        .run = graph_run,
    },
    {
        .name = "tag",
        .short_description = "Tag all the tasks filtered by a query. DEPRECATED! Use `tatr ls -update +<tag> <query>` instead.",
        .signature = "[OPTIONS] <QUERY>",
        .run = tag_run,
        .deprecated = true,
    },
    {
        .name = "untag",
        .short_description = "Untag all the tasks filtered by a query. DEPRECATED! Use `tatr ls -update -<tag> <query>` instead.",
        .signature = "[OPTIONS] <QUERY>",
        .run = untag_run,
        .deprecated = true,
    },
    {
        .name = "help",
        .signature = "[OPTIONS] [COMMAND]",
        .short_description = "Print this help message.",
        .run = help_run,
    },
    {
        .name = "version",
        .signature = "[OPTIONS]",
        .short_description = "Print the current version and date of the build.",
        .run =  version_run,
    },
};

void print_available_commands(FILE *sink, const char *program_name, bool deprecated)
{
    int max_width = 0;
    for (size_t i = 0; i < ARRAY_LEN(commands); ++i) {
        int width = strlen(commands[i].name);
        if (width > max_width) max_width = width;
    }

    fprintf(sink, "Available commands:\n");
    for (size_t i = 0; i < ARRAY_LEN(commands); ++i) {
        if (commands[i].deprecated) continue;
        fprintf(sink, "    %-*s - %s\n", max_width, commands[i].name, commands[i].short_description);
    }
    fprintf(sink, "\n");
    if (deprecated) {
        fprintf(sink, "Deprecated commands:\n");
        for (size_t i = 0; i < ARRAY_LEN(commands); ++i) {
            if (!commands[i].deprecated) continue;
            fprintf(sink, "    %-*s - %s\n", max_width, commands[i].name, commands[i].short_description);
        }
        fprintf(sink, "\n");
    }
    fprintf(sink, "Use `%s <COMMAND> -help` to learn more about a command.\n", program_name);
}

bool help_run(Command *self, const char *program_name, int argc, char **argv)
{
    bool help = false;
    bool deprecated = false;
    void *c = flag_c_new(program_name);
    flag_c_bool_var(c, &deprecated, "d", false, "Print deprecated commands");
    flag_c_bool_var(c, &help, "help", false, "Print this help message");

    if (!flag_c_parse(c, argc, argv)) {
        print_command_usage(stderr, self, program_name, c);
        flag_c_print_error(c, stderr);
        return false;
    }

    argc = flag_c_rest_argc(c);
    argv = flag_c_rest_argv(c);

    if (help) {
        print_command_usage(stdout, self, program_name, c);
        return true;
    }

    if (argc <= 0) {
        printf("tatr - Task Tracker\n");
        printf("Usage: %s <command> [OPTIONS]\n", program_name);
        printf("\n");
        print_available_commands(stdout, program_name, deprecated);
        return true;
    }

    const char *command_name = shift(argv, argc);

    for (size_t i = 0; i < ARRAY_LEN(commands); ++i) {
        if (strcmp(command_name, commands[i].name) == 0) {
            char *help_flag = "-help";
            if (!commands[i].run(&commands[i], program_name, 1, &help_flag)) return false;
            return true;
        }
    }

    print_available_commands(stderr, program_name, deprecated);
    fprintf(stderr, "\n");
    fprintf(stderr, "ERROR: Unknown command `%s`\n", command_name);
    return false;
}

int main(int argc, char **argv)
{
#ifndef TASKS_TEST
    const char *program_name = shift(argv, argc);

    if (argc <= 0) {
        print_available_commands(stderr, program_name, false);
        fprintf(stderr, "\n");
        fprintf(stderr, "ERROR: No command is provided\n");
        return 0;
    }

    const char *command_name = shift(argv, argc);

    for (size_t i = 0; i < ARRAY_LEN(commands); ++i) {
        if (strcmp(command_name, commands[i].name) == 0) {
            if (!commands[i].run(&commands[i], program_name, argc, argv)) return 1;
            return 0;
        }
    }

    print_available_commands(stderr, program_name, false);
    fprintf(stderr, "\n");
    fprintf(stderr, "ERROR: Unknown command `%s`\n", command_name);
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
#include "update.c"
#include "task.c"

#define NOB_IMPLEMENTATION
#define NOB_OVERWRITE_TEMP_ON_REWIND
#include "nob.h"
#define FLAG_IMPLEMENTATION
#define FLAG_PUSH_DASH_DASH_BACK
#include "flag.h"
#define HT_IMPLEMENTATION
#include "ht.h"
