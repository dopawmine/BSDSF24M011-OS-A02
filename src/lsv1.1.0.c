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

void do_ls(const char *dir);
void do_ls_long(const char *dir);
void mode_to_string(mode_t mode, char *str);
void get_user_name(uid_t uid, char *buf, size_t size);
void get_group_name(gid_t gid, char *buf, size_t size);

int main(int argc, char *argv[])
{
    int opt;
    int long_flag = 0;

    /* Argument parsing with getopt() */
    while ((opt = getopt(argc, argv, "l")) != -1)
    {
        switch (opt)
        {
            case 'l':
                long_flag = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-l] [directory...]\n", argv[0]);
                return 1;
        }
    }

    /* optind = index of first non-option argument */
    if (optind == argc)
    {
        if (long_flag)
            do_ls_long(".");
        else
            do_ls(".");
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            printf("Directory listing of %s : \n", argv[i]);
            if (long_flag)
                do_ls_long(argv[i]);
            else
                do_ls(argv[i]);
            puts("");
        }
    }
    return 0;
}

/* Original simple listing (unchanged logic from v1.0.0) */
void do_ls(const char *dir)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }
    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        printf("%s\n", entry->d_name);
    }
    if (errno != 0)
        perror("readdir failed");

    closedir(dp);
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
void do_ls_long(const char *dir)
{
    struct dirent *entry;
    struct stat st;
    char path[PATH_MAX];
    char owner[64], group[64];
    char perms[11];

    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }

    int w_links = 0, w_owner = 0, w_group = 0, w_size = 0;
    long long total_blocks = 0;
    char tmp[64];

    /* ---------- Pass 1: compute column widths ---------- */
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
        if (lstat(path, &st) == -1)
            continue;

        total_blocks += st.st_blocks;

        int len;
        len = snprintf(tmp, sizeof(tmp), "%lu", (unsigned long)st.st_nlink);
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

    printf("total %lld\n", total_blocks / 2);   /* st_blocks are 512-byte units; ls shows 1K */

    /* ---------- Pass 2: print ---------- */
    rewinddir(dp);
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
        if (lstat(path, &st) == -1)
        {
            perror(path);
            continue;
        }

        mode_to_string(st.st_mode, perms);
        get_user_name(st.st_uid, owner, sizeof(owner));
        get_group_name(st.st_gid, group, sizeof(group));

        /* ctime() returns "Wed Jun 30 21:49:08 1993\n"; chars 4..15 = "Jun 30 21:49" */
        time_t mtime = st.st_mtime;
        char *timestr = ctime(&mtime);
        char timebuf[13];
        strncpy(timebuf, timestr + 4, 12);
        timebuf[12] = '\0';

        printf("%s %*lu %-*s %-*s %*lld %s %s",
               perms,
               w_links, (unsigned long)st.st_nlink,
               w_owner, owner,
               w_group, group,
               w_size, (long long)st.st_size,
               timebuf,
               entry->d_name);

        /* Show symlink target like real ls */
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

    if (errno != 0)
        perror("readdir failed");

    closedir(dp);
}
