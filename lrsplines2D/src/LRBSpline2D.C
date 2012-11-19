#include <array>
#if (defined(__GNUC__) || _MSC_VER > 1600) // No <thread> in VS2010
#include <thread>
#endif
#include "GoTools/lrsplines2D/LRBSpline2D.h"
#include "GoTools/utils/checks.h"
#include "GoTools/utils/StreamUtils.h"

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

//------------------------------------------------------------------------------
double B(int deg, double t, const int* knot_ix, const double* kvals, bool at_end)
//------------------------------------------------------------------------------
{
  // a POD rather than a stl vector used below due to the limitations of thread_local as currently
  // defined (see #defines at the top of this file).  A practical consequence is that 
  // MAX_DEGREE must be known at compile time.
  static double thread_local tmp[MAX_DEGREE+2];

  // only evaluate if within support
  if ((t < kvals[knot_ix[0]]) || (t > kvals[knot_ix[deg+1]])) return 0;

  assert(deg <= MAX_DEGREE);
  fill (tmp, tmp+deg+1, 0);

  // computing lowest-degree B-spline components (all zero except one)
  int nonzero_ix = 0;
  if (at_end)  while (kvals[knot_ix[nonzero_ix+1]] <  t) ++nonzero_ix;
  else         while (kvals[knot_ix[nonzero_ix+1]] <= t) ++nonzero_ix;

  assert(nonzero_ix <= deg);
  tmp[nonzero_ix] = 1;

  // accumulating to attain correct degree

  for (int d = 1; d != deg+1; ++d) {
    const int lbound = max (0, nonzero_ix - d);
    const int ubound = min (nonzero_ix, deg - d);
    for (int i = lbound; i <= ubound; ++i) {
      const double k_i     = kvals[knot_ix[i]];
      const double k_ip1   = kvals[knot_ix[i+1]];
      const double k_ipd   = kvals[knot_ix[i+d]]; 
      const double k_ipdp1 = kvals[knot_ix[i+d+1]];
      const double alpha =  (k_ipd == k_i) ? 0 : (t - k_i) / (k_ipd - k_i);
      const double beta  =  (k_ipdp1 == k_ip1) ? 0 : (k_ipdp1 - t) / (k_ipdp1 - k_ip1);

      tmp[i] = alpha * tmp[i] + beta * tmp[i+1];
    }
  }
  return tmp[0];
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
  return 
    ( (fac1 != 0) ? 
      ( fac1 * ( (der>1) ? 
        dB(deg-1, t, knot_ix, kvals, at_end, der-1)
		 : B(deg-1, t, knot_ix, kvals, at_end) ) )
      : 0 ) + 
    ( (fac2 != 0) ? 
      ( fac2 * ( (der>1) ? 
	       dB(deg-1, t, knot_ix+1, kvals, at_end, der-1) 
		 : B(deg-1, t, knot_ix+1, kvals, at_end) ) )
      : 0 ) ;
}

//------------------------------------------------------------------------------
double compute_univariate_spline(int deg, 
				 double u, 
				 const vector<int>& k_ixes, 
				 const double*  kvals, 
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
  object_to_stream(os, coef_times_gamma_);
  object_to_stream(os, gamma_);
  object_to_stream(os, '\n');
  object_to_stream(os, kvec_u_);
  object_to_stream(os, kvec_v_);
}

//==============================================================================
void LRBSpline2D::read(istream& is)
//==============================================================================
{
  object_from_stream(is, coef_times_gamma_);
  object_from_stream(is, gamma_);
  object_from_stream(is, kvec_u_);
  object_from_stream(is, kvec_v_);
}

//==============================================================================
double LRBSpline2D::evalBasisFunction(double u, 
					  double v, 
					  const double* const kvals_u, 
					  const double* const kvals_v,
					  int u_deriv, 
					  int v_deriv,
					  bool u_at_end,
					  bool v_at_end) const
//==============================================================================
{
  return 
    compute_univariate_spline(degree(XFIXED), u, kvec(XFIXED), kvals_u, u_deriv, u_at_end) *
    compute_univariate_spline(degree(YFIXED), v, kvec(YFIXED), kvals_v, v_deriv, v_at_end);
}

//==============================================================================
bool LRBSpline2D::overlaps(Element2D *el) const
//==============================================================================
{
  // To be implemented
  return false;
}

//==============================================================================
bool LRBSpline2D::addSupport(const Element2D *el)
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
void LRBSpline2D::removeSupport(const Element2D *el)
//==============================================================================
{
  for (size_t i=0; i<support_.size(); i++) {
    if(el == support_[i]) {
      support_[i] = support_.back();
      support_[support_.size()-1] = NULL;
      support_.pop_back();
      return;
    }
  }
}

}; // end namespace Go