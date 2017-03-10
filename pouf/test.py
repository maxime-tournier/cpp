

import pouf

graph = pouf.graph()

dofs = pouf.dofs_vec3()
graph.add(dofs)

mass = pouf.uniform_mass_vec3()
graph.add(mass)

graph.connect(dofs, mass)
graph.disconnect(dofs, mass)

print(len(graph))

