/** @file aimplicitsolver.hpp
 * @brief Implicit solution of Euler/Navier-Stokes equations
 * @author Aditya Kashi
 * @date May 6, 2016
 */
#ifndef __AIMPLICITSOLVER_H

#ifndef __ACONSTANTS_H
#include "aconstants.hpp"
#endif

#ifndef __AMATRIX_H
#include "amatrix.hpp"
#endif

#ifndef __AMESH2DH_H
#include "amesh2dh.hpp"
#endif

#ifndef __ANUMERICALFLUX_H
#include "anumericalflux.hpp"
#endif

#ifndef __ALIMITER_H
#include "alimiter.hpp"
#endif

#ifndef __ARECONSTRUCTION_H
#include "areconstruction.hpp"
#endif

#ifndef __ALINALG_H
#include "alinalg.hpp"
#endif

#define __AIMPLICITSOLVER_H 1

namespace acfd {

/// A base class to control the implicit time-stepping solution process
/** \note Make sure compute_topological(), compute_face_data() and compute_jacobians() have been called on the mesh object prior to initialzing an object of (a child class of) this class.
 */
class ImplicitSolverBase
{
protected:
	const UMesh2dh* const m;
	amat::Matrix<a_real> m_inverse;			///< mass matrix (just the volume of the element for FV)
	amat::Matrix<a_real> residual;			///< Right hand side for boundary integrals and source terms
	int nvars;									///< number of conserved variables
	amat::Matrix<a_real> uinf;				///< Free-stream/reference condition
	a_real g;								///< adiabatic index

	/// stores (for each cell i) \f$ \sum_{j \in \partial\Omega_I} \int_j( |v_n| + c) d \Gamma \f$, where v_n and c are average values for each face of the cell
	amat::Matrix<a_real> integ;
	
	/// Local time steps
	amat::Matrix<a_real> dtl;

	/// Flux (boundary integral) calculation context
	InviscidFlux* inviflux;

	/// Reconstruction context
	Reconstruction* rec;

	/// Limiter context
	FaceDataComputation* lim;

	/// Cell centers
	amat::Matrix<a_real> rc;

	/// Ghost cell centers
	amat::Matrix<a_real> rcg;
	/// Ghost cell-center flow quantities
	amat::Matrix<a_real> ug;

	/// Number of Guass points per face
	int ngaussf;
	/// Faces' Gauss points' coords, stored a 3D array of dimensions naface x nguassf x ndim (in that order)
	amat::Matrix<a_real>* gr;

	/// Left state at each face (assuming 1 Gauss point per face)
	amat::Matrix<a_real> uleft;
	/// Rigt state at each face (assuming 1 Gauss point per face)
	amat::Matrix<a_real> uright;
	
	/// vector of unknowns
	amat::Matrix<a_real> u;
	/// x-slopes
	amat::Matrix<a_real> dudx;
	/// y-slopes
	amat::Matrix<a_real> dudy;

	/// Linear solver to use
	IterativeSolver* solver;

	amat::Matrix<a_real> scalars;		///< Holds density, Mach number and pressure for each cell
	amat::Matrix<a_real> velocities;		///< Holds velocity components for each cell

	int order;					///< Formal order of accuracy of the scheme (1 or 2)

	int solid_wall_id;			///< Boundary marker corresponding to solid wall
	int inflow_outflow_id;		///< Boundary marker corresponding to inflow/outflow
	const double cfl_init;		///< Starting CFL number
	const double cfl;			///< Final CFL number
	const int switchstepi;		///< Time step at which to start CFL ramping-up
	const int switchstep;		///< Time step at which to reach higher CFL number
	const double w;				///< Relaxation factor

public:
	ImplicitSolverBase(const UMesh2dh* const mesh, const int _order, std::string invflux, std::string reconst, std::string limiter, std::string linear_solver, 
			const double cfl, const double cfl_init, const int switchstepi, const int switchstep, const double relaxation_factor);
	virtual ~ImplicitSolverBase();
	void loaddata(a_real Minf, a_real vinf, a_real a, a_real rhoinf);

	/// Computes flow variables at boundaries (either Gauss points or ghost cell centers) using the interior state provided
	/** \param[in] instates provides the left (interior state) for each boundary face
	 * \param[out] bounstates will contain the right state of boundary faces
	 *
	 * Currently does not use characteristic BCs.
	 * \todo Implement and test characteristic BCs
	 */
	void compute_boundary_states(const amat::Matrix<a_real>& instates, amat::Matrix<a_real>& bounstates);

	/// Calls functions to assemble the [right hand side](@ref residual)
	/** Note that the residual corresponds to R in the equation
	 * dU/dt = R(u)
	 */
	void compute_RHS();

	/// Compute diagonal blocks and eigenvalues of simplified for LHS; note that only the flux jacobian is computed. V/dt terms are not added.
	/** Boundary conditions need to be taken into account for the diagonal blocks
	 * so make sure this function is called after the previous one (compute_RHS) as ug is needed.
	 */
	virtual void compute_LHS() = 0;
	
	/// Computes the left and right states at each face, using the [reconstruction](@ref rec) and [limiter](@ref limiter) objects
	void compute_face_states();

	virtual void solve() = 0;

	/// Computes the L2 norm of a cell-centered quantity
	a_real l2norm(const amat::Matrix<a_real>* const v);
	
	/// Compute cell-centred quantities to export
	void postprocess_cell();
	
	/// Compute nodal quantities to export, based on area-weighted averaging (which takes into account ghost cells as well)
	void postprocess_point();

	/// Compute norm of cell-centered entropy production
	/// Call aftr computing pressure etc \sa postprocess_cell
	a_real compute_entropy_cell();

	amat::Matrix<a_real> getscalars() const;
	amat::Matrix<a_real> getvelocities() const;

	/// computes ghost cell centers assuming symmetry about the midpoint of the boundary face
	void compute_ghost_cell_coords_about_midpoint();

	/// computes ghost cell centers assuming symmetry about the face
	void compute_ghost_cell_coords_about_face();
};

/// A driver class to control a full matrix storage implicit time-stepping process
class ImplicitSolver : public ImplicitSolverBase
{
protected:
	/// Diagonal blocks of the residual Jacobian
	amat::Matrix<a_real>* diag;
	/// LU-factorized diagonal blocks
	amat::Matrix<a_real>* ludiag;
	/// Permuation arrays of the diagonal blocks after LU factorization
	amat::Matrix<int>* diagp;
	/// Blocks in the lower triangular part
	amat::Matrix<a_real>* lower;
	/// Blocks in the  upper triangular part
	amat::Matrix<a_real>* upper;
	/// Residual corresponding to linear system to be solved (as opposed to that of the ODE to be solved)
	amat::Matrix<a_real> afresidual;

public:
	ImplicitSolver(const UMesh2dh* const mesh, const int _order, std::string invflux, std::string reconst, std::string limiter, std::string linear_solver, 
			const double cfl, const double cfl_init, const int switchstepi, const int switchstep, const double relaxation_factor);

	~ImplicitSolver();

	virtual void compute_LHS();

	/// Computes the product of a vector with the jacobian (D+L+U) of the system
	void jacobianVectorProduct(const amat::Matrix<a_real>* const du, amat::Matrix<a_real>& ans);

	virtual void solve() = 0;
};

class SteadyStateImplicitSolver : public ImplicitSolver
{
	const a_real lintol;
	const a_real steadytol;
	const int linmaxiter;
	const int steadymaxiter;
public:
	SteadyStateImplicitSolver(const UMesh2dh* const mesh, const int _order, std::string invflux, std::string reconst, std::string limiter, std::string linear_solver, 
			const double cfl, const double icfl, const int switchstepi, const int swtchstp, const double omega, const a_real steady_tol, const int steady_maxiter, const a_real lin_tol, const int lin_maxiter);

	/// Solves a steady problem by an implicit method first order in time, using local time-stepping
	void solve();
};


/// A driver class to control a matrix-free implicit time-stepping solution process
/** \note Make sure compute_topological(), compute_face_data() and compute_jacobians() have been called on the mesh object prior to initialzing an object of (a child class of) this class.
 */
class ImplicitSolverMF : public ImplicitSolverBase
{
protected:
	/// Euler flux
	Flux* eulerflux;

	/// Diagonal blocks of the residual Jacobian
	amat::Matrix<a_real>* diag;
	/// Permuation arrays of the diagonal blocks after LU factorization
	amat::Matrix<int>* diagp;

	/// `Eigenvalues' of flux for LHS
	amat::Matrix<a_real> lambdaij;

	/// Flux evaluation for LHS; stores Euler flux for either cell for each face
	amat::Matrix<a_real>* elemfaceflux;

public:
	ImplicitSolverMF(const UMesh2dh* const mesh, const int _order, std::string invflux, std::string reconst, std::string limiter, std::string linear_solver, 
			const double cfl, const double cfl_init, const int switchstepi, const int switchstep, const double relaxation_factor);

	~ImplicitSolverMF();

	/// Compute diagonal blocks and eigenvalues of simplified flux for LHS; note that only the flux jacobian is computed. V/dt terms are not added.
	/** Boundary conditions need to be taken into account for the diagonal blocks
	 * so make sure this function is called after the previous one (compute_RHS) as ug is needed.
	 */
	virtual void compute_LHS();
	
	virtual void solve() = 0;
};

/// Solves for a steady-state solution by a first-order implicit scheme
/** The ODE system is linearized at each time step; equivalent to a single Newton iteration at each time step.
 * Only one SSOR iteration is carried out per time step.
 */
class LUSSORSteadyStateImplicitSolverMF : public ImplicitSolverMF
{
	const a_real steadytol;
	const int steadymaxiter;
public:
	LUSSORSteadyStateImplicitSolverMF(const UMesh2dh* const mesh, const int _order, std::string invflux, std::string reconst, std::string limiter, const double cfl, const double icfl, const int switchstepi, const int swtchstp,
			const double omega, const a_real steady_tol, const int steady_maxiter);

	/// Solves a steady problem by an implicit method first order in time, using local time-stepping
	void solve();
};

/// Solves for a steady-state solution by a first-order implicit scheme
/** The ODE system is linearized at each time step; equivalent to a single Newton iteration at each time step.
 */
class SteadyStateImplicitSolverMF : public ImplicitSolverMF
{
	const a_real lintol;
	const a_real steadytol;
	const int linmaxiter;
	const int steadymaxiter;
public:
	SteadyStateImplicitSolverMF(const UMesh2dh* const mesh, const int _order, std::string invflux, std::string reconst, std::string limiter, std::string linear_solver, 
			const double cfl, const double icfl, const int switchstepi, const int swtchstp, const double omega, const a_real steady_tol, const int steady_maxiter, const a_real lin_tol, const int lin_maxiter);

	/// Solves a steady problem by an implicit method first order in time, using local time-stepping
	void solve();
};

class UnsteadyImplicitSolver : public ImplicitSolver
{
	const a_real lintol;
	const a_real newtontol;
	const int linmaxiter;
	const int newtonmaxiter;
public:
	UnsteadyImplicitSolver(const UMesh2dh* mesh, const int _order, std::string invflux, std::string reconst, std::string limiter, std::string linear_solver, 
			const double cfl, const double icfl, const int switchstepi, const int swtchstp, const double omega, const a_real lin_tol, const int lin_maxiter, const a_real newton_tol, const int newton_maxiter);

	/// Solves unsteady problem
	virtual void solve() = 0;
};

/// Solves an unsteady problem using first-order implicit time-stepping
class BackwardEulerSolver : public UnsteadyImplicitSolver
{
public:
	BackwardEulerSolver(const UMesh2dh* mesh, const int _order, std::string invflux, std::string reconst, std::string limiter, std::string linear_solver, 
			const double cfl, const double icfl, const int switchstepi, const int swtchstp, const double omega, const a_real lin_tol, const int lin_maxiter, const a_real newton_tol, const int newton_maxiter);

	void solve();
};

}	// end namespace
#endif
