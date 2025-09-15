#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#define WAL_FILE "wallog"
#define DB_FILE  "dbtxt"
#define LINE_BUF 256

static int txn_id = 1;

static void safe_write(int fd, const char *msg, int sync) {
    ssize_t n = write(fd, msg, strlen(msg));
    if (n < 0) {
        perror("write");
        exit(1);
    }
    if (sync && fsync(fd) < 0) {
        perror("fsync");
        exit(1);
    }
}

static void wal_append(const char *key, const char *value, int sync) {
    char buf[LINE_BUF];
    int fd = open(WAL_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        perror("open wal");
        exit(1);
    }

    snprintf(buf, sizeof(buf), "TRANSACTION %d BEGIN\n", txn_id);
    safe_write(fd, buf, sync);

    snprintf(buf, sizeof(buf), "SET %s %s\n", key, value);
    safe_write(fd, buf, sync);

    snprintf(buf, sizeof(buf), "TRANSACTION %d COMMIT\n", txn_id);
    safe_write(fd, buf, sync);

    close(fd);
}

static void db_append(const char *key, const char *value) {
    char buf[LINE_BUF];
    int fd = open(DB_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        perror("open db");
        exit(1);
    }

    snprintf(buf, sizeof(buf), "%s=%s\n", key, value);
    if (write(fd, buf, strlen(buf)) < 0) {
        perror("db write");
    }

    fsync(fd);
    close(fd);
}

static void cmd_commit(const char *key, const char *value) {
    wal_append(key, value, 1);
    db_append(key, value);
    printf("Committed: %s=%s\n", key, value);
    txn_id++;
}

static void cmd_commit_nosync(const char *key, const char *value) {
    wal_append(key, value, 0);
    db_append(key, value);
    printf("Committed (no sync): %s=%s\n", key, value);
    txn_id++;
}

static void cmd_crash_after_wal(const char *key, const char *value) {
    wal_append(key, value, 1);
    printf("Simulated crash after WAL write\n");
    exit(1);
}

static void cmd_recover() {
    FILE *f = fopen(WAL_FILE, "r");
    if (!f) {
        printf("No WAL to recover\n");
        return;
    }

    char line[LINE_BUF], key[128], val[128];
    int inside_tx = 0, committed = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "BEGIN")) {
            inside_tx = 1;
            committed = 0;
        }
        else if (strncmp(line, "SET", 3) == 0 && inside_tx) {
            sscanf(line, "SET %127s %127s", key, val);
        }
        else if (strstr(line, "COMMIT") && inside_tx) {
            db_append(key, val);
            printf("Recovered: %s=%s\n", key, val);
            inside_tx = 0;
            committed = 1;
        }
    }

    if (inside_tx && !committed) {
        printf("Found incomplete transaction in WAL, ignoring\n");
    }

    fclose(f);
}

static void cmd_show() {
    printf("===== WAL LOG =====\n");
    FILE *wal_fp = fopen(WAL_FILE, "r");
    if (wal_fp) {
        char line[LINE_BUF];
        while (fgets(line, sizeof(line), wal_fp)) {
            fputs(line, stdout);
        }
        fclose(wal_fp);
    }

    printf("\n===== DB CONTENTS =====\n");
    FILE *db_fp = fopen(DB_FILE, "r");
    if (db_fp) {
        char line[LINE_BUF];
        while (fgets(line, sizeof(line), db_fp)) {
            fputs(line, stdout);
        }
        fclose(db_fp);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("\nUsage:\n");
        printf("%s commit <key> <value>\n", argv[0]);
        printf("%s commit-nosync <key> <value>\n", argv[0]);
        printf("%s crash-after-wal <key> <value>\n", argv[0]);
        printf("%s recover\n", argv[0]);
        printf("%s show\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "commit") == 0 && argc == 4) {
        cmd_commit(argv[2], argv[3]);
    }
    else if (strcmp(argv[1], "commit-nosync") == 0 && argc == 4) {
        cmd_commit_nosync(argv[2], argv[3]);
    }
    else if (strcmp(argv[1], "crash-after-wal") == 0 && argc == 4) {
        cmd_crash_after_wal(argv[2], argv[3]);
    }
    else if (strcmp(argv[1], "recover") == 0) {
        cmd_recover();
    }
    else if (strcmp(argv[1], "show") == 0) {
        cmd_show();
    }
    else {
        fprintf(stderr, "Invalid command\n");
        return 1;
    }

    return 0;
}
