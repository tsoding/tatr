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
#define NOB_IMPLEMENTATION
#define NOB_WARN_DEPRECATED
#include "./thirdparty/nob.h"
#define FLAG_IMPLEMENTATION
#include "./thirdparty/flag.h"
#include "./src/huid.h"

#define BUILD_FOLDER      "build/"
#define SRC_FOLDER        "src/"
#define THIRDPARTY_FOLDER "thirdparty/"

typedef enum {
    CC,
    GCC,
    CLANG,
    TCC,
    __compiler_count,
} Compiler;

static_assert(__compiler_count == 4, "Amount of compilers have changed");
const char *compiler_names[__compiler_count] = {
    [CC]    = "cc",
    [GCC]   = "gcc",
    [CLANG] = "clang",
    [TCC]   = "tcc",
};

bool compiler_by_name(const char *name, Compiler *compiler)
{
    for (int i = 0; i < __compiler_count; ++i) {
        if (strcmp(name, compiler_names[i]) == 0) {
            *compiler = i;
            return true;
        }
    }
    return false;
}

typedef struct {
    Cmd cmd;
    String_Builder sb_stdout;
    String_Builder sb_stderr;
    bool ok;
} Test_Runner;

bool test_runner_run(Test_Runner *r)
{
    const char *test_stdout_path = BUILD_FOLDER"test_stdout.txt";
    const char *test_stderr_path = BUILD_FOLDER"test_stderr.txt";
    Log_Handler *saved_log_handler = get_log_handler();
    set_log_handler(null_log_handler);
    {
        r->ok = cmd_run(
            &r->cmd,
            .stdout_path = test_stdout_path,
            .stderr_path = test_stderr_path,
        );
    }
    set_log_handler(saved_log_handler);
    r->sb_stdout.count = 0;
    if (!read_entire_file(test_stdout_path, &r->sb_stdout)) return false;
    r->sb_stderr.count = 0;
    if (!read_entire_file(test_stderr_path, &r->sb_stderr)) return false;
    return true;
}

bool run_query_payload(Test_Runner *r, const char *query_payload)
{
    cmd_append(&r->cmd, BUILD_FOLDER"tatr");
    cmd_append(&r->cmd, "ls");
    cmd_append(&r->cmd, "-debug");
    cmd_append(&r->cmd, query_payload);
    if (!test_runner_run(r)) return false;
    return true;
}

bool expect_failure(Test_Runner *r)
{
    if (r->ok) {
        nob_log(ERROR, "Command succeeded, but should've failed");
        String_View svout = sb_to_sv(r->sb_stdout);
        nob_log(ERROR, "STDOUT:");
        fprintf(stderr, SV_Fmt"\n", SV_Arg(svout));
        String_View sverr = sb_to_sv(r->sb_stderr);
        nob_log(ERROR, "STDERR:");
        fprintf(stderr, SV_Fmt"\n", SV_Arg(sverr));
        return false;
    }
    return true;
}

bool expect_success(Test_Runner *r)
{
    if (!r->ok) {
        nob_log(ERROR, "Command failed, but should've suceeded");
        String_View svout = sb_to_sv(r->sb_stdout);
        nob_log(ERROR, "STDOUT:");
        fprintf(stderr, SV_Fmt"\n", SV_Arg(svout));
        String_View sverr = sb_to_sv(r->sb_stderr);
        nob_log(ERROR, "STDERR:");
        fprintf(stderr, SV_Fmt"\n", SV_Arg(sverr));
        return false;
    }
    return true;
}

#define assert_test_output(output_label, expected_stdout, actual_stdout) \
    assert_test_output_loc(__FILE__, __LINE__, output_label, expected_stdout, actual_stdout)
bool assert_test_output_loc(const char *file_loc, int line_loc, const char *output_label, String_View expected_stdout, String_View actual_stdout)
{
    if (!sv_eq(expected_stdout, actual_stdout)) {
        nob_log(ERROR, "%s:%d: UNEXPECTED %s !!!", file_loc, line_loc, output_label);
        nob_log(ERROR, "EXPECTED:");
        fprintf(stderr, SV_Fmt"\n", SV_Arg(expected_stdout));
        nob_log(ERROR, "ACTUAL:");
        fprintf(stderr, SV_Fmt"\n", SV_Arg(actual_stdout));
        return false;
    }
    return true;
}

bool test_ls_query_negation_of_complex_expression_in_parens(Test_Runner *r)
{
    nob_log(INFO, "Running %s...", __func__);
    if (!run_query_payload(r, "not [tagged or :bug and :test and :foo and :bar]")) return false;
    if (!expect_success(r)) return false;
    if (!assert_test_output(
        "STDOUT",
        sv_from_cstr(
            "QUERY TOKENS:\n"
            "    not\n"
            "    [\n"
            "    tagged\n"
            "    or\n"
            "    :bug\n"
            "    and\n"
            "    :test\n"
            "    and\n"
            "    :foo\n"
            "    and\n"
            "    :bar\n"
            "    ]\n"
            "\n"
            "QUERY OPS:\n"
            "    QOP_TAGGED\n"
            "    QOP_TAG bug\n"
            "    QOP_TAG test\n"
            "    QOP_AND\n"
            "    QOP_TAG foo\n"
            "    QOP_AND\n"
            "    QOP_TAG bar\n"
            "    QOP_AND\n"
            "    QOP_OR\n"
            "    QOP_NOT\n"),
        sb_to_sv(r->sb_stdout))) return false;
    if (!assert_test_output("STDERR", (String_View){0}, sb_to_sv(r->sb_stderr))) return false;
    nob_log(INFO, "OK");
    return true;
}

bool test_ls_query_not_stuck_to_open_paren(Test_Runner *r)
{
    nob_log(INFO, "Running %s...", __func__);
    if (!run_query_payload(r, "not[:bug and :test] and :query")) return false;
    if (!expect_success(r)) return false;
    if (!assert_test_output(
        "STDOUT",
        sv_from_cstr(
            "QUERY TOKENS:\n"
            "    not\n"
            "    [\n"
            "    :bug\n"
            "    and\n"
            "    :test\n"
            "    ]\n"
            "    and\n"
            "    :query\n"
            "\n"
            "QUERY OPS:\n"
            "    QOP_TAG bug\n"
            "    QOP_TAG test\n"
            "    QOP_AND\n"
            "    QOP_NOT\n"
            "    QOP_TAG query\n"
            "    QOP_AND\n"),
        sb_to_sv(r->sb_stdout))) return false;
    if (!assert_test_output("STDERR", (String_View){0}, sb_to_sv(r->sb_stderr))) return false;
    nob_log(INFO, "OK");
    return true;
}

bool test_ls_query_report_error_utf8(Test_Runner *r)
{
    nob_log(INFO, "Running %s...", __func__);
    if (!run_query_payload(r, ":привет hello")) return false;
    if (!expect_failure(r)) return false;
    if (!assert_test_output("STDOUT", (String_View){0}, sb_to_sv(r->sb_stdout))) return false;
    if (!assert_test_output(
        "STDERR",
        sv_from_cstr(
            "Supported infix operators:\n"
            "\n"
            "    and  or                  - logical operators\n"
            "    lt  le  gt  ge  eq  ne   - comparison operators\n"
            "\n"
            ":привет hello\n"
            "        ^\n"
            "ERROR: Unexpected infix operator `hello`\n"
            ),
        sb_to_sv(r->sb_stderr))) return false;
    nob_log(INFO, "OK");
    return true;
}

bool test_ls_query_priority_above_20(Test_Runner *r)
{
    nob_log(INFO, "Running %s...", __func__);
    if (!run_query_payload(r, "priority gt 20")) return false;
    if (!expect_success(r)) return false;
    if (!assert_test_output(
        "STDOUT",
        SVLIT(
            "QUERY TOKENS:\n"
            "    priority\n"
            "    gt\n"
            "    20\n"
            "\n"
            "QUERY OPS:\n"
            "    QOP_PRIORITY\n"
            "    QOP_INTEGER 20\n"
            "    QOP_GT\n"),
        sb_to_sv(r->sb_stdout))) return false;
    if (!assert_test_output(
        "STDERR",
        (String_View){0},
        sb_to_sv(r->sb_stderr))) return false;
    nob_log(INFO, "OK");
    return true;
}

void cc(Cmd *cmd, Compiler compiler)
{
    static_assert(__compiler_count == 4, "Amount of compilers have changed");

    switch (compiler) {
    case CC:    cmd_append(cmd, "cc");    break;
    case GCC:   cmd_append(cmd, "gcc");   break;
    case CLANG: cmd_append(cmd, "clang"); break;
    case TCC:   cmd_append(cmd, "tcc");   break;
    case __compiler_count:
    default:
        UNREACHABLE("Compiler");
    }

    cmd_append(cmd, "-Wall");
    cmd_append(cmd, "-Wextra");
    cmd_append(cmd, "-Wswitch-enum");
    cmd_append(cmd, "-Wno-unused-function");
    cmd_append(cmd, "-I"THIRDPARTY_FOLDER);
    cmd_append(cmd, "-I"BUILD_FOLDER);
    if (compiler == CLANG) cmd_append(cmd, "-fsanitize=memory");
    cmd_append(cmd, "-ggdb");
}

bool make_build_h(Compiler compiler);
const char *get_current_date(void);
bool test_tatr_new_sanity(Cmd *cmd);
bool test_tatr_todo_sanity(Cmd *cmd);
bool test_tatr_tag_task_without_tags_property(const char *file_loc, int line_loc, Cmd *cmd);

int main(int argc, char **argv)
{
    GO_REBUILD_URSELF_PLUS(argc, argv, "./src/huid.h", "./src/huid.c", "./thirdparty/nob.h", "./thirdparty/flag.h");
    Cmd cmd = {0};

    bool test = false;
    bool run = false;
    bool help = false;
    bool debug = false;
    char *compiler_name = NULL;
    flag_bool_var(&test, "test", false, "Run the tests after building");
    flag_bool_var(&run, "run", false, "Run the app after the build");
    flag_bool_var(&debug, "debug", false, "Run the app in the debugger (gf2 specifically)");
    flag_bool_var(&help, "help", false, "Print this help message");
    flag_str_var(&compiler_name, "cc", "cc", "Compiler to use");

    if (!flag_parse(argc, argv)) {
        fprintf(stderr, "Usage: %s [OPTIONS]\n", flag_program_name());
        flag_print_options(stderr);
        flag_print_error(stderr);
        return 1;
    }

    Compiler compiler;
    if (!compiler_by_name(compiler_name, &compiler)) {
        nob_log(ERROR, "Unknown compiler \"%s\"", compiler_name);
        nob_log(ERROR, "Supported compilers:");
        for (int i = 0; i < __compiler_count; ++i) {
            nob_log(ERROR, "  %s", compiler_names[i]);
        }
        return 1;
    }

    argc = flag_rest_argc();
    argv = flag_rest_argv();

    if (help) {
        fprintf(stderr, "Usage: %s [OPTIONS]\n", flag_program_name());
        flag_print_options(stderr);
        return 0;
    }

    if (!mkdir_if_not_exists(BUILD_FOLDER)) return 1;

    if (!make_build_h(compiler)) return 1;

    cc(&cmd, compiler);
    cmd_append(&cmd, "-o", BUILD_FOLDER"tatr");
    cmd_append(&cmd, SRC_FOLDER"tatr.c");
    if (!cmd_run(&cmd)) return 1;

    if (test) {
        cc(&cmd, compiler);
        cmd_append(&cmd, "-DTASKS_TEST");
        cmd_append(&cmd, "-o", BUILD_FOLDER"tatr-test");
        cmd_append(&cmd, SRC_FOLDER"tatr.c");
        if (!cmd_run(&cmd)) return 1;

        cmd_append(&cmd, BUILD_FOLDER"tatr-test");
        if (!cmd_run(&cmd)) return 1;

        Test_Runner r = {0};
        if (!test_ls_query_negation_of_complex_expression_in_parens(&r)) return 1;
        if (!test_ls_query_not_stuck_to_open_paren(&r)) return 1;
        if (!test_ls_query_report_error_utf8(&r)) return 1;
        if (!test_ls_query_priority_above_20(&r)) return 1;
        if (!test_tatr_new_sanity(&r.cmd)) return 1;
        if (!test_tatr_todo_sanity(&r.cmd)) return 1;
        if (!test_tatr_tag_task_without_tags_property(__FILE__, __LINE__, &r.cmd)) return 1;
    }

    if (run) {
        if (debug) cmd_append(&cmd, "gf2");
        cmd_append(&cmd, BUILD_FOLDER"tatr");
        if (!debug) da_append_many(&cmd, argv, argc);
        if (!cmd_run(&cmd)) return false;
    }

    return 0;
}

void sv_quote(String_View sv, String_Builder *quoted_sb)
{
    da_foreach (const char, x, &sv) {
        switch (*x) {
        case '\n': sb_appendf(quoted_sb, "\\n");  break;
        case '"':  sb_appendf(quoted_sb, "\\\""); break;
        case '\\': sb_appendf(quoted_sb, "\\\\"); break;
        default:   sb_append (quoted_sb, *x);     break;
        }
    }
}

size_t count_needles(String_View haystack, String_View needle)
{
    int index = sv_find(haystack, needle);
    size_t count = 0;
    while (index >= 0) {
        count += 1;
        haystack.data  += index + needle.count;
        haystack.count -= index + needle.count;
        index = sv_find(haystack, needle);
    }
    return count;
}

bool count_needles_in_file(const char *file_path, String_View needle, size_t *needle_count)
{
    bool result = true;
    String_Builder sb = {0};
    if (!read_entire_file(file_path, &sb)) return_defer(false);
    *needle_count = count_needles(sb_to_sv(sb), needle);
defer:
    free(sb.items);
    return result;
}

bool expect_needle_count_in_file(const char *file_loc, int line_loc, const char *file_path, String_View needle, size_t expected_needle_count)
{
    String_Builder sb = {0};
    bool result = true;
    size_t needle_count = 0;
    if (!count_needles_in_file(file_path, needle, &needle_count)) return_defer(false);
    if (needle_count != expected_needle_count) {
        sv_quote(needle, &sb);
        String_View quoted_needle = sb_to_sv(sb);
        fprintf(stderr, "%s:%d: Expected %zu \""SV_Fmt"\" in %s but got %zu\n", file_loc, line_loc, expected_needle_count, SV_Arg(quoted_needle), file_path, needle_count);
        return_defer(false);
    }
defer:
    free(sb.items);
    return result;
}

bool count_newlines_in_file(const char *file_path, size_t *newline_count)
{
    return count_needles_in_file(file_path, SVLIT("\n"), newline_count);
}

bool expect_newline_count_in_file(const char *file_loc, int line_loc, const char *file_path, size_t expected_newline_count)
{
    return expect_needle_count_in_file(file_loc, line_loc, file_path, SVLIT("\n"),  expected_newline_count);
}

bool test_tatr_tag_task_without_tags_property(const char *file_loc, int line_loc, Cmd *cmd)
{
    bool result = true;
    const char *saved_cwd = get_current_dir_temp();
    String_Builder sb = {0};
    size_t newlines = 0;

    if (file_exists(BUILD_FOLDER"test/")) {
        if (!delete_directory_recursively(BUILD_FOLDER"test/")) return_defer(false);
    }

    if (!mkdir_if_not_exists(BUILD_FOLDER"test/")) return_defer(false);

    set_current_dir(BUILD_FOLDER"test/");

    cmd_append(cmd, "../tatr", "init");
    if (!cmd_run(cmd, .stdout_path = "stdout.txt")) return_defer(false);

    const char *task_id = temp_new_huid(NULL);
    if (!mkdir_if_not_exists(temp_sprintf("tasks/%s", task_id))) return_defer(false);
    const char *task_md_path = temp_sprintf("tasks/%s/TASK.md", task_id);

    sb_appendf(&sb, "# Test\n");
    sb_appendf(&sb, "\n");
    sb_appendf(&sb, "- PRIORITY: 69\n");
    sb_appendf(&sb, "- STATUS: OPEN\n");
    sb_appendf(&sb, "\n");
    sb_appendf(&sb, "A task without TAGS property\n");
    if (!write_entire_file(task_md_path, sb.items, sb.count)) return_defer(false);

    cmd_append(cmd, "../tatr", "ls", "-update", "+aboba", "any");
    if (!cmd_run(cmd, .stdout_path = "stdout.txt")) return_defer(false);

    if (!expect_needle_count_in_file(__FILE__, __LINE__, task_md_path, SVLIT("aboba"), 1)) return_defer(false);

defer:
    set_current_dir(saved_cwd);
    free(sb.items);
    if (!result) {
        fprintf(stderr, "%s:%d: TEST FAILED\n", file_loc, line_loc);
    }
    return result;
}

bool test_tatr_todo_sanity(Cmd *cmd)
{
    bool result = true;
    const char *saved_cwd = get_current_dir_temp();
    String_Builder sb = {0};
    size_t newlines = 0;

    if (file_exists(BUILD_FOLDER"test/")) {
        if (!delete_directory_recursively(BUILD_FOLDER"test/")) return_defer(false);
    }

    if (!mkdir_if_not_exists(BUILD_FOLDER"test/")) return_defer(false);

    set_current_dir(BUILD_FOLDER"test/");

    cmd_append(cmd, "../tatr", "init");
    if (!cmd_run(cmd, .stdout_path = "stdout.txt")) return_defer(false);

    cmd_append(cmd, "../tatr", "ls", "-id");
    if (!cmd_run(cmd, .stdout_path = "stdout.txt")) return_defer(false);
    if (!expect_newline_count_in_file(__FILE__, __LINE__, "stdout.txt", 1)) return_defer(false);

    sb.count = 0;
    sb_appendf(&sb, "y\n");
    sb_appendf(&sb, "n\n");
    sb_appendf(&sb, "y\n");
    if (!write_entire_file("stdin.txt", sb.items, sb.count)) return_defer(false);

    sb.count = 0;
    sb_appendf(&sb, "// TODO: task 1\n");
    sb_appendf(&sb, "/* TODO: task 2\n");
    sb_appendf(&sb, "*/\n");
    sb_appendf(&sb, "# TODO: task 3\n");
    if (!write_entire_file("todos", sb.items, sb.count)) return_defer(false);

    cmd_append(cmd, "../tatr", "todo", "./todos");
    if (!cmd_run(cmd, .stdin_path = "stdin.txt", .stdout_path = "stdout.txt")) return_defer(false);
    if (!expect_needle_count_in_file(__FILE__, __LINE__, "./todos", SVLIT("\n"),   4)) return_defer(false);
    if (!expect_needle_count_in_file(__FILE__, __LINE__, "./todos", SVLIT("TODO"), 1)) return_defer(false);
    if (!expect_needle_count_in_file(__FILE__, __LINE__, "./todos", SVLIT("TASK"), 2)) return_defer(false);

    cmd_append(cmd, "../tatr", "ls", "-id");
    if (!cmd_run(cmd, .stdout_path = "stdout.txt")) return_defer(false);
    if (!expect_newline_count_in_file(__FILE__, __LINE__, "stdout.txt", 2)) return_defer(false);

defer:
    set_current_dir(saved_cwd);
    free(sb.items);
    return result;
}

bool test_tatr_new_sanity(Cmd *cmd)
{
    bool result = true;
    const char *saved_cwd = get_current_dir_temp();

    if (file_exists(BUILD_FOLDER"test/")) {
        if (!delete_directory_recursively(BUILD_FOLDER"test/")) return_defer(false);
    }

    if (!mkdir_if_not_exists(BUILD_FOLDER"test/")) return_defer(false);

    set_current_dir(BUILD_FOLDER"test/");

    cmd_append(cmd, "../tatr", "init");
    if (!cmd_run(cmd, .stdout_path = "stdout.txt")) return_defer(false);

    cmd_append(cmd, "../tatr", "new");
    cmd_append(cmd, "-p", "69");
    cmd_append(cmd, "-t", "foo,bar,baz");
    cmd_append(cmd, "Sanity check task");
    if (!cmd_run(cmd, .stdout_path = "stdout.txt")) return_defer(false);

defer:
    set_current_dir(saved_cwd);
    return result;
}

bool make_build_h(Compiler compiler)
{
    bool result = true;

    Cmd cmd = {0};
    String_Builder sb_build_h = {0};
    String_Builder sb_compiler_version_txt = {0};
    String_Builder sb_tasks_readme_md = {0};
    String_Builder sb_git_hash = {0};

    sb_appendf(&sb_build_h, "#ifndef BUILD_H_\n");
    sb_appendf(&sb_build_h, "#define BUILD_H_\n");

    switch (sb_append_git_hash(&sb_git_hash, BUILD_FOLDER)) {
    case GIT_HASH_OK:
        sb_appendf(&sb_build_h, "#define GIT_HASH \"%"SB_Fmt2"\"\n", SB_Arg(sb_git_hash));
        break;
    case GIT_HASH_NO_GIT:
        nob_log(WARNING, "git hash will not be included in `tatr-version`");
        break;
    case GIT_HASH_ERROR:
        return_defer(false);
    default:
        UNREACHABLE("Git_Hash_Result");
    }

    static_assert(__compiler_count == 4, "Amount of compilers have changed");
    switch (compiler) {
    case CC:    cmd_append(&cmd, "cc",    "--version"); break;
    case GCC:   cmd_append(&cmd, "gcc",   "--version"); break;
    case CLANG: cmd_append(&cmd, "clang", "--version"); break;
    case TCC:   cmd_append(&cmd, "tcc",   "--version"); break;
    case __compiler_count:
    default:
        UNREACHABLE("Compiler");
    }
    if (cmd_run(&cmd, .stdout_path = BUILD_FOLDER"compiler-version.txt")) {
        if (!read_entire_file(BUILD_FOLDER"compiler-version.txt", &sb_compiler_version_txt)) return_defer(false);
        String_View compiler_version_txt = sb_to_sv(sb_compiler_version_txt);
        String_View compiler_version = sv_chop_by_delim(&compiler_version_txt, '\n');
        sb_appendf(&sb_build_h, "#define COMPILER_VERSION \""SV_Fmt"\"\n", SV_Arg(compiler_version));
    } else {
        nob_log(WARNING, "compiler version will not be included in `tatr-version`");
    }

    sb_appendf(&sb_build_h, "#define BUILD_TIME \"%s\"\n", get_current_date());

    const char *tasks_readme_md_file = "tasks/README.md";
    if (file_exists(tasks_readme_md_file)) {
        if (!read_entire_file(tasks_readme_md_file, &sb_tasks_readme_md)) return_defer(false);
        sb_appendf(&sb_build_h, "unsigned char tasks_readme_md[] = {\n");
        for (size_t i = 0; i < sb_tasks_readme_md.count;) {
            sb_appendf(&sb_build_h, "    ");
            for (size_t j = 0; i < sb_tasks_readme_md.count && j < 20; ++i, ++j) {
                sb_appendf(&sb_build_h, "0x%02X,", sb_tasks_readme_md.items[i]);
            }
            sb_appendf(&sb_build_h, "\n");
        }
        sb_appendf(&sb_build_h, "};\n");
        sb_appendf(&sb_build_h, "#define TASKS_README_MD tasks_readme_md\n");
    } else {
        nob_log(WARNING, "%s file doesn't exist. `tatr-init` will not create it along with the tasks/ folder", tasks_readme_md_file);
    }

    sb_appendf(&sb_build_h, "#endif // BUILD_H_\n");

    if (!write_entire_file(BUILD_FOLDER"build.h", sb_build_h.items, sb_build_h.count)) return_defer(false);
    nob_log(INFO, "Generated "BUILD_FOLDER"build.h");

defer:
    free(cmd.items);
    free(sb_build_h.items);
    free(sb_compiler_version_txt.items);
    free(sb_tasks_readme_md.items);
    free(sb_git_hash.items);
    return true;
}

const char *get_current_date(void)
{
    static const char *WEEKDAYS[] = {
        "Sun", "Mon", "Tue",
        "Wed", "Thu", "Fri",
        "Sat",
    };

    static const char *MONTHS[] = {
        "Jan", "Feb", "Mar",
        "Apr", "May", "Jun",
        "Jul", "Aug", "Sep",
        "Oct", "Nov", "Dec",
    };

    time_t rawtime;
    time(&rawtime);
    struct tm *timeinfo = localtime(&rawtime);

    String_Builder sb = {0};
    sb_appendf(&sb, "%s, ",  ARRAY_GET(WEEKDAYS, timeinfo->tm_wday));
    sb_appendf(&sb, "%02d ", timeinfo->tm_mday);
    sb_appendf(&sb, "%s ",   ARRAY_GET(MONTHS, timeinfo->tm_mon));
    sb_appendf(&sb, "%04d ", timeinfo->tm_year+1900);
    sb_appendf(&sb, "%02d:", timeinfo->tm_hour);
    sb_appendf(&sb, "%02d:", timeinfo->tm_min);
    sb_appendf(&sb, "%02d ", timeinfo->tm_sec);
    sb_appendf(&sb, "%s",    timeinfo->tm_zone);
    sb_append_null(&sb);
    return sb.items;
}

#include "./src/huid.c"
