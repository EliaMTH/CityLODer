import laspy
from scipy.spatial import KDTree
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


def minBoundingBox(X):
    """
    Computation of minimal bounding box of 2D points. 

    Parameters:
        - X (np.ndarray): Nx3 list of points
    
    Returns:
        - bb (np.ndarray): 2×4 array of bounding box corners
    """

    # ---- convex hull ----
    points = X.T  
    hull = ConvexHull(points)
    CH = X[:, hull.vertices]   # 2 × k

    # ---- angles of convex hull edges ----
    E = np.diff(CH, axis=1)    # 2 × (k-1)
    T = np.arctan2(E[1, :], E[0, :])
    T = np.mod(T, np.pi/2)
    T = np.unique(T)

    # ---- construct the R matrix
    A = np.tile(T, (2, 2)) 
    A = np.reshape(A, (2*len(T), 2), order='F')

    B = np.array([[0, -np.pi], [np.pi, 0]]) / 2
    B = np.tile(B, (len(T), 1))

    R = np.cos(A + B)  

    # ---- rotate convex hull CH using all angles ----
    RCH = R @ CH 
    RCH = RCH

    # ---- compute bounding sizes for each rotation ----
    bsize = np.max(RCH, axis=1) - np.min(RCH, axis=1)  
    bsize = bsize.reshape(2, -1, order='F')            
    area = np.prod(bsize, axis=0)                  

    # ---- find minimal area ----
    i = np.argmin(area)

    # ---- compute bounding box in rotated frame ----
    row1 = 2*i - 2 
    row2 = 2*i - 1
    Rf = np.vstack((R[row1, :], R[row2, :]))     

    bound = Rf @ CH
    bmin = np.min(bound, axis=1)
    bmax = np.max(bound, axis=1)

    Rf = Rf.T

    bb = np.zeros((2, 4))

    bb[:, 3] = bmax[0] * Rf[:, 0] + bmin[1] * Rf[:, 1]
    bb[:, 0] = bmin[0] * Rf[:, 0] + bmin[1] * Rf[:, 1]
    bb[:, 1] = bmin[0] * Rf[:, 0] + bmax[1] * Rf[:, 1]
    bb[:, 2] = bmax[0] * Rf[:, 0] + bmax[1] * Rf[:, 1]

    return bb


def generate_ground_polygon(xyz,other,outname = "ground_polygon"):
    
    # Compute min bounding box on (other; xyz)
    aus = np.vstack((other, xyz))
    mBB = minBoundingBox(aus[:, :2].T)      

    xy = np.hstack((mBB.T, np.zeros((mBB.shape[1], 1))))

    xy = np.zeros((mBB.shape[1], 3))
    xy[:, :2] = mBB.T

    
    # Add small space around BB  (T = 100)
    T = 100
    xy[0, 0] += T;   xy[0, 1] -= T
    xy[1, 0] -= T;   xy[1, 1] -= T
    xy[2, 0] -= T;   xy[2, 1] += T
    xy[3, 0] += T;   xy[3, 1] += T

    
    # REFINE BOUNDARY (XY)
    xy_res = 15
    xy_aus = np.vstack((xy, xy[0]))   # append first point to close

    pp_list = []
    for j in range(4):
        x_row = np.linspace(xy_aus[j,0], xy_aus[j+1,0], xy_res+1)
        y_row = np.linspace(xy_aus[j,1], xy_aus[j+1,1], xy_res+1)
        pts = np.column_stack((x_row[:-1], y_row[:-1]))
        pp_list.append(pts)

    xy = np.vstack(pp_list)

    
    # Project on ground points "other"
    tree = KDTree(other[:, :2])
    _, idx = tree.query(xy[:, :2])
    xy = np.column_stack((xy, other[idx, 2]))

    # Reverse 
    xy = xy[::-1, :]

    
    # SAVE OFF FILE
    outpath = outname if outname.endswith(".off") else outname + ".off"

    with open(outpath, "w") as f:
        f.write("OFF\n")
        f.write(f"{xy.shape[0]} 1 0\n")

        # vertices
        for row in xy:
            f.write(f"{row[0]:.8f} {row[1]:.8f} {row[2]:.8f}\n")

        # face
        indices = " ".join(str(i) for i in range(len(xy)))
        f.write(f"{len(xy)} {indices}\n")

    print("Saved boundary polygon:", outpath)
