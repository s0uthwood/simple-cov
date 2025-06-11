#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <iostream>

#include "config.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <program_to_run>" << std::endl;
        return 1;
    }
    std::string program = argv[1];
    // 创建共享内存
    std::string shm_key = SHM_FILE;
    key_t key = ftok(shm_key.c_str(), 'R');
    if (key == -1) {
        perror("ftok failed");
        return 1;
    }
    int shmid = shmget(key, 65536, IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget failed");
        return 1;
    }
    uint8_t *coverage = (uint8_t *)shmat(shmid, NULL, 0);
    if (coverage == (uint8_t *)(-1)) {
        perror("shmat failed");
        return 1;
    }
    int pipefd[2];
    pipe(pipefd);
    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO); // 可选：stderr也重定向
        close(pipefd[1]);
        // 子进程：设置环境变量并 exec 被测程序
        execl(program.c_str(), program.c_str(), NULL);
        perror("execl failed"); // 如果 execl 失败，会打印错误信息
        _exit(1);
    } else {
        close(pipefd[1]);
        // 父进程：等待子进程结束
        char buffer[256];
        ssize_t n;
        while ((n = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
            fwrite(buffer, 1, n, stdout);
            fflush(stdout);
        }
        close(pipefd[0]);
        waitpid(pid, NULL, 0);
        // 统计覆盖
        int count = 0;
        for (int i = 0; i < 65536; ++i)
            if (coverage[i]) ++count;
        std::cout << "Branches covered: " << count << std::endl;
        // 清理
        shmdt(coverage);
        shmctl(shmid, IPC_RMID, NULL);  // 删除共享内存
    }
    return 0;
}
