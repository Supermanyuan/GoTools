//===========================================================================
//                                                                           
// File: GSCsubCurve.C                                                        
//                                                                           
// Created: Thu Apr  5 17:32:06 2001                                         
//                                                                           
// Author: Sverre Briseid <Sverre.Briseid@math.sintef.no>
//                                                                           
// Revision: $Id: GSCsubCurve.C,v 1.19 2009-06-12 08:49:47 vsk Exp $
//                                                                           
// Description: Extracts subcurve (specified by parameters) from curve
//                                                                           
//===========================================================================


#include <algorithm>
#include "GoTools/geometry/SplineCurve.h"
#include "GoTools/geometry/GeometryTools.h"
#include <memory>

#include <iterator> // For back_inserter.  This one should be required by VC++ and GCC as well...

#ifdef __BORLANDC__
#include <iterator> // For back_inserter.  This one should be required by VC++ and GCC as well...
#endif

using std::vector;
using std::shared_ptr;

namespace Go{


//===========================================================================
// #if ((_MSC_VER > 0) && (_MSC_VER < 1300))
// ParamCurve* SplineCurve::subCurve(double from_par, double to_par,
// 				      double fuzzy) const
// #else
SplineCurve* SplineCurve::subCurve(double from_par, double to_par,
				       double fuzzy) const
// #endif
//===========================================================================
{
    if (from_par >= to_par) {
	THROW("First parameter must be strictly less than second.");
    }
    if (from_par < startparam()-fuzzy) {
	THROW("Subcurve defined outside curve.");
    }

    // Check for periodic case. If to_par is greater than endparam() +
    // fuzzy, we assume the user wants to treat the curve as periodic.
    if (to_par > endparam() + fuzzy) {
	// Periodic case.
	if (to_par > endparam() + (endparam() - startparam()) + fuzzy) {
	    THROW("The subCurve across the seam can only cross the seam once.");
	}
	// We build a new curve consisting of this curve twice.
	SplineCurve twicecurve(*this);
	twicecurve.appendSelfPeriodic();
	// Then we call subCurve on the new curve.
	return twicecurve.subCurve(from_par, to_par);
    }

    // If boundaries are close to existing knots, we snap.
    // Otherwise insertKnot() will not perform very well.
    basis().knotIntervalFuzzy(from_par, fuzzy);
    basis().knotIntervalFuzzy(to_par, fuzzy);


    std::vector<double> knots, new_knots;
    SplineCurve the_curve = *this;
    int k = the_curve.order(); // order of the curve

    int i;
    for (i = 0; i < k; ++i) knots.push_back(from_par);
    for (i = 0; i < k; ++i) knots.push_back(to_par);

    std::set_difference(knots.begin(), knots.end(),
		   the_curve.basis().begin(), the_curve.basis().end(),
		   std::back_inserter(new_knots));

    // In order to extract subcurve, we make sure that both from_par and to_par
    // have multiplicity k
    the_curve.insertKnot(new_knots);

    std::vector<double>::const_iterator kend = the_curve.basis().end();

    // Iterator to first occurence of from_par
    std::vector<double>::const_iterator b =
	std::find(the_curve.basis().begin(),
	     the_curve.basis().end(), from_par);

    // Iterator to last occurence of to_par
    std::vector<double>::const_iterator e =
	std::find(b + k, kend, to_par) + (k - 1);

    // Depending on whether the curve is rational or not, the constructor
    // takes slightly different arguments
    std::vector<double>::const_iterator coefs_start = 
	(the_curve.rational_ ?
	 (the_curve.rcoefs_begin() 
	  + (the_curve.dimension() + 1) * (b - the_curve.basis().begin())) :
	 (the_curve.coefs_begin() 
	  + the_curve.dimension() * (b - the_curve.basis().begin())));
    // Constructing the subcurve
    SplineCurve* the_subCurve = new SplineCurve
	((int)(e - b) + 1 - k, k, b, coefs_start,
	 the_curve.dimension(), the_curve.rational());

    return the_subCurve;
}


//===========================================================================
  // Split curve in a specified parameter value
  std::vector<std::shared_ptr<ParamCurve> > 
  SplineCurve::split(double param, double fuzzy) const 
//===========================================================================
  {
    vector<double> parvals(1, param);
    vector<shared_ptr<SplineCurve> > sub_cvs = split(parvals, fuzzy);
    vector<shared_ptr<ParamCurve> > sub_cvs2;
    sub_cvs2.insert(sub_cvs2.end(), sub_cvs.begin(), sub_cvs.end());

    return sub_cvs2;
  }

//===========================================================================
  // Split curve in specified parameter values
  std::vector<std::shared_ptr<SplineCurve> > 
  SplineCurve::split(std::vector<double>& param, double fuzzy) const 
//===========================================================================
  {
    std::vector<std::shared_ptr<SplineCurve> > sub_cvs;

    // Make sure that the curve is k-periodic
    std::shared_ptr<SplineCurve> cv = std::shared_ptr<SplineCurve>(clone());
    cv->makeKnotStartRegular();
    cv->makeKnotEndRegular();

    // Make sure that the input parameters are in increasing sequence
    std::sort(param.begin(), param.end());

    // If split parameters are close to existing knots, we snap
    size_t ki;
    for (ki=0; ki<param.size(); ++ki)
      cv->basis().knotIntervalFuzzy(param[ki], fuzzy);

    // Add knots until all split parameter have multiplicity equal to the order
    std::vector<double> knots, new_knots;
    int kk = cv->order();

    int kj;
    for (ki=0; ki<param.size(); ++ki)
      for (kj=0; kj<kk; ++kj)
	knots.push_back(param[ki]);

    // Extract the knots to insert
    std::set_difference(knots.begin(), knots.end(),
			cv->basis().begin(), cv->basis().end(),
			std::back_inserter(new_knots));

    // Insert knots
    cv->insertKnot(new_knots);

    // Extract sub curves
    std::vector<double>::const_iterator end = cv->basis().end();
    std::vector<double>::const_iterator start = cv->basis().begin();
    SplineCurve* the_subCurve;
    std::vector<double>::const_iterator coefs_start;
    for (ki=0; ki<param.size(); ++ki)
      {
	std::vector<double>::const_iterator curr =
	  std::find(start + kk, end, param[ki]) + (kk - 1);

	// Depending on whether the curve is rational or not, the constructor
	// takes slightly different arguments
	coefs_start = 
	  (cv->rational_ ?
	   (cv->rcoefs_begin() 
	    + (cv->dimension() + 1) * (start - cv->basis().begin())) :
	   (cv->coefs_begin() 
	    + cv->dimension() * (start - cv->basis().begin())));
    
	// Constructing the subcurve
	the_subCurve = new SplineCurve
	    ((int)(curr - start) + 1 - kk, kk, start, coefs_start,
	   cv->dimension(), cv->rational());

	sub_cvs.push_back(std::shared_ptr<SplineCurve>(the_subCurve));
	start = curr - kk + 1;
      }

    // Depending on whether the curve is rational or not, the constructor
    // takes slightly different arguments
    coefs_start = 
      (cv->rational_ ?
       (cv->rcoefs_begin() 
	+ (cv->dimension() + 1) * (start - cv->basis().begin())) :
       (cv->coefs_begin() 
	+ cv->dimension() * (start - cv->basis().begin())));
    
    // Constructing the subcurve
    the_subCurve = new SplineCurve
	((int)(end - start) - kk, kk, start, coefs_start,
       cv->dimension(), cv->rational());

    sub_cvs.push_back(std::shared_ptr<SplineCurve>(the_subCurve));
    
    return sub_cvs;
  }

//===========================================================================
void SplineCurve::appendSelfPeriodic()
//===========================================================================
{
    // Testing that the curve actually is knot-periodic.
    // This test may be superfluous, the caller is supposed to know
    // that the curve is periodic before calling this function.
    // If that test was done with a larger tolerance than default,
    // this test may fail, making a mess of things.

    // Eventually, the cont number could be supplied from outside,
    // but this is hard to make work without changing calling code
    // a lot (in subCurve). Maybe we should let the tolerance be an
    // argument?

    int cont = analyzePeriodicity(*this);
    if (cont < 0) {
	THROW ("Curve seems to be nonperiodic. Should have been periodic!");
    }
    // Fill in new knot vector.
    std::vector<double> new_knots(basis_.begin(), basis_.end());
    double delta = endparam() - startparam();
    std::transform(basis_.begin() + order() + cont + 1, basis_.end(),
		   std::back_inserter(new_knots),
		   std::bind1st(std::plus<double>(), delta));
    int newn = 2*numCoefs() - cont - 1;
    ASSERT(newn + order() == int(new_knots.size()));
    // Fill in new coefficient vector.
    std::vector<double>& c = rational_ ? rcoefs_ : coefs_;
    std::vector<double> new_coefs(c);
    int effdim = rational_ ? dim_ + 1 : dim_;
    std::copy(c.begin() + effdim*(cont + 1), c.end(),
	      std::back_inserter(new_coefs));
    ASSERT(effdim*newn == int(new_coefs.size()));

    // Make a BsplineBasis object in order to swap.
    BsplineBasis temp(newn, order(), new_knots.begin());
    basis_.swap(temp);
    c.swap(new_coefs);
}

} // namespace Go;