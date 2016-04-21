/** @file areconstruction.cpp
 * @brief Implementations for different gradient reconstruction schemes.
 * @author Aditya Kashi
 * @date February 3, 2016
 */

#include <areconstruction.hpp>

namespace acfd
{

Reconstruction::~Reconstruction()
{ }

void Reconstruction::setup(const UMesh2dh* mesh, const amat::Matrix<acfd_real>* unk, const amat::Matrix<acfd_real>* unkg, amat::Matrix<acfd_real>* gradx, amat::Matrix<acfd_real>* grady, 
		const amat::Matrix<acfd_real>* _rc, const amat::Matrix<acfd_real>* const _rcg)
{
	m = mesh;
	u = unk;
	ug = unkg;
	dudx = gradx;
	dudy = grady;
	rc = _rc;
	rcg = _rcg;
	nvars = u->cols();
}

/* The state at the face is approximated as an inverse-distance-weighted average.
 */
void GreenGaussReconstruction::compute_gradients()
{
	dudx->zeros(); dudy->zeros();
	
	int iface, idim, ielem, jelem, ivar, ip1, ip2;
	acfd_real areainv1, areainv2;
	std::vector<acfd_real> ut(nvars);
	acfd_real dL, dR, mid[NDIM];
	
	for(iface = 0; iface < m->gnbface(); iface++)
	{
		ielem = m->gintfac(iface,0);
		ip1 = m->gintfac(iface,2);
		ip2 = m->gintfac(iface,3);
		dL = 0; dR = 0;
		for(idim = 0; idim < NDIM; idim++)
		{
			mid[idim] = (m->gcoords(ip1,idim) + m->gcoords(ip2,idim)) * 0.5;
			dL += (mid[idim]-rc->get(ielem,idim))*(mid[idim]-rc->get(ielem,idim));
			dR += (mid[idim]-rcg->get(iface,idim))*(mid[idim]-rcg->get(iface,idim));
		}
		dL = sqrt(dL);
		dR = sqrt(dR);
		areainv1 = 2.0/m->gjacobians(ielem);
		
		for(ivar = 0; ivar < nvars; ivar++)
		{
			ut[ivar] = (u->get(ielem,ivar)*dL + ug->get(iface,ivar)*dR)/(dL+dR) * m->ggallfa(iface,2);
			(*dudx)(ielem,ivar) += (ut[ivar] * m->ggallfa(iface,0))*areainv1;
			(*dudy)(ielem,ivar) += (ut[ivar] * m->ggallfa(iface,1))*areainv1;
		}
	}

	for(iface = m->gnbface(); iface < m->gnaface(); iface++)
	{
		ielem = m->gintfac(iface,0);
		jelem = m->gintfac(iface,1);
		ip1 = m->gintfac(iface,2);
		ip2 = m->gintfac(iface,3);
		dL = 0; dR = 0;
		for(idim = 0; idim < NDIM; idim++)
		{
			mid[idim] = (m->gcoords(ip1,idim) + m->gcoords(ip2,idim)) * 0.5;
			dL += (mid[idim]-rc->get(ielem,idim))*(mid[idim]-rc->get(ielem,idim));
			dR += (mid[idim]-rc->get(jelem,idim))*(mid[idim]-rc->get(jelem,idim));
		}
		dL = sqrt(dL);
		dR = sqrt(dR);
		areainv1 = 2.0/m->gjacobians(ielem);
		areainv2 = 2.0/m->gjacobians(jelem);
		
		for(ivar = 0; ivar < nvars; ivar++)
		{
			ut[ivar] = (u->get(ielem,ivar)*dL + u->get(jelem,ivar)*dR)/(dL+dR) * m->ggallfa(iface,2);
			(*dudx)(ielem,ivar) += (ut[ivar] * m->ggallfa(iface,0))*areainv1;
			(*dudy)(ielem,ivar) += (ut[ivar] * m->ggallfa(iface,1))*areainv1;
			(*dudx)(jelem,ivar) -= (ut[ivar] * m->ggallfa(iface,0))*areainv2;
			(*dudy)(jelem,ivar) -= (ut[ivar] * m->ggallfa(iface,1))*areainv2;
		}
	}
}


void WeightedLeastSquaresReconstruction::setup(const UMesh2dh* mesh, const amat::Matrix<acfd_real>* unk, const amat::Matrix<acfd_real>* unkg, amat::Matrix<acfd_real>* gradx, amat::Matrix<acfd_real>* grady, 
		const amat::Matrix<acfd_real>* _rc, const amat::Matrix<acfd_real>* const _rcg)
{
	Reconstruction::setup(mesh, unk, unkg, gradx, grady, _rc, _rcg);
	std::cout << "WeightedLeastSquaresReconstruction: Setting up leastsquares; nvars = " << nvars << std::endl;

	V.resize(m->gnelem());
	f.resize(m->gnelem());
	for(int i = 0; i < m->gnelem(); i++)
	{
		V[i].setup(2,2);
		V[i].zeros();
		f[i].setup(2,nvars);
	}
	d.setup(2,nvars);
	idets.setup(m->gnelem(),1);
	du.setup(nvars,1);

	// compute LHS of least-squares problem
	int iface, ielem, jelem, idim;
	acfd_real w2, dr[2];

	for(iface = 0; iface < m->gnbface(); iface++)
	{
		ielem = m->gintfac(iface,0);
		w2 = 0;
		for(idim = 0; idim < 2; idim++)
		{
			w2 += (rc->get(ielem,idim)-rcg->get(iface,idim))*(rc->get(ielem,idim)-rcg->get(iface,idim));
			dr[idim] = rc->get(ielem,idim)-rcg->get(iface,idim);
		}
		w2 = 1.0/w2;

		V[ielem](0,0) += w2*dr[0]*dr[0];
		V[ielem](1,1) += w2*dr[1]*dr[1];
		V[ielem](0,1) += w2*dr[0]*dr[1];
		V[ielem](1,0) += w2*dr[0]*dr[1];
	}
	for(iface = m->gnbface(); iface < m->gnaface(); iface++)
	{
		ielem = m->gintfac(iface,0);
		jelem = m->gintfac(iface,1);
		w2 = 0;
		for(idim = 0; idim < 2; idim++)
		{
			w2 += (rc->get(ielem,idim)-rc->get(jelem,idim))*(rc->get(ielem,idim)-rc->get(jelem,idim));
			dr[idim] = rc->get(ielem,idim)-rc->get(jelem,idim);
		}
		w2 = 1.0/w2;

		V[ielem](0,0) += w2*dr[0]*dr[0];
		V[ielem](1,1) += w2*dr[1]*dr[1];
		V[ielem](0,1) += w2*dr[0]*dr[1];
		V[ielem](1,0) += w2*dr[0]*dr[1];
		
		V[jelem](0,0) += w2*dr[0]*dr[0];
		V[jelem](1,1) += w2*dr[1]*dr[1];
		V[jelem](0,1) += w2*dr[0]*dr[1];
		V[jelem](1,0) += w2*dr[0]*dr[1];
	}

	for(ielem = 0; ielem < m->gnelem(); ielem++)
	{
		idets(ielem) = 1.0/(V[ielem].get(0,0)*V[ielem].get(1,1) - V[ielem].get(0,1)*V[ielem].get(1,0));
		f[ielem].zeros();
	}
}

void WeightedLeastSquaresReconstruction::compute_gradients()
{
	int iface, ielem, jelem, idim, ivar;
	acfd_real w2, dr[2];

	// compute least-squares RHS

	for(iface = 0; iface < m->gnbface(); iface++)
	{
		ielem = m->gintfac(iface,0);
		w2 = 0;
		for(idim = 0; idim < 2; idim++)
		{
			w2 += (rc->get(ielem,idim)-rcg->get(iface,idim))*(rc->get(ielem,idim)-rcg->get(iface,idim));
			dr[idim] = rc->get(ielem,idim)-rcg->get(iface,idim);
		}
		w2 = 1.0/w2;
		for(ivar = 0; ivar < nvars; ivar++)
			du(ivar) = u->get(ielem,ivar) - ug->get(iface,ivar);
		
		for(ivar = 0; ivar < nvars; ivar++)
		{
			f[ielem](0,ivar) += w2*dr[0]*du(ivar);
			f[ielem](1,ivar) += w2*dr[1]*du(ivar);
		}
	}
	for(iface = m->gnbface(); iface < m->gnaface(); iface++)
	{
		ielem = m->gintfac(iface,0);
		jelem = m->gintfac(iface,1);
		w2 = 0;
		for(idim = 0; idim < 2; idim++)
		{
			w2 += (rc->get(ielem,idim)-rc->get(jelem,idim))*(rc->get(ielem,idim)-rc->get(jelem,idim));
			dr[idim] = rc->get(ielem,idim)-rc->get(jelem,idim);
		}
		w2 = 1.0/w2;
		for(ivar = 0; ivar < nvars; ivar++)
			du(ivar) = u->get(ielem,ivar) - u->get(jelem,ivar);

		for(ivar = 0; ivar < nvars; ivar++)
		{
			f[ielem](0,ivar) += w2*dr[0]*du(ivar);
			f[ielem](1,ivar) += w2*dr[1]*du(ivar);
			f[jelem](0,ivar) += w2*dr[0]*du(ivar);
			f[jelem](1,ivar) += w2*dr[1]*du(ivar);
		}
	}

	// solve normal equations by Cramer's rule
	for(ielem = 0; ielem < m->gnelem(); ielem++)
	{
		for(ivar = 0; ivar < nvars; ivar++)
		{
			(*dudx)(ielem,ivar) = (f[ielem].get(0,ivar)*V[ielem].get(1,1) - f[ielem].get(1,ivar)*V[ielem].get(0,1)) * idets.get(ielem);
			(*dudy)(ielem,ivar) = (V[ielem].get(0,0)*f[ielem].get(1,ivar) - V[ielem].get(1,0)*f[ielem].get(0,ivar)) * idets.get(ielem);
		}
		f[ielem].zeros();
	}
}

} // end namespace
