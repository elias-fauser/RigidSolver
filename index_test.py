

edge_length = 10
particles_per_model = 3

def particleIdx2ParticleCoords(idx):
	x = int(idx % edge_length);
	y = int(idx / edge_length);

	print x, y

	return x, y

assert particleIdx2ParticleCoords(0) == (0, 0)
assert particleIdx2ParticleCoords(1) == (1, 0)
assert particleIdx2ParticleCoords(2) == (2, 0)
assert particleIdx2ParticleCoords(9) == (9, 0)
assert particleIdx2ParticleCoords(10) == (0, 1)
assert particleIdx2ParticleCoords(99) == (9, 9)

"""
12 ...
10 10 11 11 11 12 12
7 7 7 8 8 8 9 9 9 10
3 3 4 4 4 5 5 5 6 6
0 0 0 1 1 1 2 2 2 3
"""
v1 = [1, 0, -2]
v2 = [5, 9, 2]

v1_v2 = [v1[i] - v2[i] for i in range(len(v1))]
v2_v1 = [v2[i] - v1[i] for i in range(len(v1))]

print v1_v2
print v2_v1