import pylab as plt
import numpy as np
import os
import json

f = 'cb_2018_us_state_20m'
if not os.path.exists('%s.zip' % f):
    import urllib.request
    urllib.request.urlretrieve(
        'https://www2.census.gov/geo/tiger/GENZ2018/shp/%s.zip' % f,
        '%s.zip' % f)

if not os.path.exists(f):
    import zipfile
    with zipfile.ZipFile('%s.zip' % f, 'r') as zip_ref:
        zip_ref.extractall(f)

if os.path.exists('triangles.json'):
    ts = json.load(open('triangles.json'))
else:
    # This is very slow + inefficient, but it only runs once :)
    import shapefile

    r = shapefile.Reader('%s/%s' % (f, f))
    def point_line_sign(pt, a, b):
        return np.linalg.det(
            np.hstack([np.vstack([a, b, pt]),
                       [[1], [1], [1]]])) > 0

    def point_in_triangle(pt, tri):
        sa = point_line_sign(pt, tri[0,:], tri[1,:])
        sb = point_line_sign(pt, tri[1,:], tri[2,:])
        sc = point_line_sign(pt, tri[2,:], tri[0,:])
        return sa == sb and sb == sc and sa == sc

    def triangle_sign(tri):
        return np.cross(tri[0,:] - tri[1,:], tri[2,:] - tri[1,:]) > 0

    def triangulate(pts):
        triangles = []
        while pts.shape[0] > 3:
            n = pts.shape[0]
            for v in range(0, n):
                tri = pts[[v - 1, v, (v + 1) % n], :]
                if not triangle_sign(tri):
                    continue

                for u in range(0, n):
                    if abs(u - v) > 1 and point_in_triangle(pts[u,:], tri):
                        break
                else:
                    triangles.append(tri)
                    pts = np.delete(pts, v, 0)
                    break
        triangles.append(pts)
        return triangles

    triangles = {}
    for (i,d) in enumerate(r.records()):
        shape = r.shape(i)
        parts = list(shape.parts) + [0]
        points = shape.points
        tris = []
        for (a,b) in zip(parts, parts[1:]):
            pts = np.array(points[a:b-1])
            tris += triangulate(pts)
        triangles[d[5]] = tris

    ts = {k:[q.tolist() for q in v] for (k,v) in triangles.items()}
    json.dump(ts, open('triangles.json','w'), indent=2)

# Convert each set of triangles into an indexed representation
indexed = {}
offset = 0
for (s,triangles) in ts.items():
    if s in ['District of Columbia', 'Puerto Rico']:
        continue
    vs = {}
    ti = []
    for tri in triangles:
        for row in tri:
            row = tuple(row)
            if not row in vs:
                i = len(vs)
                vs[row] = i
        t = [vs[tuple(row)] + offset for row in tri]
        ti.append(t)
    indexed[s] = (list(vs.keys()), ti)
    offset += len(vs.keys())

# Pack up the indexed representations into two big arrays
packed_verts = []
for (i,s) in enumerate(indexed):
    vertices = np.array(indexed[s][0])
    packed_verts.append(np.hstack([
        vertices,
        np.ones((vertices.shape[0], 1)) * (i + 1)]))
packed_verts = np.vstack(packed_verts)
packed_indexes = np.vstack([i[1] for i in indexed.values()])

# Special-case to wrap Alaskan island back around
packed_verts[packed_verts[:,0] > 90, 0] -= 360

def translate(state, scale, offset):
    i = list(indexed.keys()).index(state) + 1
    sel = (packed_verts[:,2] == i)
    ax = packed_verts[sel,0]
    ay = packed_verts[sel,1]
    (xmin, xmax) = (np.min(ax), np.max(ax))
    (ymin, ymax) = (np.min(ay), np.max(ay))
    xc = (xmax + xmin) / 2
    yc = (ymax + ymin) / 2
    packed_verts[sel,0] -= xc
    packed_verts[sel,0] *= scale
    packed_verts[sel,0] += xc + offset[0]

    packed_verts[sel,1] -= yc
    packed_verts[sel,1] *= scale
    packed_verts[sel,1] += yc + offset[1]

# Shrink and offset Alaska
translate('Alaska', 0.4, [20, -20])
translate('Hawaii', 1.0, [25, 15])

# Print a C file to stdout
print("// This file was generated by gen.py; do not edit by hand!\n")
print("#include <stdint.h>\n")
print("const unsigned STATES_COUNT = %u;" % len(indexed))
print("const char* STATES_NAMES[] = {")
for (i,s) in enumerate(indexed):
    print('    "' + s + '",');
print("};\n");

print("const unsigned STATES_VERT_COUNT = %u;" % (packed_verts.shape[0]))
print("const float STATES_VERTS[%u] = {" % packed_verts.size)
for row in packed_verts:
    print("    " + ",".join(map(str, row)) + ",")
print("};\n")
print("const unsigned STATES_TRI_COUNT = %u;" % (packed_indexes.shape[0]))
print("const uint16_t STATES_INDEXES[%u] = {" % packed_indexes.size)
for row in packed_indexes:
    print("    " + ",".join(map(str, row)) + ",")
print("};")

font = open('Inconsolata.ttf', 'rb').read()
print("const uint8_t FONT[] = {")
for (i,c) in enumerate(font):
    print("%i," % c, end='\n    ' if i and i % 16 == 0 else ' ')
print("\n};")
