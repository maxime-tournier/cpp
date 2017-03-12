

import pouf

graph = pouf.graph()

dofs = pouf.dofs_vec3()

dofs.pos = [[1, 2, 3]]
dofs.vel = 1

print(dofs.pos)


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

# ff = pouf.uniform_stiffness_real()
# graph.add(ff)
# graph.connect(ff, norm2)

simu = pouf.simulation()

dt = 0.1
simu.step(graph, dt)


