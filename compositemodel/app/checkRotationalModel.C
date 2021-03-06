/*
 * Copyright (C) 1998, 2000-2007, 2010, 2011, 2012, 2013 SINTEF ICT,
 * Applied Mathematics, Norway.
 *
 * Contact information: E-mail: tor.dokken@sintef.no                      
 * SINTEF ICT, Department of Applied Mathematics,                         
 * P.O. Box 124 Blindern,                                                 
 * 0314 Oslo, Norway.                                                     
 *
 * This file is part of GoTools.
 *
 * GoTools is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version. 
 *
 * GoTools is distributed in the hope that it will be useful,        
 * but WITHOUT ANY WARRANTY; without even the implied warranty of         
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public
 * License along with GoTools. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * In accordance with Section 7(b) of the GNU Affero General Public
 * License, a covered work must retain the producer line in every data
 * file that is created or manipulated using GoTools.
 *
 * Other Usage
 * You can be released from the requirements of the license by purchasing
 * a commercial license. Buying such a license is mandatory as soon as you
 * develop commercial activities involving the GoTools library without
 * disclosing the source code of your own applications.
 *
 * This file may be used in accordance with the terms contained in a
 * written agreement between you and SINTEF ICT. 
 */

#include "GoTools/compositemodel/SurfaceModel.h"
#include "GoTools/compositemodel/CompositeModelFactory.h"
#include "GoTools/geometry/SweepSurfaceCreator.h"
#include <fstream>

using namespace Go;

int main( int argc, char* argv[] )
{
  if (argc != 3) {
    std::cout << "Input parameters : Input file(g2), output file" << std::endl;
    exit(-1);
  }

  // Read input arguments
  std::ifstream file1(argv[1]);
  ALWAYS_ERROR_IF(file1.bad(), "Input file not found or file corrupt");

  std::ofstream file2(argv[2]);

  double gap = 0.001;
  double neighbour = 0.01;
  double kink = 0.01;
  double approxtol = 0.01;

  CompositeModelFactory factory(approxtol, gap, neighbour, kink, 10.0*kink);

  CompositeModel *model = factory.createFromG2(file1);

  SurfaceModel *sfmodel = dynamic_cast<SurfaceModel*>(model);

   if (sfmodel)
  {
    Point centre, axis, vec;
    double angle;

    double min_ang;
    bool rotational = sfmodel->isAxisRotational(centre, axis, vec, angle,
						min_ang);
    std::cout << "Axis rotational: " << rotational << std::endl;

    if (rotational)
      {
	std::cout << "Centre: " << centre << std::endl;
	std::cout << "Axis: " << axis << std::endl;
	std::cout << "Startvector: " << vec << std::endl;
	std::cout << "Angle: " << angle << std::endl;

	BoundingBox box = sfmodel->boundingBox();
	std::cout << "Box: " << box.low() << std::endl;
	std::cout << box.high() << std::endl;

	// Compute intersections with box
	vector<Point> result;
	vector<Point> res = box.lineIntersect(centre, axis);
	if (res.size() > 0)
	  result.insert(result.end(), res.begin(), res.end());
	res.clear();
	res = box.lineIntersect(centre, -1.0*axis);
	if (res.size() > 0)
	  result.insert(result.end(), res.begin(), res.end());
	res.clear();
	Point mid(0.0, 0.0, 0.0);
	for (size_t kj=0; kj<result.size(); ++kj)
	  mid += result[kj];
	mid /= (double)(result.size());
	res = box.lineIntersect(mid, vec);
	if (res.size() > 0)
	  result.insert(result.end(), res.begin(), res.end());
	std::ofstream of("hitpt.g2");
	for (size_t ki=0; ki<result.size(); ++ki)
	  {
	    of << "400 1 0 4 255 0 0 255" << std::endl;
	    of << "1" << std::endl;
	    of << result[ki] << std::endl;
	  }

	if (result.size() == 3)
	  {
	    // Create planar B-spline surface interpolating these three points
	    Point vec1 = result[1] - result[0];
	    shared_ptr<SplineCurve> cv1(new SplineCurve(result[0]-0.1*vec1,
							result[1]+0.1*vec1));

	    Point vec2 = result[2] - mid;
	    shared_ptr<SplineCurve> cv2(new SplineCurve(mid, result[2]+0.1*vec2));

	    SweepSurfaceCreator sweep;
	    shared_ptr<SplineSurface> surf(sweep.linearSweptSurface(*cv1, *cv2, mid));

	    // Make surface model
	    vector<shared_ptr<ParamSurface> > sfs;
	    sfs.push_back(surf);

	    shared_ptr<SurfaceModel> model2(new SurfaceModel(approxtol, gap, neighbour, 
							     kink, 10.0*kink, sfs));

	    // Intersect
	    vector<shared_ptr<SurfaceModel> > submodels = 
	      sfmodel->splitSurfaceModels(model2);

	    std::cout << "Number of split models: " << submodels.size() << std::endl;
	    int nmb_faces = submodels[2]->nmbEntities();
	    for (int kr=0; kr<nmb_faces; ++kr)
	      {
		shared_ptr<ParamSurface> sf = submodels[3]->getSurface(kr);
		sf->writeStandardHeader(file2);
		sf->write(file2);
	      }
	  }
      }
  }
}
