#ifndef KGO_H
#define KGO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define KGO_MAX_PATTERNS 32
#define KGO_PATTERN_LEN  256
#define KGO_REASON_LEN   128
#define KGO_CMDLINE_LEN  4096
#define KGO_VERSION      "1.0.0"

typedef struct {
    char id[64];
    char match[KGO_PATTERN_LEN];
    char reason[KGO_REASON_LEN];
    bool shell_only;
} kgo_pattern_t;

typedef struct {
    kgo_pattern_t patterns[KGO_MAX_PATTERNS];
    size_t pattern_count;
    int interval_sec;
    int grace_sec;
} kgo_config_t;

typedef struct {
    pid_t pid;
    pid_t ppid;
    char cmdline[KGO_CMDLINE_LEN];
    char reason[KGO_REASON_LEN];
} kgo_target_t;

int  kgo_load_config(const char *path, kgo_config_t *cfg);
bool kgo_is_orphan_ppid(pid_t ppid);
bool kgo_match_target(const kgo_config_t *cfg, pid_t pid, pid_t ppid,
                      const char *cmdline, kgo_target_t *out);
int  kgo_scan_orphans(const kgo_config_t *cfg, kgo_target_t *out,
                      size_t max_out, size_t *found);
int  kgo_kill_target(const kgo_target_t *target, int grace_sec);
void kgo_log(int priority, const char *fmt, ...);

#endif