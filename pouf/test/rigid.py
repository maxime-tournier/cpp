import pouf
import snap
from snap import viewer, gl
from snap.math import *

dofs = pouf.dofs_rigid()
mass = pouf.mass_rigid()

mass.mass[0] = 5

axis = vec(0.5, 1.0, 1.5)
axis2 = axis * axis
mass.inertia[0] = mass.mass[0] * (sum(axis2) - axis2) / 5


graph = pouf.graph()

graph.add(dofs)
graph.add(mass)
graph.connect(mass, dofs)


sim = pouf.simulation()

dofs.vel[0][3:] = 3 * vec(1.0, 1.0, 1.0)
sim.gravity = 0
dt = 0.01

def reset():
    sim.reset(graph)

def init():
    sim.init(graph)
    sim.step(graph, dt)

    gl.glEnable(gl.GL_NORMALIZE)
    
def animate():
    sim.step(graph, dt)

inv = np.linalg.inv( np.ones( (3, 3) ) - np.identity(3) )
print(inv)

def draw():
    with gl.frame( dofs.pos[0].view(Rigid3) ):

        scale = np.sqrt( inv.dot(mass.inertia[0] / mass.mass * 5.0) )

        am = mass.inertia[0] * dofs.vel[0].view(Rigid3.Deriv).angular
        
        with gl.lookat(am):
            gl.glColor(1, 1, 1)
            gl.arrow( height = norm(am) )
        
        for i in range(3):
            axis = np.zeros(3)
            axis[i] = 1
            gl.glColor(*axis)
            with gl.lookat(axis):
                gl.arrow( height = scale[i])
            

viewer.run()
