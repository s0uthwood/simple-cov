#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <iostream>

#include "config.h"

static uint8_t* coverage_ptr = nullptr;
int shmid = -1;

void periodic_report(int) {
    int count = 0;
    for (int i = 0; i < MAP_SIZE; ++i)
        if (coverage_ptr[i])
            count += __builtin_popcount(coverage_ptr[i]);
    std::cout << "[Periodic] Branches covered: " << count << std::endl;
}

void end_signal_handler(int) {
    periodic_report(0);
    shmdt(coverage_ptr);
    shmctl(shmid, IPC_RMID, NULL);
    // Kill the child process if it's still running
    exit(0);
}

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
    shmid = shmget(key, MAP_SIZE, IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget failed");
        return 1;
    }
    uint8_t *coverage = (uint8_t *)shmat(shmid, NULL, 0);
    if (coverage == (uint8_t *)(-1)) {
        perror("shmat failed");
        return 1;
    }
    coverage_ptr = coverage;
    memset(coverage_ptr, 0, MAP_SIZE); // initialize coverage array
    signal(SIGALRM, periodic_report);
    struct itimerval timer;
    timer.it_value.tv_sec = 10;  // First alarm in 5 seconds
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 300;  // Alarm every 5 minutes
    timer.it_interval.tv_usec = 0;
    periodic_report(0); // Report immediately
    setitimer(ITIMER_REAL, &timer, NULL);
    int pipefd[2];
    pipe(pipefd);
    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO); // 可选：stderr也重定向
        close(pipefd[1]);
        // 子进程：设置环境变量并 exec 被测程序
        // execl(program.c_str(), program.c_str(), NULL);
        execv(program.c_str(), &argv[1]); // 使用 execv 执行程序
        perror("execl failed"); // 如果 execl 失败，会打印错误信息
        _exit(1);
    } else {
        signal(SIGINT, end_signal_handler);
        close(pipefd[1]);
        // 父进程：等待子进程结束
        char buffer[256];
        while (true) {
            ssize_t n = read(pipefd[0], buffer, sizeof(buffer));
            if (n > 0) {
                fwrite(buffer, 1, n, stdout);
                fflush(stdout);
            } else if (n == 0) {
                break;  // Subprocess has closed the pipe
            } else {
                perror("read failed");
                break;
            }
        }
        close(pipefd[0]);
        waitpid(pid, NULL, 0);
        // Count branches covered
        periodic_report(0);
        // Clean up shared memory
        shmdt(coverage_ptr);
        shmctl(shmid, IPC_RMID, NULL);  // Delete shared memory
    }
    return 0;
}
