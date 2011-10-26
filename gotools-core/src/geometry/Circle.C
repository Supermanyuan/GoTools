//==========================================================================
//                                                                          
// File: Circle.C                                                            
//                                                                          
// Created: Thu Oct 16 15:18:51 2008                                         
//                                                                          
// Author: Jan B. Thomassen <jbt@sintef.no>
//                                                                          
// Revision: $Id: Circle.C,v 1.10 2009-03-03 13:12:01 jbt Exp $
//                                                                          
// Description:
//                                                                          
//==========================================================================


#include "GoTools/geometry/Circle.h"
#include "GoTools/geometry/SplineCurve.h"
#include "GoTools/geometry/GeometryTools.h"
#include <vector>


using std::vector;
using std::istream;
using std::ostream;
using std::cout;
using std::endl;
using std::shared_ptr;


namespace Go {


// // Default constructor. Constructs a 3D circle.
// //===========================================================================
// Circle::Circle()
//     : radius_(0.0), centre_(Point(0, 0, 0)),
//       normal_(Point(0, 0, 1)), vec1_(Point(1, 0, 0)),
//       startparam_(0.0), endparam_(2.0 * M_PI)
// //===========================================================================
// {
//     setSpanningVectors();
// }


// Constructor
//===========================================================================
Circle::Circle(double radius,
	       Point centre, Point normal, Point x_axis)
    : radius_(radius), centre_(centre),
      normal_(normal), vec1_(x_axis),
      startparam_(0.0), endparam_(2.0 * M_PI)
//===========================================================================
{
    int dim = centre.dimension();
    if (dim != 2 && dim != 3)
	THROW("Dimension must be 2 or 3");

    if (dim == 3)
	normal_.normalize();
    setSpanningVectors();
}


// Destructor
//===========================================================================
Circle::~Circle()
//===========================================================================
{
}

//===========================================================================
void Circle::read(std::istream& is)
//===========================================================================
{
    bool is_good = is.good();
    if (!is_good) {
	THROW("Invalid geometry file!");
    }

    int dim;
    is >> dim;
    centre_.resize(dim);
    normal_.resize(dim);
    vec1_.resize(dim);
    is >> radius_
       >> centre_
       >> normal_
       >> vec1_;

    if(dim == 3)
	normal_.normalize();
    setSpanningVectors();

    // Not supported in read/write, but set parameter bounds as well
    startparam_ = 0.0;
    endparam_ = 2.0 * M_PI;

}


//===========================================================================
void Circle::write(std::ostream& os) const
//===========================================================================
{
    int dim = dimension();
    os << dim << endl
       << radius_ << endl
       << centre_ << endl
       << normal_ << endl
       << vec1_ << endl;
    
}


//===========================================================================
BoundingBox Circle::boundingBox() const
//===========================================================================
{
    // A rather unefficient hack...
    Circle* circ = const_cast<Circle*>(this);
    SplineCurve* tmp = circ->geometryCurve();
    BoundingBox box = tmp->boundingBox();
    delete tmp;
    return box;
}


//===========================================================================
int Circle::dimension() const
//===========================================================================
{
    // Should be 2 or 3
    return centre_.dimension();
}


//===========================================================================
ClassType Circle::instanceType() const
//===========================================================================
{
    return classType();
}


//===========================================================================
ClassType Circle::classType()
//===========================================================================
{
    return Class_Circle;
}


//===========================================================================
Circle* Circle::clone() const
//===========================================================================
{
    Circle* circle = new Circle(radius_, centre_, normal_, vec1_);
    circle->setParamBounds(startparam_, endparam_);
    return circle;
}


//===========================================================================
void Circle::point(Point& pt, double tpar) const
//===========================================================================
{
    pt = centre_ + radius_ * (cos(tpar) * vec1_ + sin(tpar) * vec2_);
}



//===========================================================================
void Circle::point(vector<Point>& pts,
		   double tpar,
		   int derivs,
		   bool from_right) const
//===========================================================================
{
    DEBUG_ERROR_IF(derivs < 0, 
		   "Negative number of derivatives makes no sense.");
    int totpts = (derivs + 1);
    int ptsz = (int)pts.size();
    DEBUG_ERROR_IF(ptsz < totpts, 
		   "The vector of points must have sufficient size.");

    int dim = dimension();
    for (int i = 0; i < totpts; ++i) {
        if (pts[i].dimension() != dim) {
            pts[i].resize(dim);
	}
	pts[i].setValue(0.0);
    }

    point(pts[0], tpar);
    if (derivs == 0)
        return;

    // We use a trick that holds for a circle C(t) at the origin: The
    // n'th derivative of C equals C(t + n*pi/2).
    for (int i = 1; i <= derivs; ++i) {
	point(pts[i], tpar + i*0.5*M_PI);
	pts[i] -= centre_;
    }
    return;

}


//===========================================================================
double Circle::startparam() const
//===========================================================================
{
    return startparam_;
}


//===========================================================================
double Circle::endparam() const
//===========================================================================
{
    return endparam_;
}


//===========================================================================
void Circle::reverseParameterDirection(bool switchparam)
//===========================================================================
{
    if (switchparam) {
	if (dimension() == 2) {
	    Point tmp = vec1_;
	    vec1_ = vec2_;
	    vec2_ = tmp;
	}
	return;
    }

    // Flip
    normal_ = -normal_;
    vec2_ = -vec2_;

    // Rotate to keep parametrization consistent
    double alpha = startparam_ + endparam_;
    if (alpha >= 2.0 * M_PI)
	alpha -= 2.0 * M_PI;
    if (alpha <= -2.0 * M_PI)
	alpha += 2.0 * M_PI;
    if (alpha != 0.0) {
	rotatePoint(normal_, -alpha, vec1_);
	rotatePoint(normal_, -alpha, vec2_);
    }
}


//===========================================================================
void Circle::setParameterInterval(double t1, double t2)
//===========================================================================
{
    setParamBounds(t1, t2);
}


//===========================================================================
SplineCurve* Circle::geometryCurve()
//===========================================================================
{
    // Based on SISL function s1522.

    double tworoot = sqrt ((double) 2.0);
    double weight  = (double) 1.0 / tworoot;
    double factor = 2.0 * M_PI;

    // Knot vector
    double et[12];
    et[0] = 0.0;
    int i;
    for ( i=1;  i < 3;  i++ ) {
	et[i]     = 0.0;
	et[2 + i] = factor * 0.25;
	et[4 + i] = factor * 0.5;
	et[6 + i] = factor * 0.75;
	et[8 + i] = factor;
    }
    et[11] = factor;

    // Vertices
    double coef[36];
    int dim = dimension();
    Point axis1 = radius_ * vec1_;
    Point axis2 = radius_ * vec2_;
    if (dim == 2) {
	for ( i=0;  i < 2;  i++ ) {
	    coef[     i] = centre_[i] + axis1[i];
	    coef[3 +  i] = weight*(centre_[i] + axis1[i] + axis2[i]);
	    coef[6 +  i] = centre_[i] + axis2[i];
	    coef[9 + i] = weight*(centre_[i] - axis1[i] + axis2[i]);
	    coef[12 + i] = centre_[i] - axis1[i];
	    coef[15 + i] = weight*(centre_[i] - axis1[i] - axis2[i]);
	    coef[18 + i] = centre_[i] - axis2[i];
	    coef[21 + i] = weight*(centre_[i] + axis1[i] - axis2[i]);
	    coef[24 + i] = centre_[i] + axis1[i];
	}
	// The rational weights.
	coef[2] = 1.0;
	coef[5] = weight;
	coef[8] = 1.0;
	coef[11] = weight;
	coef[14] = 1.0;
	coef[17] = weight;
	coef[20] = 1.0;
	coef[23] = weight;
	coef[26] = 1.0;
    }
    else {
	for ( i=0;  i < 3;  i++ ) {
	    coef[     i] = centre_[i] + axis1[i];
	    coef[4 +  i] = weight*(centre_[i] + axis1[i] + axis2[i]);
	    coef[8 +  i] = centre_[i] + axis2[i];
	    coef[12 + i] = weight*(centre_[i] - axis1[i] + axis2[i]);
	    coef[16 + i] = centre_[i] - axis1[i];
	    coef[20 + i] = weight*(centre_[i] - axis1[i] - axis2[i]);
	    coef[24 + i] = centre_[i] - axis2[i];
	    coef[28 + i] = weight*(centre_[i] + axis1[i] - axis2[i]);
	    coef[32 + i] = centre_[i] + axis1[i];
	}
	// The rational weights.
	coef[3] = 1.0;
	coef[7] = weight;
	coef[11] = 1.0;
	coef[15] = weight;
	coef[19] = 1.0;
	coef[23] = weight;
	coef[27] = 1.0;
	coef[31] = weight;
	coef[35] = 1.0;
    }

    int ncoefs = 9;
    int order = 3;
    bool rational = true;
    SplineCurve curve(ncoefs, order, et, coef, dim, rational);

    // Extract segment. We need all this because 'curve' is not an
    // arc-length parametrized circle.
    Point pt, tmppt;
    point(pt, endparam_ - startparam_);
    double tmpt, tmpdist;
    double tmin = 0.0;
    double tmax = 2.0 * M_PI;
    double epsilon = 1.0e-10;
    double seed = endparam_ - startparam_;
    curve.closestPoint(pt, tmin, tmax, tmpt, tmppt, tmpdist, &seed);
    if (tmpt < epsilon && endparam_ - startparam_ == 2.0 * M_PI) {
	tmpt = 2.0 * M_PI;
    }
    SplineCurve* segment = curve.subCurve(0.0, tmpt);
    segment->basis().rescale(startparam_, endparam_);
    translateSplineCurve(-centre_, *segment);
    rotateSplineCurve(normal_, startparam_, *segment);
    translateSplineCurve(centre_, *segment);

    return segment;
}


//===========================================================================
bool Circle::isDegenerate(double degenerate_epsilon)
//===========================================================================
{
    // We consider a Circle as degenerate if the radius is smaller
    // than the epsilon.

    return radius_ < degenerate_epsilon;
}


//===========================================================================
Circle* Circle::subCurve(double from_par, double to_par,
			 double fuzzy) const
//===========================================================================
{
    if (from_par >= to_par)
	THROW("First parameter must be strictly less than second.");

    Circle* circle = clone();
    circle->setParamBounds(from_par, to_par);
    return circle;
}


//===========================================================================
DirectionCone Circle::directionCone() const
//===========================================================================
{
    double tmin = startparam();
    double tmax = endparam();
    vector<Point> pts;
    point(pts, 0.5*(tmin+tmax), 1);
    return DirectionCone(pts[1], 0.5*(tmax-tmin));
}


//===========================================================================
void Circle::appendCurve(ParamCurve* cv, bool reparam)
//===========================================================================
{
    MESSAGE("Not implemented!");
}


//===========================================================================
void Circle::appendCurve(ParamCurve* cv,
		       int continuity, double& dist, bool reparam)
//===========================================================================
{
    MESSAGE("Not implemented!");
}


//===========================================================================
void Circle::closestPoint(const Point& pt,
			double tmin,
			double tmax,
			double& clo_t,
			Point& clo_pt,
			double& clo_dist,
			double const *seed) const
//===========================================================================
{
    // Check and fix the parameter bounds
    if (tmin < startparam_) {
	tmin = startparam_;
	MESSAGE("tmin too small. Using startparam_.");
    }
    if (tmax > endparam_) {
	tmax = endparam_;
	MESSAGE("tmax too large. Using endparam_.");
    }

    // If input is on the "centre line", we arbitrarily assign the
    // point with t = tmin.
    Point vec = pt - centre_;
    Point tmp = vec.cross(normal_);
    if (tmp.length() == 0.0) {
	clo_t = tmin;
	point(clo_pt, clo_t);
	clo_dist = radius_;
	MESSAGE("Input to Circle::closestPoint() is the centre.");
	return;
    }

    Point proj;
    if (dimension() == 2)
	proj = vec;
    else if (dimension() == 3)
	proj = vec - (vec * normal_) * normal_;
    else
	THROW("Dimension must be 2 or 3");
    double x = proj * vec1_;
    double y = proj * vec2_;
    if (x == 0.0) {
	if (y > 0.0) {
	    clo_t = 0.5 * M_PI;
	    clo_pt = centre_ + radius_ * vec2_;
	}
	else {
	    clo_t = 1.5 * M_PI;
	    clo_pt = centre_ - radius_ * vec2_;
	}
    }
    else {
	clo_t = atan(y / x);
	// We need to correct the angle when we are in quadrants II,
	// III and IV
	if (x < 0.0)
	    clo_t += M_PI; // II + III
	if (x > 0.0 && y < 0.0)
	    clo_t += 2.0 * M_PI; // IV

	point(clo_pt, clo_t);
    }
    clo_dist = (clo_pt - pt).length();

    // We must handle the case of a proper circle segment
    double tlen = tmax - tmin;
    double tmp_t = clo_t - tmin;
    if (tmp_t > 2.0 * M_PI)
	tmp_t -= 2.0 * M_PI;
    else if (tmp_t < 0.0)
	tmp_t += 2.0 * M_PI;
    if (tmp_t >= 0.5 * tlen + M_PI) {
	// Start of segment is closest
	clo_t = tmin;
	point(clo_pt, clo_t);
	clo_dist = (clo_pt - pt).length();
	return;
    }
    if (tmp_t >= tlen) {
	// End of segment is closest
	clo_t = tmax;
	point(clo_pt, clo_t);
	clo_dist = (clo_pt - pt).length();
	return;
    }
    // If we get here, point on segment is closest
    clo_t = tmp_t + tmin;
    point(clo_pt, clo_t);
    clo_dist = (clo_pt - pt).length();

}


//===========================================================================
double Circle::length(double tol)
//===========================================================================
{
    return (endparam_ - startparam_) * radius_;
}


//===========================================================================
void Circle::setParamBounds(double startpar, double endpar)
//===========================================================================
{
    if (startpar >= endpar)
	THROW("First parameter must be strictly less than second.");
    if (startpar < -2.0 * M_PI || endpar > 2.0 * M_PI)
	THROW("Parameters must be in [-2pi, 2pi].");
    if (endpar - startpar > 2.0 * M_PI)
	THROW("(endpar - startpar) must not exceed 2pi.");

    startparam_ = startpar;
    endparam_ = endpar;
}


//===========================================================================
void Circle::setSpanningVectors()
//===========================================================================
{
    // In 3D, the spanning vectors vec1_, vec2_, and the vector
    // normal_ defines a right-handed coordinate system. Similar to an
    // axis2_placement_3d entity in STEP.

    int dim = centre_.dimension();
    if (dim == 2) {
	vec2_.resize(2);
	vec2_[0] = -vec1_[1];
	vec2_[1] = vec1_[0];
    }
    else if (dim ==3) {
	Point tmp = vec1_ - (vec1_ * normal_) * normal_;
	if (tmp.length() == 0.0) 
	    THROW("X-axis parallel to normal.");
	vec1_ = tmp;
	vec2_ = normal_.cross(vec1_);
    }
    else {
	THROW("Dimension must be 2 or 3");
    }
    vec1_.normalize();
    vec2_.normalize();

}


//===========================================================================


} // namespace Go