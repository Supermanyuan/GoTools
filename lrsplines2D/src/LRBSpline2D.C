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

#include <array>
#if (defined(__GNUC__) || _MSC_VER > 1600) // No <thread> in VS2010
#include <thread>
#endif
#include "GoTools/lrsplines2D/LRBSpline2D.h"
#include "GoTools/utils/checks.h"
#include "GoTools/utils/StreamUtils.h"
#include "GoTools/geometry/BsplineBasis.h"
#include "GoTools/geometry/SplineUtils.h"

//#define DEBUG

// The following is a workaround since 'thread_local' is not well supported by compilers yet
#if defined(__GNUC__)
#define thread_local __thread
#elif _MSC_VER > 1600  //defined(_WIN32)
#define thread_local __declspec( thread )
#else
#define thread_local // _MSC_VER == 1600, i.e. VS2010
#endif

using namespace std;

namespace Go
{


//------------------------------------------------------------------------------
namespace
//------------------------------------------------------------------------------
{
// Since some static buffers (provided for efficiency reasons) need to know the maximum degree
// used at compile time, the following constant, MAX_DEGREE, is here defined.
const int MAX_DEGREE = 20;
  const int MAX_DER = 3;
  const int MAX_DIM = 3;

//------------------------------------------------------------------------------
double B(int deg, double t, const int* knot_ix, const double* kvals, bool at_end)
//------------------------------------------------------------------------------
{
  // a POD rather than a stl vector used below due to the limitations of thread_local as currently
  // defined (see #defines at the top of this file).  A practical consequence is that 
  // MAX_DEGREE must be known at compile time.
  //static double thread_local tmp[MAX_DEGREE+2];
  double tmp[MAX_DEGREE+2];

  // only evaluate if within support
  if ((t < kvals[knot_ix[0]]) || (t > kvals[knot_ix[deg+1]])) 
    return 0;

  assert(deg <= MAX_DEGREE);
  fill (tmp, tmp+deg+1, 0);

  // computing lowest-degree B-spline components (all zero except one)
  int nonzero_ix = 0;
  if (at_end)  
    while (kvals[knot_ix[nonzero_ix+1]] <  t) 
      ++nonzero_ix;
  else         
    while (nonzero_ix <= deg && kvals[knot_ix[nonzero_ix+1]] <= t) 
      ++nonzero_ix;

  if (nonzero_ix > deg+1)
    return 0.0; // Basis function defined to be 0.0 for value outside the support.
//  assert(nonzero_ix <= deg);

  tmp[nonzero_ix] = 1;

  // accumulating to attain correct degree

  double alpha, beta;
  double tt1, tt2, tt3, tt4, td1, td2;
  for (int d = 1; d != deg+1; ++d) {
    const int lbound = max (0, nonzero_ix - d);
    const int ubound = min (nonzero_ix, deg - d);
    tt1 = kvals[knot_ix[lbound]];
    tt3 = kvals[knot_ix[lbound+d]];
    td1 = tt3 - tt1;
    if (d <= nonzero_ix && lbound <= ubound)
      {
	tt2 = kvals[knot_ix[lbound+1]];
	tt4 = kvals[knot_ix[lbound+d+1]];
	td2 = tt4 - tt2;
	beta = (tt4 == tt2) ? 0.0  : (tt4 - t)/td2;
	tmp[lbound] = beta*tmp[lbound+1];
	tt1 = tt2;
	tt3 = tt4;
	td1 = td2; 
      }
    for (int i = lbound+(d <=nonzero_ix); i <= ubound; ++i) 
      {
	tt2 = kvals[knot_ix[i+1]];
	tt4 = kvals[knot_ix[i+d+1]];
	td2 = tt4 - tt2;
	alpha = (tt3 == tt1) ? 0.0 : (t - tt1)/td1;
	beta = (tt2 == tt4) ? 0.0 : (tt4 - t)/td2;
	tmp[i] = alpha * tmp[i] + beta * tmp[i+1];
	tt1 = tt2;
	tt3 = tt4;
	td1 = td2; 
    }
  }

  return tmp[0];
}


//------------------------------------------------------------------------------
  void Bder(const int& deg, const double& t, int& nder, const int* knot_ix, 
	    const double* kvals, double der[], const bool& at_end)
//------------------------------------------------------------------------------
{
  // a POD rather than a stl vector used below due to the limitations of thread_local as currently
  // defined (see #defines at the top of this file).  A practical consequence is that 
  // MAX_DEGREE must be known at compile time.
  //static double thread_local tmp[MAX_DEGREE+2];
  double tmp[MAX_DEGREE+8]; // Assumes maximum number of derivatives equal to 3

  // Adjust derivative if too large
  nder = min(nder, deg);

  // only evaluate if within support
  if ((t < kvals[knot_ix[0]]) || (t > kvals[knot_ix[deg+1]])) 
    return;

  assert(deg <= MAX_DEGREE);
  fill (tmp, tmp+deg+1+2*nder, 0);

  // computing lowest-degree B-spline components (all zero except one)
  int nonzero_ix = 0;
  if (at_end)  
    while (kvals[knot_ix[nonzero_ix+1]] <  t) 
      ++nonzero_ix;
  else         
    while (nonzero_ix <= deg && kvals[knot_ix[nonzero_ix+1]] <= t) 
      ++nonzero_ix;

  if (nonzero_ix > deg+1)
    return; // Basis function defined to be 0.0 for value outside the support.
//  assert(nonzero_ix <= deg);

  tmp[nonzero_ix] = 1;

  // accumulating to attain correct degree

  double alpha, beta;
  double tt1, tt2, tt3, tt4, td1, td2;
  for (int d = 1; d != deg + 1; ++d) {
    const int lbound = max (0, nonzero_ix - d);
    const int ubound = min (nonzero_ix, deg-d);
    tt1 = kvals[knot_ix[lbound]];
    tt3 = kvals[knot_ix[lbound+d]];
    td1 = (tt1 != tt3) ? 1.0/(tt3 - tt1) : 0.0;

   if (d <= nonzero_ix && lbound <= ubound)
      {
	tt2 = kvals[knot_ix[lbound+1]];
	tt4 = kvals[knot_ix[lbound+d+1]];
	td2 = (tt2 != tt4) ? 1.0/(tt4 - tt2) : 0.0;
	for (int j=nder; j>deg-d; --j)
	  {
	    int k = j + d - deg - 1;
	    tmp[2*(k+1)+lbound] = -d*td2*tmp[lbound+2*k+1];
	  }
	beta = td2*(tt4-t);
	tmp[lbound] = beta*tmp[lbound+1];
	tt1 = tt2;
	tt3 = tt4;
	td1 = td2; 
      }
    for (int i = lbound+(d <=nonzero_ix); i <= ubound; ++i) 
      {
	tt2 = kvals[knot_ix[i+1]];
	tt4 = kvals[knot_ix[i+d+1]];
	td2 = (tt2 != tt4) ? 1.0/(tt4 - tt2) : 0.0;
	//for (int j=d-1; j>=deg-nder; --j)
	for (int j=nder; j>deg-d; --j)
	  {
	    int k = j + d - deg - 1;
	    tmp[2*(k+1)+i] = d*(td1*tmp[i+2*k] - td2*tmp[i+2*k+1]);
	  }

	alpha = td1*(t-tt1);
	beta = td2*(tt4-t);
	tmp[i] = alpha * tmp[i] + beta * tmp[i+1];
	tt1 = tt2;
	tt3 = tt4;
	td1 = td2; 
    }
    tmp[ubound+1] = 0.0;
  }

  der[0] = tmp[0];
  for (int i=1; i<=nder; ++i)
    der[i] = tmp[i*2];
}


// //------------------------------------------------------------------------------
// // B-spline evaluation function
// double B_recursive(int deg, double t, const int* knot_ix, const double* kvals, bool at_end)
// //------------------------------------------------------------------------------
// {
//   const double k0   = kvals[knot_ix[0]];
//   const double k1   = kvals[knot_ix[1]];
//   const double kd   = kvals[knot_ix[deg]];
//   const double kdp1 = kvals[knot_ix[deg+1]];

//   assert(deg >= 0);
//   if (deg == 0) 
//     return // at_end: half-open interval at end of domain should be considered differently
//       (! at_end) ? ((k0 <= t && t <  k1) ? 1 : 0) : 
//                    ((k0 <  t && t <= k1) ? 1 : 0);

//   const double fac1 = (kd   > k0) ? (t -   k0) / (kd - k0) : 0;
//   const double fac2 = (kdp1 > k1) ? (kdp1 - t) / (kdp1 - k1) : 0;
  
//   return 
//     ( (fac1 > 0) ? fac1 * B(deg-1, t, knot_ix, kvals, at_end) : 0) +
//     ( (fac2 > 0) ? fac2 * B(deg-1, t, knot_ix+1, kvals, at_end) : 0);
// }

//------------------------------------------------------------------------------
// B-spline derivative evaluation
double dB(int deg, double t, const int* knot_ix, const double* kvals, bool at_end, int der=1)
//------------------------------------------------------------------------------
{
  const double k0   = kvals[knot_ix[0]];
  const double k1   = kvals[knot_ix[1]];
  const double kdeg = kvals[knot_ix[deg]];
  const double kdp1 = kvals[knot_ix[deg+1]];

  assert(der >  0); //@@ we should perhaps check that derivative <=
		    //   degree - multiplicity
  if (deg == 0) 
    return 0;
  double fac1 = (kdeg > k0) ? ( deg) / (kdeg - k0) : 0;
  double fac2 = (kdp1 > k1) ? (-deg) / (kdp1 - k1) : 0;

  double part1 = (fac1 != 0) ? 
    ( fac1 * ( (der>1) ? 
	       dB(deg-1, t, knot_ix, kvals, at_end, der-1)
	       : B(deg-1, t, knot_ix, kvals, at_end) ) ) : 0.0;

  double part2 = (fac2 != 0) ? 
      ( fac2 * ( (der>1) ? 
	       dB(deg-1, t, knot_ix+1, kvals, at_end, der-1) 
		 : B(deg-1, t, knot_ix+1, kvals, at_end) ) ) : 0.0;

  return part1 + part2; // The product rule.
    // ( (fac1 != 0) ? 
    //   ( fac1 * ( (der>1) ? 
    //     dB(deg-1, t, knot_ix, kvals, at_end, der-1)
    // 		 : B(deg-1, t, knot_ix, kvals, at_end) ) )
    //   : 0 ) + 
    // ( (fac2 != 0) ? 
    //   ( fac2 * ( (der>1) ? 
    // 	       dB(deg-1, t, knot_ix+1, kvals, at_end, der-1) 
    // 		 : B(deg-1, t, knot_ix+1, kvals, at_end) ) )
    //   : 0 ) ;
}

//------------------------------------------------------------------------------
double compute_univariate_spline(int deg, 
				 double u, 
				 const vector<int>& k_ixes, 
				 const double* kvals, 
				 int deriv,
				 bool on_end)
//------------------------------------------------------------------------------
{
  return (deriv>0) ? 
    dB(deg, u, &k_ixes[0], kvals, on_end, deriv) : 
    B( deg, u, &k_ixes[0], kvals, on_end);
}


}; // anonymous namespace


//==============================================================================
LRBSpline2D::LRBSpline2D(const LRBSpline2D& rhs)
//==============================================================================
{
  coef_fixed_ = rhs.coef_fixed_;
  coef_times_gamma_ = rhs.coef_times_gamma_;
  gamma_ = rhs.gamma_;
  kvec_u_ = rhs.kvec_u_;
  kvec_v_ = rhs.kvec_v_;
  mesh_ = rhs.mesh_;
  rational_ = rhs.rational_;
  // don't copy the support
  weight_ = rhs.weight_;

}

//==============================================================================
bool LRBSpline2D::operator<(const LRBSpline2D& rhs) const
//==============================================================================
{
  const int tmp1 = compare_seq(kvec_u_.begin(), kvec_u_.end(), rhs.kvec_u_.begin(), rhs.kvec_u_.end());
  if (tmp1 != 0) return (tmp1 < 0);

  const int tmp2 = compare_seq(kvec_v_.begin(), kvec_v_.end(), rhs.kvec_v_.begin(), rhs.kvec_v_.end());
  if (tmp2 != 0) return (tmp2 < 0);

  const int tmp3 = compare_seq(coef_times_gamma_.begin(), coef_times_gamma_.end(), 
			       rhs.coef_times_gamma_.begin(), rhs.coef_times_gamma_.end());
  if (tmp3 != 0) return (tmp3 < 0);

  return gamma_ < rhs.gamma_;
}

//==============================================================================
bool LRBSpline2D::operator==(const LRBSpline2D& rhs) const
//==============================================================================
{
#if 0//ndef NDEBUG
  // double umi = umin();
  // double uma = umax();
  // double vmi = vmin();
  // double vma = vmax();
  // std::cout << "umin: " << umi << ", umax: " << uma << ", vmin: " << vmi << ", vmax: " << vma << std::endl;
  auto iter = kvec_u_.begin();
  std::cout << "DEBUG: kvec_u_: ";
  while (iter != kvec_u_.end())
    {
      std::cout << *iter << " ";
      ++iter;
    }
  std::cout << "kvec_v_: ";
  iter = kvec_v_.begin();  
  while (iter != kvec_v_.end())
    {
      std::cout << *iter << " ";
      ++iter;
    }
  std::cout << std::endl;

  iter = rhs.kvec_u_.begin();  
  std::cout << "DEBUG: rhs.kvec_u_: ";
  while (iter != rhs.kvec_u_.end())
    {
      std::cout << *iter << " ";
      ++iter;
    }

  iter = rhs.kvec_v_.begin();  
  std::cout << "rhs.kvec_v_: ";
  while (iter != rhs.kvec_v_.end())
    {
      std::cout << *iter << " ";
      ++iter;
    }
  std::cout << std::endl;
  
#endif
#ifndef NDEBUG
  int kvec_u_size = kvec_u_.size();
  int kvec_v_size = kvec_v_.size();
  int kvec_u_size2 = rhs.kvec_u_.size();
  int kvec_v_size2 = rhs.kvec_v_.size();
  if ((kvec_u_size != kvec_u_size2) || (kvec_v_size != kvec_v_size2))
      MESSAGE("DEBUG: Pairwise vectors are of different size!");
//    ;
#endif

  const int tmp1 = compare_seq(kvec_u_.begin(), kvec_u_.end(), 
			       rhs.kvec_u_.begin(), rhs.kvec_u_.end());
  if (tmp1 != 0)
    return false;

  const int tmp2 = compare_seq(kvec_v_.begin(), kvec_v_.end(), 
			       rhs.kvec_v_.begin(), rhs.kvec_v_.end());
  if (tmp2 != 0)
    return false;

  return true;
}

 //==============================================================================
void LRBSpline2D::write(ostream& os) const
//==============================================================================
{
  // @@sbr201301 We must decide on a file format for the LRBSpline2D.
  // For rational case the dimension is currently written as dim + 1.
  // It makes more sense to keep geometric dimension and set rational boolean.
  int dim = coef_times_gamma_.dimension();
  object_to_stream(os, dim);
  int rat = (rational_) ? 1 : 0;
  object_to_stream(os, rat);
  object_to_stream(os, '\n');
  object_to_stream(os, coef_times_gamma_);
  object_to_stream(os, gamma_);
  object_to_stream(os, weight_);
  object_to_stream(os, '\n');
  object_to_stream(os, kvec_u_);
  object_to_stream(os, kvec_v_);
}

//==============================================================================
void LRBSpline2D::read(istream& is)
//==============================================================================
{
  // @@sbr201301 Currently we are expecting the rational weight to be
  // included in file format, even for non-rational cases.
  int dim = -1;
  object_from_stream(is, dim);
  coef_times_gamma_.resize(dim);
  int rat = -1;
  object_from_stream(is, rat);
  rational_ = (rat == 1);
  object_from_stream(is, coef_times_gamma_);
  object_from_stream(is, gamma_);
  // if (gamma_ < 1.0)
  // {
  //     MESSAGE("DEBUGGING: Changing gamma from " << gamma_ << " to 1.0!");
  //     coef_times_gamma_ /= gamma_;
  //     gamma_ = 1.0;
  // }
  object_from_stream(is, weight_);
  object_from_stream(is, kvec_u_);
  object_from_stream(is, kvec_v_);
  coef_fixed_ = 0;
}

//==============================================================================
double LRBSpline2D::evalBasisFunc(double u, 
				  double v) const
//==============================================================================
{
  //double eps = 1.0e-12;
  bool u_on_end = (u >= umax());//-eps);
  bool v_on_end = (v >= vmax());//-eps);

  return 
    B(degree(XFIXED), u, &kvec(XFIXED)[0], mesh_->knotsBegin(XFIXED), u_on_end)*
    B(degree(YFIXED), v, &kvec(YFIXED)[0], mesh_->knotsBegin(YFIXED), v_on_end);
}


//==============================================================================
double LRBSpline2D::evalBasisFunction(double u, 
					  double v, 
					  int u_deriv, 
					  int v_deriv,
					  bool u_at_end,
					  bool v_at_end) const
//==============================================================================
{
  // double eps = 1.0e-12;
  //  u_at_end = (u >= umax()-eps);
  //  v_at_end = (v >= vmax()-eps);
  double bval1 = compute_univariate_spline(degree(XFIXED), u, kvec(XFIXED),
					   mesh_->knotsBegin(XFIXED), 
					   u_deriv, u_at_end);
  double bval2 =  compute_univariate_spline(degree(YFIXED), v, kvec(YFIXED), 
					    mesh_->knotsBegin(YFIXED),  
					    v_deriv, v_at_end);
  return bval1*bval2;
}


//==============================================================================
  void LRBSpline2D::evalder_add(double u, double v, 
				int deriv,
				Point der[],
				bool u_at_end, bool v_at_end) const
//==============================================================================
{
  double eps = 1.0e-12;
   u_at_end = (u >= umax()-eps);
   v_at_end = (v >= vmax()-eps);

   // vector<double> bder1(deriv+1);
   // vector<double> bder2(deriv+1);
   deriv = std::min(MAX_DER, deriv);
   double dd[2*MAX_DER+2];
   double *bder1 = dd;
   double *bder2 = dd+deriv+1;
   Bder(degree(XFIXED), u, deriv, &kvec(XFIXED)[0], 
	mesh_->knotsBegin(XFIXED), bder1 /*&bder1[0]*/, u_at_end);
   Bder(degree(YFIXED), v, deriv, &kvec(YFIXED)[0], 
	mesh_->knotsBegin(YFIXED), bder2/*&bder2[0]*/, v_at_end);

   int ki, kj, kr, kh;
   // vector<double> bb((deriv+1)*(deriv+2)/2);
   // for (kj=0, kr=0; kj<=deriv; ++kj)
   //   for (ki=0; ki<=kj; ++ki, ++kr)
   //     bb[kr] = evalBasisFunction(u, v, kj-ki, ki, u_at_end, v_at_end);

   if (rational_)
     {
       int dim = coef_times_gamma_.dimension();
       int nmb = (deriv+1)*(deriv+2)/2;
       double tmp[(int)((MAX_DER+1)*(MAX_DER+2)*(MAX_DIM+1))];
       double *tmpder = tmp;
       double val;
       Point tmppt(dim);
       kh = 0;
       for (ki=0; ki<=deriv; ++ki)
	 for (kj=0; kj<=ki; ++kj, ++kh)
	   {
	     val = weight_*bder1[ki-kj]*bder2[kj];
	     for (kr=0; kr<dim; ++kr)
	       tmpder[kh*(dim+1)+kr] = coef_times_gamma_[kr]*val;
	     tmpder[kh*(dim+1)+dim] = val;
	   }
       double *tmpder2 = tmpder+nmb*(dim+1);
       SplineUtils::surface_ratder(tmpder, dim, deriv, tmpder2);
       for (kh=0; kh<nmb; ++kh)
	 {
	   for (kr=0; kr<dim; ++kr)
	     tmppt[kr] = tmpder2[kh*dim+kr];
	   der[kh] = tmppt;
	 }
     }
   else
     {
       kh = 0;
       for (ki=0; ki<=deriv; ++ki)
	 for (kj=0; kj<=ki; ++kj, ++kh)
	   {
	     der[kh] += coef_times_gamma_*bder1[ki-kj]*bder2[kj];
	     // Point bb2 = eval(u, v, ki-kj, kj, u_at_end, v_at_end);
	     // int stop_break = 1;
	   }
     }


   // return 
  //   compute_univariate_spline(degree(XFIXED), u, kvec(XFIXED), mesh_->knotsBegin(XFIXED), 
  // 			      u_deriv, u_at_end) *
  //   compute_univariate_spline(degree(YFIXED), v, kvec(YFIXED), mesh_->knotsBegin(YFIXED),  
  // 			      v_deriv, v_at_end);
}


//==============================================================================
void LRBSpline2D::evalBasisGridDer(int nmb_der, const vector<double>& par1, 
				   const vector<double>& par2, 
				   vector<double>& derivs) const
//==============================================================================
{
  if (nmb_der == 0)
    return;  // No derivatives to compute
  nmb_der = std::max(nmb_der, 3); // At most third order derivatives
  // Allocate stratch
  int nmb1 = (int)(par1.size());
  int nmb2 = (int)(par2.size());
  int nmb_part_der = 2;
  if (nmb_der > 1)
    nmb_part_der += 3;
  if (nmb_der > 2)
    nmb_part_der += 4;  
  derivs.resize(nmb_part_der*nmb1*nmb2);
  vector<double> ebder1((nmb_der+1)*nmb1);
  vector<double> ebder2((nmb_der+1)*nmb2);

  // Compute derivatives of univariate basis
  int ki, kj;
  for (ki=0; ki<nmb1; ++ki)
    Bder(degree(XFIXED), par1[ki], nmb_der, &kvec(XFIXED)[0], 
	 mesh_->knotsBegin(XFIXED), 
	 &ebder1[ki*(nmb_der+1)], false);
  for (ki=0; ki<nmb1; ++ki)
    Bder(degree(YFIXED), par2[ki], nmb_der, &kvec(YFIXED)[0], 
	 mesh_->knotsBegin(XFIXED), 
	 &ebder2[ki*(nmb_der+1)], false);
  // for (ki=0; ki<nmb1; ++ki)
  //   {
  //     // For the time being. Should be made more effective
  //     for (int kii=0; kii<=nmb_der; ++kii)
  // 	{
  // 	  ebder1[ki*(nmb_der+1)+kii] = compute_univariate_spline(degree(XFIXED), 
  // 								 par1[ki], kvec(XFIXED), 
  // 								 mesh_->knotsBegin(XFIXED), 
  // 								 kii, false);
  // 	}
  //   }

  // for (ki=0; ki<nmb2; ++ki)
  //   {
  //     // For the time being. Should be made more effective
  //     for (int kii=0; kii<=nmb_der; ++kii)
  // 	{
  // 	  ebder2[ki*(nmb_der+1)+kii] = compute_univariate_spline(degree(YFIXED), 
  // 								 par2[ki], kvec(YFIXED), 
  // 								 mesh_->knotsBegin(YFIXED), 
  // 								 kii, false);
  // 	}
  //   }

  // Combine univariate results
  // NOTE that rational functions are NOT handled
  int kr;
  for (kj=0; kj<nmb2; ++kj)
    for (ki=0; ki<nmb1; ++ki)
      {
	derivs[kj*nmb1+ki] = 
	  gamma_*ebder1[ki*nmb_der+1]*ebder2[kj*nmb_der]; // du
	derivs[(nmb2+kj)*nmb1+ki] = 
	  gamma_*ebder1[ki*nmb_der]*ebder2[kj*nmb_der+1]; // dv
	if (nmb_der > 1)
	  {
	    derivs[(2*nmb2+kj)*nmb1+ki] = 
	      gamma_*ebder1[ki*nmb_der+2]*ebder2[kj*nmb_der]; // duu
	    derivs[(3*nmb2+kj)*nmb1+ki] = 
	      gamma_*ebder1[ki*nmb_der+1]*ebder2[kj*nmb_der+1]; // duv
	    derivs[(4*nmb2+kj)*nmb1+ki] = 
	      gamma_*ebder1[ki*nmb_der]*ebder2[kj*nmb_der+2]; // dvv
	    if (nmb_der > 2)
	      {
		derivs[(5*nmb2+kj)*nmb1+ki] = 
		  gamma_*ebder1[ki*nmb_der+3]*ebder2[kj*nmb_der]; // duuu
		derivs[(6*nmb2+kj)*nmb1+ki] = 
		  gamma_*ebder1[ki*nmb_der+2]*ebder2[kj*nmb_der+1]; // duuv
		derivs[(7*nmb2+kj)*nmb1+ki] = 
		  gamma_*ebder1[ki*nmb_der+1]*ebder2[kj*nmb_der+2]; // duvv
		derivs[(8*nmb2+kj)*nmb1+ki] = 
		  gamma_*ebder1[ki*nmb_der]*ebder2[kj*nmb_der+3]; // dvvv
	      }
	  }
      }
}

//==============================================================================
  void LRBSpline2D::evalBasisLineDer(int nmb_der, Direction2D d, 
				     const vector<double>& parval, 
				     vector<double>& derivs) const
//==============================================================================
{
  if (nmb_der == 0)
    return;  // No derivatives to compute
  nmb_der = std::max(nmb_der, 3); // At most third order derivatives
  // Allocate stratch
  int nmb = (int)(parval.size());
  derivs.resize(nmb_der*nmb);
  vector<double> ebder((nmb_der+1)*nmb);

  // Compute derivatives of univariate basis
  int ki;
  for (ki=0; ki<nmb; ++ki)
    {
      // For the time being. Should be made more effective
      for (int kii=0; kii<=nmb_der; ++kii)
	{
	  ebder[ki*(nmb_der+1)+kii] = compute_univariate_spline(degree(d), 
								parval[ki], kvec(d), 
								mesh_->knotsBegin(d), 
								kii, false);
	}
    }

  // Multiply with weight
  // NOTE that rational functions are NOT handled
  int kr;
  for (ki=0; ki<nmb; ++ki)
      {
	derivs[ki] = gamma_*ebder[ki*nmb_der+1]; // dt
	if (nmb_der > 1)
	  {
	    derivs[nmb+ki] = gamma_*ebder[ki*nmb_der+2]; // dtt
	    if (nmb_der > 2)
	      {
		derivs[2*nmb+ki] = gamma_*ebder[ki*nmb_der+3]; // dttt
	      }
	  }
      }
}

//==============================================================================
int LRBSpline2D::endmult_u(bool atstart) const
//==============================================================================
{
  int idx;
  size_t ki;
  if (atstart)
    {
      idx = 1;
      for (ki=1; ki<kvec_u_.size(); ++ki)
	{
	  if (kvec_u_[ki] != kvec_u_[ki-1])
	    break;
	  idx++;
	}
    }
  else
    {
      idx = 1;
      for (ki=kvec_u_.size()-2; ki>=0; --ki)
	{
	  if (kvec_u_[ki] != kvec_u_[ki+1])
	    break;
	  idx++;
	}
     }
  return idx;
}

//==============================================================================
int LRBSpline2D::endmult_v(bool atstart) const
//==============================================================================
{
  int idx;
  size_t ki;
  if (atstart)
    {
      idx = 1;
      for (ki=1; ki<kvec_v_.size(); ++ki)
	{
	  if (kvec_v_[ki] != kvec_v_[ki-1])
	    break;
	  idx++;
	}
    }
  else
    {
      idx = 1;
      for (ki=kvec_v_.size()-2; ki>=0; --ki)
	{
	  if (kvec_v_[ki] != kvec_v_[ki+1])
	    break;
	  idx++;
	}
     }
  return idx;
}

//==============================================================================
Point LRBSpline2D::getGrevilleParameter() const
{
  int nmb1 = (int)kvec_u_.size() - 1;
  int nmb2 = (int)kvec_v_.size() - 1;
  int ki;
  double upar = 0, vpar = 0;
  for (ki=1; ki<nmb1; ++ki)
    upar += mesh_->kval(XFIXED, kvec_u_[ki]);
  for (ki=1; ki<nmb2; ++ki)
    vpar += mesh_->kval(YFIXED, kvec_v_[ki]);
  upar /= (double)(nmb1-1);
  vpar /= (double)(nmb2-1);

  Point greville(upar, vpar);
  return greville;
}

//==============================================================================
double LRBSpline2D::getGrevilleParameter(Direction2D d) const
{
  int nmb = (d == XFIXED) ? (int)kvec_u_.size() - 1 : (int)kvec_v_.size() - 1;
  int ki;
  double par = 0;
  for (ki=1; ki<nmb; ++ki)
    par += mesh_->kval(d, kvec(d)[ki]);
  par /= (double)(nmb-1);

  return par;
}

//==============================================================================
bool LRBSpline2D::overlaps(double domain[]) const
//==============================================================================
{
  // Does it make sense to include equality?
  if (domain[0] >= umax())
    return false;
  if (domain[1] <= umin())
    return false;
  if (domain[2] >= vmax())
    return false;
  if (domain[3] <= vmin())
    return false;
  
  return true;
}

//==============================================================================
bool LRBSpline2D::overlaps(Element2D *el) const
//==============================================================================
{
  // Does it make sense to include equality?
  if (el->umin() >= umax())
    return false;
  if (el->umax() <= umin())
    return false;
  if (el->vmin() >= vmax())
    return false;
  if (el->vmax() <= vmin())
    return false;
  
  return true;
}


//==============================================================================
bool LRBSpline2D::addSupport(Element2D *el)
//==============================================================================
{
  for (size_t i=0; i<support_.size(); i++) {
    if(el == support_[i]) {
      return false; 
    }
  }
  support_.push_back(el);
  return true;
}

//==============================================================================
void LRBSpline2D::removeSupport(Element2D *el)
//==============================================================================
{
  for (size_t i=0; i<support_.size(); i++) {
    if(el == support_[i]) {
      if (i < support_.size() - 1)
	{
	  support_[i] = support_.back();
	  support_[support_.size()-1] = NULL;
	}
      support_.pop_back();
      return;
    }
  }
}

//==============================================================================
bool LRBSpline2D::hasSupportedElement(Element2D *el) 
//==============================================================================
{
  for (size_t i=0; i<support_.size(); i++) 
    {
    if(el == support_[i]) 
      return true;
    }
  return false;
}

//==============================================================================
std::vector<Element2D*>::iterator LRBSpline2D::supportedElementBegin()
//==============================================================================
{
  return support_.begin();
}

//==============================================================================
std::vector<Element2D*>::iterator LRBSpline2D::supportedElementEnd()
//==============================================================================
{
  return support_.end();
}

//==============================================================================
void LRBSpline2D::subtractKnotIdx(int u_del, int v_del)
//==============================================================================
{
  for (size_t kj=0; kj<kvec_u_.size(); ++kj)
    kvec_u_[kj] -= u_del;

  for (size_t kj=0; kj<kvec_v_.size(); ++kj)
    kvec_v_[kj] -= v_del;
}

//==============================================================================
void LRBSpline2D::reverseParameterDirection(bool dir_is_u)
//==============================================================================
{
  int num_unique_knots = (dir_is_u) ? mesh_->numDistinctKnots(XFIXED) : mesh_->numDistinctKnots(YFIXED);
  auto iter_beg = (dir_is_u) ? kvec_u_.begin() : kvec_v_.begin();
  auto iter_end = (dir_is_u) ? kvec_u_.end() : kvec_v_.end();
  
  auto iter = iter_beg;
  while (iter != iter_end)
    {
      *iter = num_unique_knots - 1 - *iter;
      ++iter;
    }

  std::reverse(iter_beg, iter_end);
}


//==============================================================================
void LRBSpline2D::swapParameterDirection()
//==============================================================================
{
  std::swap(kvec_u_, kvec_v_);
}


//==============================================================================
vector<double> LRBSpline2D::unitSquareBernsteinBasis(double start_u, double stop_u, double start_v, double stop_v) const
//==============================================================================
{
  vector<double> result;

  vector<double> coefs_u = unitIntervalBernsteinBasis(start_u, stop_u, XFIXED);
  vector<double> coefs_v = unitIntervalBernsteinBasis(start_v, stop_v, YFIXED);

  for (vector<double>::const_iterator it_v = coefs_v.begin(); it_v != coefs_v.end(); ++it_v)
    {
      Point coefs_point = coef_times_gamma_ * (*it_v);
      for (vector<double>::const_iterator it_u = coefs_u.begin(); it_u != coefs_u.end(); ++it_u)
	for (int i = 0; i < coefs_point.size(); ++i)
	  result.push_back(coefs_point[i] * (*it_u));
    }

  return result;
}


//==============================================================================
vector<double> LRBSpline2D::unitIntervalBernsteinBasis(double start, double stop, Direction2D d) const
//==============================================================================
{
  // Get knot vector, where the knots are translated by start -> 0.0 and stop -> 1.0
  vector<double> knots;
  vector<int> knots_int = kvec(d);

  double slope = 1.0/(stop - start);
  int deg = degree(d);

  for (int i = 0; i < deg + 2; ++i)
    knots.push_back(slope * (mesh_->kval(d,knots_int[i]) - start));

  // Get the position of the interval containing [0,1]. We assume that for
  // some k, knots[k] <= 0.0 and knots[k+1] >= 1.0, and let interval_pos be this k.
  // We use 0.5 instead of 1.0 to break the loop, in order to avoid using tolerances.
  // Any number in the open interval (0,1) would work.
  int interval_pos;
  for (interval_pos = 0; interval_pos <= deg; ++interval_pos)
    if (knots[interval_pos + 1] >= 0.5)
      break;

  // Prepare array holding the Bernstein basis coefficients.
  // After each step for each polynomial degree (value of k in outermost loop below),
  // the polynomial part on the interval [ knots[interval_pos], knots[interval_pos+1] ])
  // of the k-degree B-spline defined by knot vector knot[i],...,knot[i+k+1] is given
  // by coefficients coefs[i][0],...,coefs[i][k]. At the end, the coefficients to be
  // returned are in coefs[0]
  vector<vector<double> > coefs(deg+1);
  for (int i = 0; i <= deg; ++i)
    coefs[i].resize(deg + 1 - i);
  coefs[interval_pos][0] = 1.0;

  for (int k = 1; k <=deg; ++k)
    for (int i = 0; i <= deg - k; ++i)
      if (i >= interval_pos - k && i <= interval_pos)   // Only look at B-splines with support in interval
      {
	double coefs_i_jmin1 = 0.0;  // For caching coefs[i][j-1] in inner loop

	// Store 1/(k*(knots[i+k]-knots[i])) and same for next interval. The denominator should not be zero
	// (because knots[interval_pos] < knots[interval_pos +1]) but just in case we use the standard
	// assumption 1/0 = 0 from spline arithmetics
	double denom_0 = (double)k*(knots[i + k] - knots[i]);
	if (denom_0 != 0.0)
	  denom_0 = 1.0/denom_0;
	double denom_1 = (double)k*(knots[i + k + 1] - knots[i + 1]);
	if (denom_1 != 0.0)
	  denom_1 = 1.0/denom_1;

	// Some factors used several times
	double f0 = (1.0 - knots[i]) * denom_0;
	double f1 = (knots[i + k + 1] - 1.0) * denom_1;
	double f2 = f0 - denom_0;
	double f3 = f1 + denom_1;

	// Calculate the new coefficients
	for (int j = 0; j <= k; ++j)
	  {
	    double res = 0.0;
	    if (j > 0)
	      res += (f0 * coefs_i_jmin1 + f1 * coefs[i + 1][j - 1]) * (double)j;
	    if (j < k)
	      res += (f2 * coefs[i][j] + f3 * coefs[i + 1][j]) * (double)(k - j);
	    coefs_i_jmin1 = coefs[i][j];
	    coefs[i][j] = res;
	  }
      }

  return coefs[0];
}


}; // end namespace Go
