
def process_file(filename):
    f = open(filename, "r")
    lines = f.readlines()
    t = float(lines[1][:-2])
    return t


# mutex只需要考虑线程数量
def benchmark_result(task, ltype):
    base_file = "res/{0}/task_{1}_{2}.txt"
    v = 0.0
    for iter in range(3):
        file = base_file.format(ltype, task, iter)
        t = process_file(file)
        v = v + t
    v /= 3
    return task * 100000 / v


def benchmark_comutex(task):
    return benchmark_result(task, "comutex")


def benchmark_mutex(task):
    return benchmark_result(task, "mutex")


def benchmark_cospinlock(task):
    return benchmark_result(task, "cospinlock")


def benchmark_spinlock(task):
    return benchmark_result(task, "spinlock")

# 只检查只读的情况，因为只读的时候就有很大的开销了


def benchmark_cosmutex(task):
    return benchmark_result(task, "cosmutex")

# 只检查只读的情况，因为只读的时候就有很大的开销了


def benchmark_smutex(task):
    return benchmark_result(task, "smutex")


def benchmark_rcu(task):
    return benchmark_result(task, "rcu")


if __name__ == "__main__":
    f = open("./res/res.csv", "w")
    f.write("tasks,rcu,mutex,comutex,spinlock,cospinlock,smutex,cosmutex\n")
    task = 1
    for i in range(10):
        rcu = benchmark_rcu(task)
        mutex = benchmark_mutex(task)
        comutex = benchmark_comutex(task)
        spinlock = benchmark_spinlock(task)
        cospinlock = benchmark_cospinlock(task)
        smutex = benchmark_smutex(task)
        cosmutex = benchmark_cosmutex(task)
        f.write(
            f"{task},{rcu},{mutex},{comutex},{spinlock},{cospinlock},{smutex},{cosmutex}\n")
        task = task * 2
