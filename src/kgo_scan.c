#define _GNU_SOURCE
#include "kgo.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdarg.h>
#include <syslog.h>

static int read_cmdline(pid_t pid, char *buf, size_t buflen)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", (int)pid);
    FILE *f = fopen(path, "r");
    if (!f)
        return -1;
    size_t n = fread(buf, 1, buflen - 1, f);
    fclose(f);
    for (size_t i = 0; i < n; i++)
        if (buf[i] == '\0')
            buf[i] = ' ';
    buf[n] = '\0';
    while (n > 0 && isspace((unsigned char)buf[n - 1]))
        buf[--n] = '\0';
    return (int)n;
}

static int read_ppid(pid_t pid, pid_t *ppid_out)
{
    char path[64];
    char line[512];
    snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
    FILE *f = fopen(path, "r");
    if (!f)
        return -1;
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return -1;
    }
    fclose(f);
    char *rp = strrchr(line, ')');
    if (!rp)
        return -1;
    rp++;
    long ppid = 0;
    if (sscanf(rp, " %*c %ld", &ppid) != 1)
        return -1;
    *ppid_out = (pid_t)ppid;
    return 0;
}

bool kgo_is_orphan_ppid(pid_t ppid)
{
    return ppid == 1;
}

static bool is_shell_cmd(const char *cmdline)
{
    return strstr(cmdline, "bash") != NULL
        || strstr(cmdline, "/sh ") != NULL
        || strstr(cmdline, "/sh\0") != NULL
        || strstr(cmdline, "dash") != NULL
        || strstr(cmdline, "zsh") != NULL;
}

bool kgo_match_target(const kgo_config_t *cfg, pid_t pid, pid_t ppid,
                      const char *cmdline, kgo_target_t *out)
{
    if (!kgo_is_orphan_ppid(ppid))
        return false;
    if (pid <= 1)
        return false;
    if (getpid() == pid)
        return false;

    for (size_t i = 0; i < cfg->pattern_count; i++) {
        const kgo_pattern_t *p = &cfg->patterns[i];
        if (!strstr(cmdline, p->match))
            continue;
        if (p->shell_only && !is_shell_cmd(cmdline))
            continue;
        memset(out, 0, sizeof(*out));
        out->pid = pid;
        out->ppid = ppid;
        snprintf(out->cmdline, sizeof(out->cmdline), "%s", cmdline);
        snprintf(out->reason, sizeof(out->reason), "%s", p->reason);
        return true;
    }
    return false;
}

int kgo_scan_orphans(const kgo_config_t *cfg, kgo_target_t *out,
                     size_t max_out, size_t *found)
{
    DIR *d = opendir("/proc");
    if (!d)
        return -1;

    size_t n = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && n < max_out) {
        if (!isdigit((unsigned char)ent->d_name[0]))
            continue;
        pid_t pid = (pid_t)atoi(ent->d_name);
        if (pid <= 1)
            continue;

        pid_t ppid = 0;
        if (read_ppid(pid, &ppid) != 0)
            continue;

        char cmdline[KGO_CMDLINE_LEN];
        if (read_cmdline(pid, cmdline, sizeof(cmdline)) <= 0)
            continue;

        kgo_target_t t;
        if (!kgo_match_target(cfg, pid, ppid, cmdline, &t))
            continue;

        out[n++] = t;
    }
    closedir(d);
    *found = n;
    return 0;
}

int kgo_kill_target(const kgo_target_t *target, int grace_sec)
{
    if (kill(target->pid, 0) != 0)
        return -1;

    kgo_log(LOG_WARNING, "killing orphan pid=%d ppid=%d reason=%s cmd=%s",
            (int)target->pid, (int)target->ppid, target->reason, target->cmdline);

    if (kill(target->pid, SIGTERM) != 0)
        return -1;

    for (int i = 0; i < grace_sec * 10; i++) {
        if (kill(target->pid, 0) != 0)
            return 0;
        usleep(100000);
    }

    if (kill(target->pid, SIGKILL) == 0)
        kgo_log(LOG_WARNING, "SIGKILL pid=%d", (int)target->pid);
    return 0;
}

void kgo_log(int priority, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsyslog(priority, fmt, ap);
    va_end(ap);
}