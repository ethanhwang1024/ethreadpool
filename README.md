# ethreadpool
c++ thread pool

## Fixed mode
```c++
#include "ethreadpool.h"

int sum(int a, int b, int c){
    return a + b + c;
}

int main() {
    ThreadPool pool(PoolMode::MODE_FIXED,4);
    pool.start();
    std::future<int> r1 = pool.submitTask(sum,1,2,3);
    int i = r1.get();
    printf("%d",i);
}

```
## Cached mode
```c++
#include "ethreadpool.h"

int sum(int a, int b, int c){
    return a + b + c;
}

int main() {
    ThreadPool pool(PoolMode::MODE_CACHED,4,12);
    pool.start();
    std::future<int> r1 = pool.submitTask(sum,1,2,3);
    int i = r1.get();
    printf("%d",i);
}
```
