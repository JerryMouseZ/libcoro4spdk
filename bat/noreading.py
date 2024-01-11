import os


# mutex只需要考虑线程数量
def benchmark_comutex(task):
    os.system("mkdir -p noreading/comutex")
    base_cmd = "sudo ./build/benchmarks/colocking_benchmarks -t mutex -r 0 -w {0} -c {1} -i 100000 > noreading/comutex/task_{0}_{2}.txt"
    core = task
    if core >= 12:
        core = 12
    for iter in range(3):
        cmd = base_cmd.format(task, core, iter)
        os.system(cmd)


def benchmark_mutex(task):
    os.system("mkdir -p noreading/mutex")
    base_cmd = "./threadpoolbench -t mutex -r 0 -w {0} -c {1} -i 100000 > noreading/mutex/task_{0}_{2}.txt"
    core = task
    if core >= 12:
        core = 12
    for iter in range(3):
        cmd = base_cmd.format(task, core, iter)
        os.system(cmd)


# 只检查只读的情况，因为只读的时候就有很大的开销了
def benchmark_cosmutex(task):
    os.system("mkdir -p noreading/cosmutex")
    base_cmd = "sudo ./build/benchmarks/colocking_benchmarks -t sharedmutex -r {0} -w 0 -c {1} -i 100000 > noreading/cosmutex/task_{0}_{2}.txt"
    core = task
    if core >= 12:
        core = 12
    for iter in range(3):
        cmd = base_cmd.format(task, core, iter)
        os.system(cmd)


# 只检查只读的情况，因为只读的时候就有很大的开销了
def benchmark_smutex(task):
    os.system("mkdir -p noreading/smutex")
    base_cmd = "./threadpoolbench -t sharedmutex -r {0} -w 0 -c {1} -i 1000000 > noreading/smutex/task_{0}_{2}.txt"
    core = task
    if core >= 12:
        core = 12
    for iter in range(3):
        cmd = base_cmd.format(task, core, iter)
        os.system(cmd)


def benchmark_rcu(task):
    os.system("mkdir -p noreading/rcu")
    base_cmd = "sudo ./build/benchmarks/colocking_benchmarks -t rcu -r {0} -w 0 -c {1} -i 100000 > noreading/rcu/task_{0}_{2}.txt"
    core = task
    if core >= 12:
        core = 12
    for iter in range(3):
        cmd = base_cmd.format(task, core, iter)
        os.system(cmd)


if __name__ == "__main__":
    task = 1
    for i in range(10):
        benchmark_rcu(task)
        benchmark_mutex(task)
        benchmark_comutex(task)
        benchmark_smutex(task)
        benchmark_cosmutex(task)
        task = task * 2
