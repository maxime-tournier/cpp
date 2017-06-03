import spam
import gc


def func():
    s = spam.some()

func()
    
spam.clear()

gc.collect()
# gc.collect()
