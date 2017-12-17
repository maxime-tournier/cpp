from __future__ import print_function

import spam
import gc

def func():
    s = spam.spam()
    print('func', s.bacon())
    
func()


class SuperSpam(spam.spam):

    def bacon(self): return 'super bacon'


def func2():
    s = SuperSpam()
    print('func2', s.bacon())


func2()
        
# spam.clear()

# x = spam.some()
# del x
# spam.clear()


print('python end')
# gc.set_debug(gc.DEBUG_LEAK)
# print(gc.collect())
# gc.collect()
# gc.collect()
