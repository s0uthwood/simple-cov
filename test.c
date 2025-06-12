#include <stdio.h>

int main()
{
    setvbuf(stdout, NULL, _IONBF, 0);
    while (1) {
        int a = 0;
        int b = 100;
        int c = a + b;
        if (c > 50) {
            c += 1;
        } else {
            c -= 1;
        }
        if (c < 100) {
            c *= 2;
        } else {
            c /= 2;
        }
        if (c % 2 == 0) {
            c += 1;
        } else {
            c -= 1;
        }
        printf("%d\n", c);
        sleep(5);
    }
    return 0;
}
