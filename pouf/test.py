

import pouf

graph = pouf.graph()

dofs = pouf.dofs_vec3()
graph.add(dofs)

mass = pouf.uniform_mass_vec3()
graph.add(mass)

graph.connect(mass, dofs)
# graph.disconnect(dofs, mass)

compliance = pouf.uniform_compliance_vec3()

graph.add(compliance)
graph.connect(compliance, dofs)

simu = pouf.simulation()

dt = 0.1
simu.step(graph, dt)


