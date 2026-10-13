/*
 * Programming Assignment 02: lsv1.1.0
 * Version 1.1.0: adds the -l (long listing) option
 * Usage:
 *       $ ls
 *       $ ls -l
 *       $ ls -l /home /etc
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#define SPACING 2
/* ANSI escape codes */
#define COLOR_RESET    "\033[0m"
#define COLOR_BLUE     "\033[0;34m"   /* directory */
#define COLOR_GREEN    "\033[0;32m"   /* executable */
#define COLOR_RED      "\033[0;31m"   /* tarballs */
#define COLOR_PINK     "\033[0;35m"   /* symbolic links */
#define COLOR_REVERSE  "\033[7m"      /* special files */

static int use_color = 1;             /* set in main(): off when output is piped */

typedef enum {
    MODE_DEFAULT,      /* down then across (v1.2.0) */
    MODE_LONG,         /* -l */
    MODE_HORIZONTAL    /* -x */
} display_mode_t;

void do_ls(const char *dir, display_mode_t mode);
void do_ls_long(const char *dir, char **names, size_t count);
int  compare_names(const void *a, const void *b);
void print_vertical(const char *dir, char **names, size_t count, size_t maxlen);
void print_horizontal(const char *dir, char **names, size_t count, size_t maxlen);
void print_colored(const char *name, mode_t mode);
void print_entry(const char *dir, const char *name, int pad);
int  has_extension(const char *name, const char *ext);
int  get_terminal_width(void);
void mode_to_string(mode_t mode, char *str);
void get_user_name(uid_t uid, char *buf, size_t size);
void get_group_name(gid_t gid, char *buf, size_t size);

int main(int argc, char *argv[])
{
    int opt;
    display_mode_t mode = MODE_DEFAULT;
    use_color = isatty(STDOUT_FILENO);   /* no escape codes when piped or redirected */

    while ((opt = getopt(argc, argv, "lx")) != -1)
    {
        switch (opt)
        {
            case 'l':
                mode = MODE_LONG;
                break;
            case 'x':
                mode = MODE_HORIZONTAL;
                break;
            default:
                fprintf(stderr, "Usage: %s [-l | -x] [directory...]\n", argv[0]);
                return 1;
        }
    }

    if (optind == argc)
    {
        do_ls(".", mode);
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            printf("Directory listing of %s : \n", argv[i]);
            do_ls(argv[i], mode);
            puts("");
        }
    }
    return 0;
}

/* Original simple listing (unchanged logic from v1.0.0) */

/* Terminal width via ioctl(TIOCGWSZ); falls back to 80 if unavailable (e.g. output piped) */
int get_terminal_width(void)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
        return 80;
    return ws.ws_col;
}

/* Default listing: columns, "down then across" */
/* Reads all names into a dynamic array, then dispatches on the display mode */
void do_ls(const char *dir, display_mode_t mode)
{
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }

    char **names = NULL;
    size_t count = 0, capacity = 0, maxlen = 0;
    struct dirent *entry;

    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

        if (count == capacity)
        {
            capacity = (capacity == 0) ? 64 : capacity * 2;
            char **tmp = realloc(names, capacity * sizeof(char *));
            if (tmp == NULL)
            {
                perror("realloc");
                goto cleanup;
            }
            names = tmp;
        }

        names[count] = strdup(entry->d_name);
        if (names[count] == NULL)
        {
            perror("strdup");
            goto cleanup;
        }

        size_t len = strlen(names[count]);
        if (len > maxlen)
            maxlen = len;
        count++;
    }
    if (errno != 0)
        perror("readdir failed");

    /* ---------- Sort before any display function is called ---------- */
    if (count > 1)
        qsort(names, count, sizeof(char *), compare_names);

    /* ---------- Task 4: choose display function based on the flag ---------- */
    switch (mode)
    {
        case MODE_LONG:
            if (count > 0)
                do_ls_long(dir, names, count);
        break;
        case MODE_HORIZONTAL:
            if (count > 0)
                print_horizontal(dir, names, count, maxlen);
        break;
        case MODE_DEFAULT:
        default:
            if (count > 0)
                print_vertical(dir, names, count, maxlen);
        break;
    }

    cleanup:
    for (size_t i = 0; i < count; i++)
        free(names[i]);
    free(names);
    closedir(dp);
}

/* qsort passes pointers to the array ELEMENTS, and each element is a char *,
 *  so a and b are really char ** (pointers to string pointers). */
int compare_names(const void *a, const void *b)
{
    const char *s1 = *(const char **)a;
    const char *s2 = *(const char **)b;
    return strcmp(s1, s2);
}

/* Down then across (your v1.2.0 logic, moved into its own function) */
void print_vertical(const char *dir, char **names, size_t count, size_t maxlen)
{
    int term_width = get_terminal_width();
    int col_width  = (int)maxlen + SPACING;
    int cols = term_width / col_width;
    if (cols < 1)
        cols = 1;
    int rows = (int)((count + cols - 1) / cols);

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            size_t idx = (size_t)c * rows + r;
            if (idx >= count)
                break;

            size_t next = (size_t)(c + 1) * rows + r;
            int pad = 0;
            if (c + 1 < cols && next < count)
                pad = col_width - (int)strlen(names[idx]);

            print_entry(dir, names[idx], pad);
        }
        printf("\n");
    }
}

void print_horizontal(const char *dir, char **names, size_t count, size_t maxlen)
{
    int term_width = get_terminal_width();
    int col_width  = (int)maxlen + SPACING;
    int pos = 0;

    for (size_t i = 0; i < count; i++)
    {
        if (pos > 0 && pos + col_width > term_width)
        {
            printf("\n");
            pos = 0;
        }

        int pad = 0;
        if (i + 1 < count)
            pad = col_width - (int)strlen(names[i]);

        print_entry(dir, names[i], pad);
        pos += col_width;
    }
    printf("\n");
}

/* Convert st_mode to a string like "drwxr-xr-x" (handles setuid, setgid, sticky) */
void mode_to_string(mode_t mode, char *str)
{
    /* File type */
    if      (S_ISREG(mode))  str[0] = '-';
    else if (S_ISDIR(mode))  str[0] = 'd';
    else if (S_ISLNK(mode))  str[0] = 'l';
    else if (S_ISCHR(mode))  str[0] = 'c';
    else if (S_ISBLK(mode))  str[0] = 'b';
    else if (S_ISFIFO(mode)) str[0] = 'p';
    else if (S_ISSOCK(mode)) str[0] = 's';
    else                     str[0] = '?';

    /* Owner */
    str[1] = (mode & S_IRUSR) ? 'r' : '-';
    str[2] = (mode & S_IWUSR) ? 'w' : '-';
    if (mode & S_ISUID)
        str[3] = (mode & S_IXUSR) ? 's' : 'S';
    else
        str[3] = (mode & S_IXUSR) ? 'x' : '-';

    /* Group */
    str[4] = (mode & S_IRGRP) ? 'r' : '-';
    str[5] = (mode & S_IWGRP) ? 'w' : '-';
    if (mode & S_ISGID)
        str[6] = (mode & S_IXGRP) ? 's' : 'S';
    else
        str[6] = (mode & S_IXGRP) ? 'x' : '-';

    /* Others */
    str[7] = (mode & S_IROTH) ? 'r' : '-';
    str[8] = (mode & S_IWOTH) ? 'w' : '-';
    if (mode & S_ISVTX)
        str[9] = (mode & S_IXOTH) ? 't' : 'T';
    else
        str[9] = (mode & S_IXOTH) ? 'x' : '-';

    str[10] = '\0';
}

/* uid -> name using getpwuid(); falls back to the number */
void get_user_name(uid_t uid, char *buf, size_t size)
{
    struct passwd *pw = getpwuid(uid);
    if (pw != NULL)
        snprintf(buf, size, "%s", pw->pw_name);
    else
        snprintf(buf, size, "%u", (unsigned int)uid);
}

/* gid -> name using getgrgid(); falls back to the number */
void get_group_name(gid_t gid, char *buf, size_t size)
{
    struct group *gr = getgrgid(gid);
    if (gr != NULL)
        snprintf(buf, size, "%s", gr->gr_name);
    else
        snprintf(buf, size, "%u", (unsigned int)gid);
}

/* Long listing: pass 1 finds column widths, pass 2 prints aligned rows */
/* Long listing over the already-sorted array: pass 1 = column widths, pass 2 = print */
void do_ls_long(const char *dir, char **names, size_t count)
{
    struct stat st;
    char path[PATH_MAX];
    char owner[64], group[64], perms[11], tmp[64];
    int w_links = 0, w_owner = 0, w_group = 0, w_size = 0;
    long long total_blocks = 0;

    /* ---------- Pass 1: compute column widths ---------- */
    for (size_t i = 0; i < count; i++)
    {
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
        if (lstat(path, &st) == -1)
            continue;

        total_blocks += st.st_blocks;

        int len = snprintf(tmp, sizeof(tmp), "%lu", (unsigned long)st.st_nlink);
        if (len > w_links) w_links = len;

        get_user_name(st.st_uid, owner, sizeof(owner));
        len = (int)strlen(owner);
        if (len > w_owner) w_owner = len;

        get_group_name(st.st_gid, group, sizeof(group));
        len = (int)strlen(group);
        if (len > w_group) w_group = len;

        len = snprintf(tmp, sizeof(tmp), "%lld", (long long)st.st_size);
        if (len > w_size) w_size = len;
    }

    printf("total %lld\n", total_blocks / 2);

    /* ---------- Pass 2: print in sorted order ---------- */
    for (size_t i = 0; i < count; i++)
    {
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
        if (lstat(path, &st) == -1)
        {
            perror(path);
            continue;
        }

        mode_to_string(st.st_mode, perms);
        get_user_name(st.st_uid, owner, sizeof(owner));
        get_group_name(st.st_gid, group, sizeof(group));

        time_t mtime = st.st_mtime;
        char *timestr = ctime(&mtime);
        char timebuf[13];
        strncpy(timebuf, timestr + 4, 12);
        timebuf[12] = '\0';

        printf("%s %*lu %-*s %-*s %*lld %s ",
               perms,
               w_links, (unsigned long)st.st_nlink,
               w_owner, owner,
               w_group, group,
               w_size, (long long)st.st_size,
               timebuf);
        print_colored(names[i], st.st_mode);

        if (S_ISLNK(st.st_mode))
        {
            char target[PATH_MAX];
            ssize_t n = readlink(path, target, sizeof(target) - 1);
            if (n != -1)
            {
                target[n] = '\0';
                printf(" -> %s", target);
            }
        }
        printf("\n");
    }
}

/* 1 if name ends with ext (e.g. ".tar") */
int has_extension(const char *name, const char *ext)
{
    size_t n = strlen(name), e = strlen(ext);
    return n > e && strcmp(name + n - e, ext) == 0;
}

/* Task 4: print a filename in the color that matches its st_mode */
void print_colored(const char *name, mode_t mode)
{
    const char *color = NULL;

    if (S_ISLNK(mode))
        color = COLOR_PINK;
    else if (S_ISDIR(mode))
        color = COLOR_BLUE;
    else if (S_ISCHR(mode) || S_ISBLK(mode) || S_ISFIFO(mode) || S_ISSOCK(mode))
        color = COLOR_REVERSE;
    else if (has_extension(name, ".tar") || has_extension(name, ".gz") ||
        has_extension(name, ".zip"))
        color = COLOR_RED;
    else if (mode & (S_IXUSR | S_IXGRP | S_IXOTH))
        color = COLOR_GREEN;

    if (use_color && color != NULL)
        printf("%s%s%s", color, name, COLOR_RESET);
    else
        printf("%s", name);
}

/* Task 3: lstat the file, print it colored, then add `pad` spaces */
void print_entry(const char *dir, const char *name, int pad)
{
    char path[PATH_MAX];
    struct stat st;

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (lstat(path, &st) == -1)
        printf("%s", name);                  /* can't stat: print plain */
        else
            print_colored(name, st.st_mode);

    for (int i = 0; i < pad; i++)
        putchar(' ');
}
