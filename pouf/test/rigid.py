import pouf
import snap
from snap import viewer, gl
from snap.math import *

dofs = pouf.dofs_rigid()
mass = pouf.mass_rigid()

mass.inertia[0] = [1, 2, 3]


graph = pouf.graph()

graph.add(dofs)
graph.add(mass)
graph.connect(mass, dofs)


sim = pouf.simulation()

dofs.vel[0][3:] = [1, 1, 1]
sim.gravity = 0
dt = 0.01

def reset():
    sim.reset(graph)

def init():
    sim.init(graph)
    sim.step(graph, dt)

def animate():
    sim.step(graph, dt)

def draw():
    with gl.frame( dofs.pos[0].view(Rigid3) ):
        viewer.Viewer.draw_axis()


viewer.run()
