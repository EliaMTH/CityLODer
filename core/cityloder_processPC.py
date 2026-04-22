import laspy
from scipy.spatial import cKDTree
from scipy.spatial import ConvexHull
import numpy as np

# TODO: classification step if no classification is detected

def read_las_file(file_path, class_building = 6, class_ground = 2):
    """
    Reads a LAS file and extracts XYZ coordinates and classification.

    Parameters:
        file_path (str): Path to the .las file.

    Returns:
        coords (np.ndarray): Nx3 array of XYZ coordinates.
        classification (np.ndarray): N-length array of classifications.
    """
    # Open the LAS file
    las = laspy.read(file_path)

    
    # Extract XYZ coordinates as Nx3 array
    coords = np.vstack((las.x, las.y, las.z)).T
    
    # Extract classification
    classification = np.array(las.classification)

    xyz_ground = coords[classification == class_ground, :].astype(float)
    xyz_building = coords[classification == class_building, :].astype(float)
    
    return xyz_building, xyz_ground, coords

def generate_ground_polygon(xyz, other, outname="ground_polygon", offset=50.0):
    """
    Build a fast 2D convex hull from the XY coordinates of `xyz` and `other`,
    enlarge it by `offset`, assign Z from nearest XY point in `other`,
    then export a single-face OFF polygon as `<outname>.off`.
    """
    xyz = np.asarray(xyz, dtype=np.float64)
    other = np.asarray(other, dtype=np.float64)
    aus = np.vstack((other, xyz))

    pts2d = aus[:, :2]

    # Remove exact duplicate XY points first: often a large speed win
    pts2d = np.unique(pts2d, axis=0)

    # -------------------
    # Fast 2D convex hull
    # -------------------
    pts2d = np.asarray(pts2d, dtype=float)

    hull2d = pts2d[ConvexHull(pts2d).vertices]

    # Ensure CCW orientation
    def signed_area(poly):
        x = poly[:, 0]
        y = poly[:, 1]
        return 0.5 * np.sum(x * np.roll(y, -1) - y * np.roll(x, -1))

    if len(hull2d) >= 3 and signed_area(hull2d) < 0:
        hull2d = hull2d[::-1]

    # ----------------------------------------
    # Exact convex polygon outward XY offset
    # ----------------------------------------
    def line_intersection(p1, d1, p2, d2):
        # Solve p1 + t*d1 = p2 + s*d2
        denom = d1[0] * d2[1] - d1[1] * d2[0]
        if abs(denom) < 1e-12:
            # Nearly parallel: fallback to average of shifted vertices
            return 0.5 * (p1 + p2)
        diff = p2 - p1
        t = (diff[0] * d2[1] - diff[1] * d2[0]) / denom
        return p1 + t * d1

    def offset_convex_polygon(poly, dist):
        n = len(poly)
        if dist == 0 or n < 3:
            return poly.copy()

        out = np.empty_like(poly)

        for i in range(n):
            p_prev = poly[i - 1]
            p_curr = poly[i]
            p_next = poly[(i + 1) % n]

            e1 = p_curr - p_prev
            e2 = p_next - p_curr

            n1 = np.array([e1[1], -e1[0]], dtype=np.float64)
            n2 = np.array([e2[1], -e2[0]], dtype=np.float64)

            l1 = np.linalg.norm(n1)
            l2 = np.linalg.norm(n2)
            if l1 < 1e-12 or l2 < 1e-12:
                out[i] = p_curr
                continue

            n1 /= l1
            n2 /= l2

            # Shift the two incident edges outward, then intersect them
            a1 = p_prev + dist * n1
            a2 = p_curr + dist * n1
            b1 = p_curr + dist * n2
            b2 = p_next + dist * n2

            d1 = a2 - a1
            d2 = b2 - b1

            out[i] = line_intersection(a1, d1, b1, d2)

        return out

    hull2d = offset_convex_polygon(hull2d, float(offset))

    # ----------------------------------------
    # Assign Z from nearest XY point in `other`
    # ----------------------------------------
    tree = cKDTree(other[:, :2])
    _, idx = tree.query(hull2d)
    hull3d = np.column_stack((hull2d, other[idx, 2]))

    # Write OFF
    off_path = outname if outname.lower().endswith(".off") else f"{outname}.off"
    with open(off_path, "w") as f:
        f.write("OFF\n")
        f.write(f"{len(hull3d)} 1 0\n")
        for x, y, z in hull3d:
            f.write(f"{x:.12g} {y:.12g} {z:.12g}\n")
        face_idx = " ".join(map(str, range(len(hull3d))))
        f.write(f"{len(hull3d)} {face_idx}\n")

    return hull3d








if __name__ == "__main__":
    xyz_building, xyz_ground, coords = read_las_file("matera.las", class_building = 6, class_ground = 2)
    generate_ground_polygon(xyz_building, xyz_ground)
