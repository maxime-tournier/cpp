from __future__ import print_function

import itertools

from snap import viewer

from snap.math import *
from snap.gl import *

import numpy as np


class Box(object):

    def __init__(self):
        self.size = np.ones(3)
        self.frame = Rigid3()
        
    def support(self, d):
        local = self.frame.orient.inv()(d)
        sign = (local > 0) - (local < 0)
        return self.frame(sign * self.size / 2)






def barycentric_coordinates(S, p):
    m, n = S.shape
    
    A = np.zeros( (n + 1, m))
    A[:n] = S.T
    A[-1] = 1
    b = np.zeros(m)
    
    b[:n] = p
    b[-1] = 1

    x = np.linalg.solve(A, b)
    return x




def pgs(M, q):

    n = q.size

    x = np.zeros(n)
    
    while True:

        for i in xrange(n):
            x[i] -= (M[i].dot(x) + q[i]) / M[i, i]
            x[i] = max(x[i], 0)

        yield x


def pgs_ls(A, b, p, omega = 1.0):
    '''M, q = A.dot(A.T), A.dot(p) - b'''
    
    m, n = A.shape

    x = np.zeros(m)

    # net = p + A.T.dot(x)
    net = np.copy(p)           
    
    while True:

        for i in xrange(m):
            old = x[i]
            x[i] -= omega * (A[i].dot(net) - b[i]) / A[i].dot(A[i])
            x[i] = max(x[i], 0.0)
            # net += A[i] * ( x[i] - old )

            net[:] = p + A.T.dot(x)

        yield x, net

        

def pocs(A, b, p):
    
    m, n = A.shape
    
    I = np.zeros( (m, n) )
    
    q = np.copy(p)

    def proj(i, x):
        y = b[i] - A[i].dot(x)
        if y <= 0: return x
        res = x + A[i] * (y / (A[i].dot(A[i])))
        return res

    n = 0
    
    while True:
        i = n % m
        p =  q - I[i]
        
        q[:] = proj(i, p)
        I[i] = q - p

        yield q
        n += 1




def proj_splx(x):
    '''O(n log(n) )'''
    n = x.size
    y = sorted(x)

    s = 0
    for k in range(n):
        i = n - 1 - k
        s += y[i]
        t = (s - 1.0) / float(n - i)
        if t > y[i - 1]:
            break

    return np.maximum(x - t, 0)


def proj_splx_alt(x, q):
    '''O(n log(n) )'''
    n = x.size

    # sort
    p = sorted(range(n), key=lambda k: x[k] / q[k])
    
    s = 0
    z = 0
    
    for k in range(n):
        i = n - 1 - k

        s += x[p[i]]
        z += q[p[i]]
        
        t = (s - 1.0) / z
        if t > x[ p[i - 1] ] / q[ p[i - 1] ]:
            break

    return np.maximum(x - q * t, 0)


        
def project_simplex(S, p):

    m, n = S.shape
    assert n + 1 == m
    
    Q = np.zeros( (m, m) )
    
    Q[:n] = S.T 
    Q[-1] = 1

    Qinv = np.linalg.inv(Q)
    
    A = Qinv[:, :n]
    b = - Qinv[:, -1]

    res = []

    
    for it, (x, net) in itertools.izip(xrange(n * n), pgs_ls(A, b, p) ):
        res.append(np.copy(net))
    return res


    for it, x in itertools.izip(xrange(2 * n * n), pocs(A, b, p)):
        res.append(np.copy(x))

    return res


def project_simplex(S, p):

    m, n = S.shape
    
    A = S.dot(S.T)
    b = S.dot(p)

    x = np.ones(m) / float(n)

    d = np.max(np.diag(A))
    
    for it in range(20):
        
        mask = x > 0

        d = np.max(mask * np.diag(A))
        
        # cheap subspace optimization
        grad = mask * (A.dot(x) - b)
        
        # print(alpha)
        
        old = x
        x = proj_splx( x - grad / d )
        print(it, norm(old - x))
        
    return S.T.dot(x)


def project_simplex(S, p):

    m, n = S.shape

    bias = np.random.rand(n)
    S = S + bias
    
    A = S.dot(S.T)
    b = S.dot(p + bias)
    
    x = np.ones(m) / float(n)

    # d = np.sum(A * A, axis = 0) / np.diag(A)
    d = np.diag(A)

    eps = 1e-5
    
    for it in range(20):
        
        # cheap subspace optimization
        grad = (A.dot(x) - b)

        # grad *= x > 0
        
        # print(alpha)
        
        old = x
        x = proj_splx_alt( x - grad / d, 1 / d )

        error = norm(old - x)
        print(it, error)
        if error < eps: break
        
    return S.T.dot(x) - bias
    
    


S = np.zeros( (4, 3) )
S[1:] = np.identity(3)

p = np.ones(3)
q = project_simplex(S, p)

def reset(): pass

def init(): pass

def keypress(key):
    if key == ' ':
        
        S[1:] = 2 * np.random.rand( 3, 3 ) - 1
        p[:] = 2 * np.random.rand(3) - 1

        # q[:] = proj_splx(p)
        q[:] = project_simplex(S, p)
        
def animate(): pass

    
def draw():
    glDisable(GL_LIGHTING)
    glPointSize(5)
    glLineWidth(2)
    glColor(1, 1, 1)

    glBegin(GL_POINTS)
    glVertex(p)
    glVertex(q)

    for v in xrange(3):
        col = np.zeros(3)
        col[v] = 1
        glColor(col)
        glVertex(S[1 + v])
    
    glEnd()

    
    glColor(1, 1, 1)
    glBegin(GL_LINES)

    for i, j in itertools.product(xrange(4), xrange(4)):
        glVertex(S[i])
        glVertex(S[j])        

    glEnd()

    glBegin(GL_LINE_STRIP)
    glVertex(p)

    glVertex(q)
        
    glEnd()

    
    
    glEnable(GL_LIGHTING)    

viewer.run()
