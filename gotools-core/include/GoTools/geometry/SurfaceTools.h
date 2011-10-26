//===========================================================================
//                                                                           
// File: SurfaceTools.h                                                     
//                                                                           
// Created: November 2009
//                                                                           
// Author: Vibeke Skytt
//                                                                           
// Revision: 
//                                                                           
//===========================================================================

#ifndef _SURFACETOOLS_H
#define _SURFACETOOLS_H

#include "GoTools/geometry/SplineSurface.h"
#include "GoTools/geometry/SplineCurve.h"
#include "GoTools/geometry/CurveOnSurface.h"
#include "GoTools/geometry/CurveLoop.h"
#include "GoTools/geometry/RectDomain.h"


// Free functions operating on parametric surfaces

namespace Go
{

  // Fetch the outer boundary loop of a parametric surface
  CurveLoop outerBoundarySfLoop(std::shared_ptr<ParamSurface> surf,
				double degenerate_epsilon);

  // Fetch all boundary loops of a parametric surface
  std::vector<CurveLoop> allBoundarySfLoops(std::shared_ptr<ParamSurface> surf,
					    double degenerate_epsilon); 

  // Fetch all boundary loops of a parametric surface including degenerate
  // curves
  std::vector<CurveLoop> 
    absolutelyAllBoundarySfLoops(std::shared_ptr<ParamSurface> surf,
				 double degenerate_epsilon); 
  // Iterate to an optimal corner where a number of surfaces meet
  // Positions on elementary surfaces are emphasized
  void 
    iterateCornerPos(Point& vertex, 
		     std::vector<std::pair<std::shared_ptr<ParamSurface>, Point> > sfs,
		     double tol);

  bool cornerToCornerSfs(std::shared_ptr<ParamSurface> sf1,
			 std::shared_ptr<CurveOnSurface> sf_cv1,
			 std::shared_ptr<ParamSurface> sf2,
			 std::shared_ptr<CurveOnSurface> sf_cv2,
			 double tol);

  bool getSfAdjacencyInfo(std::shared_ptr<ParamSurface> sf1,
			  std::shared_ptr<CurveOnSurface> sf_cv1,
			  std::shared_ptr<ParamSurface> sf2,
			  std::shared_ptr<CurveOnSurface> sf_cv2,
			  double tol,
			  int& bd1, int& bd2, bool& same_orient);

  bool getCorrCoefEnum(std::shared_ptr<SplineSurface> sf1,
		       std::shared_ptr<SplineSurface> sf2,
		       int bd1, int bd2, bool same_orient,
		       std::vector<std::pair<int,int> >& enumeration);

  bool getCoefEnumeration(std::shared_ptr<SplineSurface> sf, int bd,
			  std::vector<int>& enumeration);

  void surface_seedfind(const Point& pt, 
			const ParamSurface& sf, 
			const RectDomain* rd,
			double& u,
			double& v);

  double estimateTangentLength(SplineSurface *surf, int pardir, 
			       bool at_start);
} // namespace Go





#endif // _SURFACETOOLS_H
