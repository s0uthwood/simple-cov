#include <stdint.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "config.h"

#define MAP_SIZE 65536
static uint8_t *coverage = NULL;
static uint32_t __mycov_prev_loc = 0;

__attribute__((constructor))
static void setup_shm() {
    printf("Setting up shared memory for coverage...\n");
    key_t key = ftok(SHM_FILE, 'R');
    if (key == -1) {
        perror("ftok failed");
        return;
    }
    int shmid = -1;
    if (key) {
        shmid = shmget(key, MAP_SIZE, IPC_CREAT | 0666);
        if (shmid < 0) {
            perror("shmget failed");
            return;
        }
        coverage = shmat(shmid, NULL, 0);
    }
    if (!coverage) {
        // fallback: local buffer
        static uint8_t local_cov[MAP_SIZE];
        coverage = local_cov;
        printf("Using local coverage buffer\n");
    } else {
        printf("Shared memory coverage buffer initialized\n");
    }
    memset(coverage, 0, MAP_SIZE); // 初始化覆盖数组
}

void __mycov_hit(unsigned int cur_loc) {
    if (!coverage) return;
    uint32_t idx = (__mycov_prev_loc * 16777619u) ^ cur_loc;
    idx %= MAP_SIZE * 8; // 确保索引在范围内
    // if (coverage && id < MAP_SIZE)
    //     coverage[id] = 1;
    printf("Branch %u %u hit\n", __mycov_prev_loc >> 1, cur_loc);
    coverage[idx >> 3] |= (1 << (idx & 7)); // 设置对应位为1
    __mycov_prev_loc = cur_loc;
}
