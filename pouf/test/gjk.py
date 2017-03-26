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
            net += A[i] * ( x[i] - old )
            # net[:] = p + A.T.dot(x)
            
        yield net

        

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
        
def project_simplex(S, p):

    m, n = S.shape
    assert n + 1 == m
    
    Q = np.zeros( (m, m) )
    
    Q[:n] = S.T
    Q[-1] = 1

    Qinv = np.linalg.inv(Q)
    
    A = Qinv[:, :n]
    b = - Qinv[:, -1]

    for it, x in itertools.izip(xrange(3 * n), pgs_ls(A, b, p) ):
        pass

    
    return x

    for it, x in itertools.izip(xrange(3 * n), pocs(A, b, p)):
        pass

    return x

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

    glEnd()
    
    glBegin(GL_LINES)

    for i, j in itertools.product(xrange(4), xrange(4)):
        glVertex(S[i])
        glVertex(S[j])        

    glVertex(p)
    glVertex(q)
        
    glEnd()

    
    
    glEnable(GL_LIGHTING)    

viewer.run()
