//===========================================================================
//                                                                           
// File: VolumeModel.h                                                    
//                                                                           
// Created: January 2010
//                                                                           
// Author: Vibeke Skytt
//                                                                           
// Revision: 
//                                                                           
// Description:
//                                                                           
//===========================================================================

#ifndef _VOLUMEMODEL_H
#define _VOLUMEMODEL_H

#include "GoTools/compositemodel/CompositeModel.h"
#include "GoTools/trivariatemodel/ftVolume.h"
#include "GoTools/trivariate/SplineVolume.h"

namespace Go
{
  class IntResultsModel;
  class GeneralMesh;

  /// \brief A set of volumes including topology information.

  class VolumeModel : public CompositeModel
  {
  public:

  /// Constructor given a set of volumes and topologic tolerances
    VolumeModel(std::vector<std::shared_ptr<ftVolume> >& volumes,
		double gap, double neighbour, double kink, double bend);

  /// Constructor given a set of volumes and topologic tolerances
    VolumeModel(std::vector<std::shared_ptr<ftVolume> >& volumes,
		double gap, double kink);

  /// Constructor topologic tolerances
    VolumeModel(double gap, double neighbour, double kink, double bend);

    /// Copy constructor
    VolumeModel(const VolumeModel& vm);

  /// Destructor
  virtual ~VolumeModel();

  /// Make a copy of the current model
  virtual VolumeModel* clone() const
  {
    return new VolumeModel(*this);
  }

  /// Number of simple entities
  virtual int nmbEntities() const;

  /// Return one body
  std::shared_ptr<ftVolume> getBody(int idx) const;

  /// Return one volume
  std::shared_ptr<ParamVolume> getVolume(int idx) const;

  /// Return one volume as SplineVolume if possible
  std::shared_ptr<SplineVolume> getSplineVolume(int idx) const;

  /// Given a body in the volume model, return the index of this face
  int getIndex(std::shared_ptr<ftVolume> body) const;

  /// Given a body in the volume model, return the index of this face
  int getIndex(ftVolume* body) const;

  /// Return a specified body as a shared pointer
  std::shared_ptr<ftVolume> fetchAsSharedPtr(Body *body) const;

  /// Evaluate position
  virtual void evaluate(int idx,      // Index of surface
			double par[], // Parameter value
			Point& pnt) const;  // Result

  /// Evaluate position and a number of dervivatives
  virtual void evaluate(int idx,      // Index
			double par[], // Parameter value
			int nder,     // Number of derivatives to compute, 0=only position
			std::vector<Point>& der) const;  // Result

  /// Compute one closest point, interface heritage, not implemented
  virtual void
    closestPoint(Point& pnt,     // Input point
		 Point& clo_pnt, // Found closest point
		 int& idx,           // Index of surface where the closest point is found
		 double clo_par[],   // Parameter value corrsponding to the closest point
		 double& dist);  // Distance between input point and found closest point

  /// Intersection with a line, interface heritage, not implemented. 
  /// Expected output is points, probably one point. Curves 
  /// can occur in special configurations. 
     virtual std::shared_ptr<IntResultsModel> intersect(const ftLine& line);

  /// Intersection with a plane, interface heritage, not implemented. 
     virtual std::shared_ptr<IntResultsModel> intersect_plane(const ftPlane& plane);

  // Extremal point(s) in a given direction, interface heritage, not implemented
  virtual void
    extremalPoint(Point& dir,     // Direction
		  Point& clo_pnt, // Found closest point
		  int& idx,           // Index of surface where the closest point is found
		  double ext_par[]);   // Parameter value of extremal point

  /// Bounding box of the entire model
  virtual BoundingBox boundingBox();

  /// Bounding box corresponding to one entity
  virtual BoundingBox boundingBox(int idx) const;  // Index of entity

  /// Whether one particular entity is degenerate, interface heritage, 
  /// not implemented
  virtual bool isDegenerate(int idx) const;

  /// Curvature, interface heritage, not implemente
  // Not yet implemented
  // @@@ Not obvious how curvature of a volume should be defined
  virtual double curvature(int idx, // Index of entity
			   double *par) const;  // Parameter value at which to compute curvature
  /// Interface heritage, not implemented
  // @@@ Not obvious how to turn a trivariate volume
  virtual void turn(int idx);  // Turn parameter directions of one entity

  /// Interface heritage, not implemented
  virtual void turn();   // Turn parameter directions of all entities

  /// Append a new body to the volume model. The body is included in the topological
  /// structure
  void append(std::shared_ptr<ftVolume> volume);

  /// Append a vector of bodies to the volume model. The bodies are included in the topological
  /// structure
  void append(std::vector<std::shared_ptr<ftVolume> > volumes);

  /// Append all bodies from another volume model. The bodies are included in the topological
  /// structure
  void append(std::shared_ptr<VolumeModel> anotherModel);

  /// Remove one volume from the model
  void removeSolid(std::shared_ptr<ftVolume> vol);

  /// Tesselate model, interface heritage, not implemented
  virtual void tesselate(std::vector<std::shared_ptr<GeneralMesh> >& meshes) const;

  /// Tesselate model, interface heritage, not implemented
  virtual
  void tesselate(int resolution[],
		 std::vector<std::shared_ptr<GeneralMesh> >& meshes) const;

  /// Tesselate model, interface heritage, not implemented
  virtual
  void tesselate(double density,
		 std::vector<std::shared_ptr<GeneralMesh> >& meshes) const;

  /// Tesselate model, interface heritage, not implemented
  virtual 
    void tesselatedCtrPolygon(std::vector<std::shared_ptr<LineCloud> >& ctr_pol) const;

  /** Construct the topology information regarding the input geometry */
  /** Construct the topology information regarding the input geometry for all pairs of volumes */
  void buildTopology();

  /** Construct the topology information regarding the input geometry involving a given volume */
  void buildTopology(std::shared_ptr<ftVolume> body);

  /** Add topology information regarding degenerate volumes meeting along an
      edge */
  void setVertexIdentity();

  /** Compute boundary shells */
  void setBoundarySfs();

  /** Fetch all faces at the outer boundary of this model */
  std::vector<std::shared_ptr<ftSurface> > getBoundaryFaces() const;

  /** Fetch all faces at one of the boundaries of this model
      \param boundary_idx Index of one boundary
      \retval faces Vector of pointers to the faces at one boundary.*/
  std::vector<std::shared_ptr<ftSurface> > getBoundaryFaces(int boundary_idx) const;

  /** Fetch all interval unique inner faces in this model, i.e.
      a ftSurface for each boundary face with a twin, only one of the
      surfaces in the pair is returned */
  std::vector<std::shared_ptr<ftSurface> > getUniqueInnerFaces() const;

  /// Return approximation tolerance.
  /// \return Approximation tolerance.
  double getApproximationTol() const
  {
    return approxtol_;
  }

  /// Check if all entities are NURBS
  bool allSplines() const;

  /// Fetch all vertices in the model
  void getAllVertices(std::vector<std::shared_ptr<Vertex> >& vertices) const;

  /// Fetch all radial edges in the model
  void 
    getRadialEdges(std::vector<std::shared_ptr<EdgeVertex> >& rad_edges) const;

    /** Fetch edges where no radial edge exist, i.e. there is no 
	ajacent volume along any boundary surface meeting in this edge.
	Only one occurance in an edge, twin-edge pair is returned */
  void
    uniqueNonRadialEdges(std::vector<std::shared_ptr<ftEdge> >& edges ) const;

  /// Check if the model has got a corner-to-corner configuaration
  bool isCornerToCorner(double tol = DEFAULT_SPACE_EPSILON) const;

  /// Ensure that the blocks in the model meet in a corner-to-corner
  /// configuration
  void makeCornerToCorner(double tol = DEFAULT_SPACE_EPSILON);

  /// Ensure that the blocks in the model has got common spline spaces
  void makeCommonSplineSpaces();

  /// Ensure exact match between corresponding coefficients
  void averageCorrespondingCoefs();

  /** Return the number of boundaries of a volume set (including holes). */
  int nmbBoundaries() const;

   /// Fetch specfied boundary of this model
  std::shared_ptr<SurfaceModel> getOuterBoundary(int idx) const;

  /// Divide this model into connected volume model
  std::vector<std::shared_ptr<VolumeModel> > getConnectedModels();

  /// Update the model by regularizing all boundary shells,
  /// i.e. all faces in all boundary shells of all connected volumes
  /// should be 4-sided, and no T-joints are allowed
  void regularizeBdShells();

  /// Replaces volumes that are not regular with sets of regular volumes
  void replaceNonRegVolumes();

  private:
  /// Geometric description of the volumes
  std::vector<std::shared_ptr<ftVolume> > bodies_;

  /// For each separate object, we store all boundary shells
  /// First element is (what is supposed to be) the objects outer boundary.
  std::vector<std::vector<std::shared_ptr<SurfaceModel> > > boundary_shells_;

  double approxtol_;

  /// Local storage of intersection results
  typedef struct intersection_point 
  {
    /// Intersection parameter of external curve
    double par;
    /// Index of surface model
    int shell_idx;

    intersection_point(double p, int idx)
    {
      par = p;
      shell_idx = idx;
    }

  } intersection_point;

    // Comparisement function to use in std::sort
    static bool par_compare(const intersection_point& el1,
			    const intersection_point& el2)
    {
	if (el1.par < el2.par)
	    return true;
	else
	    return false;
    }
   
    std::vector<intersection_point> 
      getIntSfModelsCrv(std::vector<std::shared_ptr<SurfaceModel> >& models, 
			std::shared_ptr<SplineCurve> crv) const;

    bool findBoundaryShell(std::shared_ptr<SurfaceModel> model,
			   size_t& idx1, size_t& idx2) const;

    void 
      getCurrConnectedModel(std::shared_ptr<ftVolume>& vol,
			    std::vector<std::shared_ptr<ftVolume> >& curr_set,
			    std::vector<std::shared_ptr<ftVolume> >& all_sets) const;

    /// Average corners of spline volumes corresponding to a given vertex
    void averageVolCorner(Vertex* vx);

    void averageVolBoundaries(EdgeVertex* edge);


  };

} // namespace Go

#endif // _VOLUMEMODEL_H