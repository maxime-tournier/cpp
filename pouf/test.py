

import pouf

graph = pouf.graph()

dofs = pouf.dofs_vec3()

dofs.pos = [[1, 2, 3]]
dofs.vel = 1


graph.add(dofs)

mass = pouf.uniform_mass_vec3()
graph.add(mass)

graph.connect(mass, dofs)
# graph.disconnect(dofs, mass)

norm2 = pouf.norm2_vec3()
graph.add(norm2)
graph.connect(norm2, dofs)

compliance = pouf.uniform_compliance_real()
graph.add(compliance)
graph.connect(compliance, norm2)

simu = pouf.simulation()

dt = 0.1



from snap import viewer
class Viewer(viewer.Viewer):
	def draw(self):
		glPointSize(5)
		glDisable(GL_LIGHTING)
		glColor(1, 1, 1)
		glBegin(GL_POINTS)
		glVertex(dofs.pos[0])
		glEnd()
		glEnable(GL_LIGHTING)

	def animate(self):
		simu.step(graph, dt)

from snap import qt

from snap.gl import *

import sys
app = qt.QApplication(sys.argv)
w = Viewer()
w.show()
app.exec_()
