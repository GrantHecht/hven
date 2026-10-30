import numpy as np, scipy.sparse as sp
def load(path):
    d = {}
    with open(path) as f:
        hdr = f.readline().split(); d['n'], d['me'], d['mi'] = int(hdr[1]), int(hdr[3]), int(hdr[5])
        while True:
            line = f.readline()
            if not line: break
            t = line.split()
            if len(t) == 2:
                k = int(t[1]); d[t[0]] = np.array([float(f.readline()) for _ in range(k)])
            else:
                r, c, nz = int(t[1]), int(t[2]), int(t[3]); rows, cols, vals = [], [], []
                for _ in range(nz):
                    a, b, v = f.readline().split(); rows.append(int(a)); cols.append(int(b)); vals.append(float(v))
                d[t[0]] = sp.csr_matrix((vals, (rows, cols)), shape=(r, c))
    return d
