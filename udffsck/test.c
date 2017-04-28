
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>

int fsck_wrapper(const char * medium, char *const args, char *const argB) {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        strcpy(cwd + strlen(cwd), "/udffsck");
    } else {
        printf("getcwd error. Aborting.\n");
        return -1;
    }


    char medpwd[1024];
    sprintf(medpwd, "../../udf-samples/%s.img", medium);    
    printf("Medium: %s\n", medpwd);
    char * const pars[] = { 
        cwd,
        medpwd,
        args,
        argB,
        NULL
    };

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char fout[1024];
    char ferr[1024];
    sprintf(fout, "../../udf-samples/%d-%02d-%02d-%02d-%02d-%02d_%s.img.out", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, medium);
    sprintf(ferr, "../../udf-samples/%d-%02d-%02d-%02d-%02d-%02d_%s.img.err", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, medium);

    int pipefd[3];
    pipe(pipefd);

    int statval, exitval;
    if(fork() == 0) {
        int fdout = open(fout, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        int fderr = open(ferr, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        dup2(fdout, 1);   // make stdout go to file
        dup2(fderr, 2);   // make stderr go to file
        close(fdout);     // fd no longer needed - the dup'ed handles are sufficient
        close(fderr);     // fd no longer needed - the dup'ed handles are sufficient

        execv(cwd, pars);
    } else {
        wait(&statval);
        if(WIFEXITED(statval)) {
            printf("Child's exit code %d\n", WEXITSTATUS(statval));
            exitval = WEXITSTATUS(statval);
        } else {
            printf("Child did not terminate with exit\n");  
            exitval = -1;
        }
        return exitval;
    }
    return 0;
}

static void blank_pass(void **state) {
    (void) state;

    assert_int_equal(2, 2);
}

static void blank_fail(void **state) {
    (void) state;

    assert_int_equal(2, -3);
}

static void bs2048_dirty_file_tree_2_CHECKONLY_FORCEBLOCKSIZE_ERR(void **state) {
    (void) state;
    char *medium = "bs2048-r0201-dirty-file-tree-deleted-peregrine";
    assert_int_equal(fsck_wrapper(medium, "-vvc", "-B 2048"), 4);
}

static void bs2048_dirty_file_tree_2_FIX_FORCEBLOCKSIZE(void **state) {
    (void) state;
    char *medium = "bs2048-r0201-dirty-file-tree-deleted-peregrine";
    assert_int_equal(fsck_wrapper(medium, "-vvp", "-B 2048"), 1);
}

static void bs2048_dirty_file_tree_2_CHECKONLY_FORCEBLOCKSIZE_NOERR(void **state) {
    (void) state;
    char *medium = "bs2048-r0201-dirty-file-tree-deleted-peregrine";
    assert_int_equal(fsck_wrapper(medium, "-vvc", "-B 2048"), 0);
}

int main(void) {
    const struct CMUnitTest tests[] = {
#ifdef DEMO
        cmocka_unit_test(blank_fail),
        cmocka_unit_test(blank_pass),
#endif
        cmocka_unit_test(bs2048_dirty_file_tree_2_CHECKONLY_FORCEBLOCKSIZE_ERR),
        cmocka_unit_test(bs2048_dirty_file_tree_2_FIX_FORCEBLOCKSIZE),
        cmocka_unit_test(bs2048_dirty_file_tree_2_CHECKONLY_FORCEBLOCKSIZE_NOERR),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
