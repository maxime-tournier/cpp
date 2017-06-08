import spam
import gc


def func():
    s = spam.some()

func()
    
spam.clear()

print(gc.collect())
# gc.collect()
# gc.collect()
