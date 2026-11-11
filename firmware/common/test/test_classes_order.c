/* test_classes_order.c — classes.json 必須與模型的 labels.txt 同序，且 bin 值固定。

 * 這正是 #53 §6.1 要釘死的東西：模型 index 0..3 → 桶位 {1, 2, 3, 0}。
 * 次序錯了就拒絕啟動，不可以在運行時猜。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ITEMS 8
#define MAX_KEY 32

static int read_all(const char *path, char *buf, size_t cap)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", path);
        return -1;
    }
    size_t n = fread(buf, 1, cap - 1, f);
    fclose(f);
    buf[n] = '\0';
    return (int)n;
}

int main(void)
{
    static char json[8192];
    static char labels[1024];
    if (read_all(CLASSES_JSON_PATH, json, sizeof json) < 0) return 2;
    if (read_all(LABELS_PATH, labels, sizeof labels) < 0) return 2;

    char keys[MAX_ITEMS][MAX_KEY];
    int bins[MAX_ITEMS];
    int count = 0;

    const char *p = json;
    while (count < MAX_ITEMS) {
        const char *k = strstr(p, "\"key\"");
        if (!k) break;
        const char *colon = strchr(k, ':');
        if (!colon) break;
        const char *q1 = strchr(colon, '"');
        if (!q1) break;
        const char *q2 = strchr(q1 + 1, '"');
        if (!q2 || (size_t)(q2 - q1 - 1) >= MAX_KEY) break;
        memcpy(keys[count], q1 + 1, (size_t)(q2 - q1 - 1));
        keys[count][q2 - q1 - 1] = '\0';

        const char *b = strstr(q2, "\"bin\"");
        const char *brace = strchr(q2, '}');
        if (!b || (brace && b > brace)) {
            fprintf(stderr, "class %s has no bin\n", keys[count]);
            return 1;
        }
        const char *bc = strchr(b, ':');
        bins[count] = bc ? atoi(bc + 1) : -1;
        count++;
        p = q2 + 1;
    }

    if (count != 4) {
        fprintf(stderr, "expected 4 classes, parsed %d\n", count);
        return 1;
    }

    char expected[4][MAX_KEY];
    int lines = 0;
    char *save = NULL;
    for (char *line = strtok_r(labels, "\r\n", &save); line && lines < 4;
         line = strtok_r(NULL, "\r\n", &save)) {
        if (*line == '\0') continue;
        snprintf(expected[lines], MAX_KEY, "%s", line);
        lines++;
    }
    if (lines != 4) {
        fprintf(stderr, "labels.txt must hold 4 lines, found %d\n", lines);
        return 1;
    }

    static const int expected_bins[4] = {1, 2, 3, 0};   /* food, hazardous, other, recyclable */
    for (int i = 0; i < 4; i++) {
        if (strcmp(keys[i], expected[i]) != 0) {
            fprintf(stderr, "index %d: classes.json has %s, labels.txt has %s\n",
                    i, keys[i], expected[i]);
            return 1;
        }
        if (bins[i] != expected_bins[i]) {
            fprintf(stderr, "index %d (%s): bin %d, expected %d\n",
                    i, keys[i], bins[i], expected_bins[i]);
            return 1;
        }
    }

    printf("test_classes_order: %s,%s,%s,%s -> bins 1,2,3,0\n",
           keys[0], keys[1], keys[2], keys[3]);
    return 0;
}
