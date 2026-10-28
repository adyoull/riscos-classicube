/* Counts guest instructions executed per translated block; at exit writes
   "pc insns_executed" for every block to $INSNPROF (default insnprof.txt)
   and the total to stderr. Thread-safe enough for counting (relaxed).
   Plugin args on=<hex>,off=<hex>: count only after the block at "on" runs
   and until the block at "off" runs (the program's marker functions). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <qemu-plugin.h>
QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;
typedef struct { uint64_t pc; uint64_t n_insns; uint64_t execs; } Block;
static GHashTable *blocks; static GMutex lock;
static uint64_t mark_on, mark_off; static int counting = 1;
static void on_cb(unsigned int i, void *u) { counting = 1; }
static void off_cb(unsigned int i, void *u) { counting = 0; }
static void exec_cb(unsigned int idx, void *udata) { if (counting) __atomic_fetch_add(&((Block*)udata)->execs, 1, __ATOMIC_RELAXED); }
static void tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb) {
    uint64_t pc = qemu_plugin_tb_vaddr(tb); size_t n = qemu_plugin_tb_n_insns(tb);
    uint64_t key = pc ^ ((uint64_t)n << 48);
    Block *b;
    g_mutex_lock(&lock);
    b = g_hash_table_lookup(blocks, &key);
    if (!b) { b = g_new0(Block, 1); b->pc = pc; b->n_insns = n;
        uint64_t *k = g_new(uint64_t, 1); *k = key; g_hash_table_insert(blocks, k, b); }
    g_mutex_unlock(&lock);
    if (mark_on && pc == mark_on) qemu_plugin_register_vcpu_tb_exec_cb(tb, on_cb, QEMU_PLUGIN_CB_NO_REGS, NULL);
    if (mark_off && pc == mark_off) qemu_plugin_register_vcpu_tb_exec_cb(tb, off_cb, QEMU_PLUGIN_CB_NO_REGS, NULL);
    qemu_plugin_register_vcpu_tb_exec_cb(tb, exec_cb, QEMU_PLUGIN_CB_NO_REGS, b);
}
static void at_exit(qemu_plugin_id_t id, void *p) {
    const char *f = getenv("INSNPROF"); FILE *o = fopen(f ? f : "insnprof.txt", "w");
    GHashTableIter it; gpointer k, v; uint64_t total = 0;
    g_hash_table_iter_init(&it, blocks);
    while (g_hash_table_iter_next(&it, &k, &v)) { Block *b = v; uint64_t n = b->n_insns * b->execs;
        total += n; if (o && n) fprintf(o, "%llx %llu\n", (unsigned long long)b->pc, (unsigned long long)n); }
    if (o) fclose(o);
    fprintf(stderr, "INSNS %llu\n", (unsigned long long)total);
}
QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info, int argc, char **argv) {
    int i;
    blocks = g_hash_table_new(g_int64_hash, g_int64_equal);
    for (i = 0; i < argc; i++) {
        if (!strncmp(argv[i], "on=", 3)) mark_on = strtoull(argv[i] + 3, NULL, 16);
        if (!strncmp(argv[i], "off=", 4)) mark_off = strtoull(argv[i] + 4, NULL, 16);
    }
    if (mark_on) counting = 0;
    qemu_plugin_register_vcpu_tb_trans_cb(id, tb_trans);
    qemu_plugin_register_atexit_cb(id, at_exit, NULL);
    return 0;
}
