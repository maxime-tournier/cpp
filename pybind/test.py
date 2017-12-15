import spam
import gc


def func():
    s = spam.some()
    print(s.bacon())
    
func()
    
# spam.clear()


class SuperSpam(spam.spam):

    def bacon(self): return 'super bacon'

super_spam = SuperSpam()
print(super_spam.bacon());

# del super_spam

# print(gc.collect())
# gc.collect()
# gc.collect()
