import os


# mutex只需要考虑线程数量
def benchmark_comutex(task):
    os.system("mkdir -p res/comutex")
    base_cmd = "sudo ./build/benchmarks/colocking_benchmarks -t mutex -r 0 -w {0} -c {1} -i 100000 > res/comutex/task_{0}_{2}.txt"
    core = task
    if core >= 12:
        core = 12
    for iter in range(3):
        cmd = base_cmd.format(task, core, iter)
        os.system(cmd)


def benchmark_mutex(task):
    os.system("mkdir -p res/mutex")
    base_cmd = "./threadpoolbench -t mutex -r 0 -w {0} -c {1} -i 100000 > res/mutex/task_{0}_{2}.txt"
    core = task
    if core >= 12:
        core = 12
    for iter in range(3):
        cmd = base_cmd.format(task, core, iter)
        os.system(cmd)


def benchmark_cospinlock(task):
    os.system("mkdir -p res/cospinlock")
    base_cmd = "sudo ./build/benchmarks/colocking_benchmarks -t spinlock -r 0 -w {0} -c {1} -i 100000 > res/cospinlock/task_{0}_{2}.txt"
    core = task
    if core >= 12:
        core = 12
    for iter in range(3):
        cmd = base_cmd.format(task, core, iter)
        os.system(cmd)


def benchmark_spinlock(task):
    os.system("mkdir -p res/spinlock")
    base_cmd = "./threadpoolbench -t spinlock -r 0 -w {0} -c {1} -i 100000 > res/spinlock/task_{0}_{2}.txt"
    core = task
    if core >= 12:
        core = 12
    for iter in range(3):
        cmd = base_cmd.format(task, core, iter)
        os.system(cmd)


# 只检查只读的情况，因为只读的时候就有很大的开销了
def benchmark_cosmutex(task):
    os.system("mkdir -p res/cosmutex")
    base_cmd = "sudo ./build/benchmarks/colocking_benchmarks -t sharedmutex -r {0} -w 1 -c {1} -i 100000 > res/cosmutex/task_{0}_{2}.txt"
    core = task
    if core >= 12:
        core = 12
    for iter in range(3):
        cmd = base_cmd.format(task, core, iter)
        os.system(cmd)


# 只检查只读的情况，因为只读的时候就有很大的开销了
def benchmark_smutex(task):
    os.system("mkdir -p res/smutex")
    base_cmd = "./threadpoolbench -t sharedmutex -r {0} -w 1 -c {1} -i 1000000 > res/smutex/task_{0}_{2}.txt"
    core = task
    if core >= 12:
        core = 12
    for iter in range(3):
        cmd = base_cmd.format(task, core, iter)
        os.system(cmd)


def benchmark_rcu(task):
    os.system("mkdir -p res/rcu")
    base_cmd = "sudo ./build/benchmarks/colocking_benchmarks -t rcu -r {0} -w 0 -c {1} -i 100000 > res/rcu/task_{0}_{2}.txt"
    core = task
    if core >= 12:
        core = 12
    for iter in range(3):
        cmd = base_cmd.format(task, core, iter)
        os.system(cmd)


if __name__ == "__main__":
    task = 1
    for i in range(10):
        # benchmark_rcu(task)
        # benchmark_mutex(task)
        # benchmark_comutex(task)
        # benchmark_spinlock(task)
        # benchmark_cospinlock(task)
        # benchmark_smutex(task)
        benchmark_cosmutex(task)
        task = task * 2
