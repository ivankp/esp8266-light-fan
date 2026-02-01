def TestGroup(f):
    print(f.__name__, end=' ', flush=True)
    f()
    print(' \033[32m✔\033[0m', flush=True)

def Test(f):
    def f2(*args, **kwargs):
        f(*args, **kwargs)
        print('\033[32m.\033[0m', end='', flush=True)
    return f2
