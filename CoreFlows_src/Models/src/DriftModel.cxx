/*
 * DriftModel.cxx
 *
 *  Created on: 1 janv. 2015
 *      Author: Michael Ndjinga
 */
#include "DriftModel.hxx"

using namespace std;

DriftModel::DriftModel(pressureEstimate pEstimate, int dim){
	_Ndim=dim;
	_nVar=_Ndim+3;
	_nbPhases  = 2;
	_dragCoeffs=vector<double>(2,0);
	_fluides.resize(2);
	_saveVoidFraction=false;
	_saveEnthalpy=false;

	if( pEstimate==around1bar300K){//EOS at 1 bar and 373K
		cout<<"Fluid is water-Gas mixture around saturation point 1 bar and 373 K (100°C)"<<endl;
		_Tsat=373;//saturation temperature at 1 bar
		double esatv=2.505e6;//Gas internal energy at saturation at 1 bar
		double cv_v=1555;//Gas specific heat capacity at saturation at 1 bar
		double gamma_v=1.337;//Gas heat capacity ratio at saturation at 1 bar
		double cv_l=3770;//water specific heat capacity at saturation at 1 bar
		double rho_sat_l=958;//water density at saturation at 1 bar
		double esatl=4.174e5;//water internal energy at saturation at 1 bar
		double sound_speed_l=1543;//water sound speed at saturation at 1 bar
		_fluides[0] = new StiffenedGas(gamma_v,cv_v,_Tsat,esatv);  //ideal gas law for Gas at pressure 1 bar and temperature 100°C, gamma=1.34
		_fluides[1] = new StiffenedGas(rho_sat_l,1e5,_Tsat,esatl,sound_speed_l,cv_l);  //stiffened gas law for water at pressure 1 bar and temperature 100°C
		_hsatl=4.175e5;//water enthalpy at saturation at 1 bar
		_hsatv=2.675e6;//Gas enthalpy at saturation at 1 bar
	}
	else{//EOS at 155 bars and 618K
		cout<<"Fluid is water-Gas mixture around saturation point 155 bars and 618 K (345°C)"<<endl;
		_Tsat=618;//saturation temperature at 155 bars
		_hsatl=1.63e6;//water enthalpy at saturation at 155 bars
		_hsatv=2.6e6;//Gas enthalpy at saturation at 155 bars
		_fluides[0] = new StiffenedGasDellacherie(1.43,0  ,2.030255e6  ,1040.14,_Tsat,2.597e6); //stiffened gas law for Gas from S. Dellacherie//dernier paramètre non utilise, sert juste a eviter d'avoir deux constructeurs de même signature
		_fluides[1] = new StiffenedGasDellacherie(2.35,1e9,-1.167056e6,1816.2,_Tsat,1.6299e6); //stiffened gas law for water from S. Dellacherie// dernier paramètre non utilise, sert juste a eviter d'avoir deux constructeurs de même signature

		/*
		double esatv=2.444e6;//Gas internal energy at saturation at 155 bar
		double esatl=1.604e6;//water internal energy at saturation at 155 bar
		double sound_speed_v=433.4;//Gas sound speed at saturation at 155 bar
		double sound_speed_l=621.4;//water sound speed at saturation at 155 bar
		double cv_v=3633;//Gas specific heat capacity at saturation at 155 bar
		double cv_l=3101;//water specific heat capacity at saturation at 155 bar
		double rho_sat_v=101.9;//Gas density at saturation at 155 bar
		double rho_sat_l=594.4;//water density at saturation at 155 bar
		_fluides[0] = new StiffenedGas(rho_sat_v,1.55e7,_Tsat,esatv, sound_speed_v,cv_v); //stiffened gas law for Gas at pressure 155 bar and temperature 345°C
		_fluides[1] = new StiffenedGas(rho_sat_l,1.55e7,_Tsat,esatl, sound_speed_l,cv_l); //stiffened gas law for water at pressure 155 bar
		 */

	}
	_latentHeat=_hsatv-_hsatl;
	cout<<"Liquid saturation enthalpy "<< _hsatl<<" J/Kg"<<endl;
	cout<<"Vapour saturation enthalpy "<< _hsatv<<" J/Kg"<<endl;
	cout<<"Latent heat "<< _latentHeat<<endl;
}

void DriftModel::initialize(){
	cout<<"Initialising the drift model"<<endl;

	_Uroe = new double[_nVar];
	_gravite = vector<double>(_nVar,0);
	for(int i=0; i<_Ndim; i++)
		_gravite[i+2]=_gravity3d[i];

	_Gravity = new PetscScalar[_nVar*_nVar];
	for(int i=0; i<_nVar*_nVar;i++)
		_Gravity[i] = 0;
	if(_timeScheme==Implicit)
	{
		for(int i=0; i<_nVar;i++)
			_Gravity[i*_nVar]=-_gravite[i];
	}

	if(_saveVelocity)
		_Vitesse=Field("Velocity",CELLS,_mesh,3);//Forcement en dimension 3 (3 composantes) pour le posttraitement des lignes de courant
	if(_saveVoidFraction)
		_VoidFraction=Field("Void fraction",CELLS,_mesh,1);
	if(_saveEnthalpy)
		_Enthalpy=Field("Enthalpy",CELLS,_mesh,1);

	if(_entropicCorrection)
		_entropicShift=vector<double>(3);//at most 3 distinct eigenvalues

	ProblemFluid::initialize();
}

void DriftModel::convectionState( const long &i, const long &j, const bool &IsBord){
	//	sortie: etat de Roe rho, cm, v, H,T

	_idm[0] = _nVar*i;
	for(int k=1; k<_nVar; k++)
		_idm[k] = _idm[k-1] + 1;
	VecGetValues(_courant, _nVar, _idm, _Ui);

	_idm[0] = _nVar*j;
	for(int k=1; k<_nVar; k++)
		_idm[k] = _idm[k-1] + 1;
	if(IsBord)
		VecGetValues(_Uext, _nVar, _idm, _Uj);
	else
		VecGetValues(_courant, _nVar, _idm, _Uj);
	if(_verbose && _nbTimeStep%_freqSave ==0)
	{
		cout<<"Convection Left state cell " << i<< ": "<<endl;
		for(int k =0; k<_nVar; k++)
			cout<< _Ui[k]<<endl;
		cout<<"Convection Right state cell " << j<< ": "<<endl;
		for(int k =0; k<_nVar; k++)
			cout<< _Uj[k]<<endl;
	}
	if(_Ui[0]<0||_Uj[0]<0)
	{
		cout<<"!!!!!!!!!!!!!!!!!!!!!!!!densite de melange negative, arret de calcul!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"<<endl;
		throw CdmathException("densite negative, arret de calcul");
	}
	PetscScalar ri, rj, xi, xj, pi, pj;
	PetscInt Ii;
	ri = sqrt(_Ui[0]);//racine carre de phi_i rho_i
	rj = sqrt(_Uj[0]);

	_Uroe[0] = ri*rj/sqrt(_porosityi*_porosityj);	//moyenne geometrique des densites
	if(_verbose && _nbTimeStep%_freqSave ==0)
		cout << "Densité moyenne Roe gauche " << i << ": " << ri*ri << ", droite " << j << ": " << rj*rj << "->" << _Uroe[0] << endl;
	xi = _Ui[1]/_Ui[0];//cm gauche
	xj = _Uj[1]/_Uj[0];//cm droite

	_Uroe[1] = (xi*ri + xj*rj)/(ri + rj);//moyenne de Roe des concentrations
	if(_verbose && _nbTimeStep%_freqSave ==0)
		cout << "Concentration de Roe  gauche " << i << ": " << xi << ", droite " << j << ": " << xj << "->" << _Uroe[1] << endl;
	for(int k=0;k<_Ndim;k++){
		xi = _Ui[k+2];//phi rho u gauche
		xj = _Uj[k+2];//phi rho u droite
		_Uroe[2+k] = (xi/ri + xj/rj)/(ri + rj);
		//"moyenne" des vitesses
		if(_verbose && _nbTimeStep%_freqSave ==0)
			cout << "Vitesse de Roe composante "<< k<<"  gauche " << i << ": " << xi/(ri*ri) << ", droite " << j << ": " << xj/(rj*rj) << "->" << _Uroe[k+2] << endl;
	}
	// H = (rho E + p)/rho
	xi = _Ui[_nVar-1];//phi rho E
	xj = _Uj[_nVar-1];
	Ii = i*_nVar+1; // correct Kieu
	VecGetValues(_primitives, 1, &Ii, &pi);// _primitives pour p
	if(IsBord)
	{
		consToPrim(_Uj,_Vj,_porosityj);
		pj =  _Vj[1];
	}
	else
	{
		Ii = j*_nVar+1; // correct Kieu
		VecGetValues(_primitives, 1, &Ii, &pj);
	}
	xi = (xi + pi)/_Ui[0];
	xj = (xj + pj)/_Uj[0];
	_Uroe[_nVar-1] = (ri*xi + rj*xj)/(ri + rj);
	if(_verbose && _nbTimeStep%_freqSave ==0)
		cout << "Enthalpie totale de Roe H  gauche " << i << ": " << xi << ", droite " << j << ": " << xj << "->" << _Uroe[_nVar-1] << endl;
	// Moyenne de Roe de Tg et Td
	Ii = i*_nVar+_nVar-1;
	VecGetValues(_primitives, 1, &Ii, &xi);// _primitives pour T
	if(IsBord)
	{
		//consToPrim(_Uj,_Vj,_porosityj);//Fonction déjà appelée
		xj =  _Vj[_nVar-1];
	}
	else
	{
		Ii = j*_nVar+_nVar-1;
		VecGetValues(_primitives, 1, &Ii, &xj);
	}
	if(_verbose && _nbTimeStep%_freqSave ==0)
	{
		cout<<"etat de roe"<<endl;
		for(int k=0;k<_nVar;k++)
			cout<< _Uroe[k]<<" , "<<endl;
	}
}

void DriftModel::diffusionStateAndMatrices(const long &i,const long &j, const bool &IsBord){
	//sortie: matrices et etat de diffusion (rho, rho cm, q, T)

	_idm[0] = _nVar*i;
	for(int k=1; k<_nVar; k++)
		_idm[k] = _idm[k-1] + 1;

	VecGetValues(_courant, _nVar, _idm, _Ui);
	_idm[0] = _nVar*j;
	for(int k=1; k<_nVar; k++)
		_idm[k] = _idm[k-1] + 1;

	if(IsBord)
		VecGetValues(_Uextdiff, _nVar, _idm, _Uj);
	else
		VecGetValues(_courant, _nVar, _idm, _Uj);

	if(_verbose && _nbTimeStep%_freqSave ==0)
	{
		cout << "DriftModel::diffusionStateAndMatrices cellule gauche" << i << endl;
		cout << "Ui = ";
		for(int q=0; q<_nVar; q++)
			cout << _Ui[q]  << "\t";
		cout << endl;
		cout << "DriftModel::diffusionStateAndMatrices cellule droite" << j << endl;
		cout << "Uj = ";
		for(int q=0; q<_nVar; q++)
			cout << _Uj[q]  << "\t";
		cout << endl;
	}

	for(int k=0; k<_nVar; k++)
		_Udiff[k] = (_Ui[k]/_porosityi+_Uj[k]/_porosityj)/2;
	if(_verbose && _nbTimeStep%_freqSave ==0)
	{
		cout << "SinglePhase::diffusionStateAndMatrices conservative diffusion state" << endl;
		cout << "_Udiff = ";
		for(int q=0; q<_nVar; q++)
			cout << _Udiff[q]  << "\t";
		cout << endl;
		cout << "porosite gauche= "<<_porosityi<< ", porosite droite= "<<_porosityj<<endl;
	}

	consToPrim(_Udiff,_phi,1);
	_Udiff[_nVar-1]=_phi[_nVar-1];

	double Tm=_phi[_nVar-1];
	double RhomEm=_Udiff[_nVar-1];
	_Udiff[_nVar-1]=Tm;

	if(_timeScheme==Implicit)
	{
		double q_2=0;
		for (int l = 0; l<_Ndim;l++)
			q_2+=_Udiff[l+2]*_Udiff[l+2];
		double pression=_phi[1];
		double m_v=_Udiff[1];
		double m_l=_Udiff[0]-_Udiff[1];
		double rho_v=_fluides[0]->getDensity(pression,Tm);
		double rho_l=_fluides[1]->getDensity(pression,Tm);
		double alpha_v=m_v/rho_v,alpha_l=1-alpha_v;

		for(int l=0; l<_nVar*_nVar;l++)
			_Diffusion[l] = 0;
		double mu = alpha_v*_fluides[0]->getViscosity(Tm)+alpha_l*_fluides[1]->getViscosity(Tm);
		for(int l=2;l<(_nVar-1);l++)
		{
			_Diffusion[l*_nVar] =  mu*_Udiff[l]/(_Udiff[0]*_Udiff[0]);
			_Diffusion[l*_nVar+l] = -mu/_Udiff[0];
		}
		double lambda = alpha_v*_fluides[0]->getConductivity(Tm)+alpha_l*_fluides[1]->getConductivity(Tm);
		double C_v=  alpha_v*_fluides[0]->constante("cv");
		double C_l=	 alpha_l*_fluides[1]->constante("cv");
		double ev0=_fluides[0]->getInternalEnergy(0,rho_v);//Corriger
		double el0=_fluides[1]->getInternalEnergy(0,rho_l);//Corriger
		double Rhomem=RhomEm-0.5*q_2/(_Udiff[0]*_Udiff[0]);
		int i = (_nVar-1)*_nVar;
		//Formules a verifier
		_Diffusion[i]=lambda*((Rhomem-m_v*ev0-m_l*el0)*C_l/((m_v*C_v+m_l*C_l)*(m_v*C_v+m_l*C_l))+el0/(m_v*C_v+m_l*C_l)-q_2/(2*_Udiff[0]*_Udiff[0]*(m_v*C_v+m_l*C_l)));
		_Diffusion[i+1]=lambda*((Rhomem-m_v*ev0-m_l*el0)*(C_v-C_l)/((m_v*C_v+m_l*C_l)*(m_v*C_v+m_l*C_l))+(ev0+el0)/(m_v*C_v+m_l*C_l));
		for(int k=2;k<(_nVar-1);k++)
		{
			_Diffusion[i+k]= lambda*_Udiff[k]/(_Udiff[0]*(m_v*C_v+m_l*C_l));
		}
		_Diffusion[_nVar*_nVar-1]=-lambda/(m_v*C_v+m_l*C_l);
		/*Affichages */
		if(_verbose && _nbTimeStep%_freqSave ==0)
		{
			cout << "Matrice de diffusion D, pour le couple (" << i << "," << j<< "):" << endl;
			for(int i=0; i<_nVar; i++)
			{
				for(int j=0; j<_nVar; j++)
					cout << _Diffusion[i*_nVar+j]<<", ";
				cout << endl;
			}
			cout << endl;
		}

	}
}

void DriftModel::setBoundaryState(string nameOfGroup, const int &j,double *normale){
	int k;
	double v2=0, q_n=0;//quantité de mouvement normale à la paroi
	_idm[0] = _nVar*j;
	for(k=1; k<_nVar; k++)
		_idm[k] = _idm[k-1] + 1;
	VecGetValues(_courant, _nVar, _idm, _externalStates);//On recupere le champ conservatif de la cellule interne
	for(k=0; k<_Ndim; k++)
		q_n+=_externalStates[(k+2)]*normale[k];

	if(_verbose && _nbTimeStep%_freqSave ==0)
	{
		cout << "Boundary conditions for group "<< nameOfGroup << " face unit normal vector "<<endl;
		for(k=0; k<_Ndim; k++){
			cout<<normale[k]<<", ";
		}
		cout<<endl;
	}

	if (_limitField[nameOfGroup].bcType==Wall){
		for(k=0; k<_Ndim; k++)
			_externalStates[(k+2)]-= 2*q_n*normale[k];

		_idm[0] = 0;
		for(k=1; k<_nVar; k++)
			_idm[k] = _idm[k-1] + 1;

		VecAssemblyBegin(_Uext);
		VecSetValues(_Uext, _nVar, _idm, _externalStates, INSERT_VALUES);
		VecAssemblyEnd(_Uext);

		//Pour la diffusion, paroi à vitesse et temperature imposees
		_idm[0] = _nVar*j;
		for(k=1; k<_nVar; k++)
			_idm[k] = _idm[k-1] + 1;
		VecGetValues(_primitives, _nVar, _idm, _Vj);
		double concentration=_Vj[0];
		double pression=_Vj[1];
		double T=_limitField[nameOfGroup].T;
		double rho_v=_fluides[0]->getDensity(pression,T);
		double rho_l=_fluides[1]->getDensity(pression,T);
		if(fabs(concentration*rho_l+(1-concentration)*rho_v)<_precision)
		{
			cout<<"rhov= "<<rho_v<<", rhol= "<<rho_l<<endl;
			cout<<"concentration*rho_l+(1-concentration)*rho_v= "<<concentration*rho_l+(1-concentration)*rho_v<<endl;
			throw CdmathException("DriftModel::setBoundaryState: Inlet, impossible to compute mixture density, division by zero");
		}

		_externalStates[0]=rho_v*rho_l/(concentration*rho_l+(1-concentration)*rho_v);
		_externalStates[1]=concentration*_externalStates[0];

		_externalStates[2]=_externalStates[0]*_limitField[nameOfGroup].v_x[0];
		v2 +=_limitField[nameOfGroup].v_x[0]*_limitField[nameOfGroup].v_x[0];
		if(_Ndim>1)
		{
			v2 +=_limitField[nameOfGroup].v_y[0]*_limitField[nameOfGroup].v_y[0];
			_externalStates[3]=_externalStates[0]*_limitField[nameOfGroup].v_y[0];
			if(_Ndim==3)
			{
				_externalStates[4]=_externalStates[0]*_limitField[nameOfGroup].v_z[0];
				v2 +=_limitField[nameOfGroup].v_z[0]*_limitField[nameOfGroup].v_z[0];
			}
		}
		_externalStates[_nVar-1] = _externalStates[1]*_fluides[0]->getInternalEnergy(_limitField[nameOfGroup].T,rho_v)
																																													 +(_externalStates[0]-_externalStates[1])*_fluides[1]->getInternalEnergy(_limitField[nameOfGroup].T,rho_l) + _externalStates[0]*v2/2;
		_idm[0] = 0;
		for(k=1; k<_nVar; k++)
			_idm[k] = _idm[k-1] + 1;
		VecAssemblyBegin(_Uextdiff);
		VecSetValues(_Uextdiff, _nVar, _idm, _externalStates, INSERT_VALUES);
		VecAssemblyEnd(_Uextdiff);
	}
	else if (_limitField[nameOfGroup].bcType==Neumann){
		_idm[0] = 0;
		for(k=1; k<_nVar; k++)
			_idm[k] = _idm[k-1] + 1;

		VecAssemblyBegin(_Uext);
		VecSetValues(_Uext, _nVar, _idm, _externalStates, INSERT_VALUES);
		VecAssemblyEnd(_Uext);

		VecAssemblyBegin(_Uextdiff);
		VecSetValues(_Uextdiff, _nVar, _idm, _externalStates, INSERT_VALUES);
		VecAssemblyEnd(_Uextdiff);
	}
	else if (_limitField[nameOfGroup].bcType==Inlet){

		if(q_n<=0){
			VecGetValues(_primitives, _nVar, _idm, _Vj);
			double concentration=_limitField[nameOfGroup].conc;
			double pression=_Vj[1];
			double T=_limitField[nameOfGroup].T;
			double rho_v=_fluides[0]->getDensity(pression,T);
			double rho_l=_fluides[1]->getDensity(pression,T);
			if(fabs(concentration*rho_l+(1-concentration)*rho_v)<_precision)
			{
				cout<<"rhov= "<<rho_v<<", rhol= "<<rho_l<<endl;
				cout<<"concentration*rho_l+(1-concentration)*rho_v= "<<concentration*rho_l+(1-concentration)*rho_v<<endl;
				throw CdmathException("DriftModel::setBoundaryState: Inlet, impossible to compute mixture density, division by zero");
			}

			_externalStates[0]=rho_v*rho_l/(concentration*rho_l+(1-concentration)*rho_v);
			_externalStates[1]=concentration*_externalStates[0];
			_externalStates[2]=_externalStates[0]*(_limitField[nameOfGroup].v_x[0]);
			v2 +=(_limitField[nameOfGroup].v_x[0])*(_limitField[nameOfGroup].v_x[0]);
			if(_Ndim>1)
			{
				v2 +=_limitField[nameOfGroup].v_y[0]*_limitField[nameOfGroup].v_y[0];
				_externalStates[3]=_externalStates[0]*_limitField[nameOfGroup].v_y[0];
				if(_Ndim==3)
				{
					_externalStates[4]=_externalStates[0]*_limitField[nameOfGroup].v_z[0];
					v2 +=_limitField[nameOfGroup].v_z[0]*_limitField[nameOfGroup].v_z[0];
				}
			}
			_externalStates[_nVar-1] = _externalStates[1]*_fluides[0]->getInternalEnergy(T,rho_v)+(_externalStates[0]-_externalStates[1])*_fluides[1]->getInternalEnergy(T,rho_l) + _externalStates[0]*v2/2;
		}
		else if(_nbTimeStep%_freqSave ==0)
			cout<< "Warning : fluid going out through inlet boundary "<<nameOfGroup<<". Applying Neumann boundary condition"<<endl;

		_idm[0] = 0;
		for(k=1; k<_nVar; k++)
			_idm[k] = _idm[k-1] + 1;

		VecAssemblyBegin(_Uext);
		VecAssemblyBegin(_Uextdiff);
		VecSetValues(_Uext, _nVar, _idm, _externalStates, INSERT_VALUES);
		VecSetValues(_Uextdiff, _nVar, _idm, _externalStates, INSERT_VALUES);
		VecAssemblyEnd(_Uext);
		VecAssemblyEnd(_Uextdiff);
	}
	else if (_limitField[nameOfGroup].bcType==InletPressure){

		VecGetValues(_primitives, _nVar, _idm, _Vj);
		double concentration, Tm;
		if(q_n<=0){
			concentration=_limitField[nameOfGroup].conc;
			Tm=_limitField[nameOfGroup].T;
		}
		else{
			if(_nbTimeStep%_freqSave ==0)
				cout<< "Warning : fluid going out through inletPressure boundary "<<nameOfGroup<<". Applying Neumann boundary condition for concentration, velocity and temperature"<<endl;
			concentration=_Vj[0];
			Tm=_Vj[_nVar-1];
		}

		double pression=_limitField[nameOfGroup].p;
		double rho_v=_fluides[0]->getDensity(pression, Tm);
		double rho_l=_fluides[1]->getDensity(pression, Tm);
		if(fabs(concentration*rho_l+(1-concentration)*rho_v)<_precision)
		{
			cout<<"rhov= "<<rho_v<<", rhol= "<<rho_l<<endl;
			cout<<"concentration*rho_l+(1-concentration)*rho_v= "<<concentration*rho_l+(1-concentration)*rho_v<<endl;
			throw CdmathException("DriftModel::jacobian: Inlet, impossible to compute mixture density, division by zero");
		}

		_externalStates[0]=rho_v*rho_l/(concentration*rho_l+(1-concentration)*rho_v);
		_externalStates[1]=concentration*_externalStates[0];
		double mv=_externalStates[1], ml=_externalStates[0]-_externalStates[1];
		for(k=0; k<_Ndim; k++)
		{
			v2+=_Vj[k+2]*_Vj[k+2];
			_externalStates[k+2]=_externalStates[0]*_Vj[(k+2)] ;
		}
		_externalStates[_nVar-1] = mv*_fluides[0]->getInternalEnergy(Tm,rho_v)+ml*_fluides[1]->getInternalEnergy(Tm,rho_l) +_externalStates[0]* v2/2;

		_idm[0] = 0;
		for(k=1; k<_nVar; k++)
			_idm[k] = _idm[k-1] + 1;
		VecAssemblyBegin(_Uext);
		VecAssemblyBegin(_Uextdiff);
		VecSetValues(_Uext, _nVar, _idm, _externalStates, INSERT_VALUES);
		VecSetValues(_Uextdiff, _nVar, _idm, _externalStates, INSERT_VALUES);
		VecAssemblyEnd(_Uext);
		VecAssemblyEnd(_Uextdiff);
	}
	else if (_limitField[nameOfGroup].bcType==Outlet){
		if(q_n<=0  &&  _nbTimeStep%_freqSave ==0)
			cout<< "Warning : fluid going in through outlet boundary "<<nameOfGroup<<". Applying Neumann boundary condition for concentration, velocity and temperature"<<endl;

		VecGetValues(_primitives, _nVar, _idm, _Vj);

		double concentration=_Vj[0];
		double pression=_limitField[nameOfGroup].p;
		double Tm=_Vj[_nVar-1];
		double rho_v=_fluides[0]->getDensity(pression, Tm);
		double rho_l=_fluides[1]->getDensity(pression, Tm);
		if(fabs(concentration*rho_l+(1-concentration)*rho_v)<_precision)
		{
			cout<<"rhov= "<<rho_v<<", rhol= "<<rho_l<<endl;
			cout<<"concentration*rho_l+(1-concentration)*rho_v= "<<concentration*rho_l+(1-concentration)*rho_v<<endl;
			throw CdmathException("DriftModel::jacobian: Inlet, impossible to compute mixture density, division by zero");
		}

		_externalStates[0]=rho_v*rho_l/(concentration*rho_l+(1-concentration)*rho_v);
		_externalStates[1]=concentration*_externalStates[0];
		double mv=_externalStates[1], ml=_externalStates[0]-_externalStates[1];
		for(k=0; k<_Ndim; k++)
		{
			v2+=_Vj[k+2]*_Vj[k+2];
			_externalStates[k+2]=_externalStates[0]*_Vj[(k+2)] ;
		}
		_externalStates[_nVar-1] = mv*_fluides[0]->getInternalEnergy(Tm,rho_v)+ml*_fluides[1]->getInternalEnergy(Tm,rho_l) +_externalStates[0]* v2/2;

		_idm[0] = 0;
		for(k=1; k<_nVar; k++)
			_idm[k] = _idm[k-1] + 1;
		VecAssemblyBegin(_Uext);
		VecAssemblyBegin(_Uextdiff);
		VecSetValues(_Uext, _nVar, _idm, _externalStates, INSERT_VALUES);
		VecSetValues(_Uextdiff, _nVar, _idm, _externalStates, INSERT_VALUES);
		VecAssemblyEnd(_Uext);
		VecAssemblyEnd(_Uextdiff);
	}
	else {
		cout<<"Boundary condition not set for boundary named "<<nameOfGroup<<endl;
		cout<<"Accepted boundary condition are Neumann, Wall, Inlet, and Outlet"<<endl;
		throw CdmathException("Unknown boundary condition");
	}
}

void DriftModel::convectionMatrices()
{
	//entree: URoe = rho, cm, u, H
	//sortie: matrices Roe+  et Roe- +Roe si scheme centre

	double umn=0, u_2=0; //valeur de u.normale et |u|²
	for(int i=0;i<_Ndim;i++)
	{
		u_2 += _Uroe[2+i]*_Uroe[2+i];
		umn += _Uroe[2+i]*_vec_normal[i];//vitesse normale
	}

	vector<complex<double> > vp_dist(3);

	/*
	if(_spaceScheme==staggered && _nonLinearFormulation==VFFC)//special case
		staggeredVFFCMatrices(umn);
	else
	 */
	{
		double rhom=_Uroe[0];
		double cm=_Uroe[1];
		double Hm=_Uroe[_nVar-1];
		double hm=Hm-0.5*u_2;
		double Tm;

		if(cm<_precision)//pure liquid
			Tm=_fluides[1]->getTemperatureFromEnthalpy(hm,rhom);
		else if(cm>1-_precision)
			Tm=_fluides[0]->getTemperatureFromEnthalpy(hm,rhom);
		else//Hypothèse de saturation
			Tm=_Tsat;

		double pression= getMixturePressure(cm, rhom, Tm);
		double m_v=cm*rhom, m_l=rhom-m_v;
		if(_verbose && _nbTimeStep%_freqSave ==0)
			cout<<"Roe state rhom="<<rhom<<" cm= "<<cm<<" Hm= "<<Hm<<" Tm= "<<Tm<<" pression "<<pression<<endl;
		getMixturePressureDerivatives( m_v, m_l, pression, Tm);
		double ecin=0.5*u_2;

		//On remplit la matrice de Roe
		_Aroe[0*_nVar+0]=0;
		_Aroe[0*_nVar+1]=0;
		for(int i=0;i<_Ndim;i++)
			_Aroe[0*_nVar+2+i]=_vec_normal[i];
		_Aroe[0*_nVar+2+_Ndim]=0;
		_Aroe[1*_nVar+0]=-umn*cm;
		_Aroe[1*_nVar+1]=umn;
		for(int i=0;i<_Ndim;i++)
			_Aroe[1*_nVar+2+i]=cm*_vec_normal[i];
		_Aroe[1*_nVar+2+_Ndim]=0;
		for(int i=0;i<_Ndim;i++)
		{
			_Aroe[(2+i)*_nVar+0]=(_khi+_kappa*ecin)*_vec_normal[i]-umn*_Uroe[2+i];
			_Aroe[(2+i)*_nVar+1]=_ksi*_vec_normal[i];
			for(int j=0;j<_Ndim;j++)
				_Aroe[(2+i)*_nVar+2+j]=_Uroe[2+i]*_vec_normal[j]-_kappa*_vec_normal[i]*_Uroe[2+j];
			_Aroe[(2+i)*_nVar+2+i]+=umn;
			_Aroe[(2+i)*_nVar+2+_Ndim]=_kappa*_vec_normal[i];
		}
		_Aroe[(2+_Ndim)*_nVar+0]=(_khi+_kappa*ecin-Hm)*umn;
		_Aroe[(2+_Ndim)*_nVar+1]=_ksi*umn;
		for(int i=0;i<_Ndim;i++)
			_Aroe[(2+_Ndim)*_nVar+2+i]=Hm*_vec_normal[i]-_kappa*umn*_Uroe[2+i];
		_Aroe[(2+_Ndim)*_nVar+2+_Ndim]=(_kappa+1)*umn;

		if(_kappa*hm+_khi+cm*_ksi<0)
			throw CdmathException("Calcul matrice de Roe: vitesse du son complexe");

		double am=sqrt(_kappa*hm+_khi+cm*_ksi);//vitesse du son du melange
		if(_verbose && _nbTimeStep%_freqSave ==0)
		{
			cout<<"_khi= "<<_khi<<", _kappa= "<< _kappa << ", _ksi= "<<_ksi <<", am= "<<am<<endl;
			displayMatrix(_Aroe,_nVar,0,0);
		}
		//On remplit les valeurs propres
		vp_dist[0]=umn+am;
		vp_dist[1]=umn-am;
		vp_dist[2]=umn;

		_maxvploc=fabs(umn)+am;
		if(_maxvploc>_maxvp)
			_maxvp=_maxvploc;

		/* Construction des matrices de decentrement */
		if(_spaceScheme== centered){
			if(_entropicCorrection)
				throw CdmathException("DriftModel::roeMatrices: entropy schemes not yet available for drift model");

			for(int i=0; i<_nVar*_nVar;i++)
				_absAroe[i] = 0;
		}
		else if( _spaceScheme ==staggered){
			//Calcul de décentrement de type décalé
			_absAroe[0*_nVar+0]=0;
			_absAroe[0*_nVar+1]=0;
			for(int i=0;i<_Ndim;i++)
				_absAroe[0*_nVar+2+i]=_vec_normal[i];
			_absAroe[0*_nVar+2+_Ndim]=0;
			_absAroe[1*_nVar+0]=-umn*cm;
			_absAroe[1*_nVar+1]=umn;
			for(int i=0;i<_Ndim;i++)
				_absAroe[1*_nVar+2+i]=cm*_vec_normal[i];
			_absAroe[1*_nVar+2+_Ndim]=0;
			for(int i=0;i<_Ndim;i++)
			{
				_absAroe[(2+i)*_nVar+0]=-(_khi+_kappa*ecin)*_vec_normal[i]-umn*_Uroe[2+i];
				_absAroe[(2+i)*_nVar+1]=-_ksi*_vec_normal[i];
				for(int j=0;j<_Ndim;j++)
					_absAroe[(2+i)*_nVar+2+j]=_Uroe[2+i]*_vec_normal[j]+_kappa*_vec_normal[i]*_Uroe[2+j];
				_absAroe[(2+i)*_nVar+2+i]+=umn;
				_absAroe[(2+i)*_nVar+2+_Ndim]=-_kappa*_vec_normal[i];
			}
			_absAroe[(2+_Ndim)*_nVar+0]=(-_khi-_kappa*ecin-Hm)*umn;
			_absAroe[(2+_Ndim)*_nVar+1]=-_ksi*umn;
			for(int i=0;i<_Ndim;i++)
				_absAroe[(2+_Ndim)*_nVar+2+i]=Hm*_vec_normal[i]+_kappa*umn*_Uroe[2+i];
			_absAroe[(2+_Ndim)*_nVar+2+_Ndim]=(-_kappa+1)*umn;

			double signu;
			if(umn>0)
				signu=1;
			else if (umn<0)
				signu=-1;
			else
				signu=0;

			for(int i=0; i<_nVar*_nVar;i++)
				_absAroe[i] *= signu;
		}
		else if(_spaceScheme == upwind || _spaceScheme ==pressureCorrection || _spaceScheme ==lowMach || (_spaceScheme==staggered)){
			if(_entropicCorrection)
				entropicShift(_vec_normal);
			else
				_entropicShift=vector<double>(3,0);//at most 3 distinct eigenvalues

			vector< complex< double > > y (3,0);
			Polynoms Poly;
			for( int i=0 ; i<3 ; i++)
				y[i] = Poly.abs_generalise(vp_dist[i])+1*_entropicShift[i];
			Poly.abs_par_interp_directe(3,vp_dist, _Aroe, _nVar,_precision, _absAroe,y);

			if( _spaceScheme ==pressureCorrection)
			{
				for( int i=0 ; i<_Ndim ; i++)
					for( int j=0 ; j<_Ndim ; j++)
						_absAroe[(2+i)*_nVar+2+j]-=(vp_dist[2].real()-vp_dist[0].real())/2*_vec_normal[i]*_vec_normal[j];
			}
			else if( _spaceScheme ==lowMach){
				double M=sqrt(u_2)/am;
				for( int i=0 ; i<_Ndim ; i++)
					for( int j=0 ; j<_Ndim ; j++)
						_absAroe[(2+i)*_nVar+2+j]-=(1-M)*(vp_dist[2].real()-vp_dist[0].real())/2*_vec_normal[i]*_vec_normal[j];
			}
		}
		else
			throw CdmathException("DriftModel::roeMatrices: scheme not treated");

		for(int i=0; i<_nVar*_nVar;i++)
		{
			_AroeMinus[i] = (_Aroe[i]-_absAroe[i])/2;
			_AroePlus[i]  = (_Aroe[i]+_absAroe[i])/2;
		}
		if(_verbose && _nbTimeStep%_freqSave ==0)
		{
			cout<<"Matrice de Roe"<<endl;
			for(int i=0; i<_nVar;i++){
				for(int j=0; j<_nVar;j++)
					cout<<_Aroe[i*_nVar+j]<<" , ";
				cout<<endl;
			}
			cout<<"Valeur absolue matrice de Roe"<<endl;
			for(int i=0; i<_nVar;i++){
				for(int j=0; j<_nVar;j++)
					cout<<_absAroe[i*_nVar+j]<<" , ";
				cout<<endl;
			}
		}
	}

	/*******Calcul de la matrice signe pour VFFC, VFRoe et décentrement des termes source***/
	if(_entropicCorrection || (_spaceScheme ==pressureCorrection))
	{
		InvMatriceRoe( vp_dist);
		Polynoms Poly;
		Poly.matrixProduct(_absAroe, _nVar, _nVar, _invAroe, _nVar, _nVar, _signAroe);
	}
	else if (_spaceScheme==upwind || (_spaceScheme ==lowMach))//upwind sans entropic
		SigneMatriceRoe( vp_dist);
	else if(_spaceScheme== centered)//centre  sans entropic
		for(int i=0; i<_nVar*_nVar;i++)
			_signAroe[i] = 0;
	else if(_spaceScheme ==staggered )
	{
		double signu;
		if(umn>0)
			signu=1;
		else if (umn<0)
			signu=-1;
		else
			signu=0;
		for(int i=0; i<_nVar*_nVar;i++)
			_signAroe[i] = 0;
		_signAroe[0] = signu;
		_signAroe[1+_nVar] = signu;
		for(int i=2; i<_nVar-1;i++)
			_signAroe[i*_nVar+i] = -signu;
		_signAroe[_nVar*(_nVar-1)+_nVar-1] = signu;
	}
	else
		throw CdmathException("DriftModel::roeMatrices: well balanced option not treated");

	if(_verbose && _nbTimeStep%_freqSave ==0)
	{
		cout<<endl<<"Matrice _AroeMinus"<<endl;
		for(int i=0; i<_nVar;i++)
		{
			for(int j=0; j<_nVar;j++)
				cout << _AroeMinus[i*_nVar+j]<< " , ";
			cout<<endl;
		}
		cout<<endl<<"Matrice _AroePlus"<<endl;
		for(int i=0; i<_nVar;i++)
		{
			for(int j=0; j<_nVar;j++)
				cout << _AroePlus[i*_nVar+j]<< " , ";
			cout<<endl;
		}
	}
}

void DriftModel::addDiffusionToSecondMember
(		const int &i,
		const int &j,
		bool isBord)
{
	double Tm=_Udiff[_nVar-1];
	double lambdal=_fluides[1]->getConductivity(Tm);
	double lambdav = _fluides[0]->getConductivity(Tm);
	double mu_l = _fluides[1]->getViscosity(Tm);
	double mu_v = _fluides[0]->getViscosity(Tm);

	if(mu_v==0 && mu_l ==0 && lambdav==0 && lambdal==0 && _heatTransfertCoeff==0)
		return;

	//extraction des valeurs
	for(int k=0; k<_nVar; k++)
		_idm[k] = _nVar*i + k;

	VecGetValues(_primitives, _nVar, _idm, _Vi);
	if (_verbose && _nbTimeStep%_freqSave ==0)
	{
		cout << "Contribution diffusion: variables primitives maille " << i<<endl;
		for(int q=0; q<_nVar; q++)
			cout << _Vi[q] << endl;
		cout << endl;
	}

	if(!isBord ){
		for(int k=0; k<_nVar; k++)
			_idn[k] = _nVar*j + k;

		VecGetValues(_primitives, _nVar, _idn, _Vj);
	}
	else
	{
		lambdal=max(lambdal,_heatTransfertCoeff);//wall nucleate boing -> larger heat transfer
		for(int k=0; k<_nVar; k++)
			_idn[k] = k;

		VecGetValues(_Uextdiff, _nVar, _idn, _Uj);
		consToPrim(_Uj,_Vj,1);
	}
	if (_verbose && _nbTimeStep%_freqSave ==0)
	{
		cout << "Contribution diffusion: variables primitives maille " <<j <<endl;
		for(int q=0; q<_nVar; q++)
			cout << _Vj[q] << endl;
		cout << endl;
	}
	double pression=(_Vi[1]+_Vj[1])/2;//ameliorer car traitement different pour pression et temperature
	double m_v=_Udiff[1];
	double rho_v=_fluides[0]->getDensity(pression,Tm);
	double rho_l=_fluides[1]->getDensity(pression,Tm);
	double alpha_v=m_v/rho_v,alpha_l=1-alpha_v;
	double mu = alpha_v*mu_v+alpha_l*mu_l;
	double lambda = alpha_v*lambdav+alpha_l*lambdal;

	//pas de diffusion sur la masse totale
	_phi[0]=0;
	//on n'a pas encore mis la contribution sur la masse
	_phi[1]=0;
	//contribution visqueuse sur la quantite de mouvement
	for(int k=2; k<_nVar-1; k++)
		_phi[k] = _inv_dxi*2/(1/_inv_dxi+1/_inv_dxj)*mu*(_porosityj*_Vj[k] - _porosityi*_Vi[k]);

	//contribution visqueuse sur l'energie
	_phi[_nVar-1] = _inv_dxi*2/(1/_inv_dxi+1/_inv_dxj)*lambda*(_porosityj*_Vj[_nVar-1] - _porosityi*_Vi[_nVar-1]);

	_idm[0] = i;
	VecSetValuesBlocked(_b, 1, _idm, _phi, ADD_VALUES);

	if(_verbose && _nbTimeStep%_freqSave ==0)
	{
		cout << "Contribution diffusion au 2nd membre pour la maille " << i << ": "<<endl;
		for(int q=0; q<_nVar; q++)
			cout << _phi[q] << endl;
		cout << endl;
	}

	if(!isBord)
	{
		//On change de signe pour l'autre contribution
		for(int k=0; k<_nVar; k++)
			_phi[k] *= -_inv_dxj/_inv_dxi;
		_idn[0] = j;

		VecSetValuesBlocked(_b, 1, _idn, _phi, ADD_VALUES);

		if(_verbose && _nbTimeStep%_freqSave ==0)
		{
			cout << "Contribution diffusion au 2nd membre pour la maille  " << j << ": "<<endl;
			for(int q=0; q<_nVar; q++)
				cout << _phi[q] << endl;
			cout << endl;
		}
	}
}


void DriftModel::sourceVector(PetscScalar * Si,PetscScalar * Ui,PetscScalar * Vi, int i)
{
	double phirho=Ui[0],phim1=Ui[1],phim2=phirho-phim1,phirhoE=Ui[_nVar-1], cv=Vi[0],  P=Vi[1], T=Vi[_nVar-1];
	double norm_u=0;
	for(int k=0; k<_Ndim; k++)
		norm_u+=Vi[2+k]*Vi[2+k];
	norm_u=sqrt(norm_u);
	double h=(phirhoE-0.5*phirho*norm_u*norm_u+P*_porosityField(i))/phirho;//e+p/rho

	Si[0]=0;
	//if(T>_Tsat && cv<1-_precision)
	if(_hsatv>h  && h>_hsatl && cv<1-_precision)//heated boiling//
		Si[1]=_heatPowerField(i)/_latentHeat*_porosityField(i);//phi*gamma
	else if (P<_Psat && cv<1-_precision)//flash boiling
		Si[1]=-_dHsatl_over_dp*_dp_over_dt(i)/_latentHeat;
	else
		Si[1]=0;
	for(int k=2; k<_nVar-1; k++)
		Si[k]  =_gravite[k]*phirho-(phim1*_dragCoeffs[0]+phim2*_dragCoeffs[1])*norm_u*Vi[k];

	Si[_nVar-1]=_heatPowerField(i);

	for(int k=0; k<_Ndim; k++)
		Si[_nVar-1] +=(_gravity3d[k]*phirho-(phim1*_dragCoeffs[0]+phim2*_dragCoeffs[1])*norm_u*Vi[2+k])*Vi[2+k];

	if(_verbose && _nbTimeStep%_freqSave ==0)
	{
		cout<<"DriftModel::sourceVector"<<endl;
		cout<<"Ui="<<endl;
		for(int k=0;k<_nVar;k++)
			cout<<Ui[k]<<", ";
		cout<<endl;
		cout<<"Vi="<<endl;
		for(int k=0;k<_nVar;k++)
			cout<<Vi[k]<<", ";
		cout<<endl;
		cout<<"Si="<<endl;
		for(int k=0;k<_nVar;k++)
			cout<<Si[k]<<", ";
		cout<<endl;
	}
}

void DriftModel::pressureLossVector(PetscScalar * pressureLoss, double K, PetscScalar * Ui, PetscScalar * Vi, PetscScalar * Uj, PetscScalar * Vj)
{
	double norm_u=0, u_n=0, phirho;
	for(int i=0;i<_Ndim;i++)
		u_n += _Uroe[2+i]*_vec_normal[i];

	pressureLoss[0]=0;
	pressureLoss[1]=0;
	if(u_n>0){
		for(int i=0;i<_Ndim;i++)
			norm_u += Vi[2+i]*Vi[2+i];
		norm_u=sqrt(norm_u);
		phirho=Ui[0];
		for(int i=0;i<_Ndim;i++)
			pressureLoss[2+i]=-K*phirho*norm_u*Vi[2+i];
	}
	else{
		for(int i=0;i<_Ndim;i++)
			norm_u += Vj[2+i]*Vj[2+i];
		norm_u=sqrt(norm_u);
		phirho=Uj[0];
		for(int i=0;i<_Ndim;i++)
			pressureLoss[2+i]=-K*phirho*norm_u*Vj[2+i];
	}
	pressureLoss[_nVar-1]=-K*phirho*norm_u*norm_u*norm_u;

	if(_verbose && _nbTimeStep%_freqSave ==0)
	{
		cout<<"pressure loss vector phirho= "<< phirho<<" norm_u= "<<norm_u<<endl;
		cout<<"velocity Vi "<<endl;
		for(int i=0;i<_Ndim;i++)
			cout<< Vi[2+i]<<", ";
		cout<<endl;
		cout<<"velocity Vj= "<<endl;
		for(int i=0;i<_Ndim;i++)
			cout<< Vj[2+i]<<", ";
		cout<<endl;
	}

	if(_verbose && _nbTimeStep%_freqSave ==0)
	{
		cout<<"DriftModel::pressureLossVector K= "<<K<<endl;
		cout<<"Ui="<<endl;
		for(int k=0;k<_nVar;k++)
			cout<<Ui[k]<<", ";
		cout<<endl;
		cout<<"Vi="<<endl;
		for(int k=0;k<_nVar;k++)
			cout<<Vi[k]<<", ";
		cout<<endl;
		cout<<"Uj="<<endl;
		for(int k=0;k<_nVar;k++)
			cout<<Uj[k]<<", ";
		cout<<endl;
		cout<<"Vj="<<endl;
		for(int k=0;k<_nVar;k++)
			cout<<Vj[k]<<", ";
		cout<<endl;
		cout<<"pressureLoss="<<endl;
		for(int k=0;k<_nVar;k++)
			cout<<pressureLoss[k]<<", ";
		cout<<endl;
	}
}
void DriftModel::porosityGradientSourceVector()
{
	double u_ni=0, u_nj=0, rhoi,rhoj, pi=_Vi[1], pj=_Vj[1],pij;
	for(int i=0;i<_Ndim;i++) {
		u_ni += _Vi[2+i]*_vec_normal[i];
		u_nj += _Vj[2+i]*_vec_normal[i];
	}
	_porosityGradientSourceVector[0]=0;
	_porosityGradientSourceVector[1]=0;
	rhoj=_Uj[0]/_porosityj;
	rhoi=_Ui[0]/_porosityi;
	pij=(pi+pj)/2+rhoi*rhoj/2/(rhoi+rhoj)*(u_ni-u_nj)*(u_ni-u_nj);
	for(int i=0;i<_Ndim;i++)
		_porosityGradientSourceVector[2+i]=pij*(_porosityi-_porosityj)*2/(1/_inv_dxi+1/_inv_dxj);
	_porosityGradientSourceVector[_nVar-1]=0;
}

void DriftModel::jacobian(const int &j, string nameOfGroup,double * normale)
{//Jacobienne condition limite convection
	int k;
	for(k=0; k<_nVar*_nVar;k++)
		_Jcb[k] = 0;

	_idm[0] = _nVar*j;
	for(k=1; k<_nVar; k++)
		_idm[k] = _idm[k-1] + 1;
	VecGetValues(_courant, _nVar, _idm, _externalStates);
	double q_n=0;//quantité de mouvement normale à la paroi
	for(k=0; k<_Ndim; k++)
		q_n+=_externalStates[(k+1)]*normale[k];
	return;
	// loop of boundary types
	if (_limitField[nameOfGroup].bcType==Wall)
	{
		for(k=0; k<_nVar;k++)
			_Jcb[k*_nVar + k] = 1;
		for(k=2; k<_nVar-1;k++)
			for(int l=2; l<_nVar-1;l++)
				_Jcb[k*_nVar + l] -= 2*normale[k-2]*normale[l-2];
	}
	else if (_limitField[nameOfGroup].bcType==Inlet)
	{
		if(q_n<0){
			//Boundary state quantities
			double v[_Ndim], ve[_Ndim], v2, ve2;
			VecGetValues(_primitives, _nVar, _idm, _Vj);
			double concentration=_limitField[nameOfGroup].conc;
			double pression=_Vj[1];
			double T=_limitField[nameOfGroup].T;
			double rho_v=_fluides[0]->getDensity(pression,T);
			double rho_l=_fluides[1]->getDensity(pression,T);
			if(fabs(concentration*rho_l+(1-concentration)*rho_v)<_precision)
			{
				cout<<"rhov= "<<rho_v<<", rhol= "<<rho_l<<endl;
				cout<<"concentration*rho_l+(1-concentration)*rho_v= "<<concentration*rho_l+(1-concentration)*rho_v<<endl;
				throw CdmathException("DriftModel::jacobian: Inlet, impossible to compute mixture density, division by zero");
			}

			double rho_e=rho_v*rho_l/(concentration*rho_l+(1-concentration)*rho_v);
			double e_v=_fluides[0]->getInternalEnergy(T,rho_v);
			double e_l=_fluides[1]->getInternalEnergy(T,rho_l);
			ve[0] = _limitField[nameOfGroup].v_x[0];
			v[0]=_Vj[1];
			ve2 = ve[0]*ve[0];
			v2 = v[0]*v[0];
			if (_Ndim >1){
				ve[1] = _limitField[nameOfGroup].v_y[0];
				v[1]=_Vj[2];
				ve2 += ve[1]*ve[1];
				v2 = v[1]*v[1];
			}
			if (_Ndim >2){
				ve[2] = _limitField[nameOfGroup].v_z[0];
				v[2]=_Vj[3];
				ve2 += ve[2]*ve[2];
				v2 = v[2]*v[2];
			}
			double E_e = concentration*e_v+(1-concentration)*e_l + ve2/2;

			//Pressure differential
			double gamma_v =_fluides[0]->constante("gamma");
			double gamma_l =_fluides[1]->constante("gamma");
			double omega=concentration/((gamma_v-1)*rho_v*rho_v*e_v)+(1-concentration)/((gamma_l-1)*rho_l*rho_l*e_l);
			double rhom=_externalStates[0];
			double m_v=concentration*rhom, m_l=rhom-m_v;
			getMixturePressureDerivatives( m_v, m_l, pression, T);

			double CoeffCol1 = rho_e*rho_e*omega*(_khi+_kappa*v2/2);
			double CoeffCol2 = rho_e*rho_e*omega*_ksi;
			double CoeffCol3et4 = rho_e*rho_e*omega*_kappa;

			//Mass line
			_Jcb[0]=CoeffCol1;
			_Jcb[1]=CoeffCol2;
			for(k=0;k<_Ndim;k++)
				_Jcb[2+k]=-CoeffCol3et4*v[k];
			_Jcb[_nVar-1]=CoeffCol3et4;
			//vapour mass line
			for(k=0;k<_nVar;k++)
				_Jcb[_nVar+k]=_Jcb[k]*concentration;
			//Momentum lines
			for(int l=0;l<_Ndim;l++)
				for(k=0;k<_nVar;k++)
					_Jcb[(2+l)*_nVar+k]=_Jcb[k]*ve[l];
			//Energy line
			for(k=0;k<_nVar;k++)
				_Jcb[(_nVar-1)*_nVar+k]=_Jcb[k]*E_e;
		}
		else
			for(k=0;k<_nVar;k++)
				_Jcb[k*_nVar+k]=1;
	}
	else if (_limitField[nameOfGroup].bcType==InletPressure && q_n<0)
	{
		//Boundary state quantities
		double v[_Ndim], v2=0;
		VecGetValues(_primitives, _nVar, _idm, _Vj);
		double concentration=_limitField[nameOfGroup].conc;
		double pression=_limitField[nameOfGroup].p;
		double T=_limitField[nameOfGroup].T;
		double rho_v=_fluides[0]->getDensity(pression,T);
		double rho_l=_fluides[1]->getDensity(pression,T);
		if(fabs(concentration*rho_l+(1-concentration)*rho_v)<_precision)
		{
			cout<<"rhov= "<<rho_v<<", rhol= "<<rho_l<<endl;
			cout<<"concentration*rho_l+(1-concentration)*rho_v= "<<concentration*rho_l+(1-concentration)*rho_v<<endl;
			throw CdmathException("DriftModel::jacobian: InletPressure, impossible to compute mixture density, division by zero");
		}

		double rho_ext=rho_v*rho_l/(concentration*rho_l+(1-concentration)*rho_v);
		double rho_int=_externalStates[0];
		double ratio_densite=rho_ext/rho_int;
		for(k=0;k<_Ndim;k++){
			v[k]=_Vj[2+k];
			v2+=v[k]*v[k];
		}
		//Momentum lines
		for(int l=0;l<_Ndim;l++){
			_Jcb[(2+l)*_nVar]=-ratio_densite*v[l];
			_Jcb[(2+l)*_nVar+2+l]=ratio_densite;
		}
		//Energy line
		_Jcb[(_nVar-1)*_nVar]=-ratio_densite*v2;
		for(int l=0;l<_Ndim;l++)
			_Jcb[(_nVar-1)*_nVar+2+l]=ratio_densite*v[l];

	}
	else if (_limitField[nameOfGroup].bcType==Outlet || (_limitField[nameOfGroup].bcType==InletPressure && q_n>=0)){
		//Boundary state quantities
		double v[_Ndim], v2;
		VecGetValues(_primitives, _nVar, _idm, _Vj);
		double concentration=_Vj[0];
		double pression=_limitField[nameOfGroup].p;
		double T=_Vj[_nVar-1];
		double rho_v=_fluides[0]->getDensity(pression,T);
		double rho_l=_fluides[1]->getDensity(pression,T);
		if(fabs(concentration*rho_l+(1-concentration)*rho_v)<_precision)
		{
			cout<<"rhov= "<<rho_v<<", rhol= "<<rho_l<<endl;
			cout<<"concentration*rho_l+(1-concentration)*rho_v= "<<concentration*rho_l+(1-concentration)*rho_v<<endl;
			throw CdmathException("DriftModel::jacobian: Outlet, impossible to compute mixture density, division by zero");
		}

		double rho_ext=rho_v*rho_l/(concentration*rho_l+(1-concentration)*rho_v);
		double rho_int=_externalStates[0];
		double e_v=_fluides[0]->getInternalEnergy(T,rho_v);
		double e_l=_fluides[1]->getInternalEnergy(T,rho_l);
		double ratio_densite=rho_ext/rho_int;
		for(k=0;k<_Ndim;k++){
			v[k]=_Vj[2+k];
			v2+=v[k]*v[k];
		}
		double E_m = concentration*e_v+(1-concentration)*e_l + v2/2;//total energy

		//Temperature differential
		double C_v=  _fluides[0]->constante("cv");
		double C_l=	 _fluides[1]->constante("cv");
		double ev0=_fluides[0]->getInternalEnergy(0,rho_v);//Corriger
		double el0=_fluides[1]->getInternalEnergy(0,rho_l);//Corriger

		double omega=concentration*C_v/(rho_v*e_v)+(1-concentration)*C_l/(rho_l*e_l);
		double rhomem=_externalStates[0]*(concentration*e_v+(1-concentration)*e_l);
		double m_v=concentration*rho_ext, m_l=rho_ext-m_v;
		double alpha_v=m_v/rho_v,alpha_l=1-alpha_v;
		double denom=m_v *C_v+m_l* C_l;

		double khi=(m_v*(ev0*C_l-el0*C_v)-rhomem*C_l)/(denom*denom);
		double ksi=(rho_ext*(el0*C_v-ev0*C_l)+rhomem*(C_l-C_v))/(denom*denom);
		double kappa=1/denom;

		double CoeffCol1 = rho_int*rho_int*omega*(khi+kappa*v2/2)+ratio_densite;//The +ratio_densite helps filling the lines other than total mass
		double CoeffCol2 = rho_int*rho_int*omega*ksi;
		double CoeffCol3et4 = rho_int*rho_int*omega*kappa;

		//Mass line
		_Jcb[0]=-CoeffCol1;
		_Jcb[1]=-CoeffCol2;
		for(k=0;k<_Ndim;k++)
			_Jcb[2+k]=CoeffCol3et4*v[k];
		_Jcb[_nVar-1]=-CoeffCol3et4;
		//vapour mass line
		for(k=0;k<_nVar;k++)
			_Jcb[_nVar+k]=_Jcb[k]*concentration;
		//Momentum lines
		for(int l=0;l<_Ndim;l++)
			for(k=0;k<_nVar;k++)
				_Jcb[(2+l)*_nVar+k]=_Jcb[k]*v[l];
		//Energy line
		for(k=0;k<_nVar;k++)
			_Jcb[(_nVar-1)*_nVar+k]=_Jcb[k]*E_m;

		//adding the remaining diagonal term
		for(k=0;k<_nVar;k++)
			_Jcb[k*_nVar+k]+=ratio_densite;

	}
	else  if (_limitField[nameOfGroup].bcType!=Neumann)
	{
		cout << "group named "<<nameOfGroup << " : unknown boundary condition" << endl;
		throw CdmathException("DriftModel::jacobian: The boundary condition is not recognised: neither inlet, inltPressure, outlet, wall nor neumann");
	}
}

//calcule la jacobienne pour une CL de diffusion
void  DriftModel::jacobianDiff(const int &j, string nameOfGroup)
{
	double v2=0,cd = 1,cn=0,p0, gamma;
	int idim,jdim,k;

	for(k=0; k<_nVar*_nVar;k++)
		_JcbDiff[k] = 0;

	if (_limitField[nameOfGroup].bcType==Wall){
	}
	else if (_limitField[nameOfGroup].bcType==Outlet || _limitField[nameOfGroup].bcType==Neumann
			||_limitField[nameOfGroup].bcType==Inlet || _limitField[nameOfGroup].bcType==InletPressure)
	{
		for(k=0;k<_nVar;k++)
			_Jcb[k*_nVar+k]=1;
	}
	else{
		cout << "group named "<<nameOfGroup << " : unknown boundary condition" << endl;
		throw CdmathException("DriftModel::jacobianDiff: This boundary condition is not recognised");
	}
}


void DriftModel::computeScaling(double maxvp)
{
	//	if(_spaceScheme!=staggered)
	{
		_blockDiag[0]=1;
		_invBlockDiag[0]=1;
		_blockDiag[1]=1;
		_invBlockDiag[1]=1;
		for(int q=2; q<_nVar-1; q++)
		{
			_blockDiag[q]=1./maxvp;//
			_invBlockDiag[q]= maxvp;//1.;//
		}
		_blockDiag[_nVar - 1]=1/(maxvp*maxvp);//1
		_invBlockDiag[_nVar - 1]=  1./_blockDiag[_nVar - 1] ;// 1.;//
	}
	/*
	else{//_spaceScheme==staggered
		_blockDiag[0]=maxvp*maxvp;
		_invBlockDiag[0]=1/_blockDiag[0];
		_blockDiag[1]=maxvp*maxvp;
		_invBlockDiag[1]=1/_blockDiag[1];
		for(int q=2; q<_nVar-1; q++)
		{
			_blockDiag[q]=1;//
			_invBlockDiag[q]= 1;//1.;//
		}
		_blockDiag[_nVar - 1]=1;//1
		_invBlockDiag[_nVar - 1]=  1./_blockDiag[_nVar - 1] ;// 1.;//
	}
	 */
}
Vector DriftModel::computeExtendedPrimState(double *V)
{
	Vector Vext(7+2*_Ndim);

	double C=V[0], P=V[1], T=V[_nVar-1];
	double rho_v=_fluides[0]->getDensity(P,T);
	double rho_l=_fluides[1]->getDensity(P,T);
	double e_v=_fluides[0]->getInternalEnergy(T,rho_v);
	double e_l=_fluides[1]->getInternalEnergy(T,rho_l);
	if(fabs(rho_l*C+rho_v*(1-C))<_precision)
	{
		cout<<"rhov= "<<rho_v<<", rhol= "<<rho_l<<", concentration= "<<C<<endl;
		throw CdmathException("DriftModel::computeExtendedPrimState: impossible to compute void fraction, division by zero");
	}

	_externalStates[0]=rho_v*rho_l/(C*rho_l+(1-C)*rho_v);
	double alpha_v=rho_l*C/(rho_l*C+rho_v*(1-C)), alpha_l=1-alpha_v;
	double rho_m=alpha_v*rho_v+alpha_l*rho_l;
	double h_m=(alpha_v*rho_v*e_v+alpha_l*rho_l*e_l+P)/rho_m;
	Vector Vit(_Ndim);
	for(int i=0;i<_Ndim;i++)
		Vit(i)=V[2+i];
	Vector u_r=relative_velocity(C, Vit,rho_m);

	Vext(0)=C;
	Vext(1)=P;
	for(int i=0;i<_Ndim;i++)
		Vext(2+i)=Vit(i);
	Vext(2+_Ndim)=T;
	Vext(3+_Ndim)=alpha_v;
	for(int i=0;i<_Ndim;i++)
		Vext(4+_Ndim+i)=u_r(i);
	Vext((4+2*_Ndim))=rho_v;
	Vext((5+2*_Ndim))=rho_l;
	Vext(6+2*_Ndim)=h_m;

	return Vext;
}


void DriftModel::Prim2Cons(const double *P, const int &i, double *W, const int &j){
	double concentration=P[i*_nVar];
	double pression=P[i*_nVar+1];
	double temperature=P[i*_nVar+_nVar-1];
	double rho_v=_fluides[0]->getDensity(pression,temperature);
	double rho_l=_fluides[1]->getDensity(pression,temperature);
	double e_v = _fluides[0]->getInternalEnergy(temperature,rho_v);
	double e_l = _fluides[1]->getInternalEnergy(temperature,rho_l);
	if(fabs(concentration*rho_l+(1-concentration)*rho_v)<_precision)
	{
		cout<<"rhov= "<<rho_v<<", rhol= "<<rho_l<<endl;
		cout<<"concentration*rho_l+(1-concentration)*rho_v= "<<concentration*rho_l+(1-concentration)*rho_v<<endl;
		throw CdmathException("DriftModel::Prim2Cons: impossible to compute mixture density, division by zero");
	}
	W[j*(_nVar)]  =(rho_v*rho_l/(concentration*rho_l+(1-concentration)*rho_v))*_porosityField(j);//phi*rho
	W[j*(_nVar)+1]  =W[j*(_nVar)] *concentration;//phi *rho*c_v
	for(int k=0; k<_Ndim; k++)
		W[j*_nVar+(k+2)] = W[j*(_nVar)] *P[i*_nVar+(k+2)];//phi*rho*u

	W[j*_nVar+_nVar-1] = W[j*(_nVar)+1]* e_v + (W[j*(_nVar)]-W[j*(_nVar)+1])* e_l;//phi rhom e_m
	for(int k=0; k<_Ndim; k++)
		W[j*_nVar+_nVar-1] += W[j*_nVar]*P[i*_nVar+(k+2)]*P[i*_nVar+(k+2)]*0.5;//phi rhom e_m + phi rho u*u

	if(_verbose && _nbTimeStep%_freqSave ==0){
		cout<<"DriftModel::Prim2Cons: T="<<temperature<<", pression= "<<pression<<endl;
		cout<<"rhov= "<<rho_v<<", rhol= "<<rho_l<<", rhom= "<<W[j*(_nVar)]<<" e_v="<<e_v<<" e_l="<<e_l<<endl;
	}
}
void DriftModel::consToPrim(const double *Wcons, double* Wprim,double porosity)
{
	double m_v=Wcons[1]/porosity;
	double m_l=(Wcons[0]-Wcons[1])/porosity;
	_minm1=min(m_v,_minm1);
	_minm2=min(m_l,_minm2);
	if(m_v<-_precision || m_l<-_precision)
		_nbMaillesNeg+=1;
	else if(m_v< 0 && m_v>-_precision )
		m_v=0;
	else if(m_l< 0 && m_l>-_precision )
		m_l=0;
	double concentration=m_v/(m_v+m_l);
	double q_2 = 0;
	for(int k=0;k<_Ndim;k++)
		q_2 += Wcons[k+2]*Wcons[k+2];
	q_2 /= Wcons[0];	//phi rho u²

	double rho_m_e_m=(Wcons[_nVar-1] -0.5*q_2)/porosity;
	double pression,Temperature;

	if(concentration<_precision)
	{
		pression=_fluides[1]->getPressure(rho_m_e_m,m_l);
		Temperature=_fluides[1]->getTemperatureFromPressure(pression,m_l);
	}
	else if(concentration>1-_precision)
	{
		pression=_fluides[0]->getPressure(rho_m_e_m,m_v);
		Temperature=_fluides[0]->getTemperatureFromPressure(pression,m_v);
	}
	else//Hypothèses de saturation
	{
		//Première approche
		getMixturePressureAndTemperature(concentration, m_v+m_l, rho_m_e_m, pression, Temperature);
		//cout<<"Temperature= "<<Temperature<<", pression= "<<pression<<endl;
		//throw CdmathException("DriftModel::consToPrim: Apparition de vapeur");

		//Seconde approche : on impose la température directement
		//Temperature=_Tsat;
		//pression=getMixturePressure(concentration, m_v+m_l, Temperature);

		//Troisieme approche : on impose les enthalpies de saturation
		//double rho_m_h_m= m_v*_hsatv + m_l*_hsatl;
		//pression = rho_m_h_m - rho_m_e_m;
		//Temperature = getMixtureTemperature(concentration, m_v+m_l, pression);
	}

	if (pression<0){
		cout << "pressure = "<< pression << " < 0 " << endl;
		cout<<"Vecteur conservatif"<<endl;
		for(int k=0;k<_nVar;k++)
			cout<<Wcons[k]<<endl;
		throw CdmathException("DriftModel::consToPrim: negative pressure");
	}

	Wprim[0]=concentration;
	Wprim[1] =  pression;
	for(int k=0;k<_Ndim;k++)
		Wprim[k+2] = Wcons[k+2]/Wcons[0];
	Wprim[_nVar-1] =  Temperature;
	if(_verbose && _nbTimeStep%_freqSave ==0)
	{
		cout<<"ConsToPrim Vecteur conservatif"<<endl;
		for(int k=0;k<_nVar;k++)
			cout<<Wcons[k]<<endl;
		cout<<"ConsToPrim Vecteur primitif"<<endl;
		for(int k=0;k<_nVar;k++)
			cout<<Wprim[k]<<endl;
	}
}

double DriftModel::getMixturePressure(double c_v, double rhom, double temperature)
{
	double gamma_v=_fluides[0]->constante("gamma");
	double gamma_l=_fluides[1]->constante("gamma");
	double Pinf_v=_fluides[0]->constante("p0");
	double Pinf_l=_fluides[1]->constante("p0");
	double q_v=_fluides[0]->constante("q");
	double q_l=_fluides[1]->constante("q");
	double c_l=1-c_v;
	double a=1., b, c;

	if(		dynamic_cast<StiffenedGas*>(_fluides[0])!=NULL
			&& dynamic_cast<StiffenedGas*>(_fluides[1])!=NULL)
	{
		StiffenedGas* fluide0=dynamic_cast<StiffenedGas*>(_fluides[0]);
		StiffenedGas* fluide1=dynamic_cast<StiffenedGas*>(_fluides[1]);
		double e_v=fluide0->getInternalEnergy(temperature);
		double e_l=fluide1->getInternalEnergy(temperature);
		b=gamma_v*Pinf_v+gamma_l*Pinf_l -rhom*(c_l*(gamma_l-1)*(e_l-q_l)+c_v*(gamma_v-1)*(e_v-q_v));
		c=	gamma_v*Pinf_v*gamma_l*Pinf_l
				-rhom*(c_l*(gamma_l-1)*(e_l-q_l)*gamma_v*Pinf_v + c_v*(gamma_v-1)*(e_v-q_v)*gamma_l*Pinf_l);
	}
	else if(dynamic_cast<StiffenedGasDellacherie*>(_fluides[0])!=NULL
			&& dynamic_cast<StiffenedGasDellacherie*>(_fluides[1])!=NULL)
	{
		StiffenedGasDellacherie* fluide0=dynamic_cast<StiffenedGasDellacherie*>(_fluides[0]);
		StiffenedGasDellacherie* fluide1=dynamic_cast<StiffenedGasDellacherie*>(_fluides[1]);
		double h_v=fluide0->getEnthalpy(temperature);
		double h_l=fluide1->getEnthalpy(temperature);
		b=Pinf_v+Pinf_l -rhom*(c_l*(gamma_l-1)*(h_l-q_l)/gamma_l+c_v*(gamma_v-1)*(h_v-q_v)/gamma_v);
		c=Pinf_v*Pinf_l-rhom*(c_l*(gamma_l-1)*(h_l-q_l)*Pinf_v + c_v*(gamma_v-1)*(h_v-q_v)*Pinf_l);
	}
	else
		throw CdmathException("DriftModel::getMixturePressure: phases must have the same eos");

	double delta= b*b-4*a*c;

	if(_verbose && _nbTimeStep%_freqSave ==0)
		cout<<"DriftModel::getMixturePressure: a= "<<a<<", b= "<<b<<", c= "<<c<<", delta= "<<delta<<endl;

	if(delta<0)
		throw CdmathException("DriftModel::getMixturePressure: cannot compute pressure, delta<0");
	else
		return (-b+sqrt(delta))/(2*a);
}

void DriftModel::getMixturePressureAndTemperature(double c_v, double rhom, double rhom_em, double& pression, double& temperature)
{
	double gamma_v=_fluides[0]->constante("gamma");
	double gamma_l=_fluides[1]->constante("gamma");
	double Pinf_v=_fluides[0]->constante("p0");
	double Pinf_l=_fluides[1]->constante("p0");
	double q_v=_fluides[0]->constante("q");
	double q_l=_fluides[1]->constante("q");
	double c_l=1-c_v, m_v=c_v*rhom, m_l=rhom-m_v;
	double a, b, c, delta;

	if(		dynamic_cast<StiffenedGas*>(_fluides[0])!=NULL
			&& dynamic_cast<StiffenedGas*>(_fluides[1])!=NULL)
	{
		StiffenedGas* fluide0=dynamic_cast<StiffenedGas*>(_fluides[0]);
		StiffenedGas* fluide1=dynamic_cast<StiffenedGas*>(_fluides[1]);

		temperature= (rhom_em-m_v*fluide0->getInternalEnergy(0)-m_l*fluide1->getInternalEnergy(0))
																																											/(m_v*fluide0->constante("cv")+m_l*fluide1->constante("cv"));

		double e_v=fluide0->getInternalEnergy(temperature);
		double e_l=fluide1->getInternalEnergy(temperature);
		a=1.;
		b=gamma_v*Pinf_v+gamma_l*Pinf_l -rhom*(c_l*(gamma_l-1)*(e_l-q_l)+c_v*(gamma_v-1)*(e_v-q_v));
		c=	gamma_v*Pinf_v*gamma_l*Pinf_l
				-rhom*(c_l*(gamma_l-1)*(e_l-q_l)*gamma_v*Pinf_v + c_v*(gamma_v-1)*(e_v-q_v)*gamma_l*Pinf_l);

		delta= b*b-4*a*c;

		if(delta<0)
			throw CdmathException("DriftModel::getMixturePressureAndTemperature: cannot compute pressure, delta<0");
		else
			pression= (-b+sqrt(delta))/(2*a);

	}
	else if(dynamic_cast<StiffenedGasDellacherie*>(_fluides[0])!=NULL
			&& dynamic_cast<StiffenedGasDellacherie*>(_fluides[1])!=NULL)
	{
		StiffenedGasDellacherie* fluide0=dynamic_cast<StiffenedGasDellacherie*>(_fluides[0]);
		StiffenedGasDellacherie* fluide1=dynamic_cast<StiffenedGasDellacherie*>(_fluides[1]);

		double h0_v=fluide0->getEnthalpy(0);
		double h0_l=fluide1->getEnthalpy(0);
		double cp_v = _fluides[0]->constante("cp");
		double cp_l = _fluides[1]->constante("cp");
		double denom=m_v*cp_v+m_l*cp_l;
		double num_v=cp_v*rhom_em+m_l*(cp_l* h0_v-cp_v* h0_l);
		double num_l=cp_l*rhom_em+m_v*(cp_v* h0_l-cp_l* h0_v);

		a=gamma_v*gamma_l-(m_v*(gamma_v-1)*gamma_l*cp_v+m_l*(gamma_l-1)*gamma_v*cp_l)/denom;
		b=gamma_v*gamma_l*(Pinf_v+Pinf_l)-(m_v*(gamma_v-1)*gamma_l*cp_v*Pinf_l+m_l*(gamma_l-1)*gamma_v*cp_l*Pinf_v)/denom
				-m_v*(gamma_v-1)*gamma_l*(num_v/denom -q_v)-m_l*(gamma_l-1)*gamma_v*(num_l/denom -q_l);
		c=gamma_v*gamma_l*Pinf_v*Pinf_l
				-m_v*(gamma_v-1)*gamma_l*(num_v/denom -q_v)*Pinf_l-m_l*(gamma_l-1)*gamma_v*(num_l/denom -q_l)*Pinf_v;

		delta= b*b-4*a*c;

		if(delta<0)
			throw CdmathException("DriftModel::getMixturePressureAndTemperature: cannot compute pressure, delta<0");
		else
			pression= (-b+sqrt(delta))/(2*a);

		temperature=(rhom_em+pression-m_v*h0_v-m_l*h0_l)/denom;
	}
	else
		throw CdmathException("DriftModel::getMixturePressureAndTemperature: phases must have the same eos");


	if(_verbose && _nbTimeStep%_freqSave ==0){
		cout<<"DriftModel::getMixturePressureAndTemperature: a= "<<a<<", b= "<<b<<", c= "<<c<<", delta= "<<delta<<endl;
		cout<<"pressure= "<<pression<<", temperature= "<<temperature<<endl;
	}

}
double DriftModel::getMixtureTemperature(double c_v, double rhom, double pression)
{
	double gamma_v=_fluides[0]->constante("gamma");
	double gamma_l=_fluides[1]->constante("gamma");
	double Pinf_v = _fluides[0]->constante("p0");
	double Pinf_l = _fluides[1]->constante("p0");
	double q_v=_fluides[0]->constante("q");
	double q_l=_fluides[1]->constante("q");
	double c_l=1-c_v;

	if(		dynamic_cast<StiffenedGas*>(_fluides[0])!=NULL
			&& dynamic_cast<StiffenedGas*>(_fluides[1])!=NULL)
	{
		double cv_v = _fluides[0]->constante("cv");
		double cv_l = _fluides[1]->constante("cv");
		StiffenedGas* fluide0=dynamic_cast<StiffenedGas*>(_fluides[0]);
		StiffenedGas* fluide1=dynamic_cast<StiffenedGas*>(_fluides[1]);
		double e0_v=fluide0->getInternalEnergy(0);
		double e0_l=fluide1->getInternalEnergy(0);

		double numerator=(pression+gamma_v*Pinf_v)*(pression+gamma_l*Pinf_l)/rhom
				- c_l*(pression+gamma_v*Pinf_v)*(gamma_l-1)*(e0_l-q_v)
				- c_v*(pression+gamma_l*Pinf_l)*(gamma_v-1)*(e0_v-q_l);
		double denominator=  c_l*(pression+gamma_v*Pinf_v)*(gamma_l-1)*cv_l
				+c_v*(pression+gamma_l*Pinf_l)*(gamma_v-1)*cv_v;
		return numerator/denominator;
	}
	else if(dynamic_cast<StiffenedGasDellacherie*>(_fluides[0])!=NULL
			&& dynamic_cast<StiffenedGasDellacherie*>(_fluides[1])!=NULL)
	{
		double cp_v = _fluides[0]->constante("cp");
		double cp_l = _fluides[1]->constante("cp");
		StiffenedGasDellacherie* fluide0=dynamic_cast<StiffenedGasDellacherie*>(_fluides[0]);
		StiffenedGasDellacherie* fluide1=dynamic_cast<StiffenedGasDellacherie*>(_fluides[1]);
		double h0_v=fluide0->getEnthalpy(0);
		double h0_l=fluide1->getEnthalpy(0);

		double numerator= gamma_v*(pression+Pinf_v)* gamma_l*(pression+Pinf_l)/rhom
				- c_l*gamma_v*(pression+Pinf_v)*(gamma_l-1)*(h0_l-q_l)
				- c_v*gamma_l*(pression+Pinf_l)*(gamma_v-1)*(h0_v-q_v);
		double denominator=  c_l*gamma_v*(pression+Pinf_v)*(gamma_l-1)*cp_l
				+c_v*gamma_l*(pression+Pinf_l)*(gamma_v-1)*cp_v;
		return numerator/denominator;
	}
	else
		throw CdmathException("DriftModel::getMixtureTemperature: phases must have the same eos");

}

void DriftModel::getMixturePressureDerivatives(double m_v, double m_l, double pression, double Tm)
{
	double rho_v=_fluides[0]->getDensity(pression,Tm);
	double rho_l=_fluides[1]->getDensity(pression,Tm);
	double alpha_v=m_v/rho_v,alpha_l=1-alpha_v;
	double gamma_v=_fluides[0]->constante("gamma");
	double gamma_l=_fluides[1]->constante("gamma");
	double q_v=_fluides[0]->constante("q");
	double q_l=_fluides[1]->constante("q");
	double temp1, temp2, denom;

	if(	   dynamic_cast<StiffenedGas*>(_fluides[0])!=NULL
			&& dynamic_cast<StiffenedGas*>(_fluides[1])!=NULL)
	{//Classical stiffened gas with linear law e(T)
		double cv_v = _fluides[0]->constante("cv");
		double cv_l = _fluides[1]->constante("cv");

		double e_v=_fluides[0]->getInternalEnergy(Tm, rho_v);//Actually does not depends on rho_v
		double e_l=_fluides[1]->getInternalEnergy(Tm, rho_l);//Actually does not depends on rho_l

		//On estime les dérivées discrètes de la pression (cf doc)
		temp1= m_v* cv_v + m_l* cv_l;
		denom=(alpha_v/((gamma_v-1)*rho_v*(e_v-q_v))+alpha_l/((gamma_l-1)*rho_l*(e_l-q_l)))*temp1;
		temp2=alpha_v*cv_v/ (e_v-q_v)+alpha_l*cv_l/ (e_l-q_l);

		_khi=(temp1/rho_l-e_l*temp2)/denom;
		_ksi=(temp1*(rho_l-rho_v)/(rho_v*rho_l)+(e_l-e_v)*temp2)/denom;
		_kappa=temp2/denom;
	}

	else if(   dynamic_cast<StiffenedGasDellacherie*>(_fluides[0])!=NULL
			&& dynamic_cast<StiffenedGasDellacherie*>(_fluides[1])!=NULL)
	{//S. Dellacherie stiffened gas with linear law h(T)
		double cp_v = _fluides[0]->constante("cp");
		double cp_l = _fluides[1]->constante("cp");

		double h_v=_fluides[0]->getEnthalpy(Tm, rho_v);//Actually does not depends on rho_v
		double h_l=_fluides[1]->getEnthalpy(Tm, rho_l);//Actually does not depends on rho_l

		//On estime les dérivées discrètes de la pression (cf doc)
		temp1= m_v* cp_v + m_l* cp_l;
		temp2= alpha_v*cp_v/(h_v-q_v)+alpha_l*cp_l/(h_l-q_l);
		//denom=temp1*(alpha_v/(pression+Pinf_v)+alpha_l/(pression+Pinf_l))-temp2;
		denom=temp1*(alpha_v*gamma_v/((gamma_v-1)*rho_v*(h_v-q_v))+alpha_l*gamma_l/((gamma_l-1)*rho_l*(h_l-q_l)))-temp2;

		//On estime les dérivées discrètes de la pression (cf doc)
		_khi=(temp1/rho_l - h_l*temp2)/denom;
		_ksi=(temp1*(1/rho_v-1/rho_l)+(h_l-h_v)*temp2)/denom;
		_kappa=temp2/denom;
	}

	if(_verbose && _nbTimeStep%_freqSave ==0)
	{
		cout<<"rho_l= "<<rho_l<<", temp1= "<<temp1<<", temp2= "<<temp2<<", denom= "<<denom<<endl;
		cout<<"khi= "<<_khi<<", ksi= "<<_ksi<<", kappa= "<<_kappa<<endl;
	}
}

void DriftModel::entropicShift(double* n)//TO do: make sure _Vi and _Vj are well set
{
	//_Vi is (cm, p, u, T)
	//_Ui is (rhom, rhom cm, rhom um, rhom Em)
	consToPrim(_Ui,_Vi,_porosityi);
	double umnl=0, ul_2=0, umnr=0, ur_2=0; //valeur de u.normale et |u|²
	for(int i=0;i<_Ndim;i++)
	{
		ul_2 += _Vi[2+i]*_Vi[2+i];
		umnl += _Vi[2+i]*n[i];//vitesse normale
		ur_2 += _Vj[2+i]*_Vj[2+i];
		umnr += _Vj[2+i]*n[i];//vitesse normale
	}

	//Left sound speed
	double rhom=_Ui[0];
	double cm=_Vi[0];
	double Hm=(_Ui[_nVar-1]+_Vi[1])/rhom;
	if(_verbose && _nbTimeStep%_freqSave ==0)
		cout<<"Entropic shift left state rhom="<<rhom<<" cm= "<<cm<<"Hm= "<<Hm<<endl;
	double Tm=_Vi[_nVar-1];
	double hm=Hm-0.5*ul_2;
	double m_v=cm*rhom, m_l=rhom-m_v;
	double pression=getMixturePressure( cm, rhom, Tm);

	getMixturePressureDerivatives( m_v, m_l, pression, Tm);

	if(_kappa*hm+_khi+cm*_ksi<0)
		throw CdmathException("Valeurs propres jacobienne: vitesse du son complexe");

	double aml=sqrt(_kappa*hm+_khi+cm*_ksi);//vitesse du son du melange
	//cout<<"_khi= "<<_khi<<", _kappa= "<< _kappa << ", _ksi= "<<_ksi <<", am= "<<am<<endl;

	//Right sound speed
	consToPrim(_Uj,_Vj,_porosityj);
	rhom=_Uj[0];
	cm=_Vj[0];
	Hm=(_Uj[_nVar-1]+_Vj[1])/rhom;
	if(_verbose && _nbTimeStep%_freqSave ==0)
		cout<<"Entropic shift right state rhom="<<rhom<<" cm= "<<cm<<"Hm= "<<Hm<<endl;
	Tm=_Vj[_nVar-1];
	hm=Hm-0.5*ul_2;
	m_v=cm*rhom;
	m_l=rhom-m_v;
	pression=getMixturePressure( cm, rhom, Tm);

	getMixturePressureDerivatives( m_v, m_l, pression, Tm);

	if(_kappa*hm+_khi+cm*_ksi<0)
		throw CdmathException("Valeurs propres jacobienne: vitesse du son complexe");

	double amr=sqrt(_kappa*hm+_khi+cm*_ksi);//vitesse du son du melange
	//cout<<"_khi= "<<_khi<<", _kappa= "<< _kappa << ", _ksi= "<<_ksi <<", am= "<<am<<endl;

	_entropicShift[0]=abs(umnl-aml - (umnr-amr));
	_entropicShift[1]=abs(umnl - umnr);
	_entropicShift[2]=abs(umnl+aml - (umnr+amr));

	if(_verbose && _nbTimeStep%_freqSave ==0)
	{
		cout<<"Entropic shift left eigenvalues: "<<endl;
		cout<<"("<< umnl-aml<< ", " <<umnl<<", "<<umnl+aml << ")";
		cout<<endl;
		cout<<"Entropic shift right eigenvalues: "<<endl;
		cout<<"("<< umnr-amr<< ", " <<umnr<<", "<<umnr+amr << ")";
		cout<< endl;
		cout<<"eigenvalue jumps "<<endl;
		cout<< _entropicShift[0] << ", " << _entropicShift[1] << ", "<< _entropicShift[2] <<endl;
	}
}

Vector DriftModel::convectionFlux(Vector U,Vector V, Vector normale, double porosity){
	if(_verbose){
		cout<<"Ucons"<<endl;
		cout<<U<<endl;
		cout<<"Vprim"<<endl;
		cout<<V<<endl;
	}

	double rho_m=U(0);//phi rhom
	double m_v=U(1), m_l=rho_m-m_v;//phi rhom cv, phi rhom cl
	Vector q_m(_Ndim);
	for(int i=0;i<_Ndim;i++)
		q_m(i)=U(2+i);

	double concentration_v=V(0);
	double concentration_l= 1 - concentration_v;
	double pression=V(1);
	Vector vitesse_melange(_Ndim);
	for(int i=0;i<_Ndim;i++)
		vitesse_melange(i)=V(2+i);
	double Temperature= V(2+_Ndim);//(rho_m_e_m-m_v*internal_energy(0, e0v,c0v,T0)-m_l*internal_energy(0, e0l,c0l,T0))/(m_v*c0v+m_l*c0l);

	Vector vitesse_relative=relative_velocity(concentration_v,vitesse_melange,rho_m);
	Vector vitesse_v=vitesse_melange+concentration_l*vitesse_relative;
	Vector vitesse_l=vitesse_melange-concentration_v*vitesse_relative;
	double vitesse_vn=vitesse_v*normale;
	double vitesse_ln=vitesse_l*normale;
	//double rho_m_e_m=rho_m_E_m -0.5*(q_m*vitesse_melange + rho_m*concentration_v*concentration_l*vitesse_relative*vitesse_relative)
	double rho_v=_fluides[0]->getDensity(pression,Temperature);
	double rho_l=_fluides[1]->getDensity(pression,Temperature);
	double e_v=_fluides[0]->getInternalEnergy(Temperature,rho_v);
	double e_l=_fluides[1]->getInternalEnergy(Temperature,rho_l);

	Vector F(_nVar);
	F(0)=m_v*vitesse_vn+m_l*vitesse_ln;
	F(1)=m_v*vitesse_vn;
	for(int i=0;i<_Ndim;i++)
		F(2+i)=m_v*vitesse_vn*vitesse_v(i)+m_l*vitesse_ln*vitesse_l(i)+pression*normale(i)*porosity;
	F(2+_Ndim)=m_v*(e_v+0.5*vitesse_v*vitesse_v+pression/rho_v)*vitesse_vn+m_l*(e_l+0.5*vitesse_l*vitesse_l+pression/rho_l)*vitesse_ln;

	if(_verbose){
		cout<<"Flux F(U,V)"<<endl;
		cout<<F<<endl;
	}

	return F;
}

Vector DriftModel::staggeredVFFCFlux()
{
	if(_spaceScheme!=staggered || _nonLinearFormulation!=VFFC)
		throw CdmathException("DriftModel::staggeredVFFCFlux: staggeredVFFCFlux method should be called only for VFFC formulation and staggered upwinding");
	else//_spaceScheme==staggered
	{
		Vector Fij(_nVar);

		double uijn=0, phiqn=0;
		for(int idim=0;idim<_Ndim;idim++)
			uijn+=_vec_normal[idim]*_Uroe[2+idim];//URoe = rho, cm, u, H

		if(uijn>=0)
		{
			for(int idim=0;idim<_Ndim;idim++)
				phiqn+=_vec_normal[idim]*_Ui[2+idim];//phi rho u n
			Fij(0)=phiqn;
			Fij(1)=_Vi[0]*phiqn;
			for(int idim=0;idim<_Ndim;idim++)
				Fij(2+idim)=phiqn*_Vi[2+idim]+_Vj[1]*_vec_normal[idim]*_porosityj;
			Fij(_nVar-1)=phiqn/_Ui[0]*(_Ui[_nVar-1]+_Vj[1]*sqrt(_porosityj/_porosityi));
		}
		else
		{
			for(int idim=0;idim<_Ndim;idim++)
				phiqn+=_vec_normal[idim]*_Uj[1+idim];//phi rho u n
			Fij(0)=phiqn;
			Fij(1)=_Vj[0]*phiqn;
			for(int idim=0;idim<_Ndim;idim++)
				Fij(2+idim)=phiqn*_Vj[2+idim]+_Vi[1]*_vec_normal[idim]*_porosityi;
			Fij(_nVar-1)=phiqn/_Uj[0]*(_Uj[_nVar-1]+_Vi[1]*sqrt(_porosityi/_porosityj));
		}
		return Fij;
	}
}


void DriftModel::staggeredVFFCMatrices(double u_mn)
{
	if(_spaceScheme!=staggered || _nonLinearFormulation!=VFFC)
		throw CdmathException("DriftModel::staggeredVFFCFlux: staggeredVFFCFlux method should be called only for VFFC formulation and staggered upwinding");
	else//_spaceScheme==staggered && _nonLinearFormulation==VFFC
	{
		double ui_n=0, ui_2=0, uj_n=0, uj_2=0;//vitesse normale et carré du module
		double H;//enthalpie totale (expression particulière)
		consToPrim(_Ui,_Vi,_porosityi);
		consToPrim(_Uj,_Vj,_porosityj);

		for(int i=0;i<_Ndim;i++)
		{
			ui_2 += _Vi[2+i]*_Vi[2+i];
			ui_n += _Vi[2+i]*_vec_normal[i];
			uj_2 += _Vj[2+i]*_Vj[2+i];
			uj_n += _Vj[2+i]*_vec_normal[i];
		}

		double rhomi=_Ui[0]/_porosityi;
		double mi_v=_Ui[1]/_porosityi;
		double mi_l=rhomi-mi_v;
		double cmi=_Vi[0];
		double pi=_Vi[1];
		double Tmi=_Vi[_Ndim+2];
		double Emi=_Ui[_Ndim+2]/(rhomi*_porosityi);
		double ecini=0.5*ui_2;
		double hmi=Emi-0.5*ui_2+pi/rhomi;

		double pj=_Vj[1];
		double rhomj=_Uj[0]/_porosityj;
		double mj_v =_Uj[1]/_porosityj;
		double mj_l=rhomj-mj_v;
		double cmj=_Vj[0];
		double Tmj=_Vj[_Ndim+2];
		double Emj=_Uj[_Ndim+2]/(rhomj*_porosityj);
		double ecinj=0.5*uj_2;
		double hmj=Emj-0.5*uj_2+pj/rhomj;

		double Hm=Emi+pj/rhomi;

		if(u_mn>=0)
		{
			if(_verbose && _nbTimeStep%_freqSave ==0)
				cout<<"VFFC Staggered state rhomi="<<rhomi<<" cmi= "<<cmi<<" Hm= "<<Hm<<" Tmi= "<<Tmi<<" pj= "<<pj<<endl;

			/***********Calcul des valeurs propres ********/
			vector<complex<double> > vp_dist(3,0);
			getMixturePressureDerivatives( mj_v, mj_l, pj, Tmj);
			if(_kappa*hmj+_khi+cmj*_ksi<0)
				throw CdmathException("Calcul VFFC staggered: vitesse du son complexe");
			double amj=sqrt(_kappa*hmj+_khi+cmj*_ksi);//vitesse du son du melange

			if(_verbose && _nbTimeStep%_freqSave ==0)
				cout<<"_khi= "<<_khi<<", _kappa= "<< _kappa << ", _ksi= "<<_ksi <<", amj= "<<amj<<endl;

			//On remplit les valeurs propres
			vp_dist[0]=ui_n+amj;
			vp_dist[1]=ui_n-amj;
			vp_dist[2]=ui_n;

			_maxvploc=fabs(ui_n)+amj;
			if(_maxvploc>_maxvp)
				_maxvp=_maxvploc;

			/******** Construction de la matrice A^+ *********/
			_AroePlus[0*_nVar+0]=0;
			_AroePlus[0*_nVar+1]=0;
			for(int i=0;i<_Ndim;i++)
				_AroePlus[0*_nVar+2+i]=_vec_normal[i];
			_AroePlus[0*_nVar+2+_Ndim]=0;
			_AroePlus[1*_nVar+0]=-ui_n*cmi;
			_AroePlus[1*_nVar+1]=ui_n;
			for(int i=0;i<_Ndim;i++)
				_AroePlus[1*_nVar+2+i]=cmi*_vec_normal[i];
			_AroePlus[1*_nVar+2+_Ndim]=0;
			for(int i=0;i<_Ndim;i++)
			{
				_AroePlus[(2+i)*_nVar+0]=-ui_n*_Ui[2+i];
				_AroePlus[(2+i)*_nVar+1]=0;
				for(int j=0;j<_Ndim;j++)
					_AroePlus[(2+i)*_nVar+2+j]=_Ui[2+i]*_vec_normal[j];
				_AroePlus[(2+i)*_nVar+2+i]+=ui_n;
				_AroePlus[(2+i)*_nVar+2+_Ndim]=0;
			}
			_AroePlus[(2+_Ndim)*_nVar+0]=-Hm*ui_n;
			_AroePlus[(2+_Ndim)*_nVar+1]=0;
			for(int i=0;i<_Ndim;i++)
				_AroePlus[(2+_Ndim)*_nVar+2+i]=Hm*_vec_normal[i];
			_AroePlus[(2+_Ndim)*_nVar+2+_Ndim]=ui_n;

			/******** Construction de la matrice A^- *********/
			_AroeMinus[0*_nVar+0]=0;
			_AroeMinus[0*_nVar+1]=0;
			for(int i=0;i<_Ndim;i++)
				_AroeMinus[0*_nVar+2+i]=0;
			_AroeMinus[0*_nVar+2+_Ndim]=0;
			_AroeMinus[1*_nVar+0]=0;
			_AroeMinus[1*_nVar+1]=0;
			for(int i=0;i<_Ndim;i++)
				_AroeMinus[1*_nVar+2+i]=0;
			_AroeMinus[1*_nVar+2+_Ndim]=0;
			for(int i=0;i<_Ndim;i++)
			{
				_AroeMinus[(2+i)*_nVar+0]=(_khi+_kappa*ecinj)*_vec_normal[i];
				_AroeMinus[(2+i)*_nVar+1]=_ksi*_vec_normal[i];
				for(int j=0;j<_Ndim;j++)
					_AroeMinus[(2+i)*_nVar+2+j]=-_kappa*_vec_normal[i]*_Uj[2+j];
				_AroeMinus[(2+i)*_nVar+2+i]+=0;
				_AroeMinus[(2+i)*_nVar+2+_Ndim]=_kappa*_vec_normal[i];
			}
			_AroeMinus[(2+_Ndim)*_nVar+0]=(_khi+_kappa*ecinj)*ui_n;
			_AroeMinus[(2+_Ndim)*_nVar+1]=_ksi*ui_n;
			for(int i=0;i<_Ndim;i++)
				_AroeMinus[(2+_Ndim)*_nVar+2+i]=-_kappa*uj_n*_Ui[2+i];
			_AroeMinus[(2+_Ndim)*_nVar+2+_Ndim]=_kappa*ui_n;
		}
		else
		{
			if(_verbose && _nbTimeStep%_freqSave ==0)
				cout<<"VFFC Staggered state rhomj="<<rhomj<<" cmj= "<<cmj<<" Hm= "<<Hm<<" Tmj= "<<Tmj<<" pi= "<<pi<<endl;

			/***********Calcul des valeurs propres ********/
			vector<complex<double> > vp_dist(3,0);
			getMixturePressureDerivatives( mi_v, mi_l, pi, Tmi);
			if(_kappa*hmi+_khi+cmi*_ksi<0)
				throw CdmathException("Calcul VFFC staggered: vitesse du son complexe");
			double ami=sqrt(_kappa*hmi+_khi+cmi*_ksi);//vitesse du son du melange
			if(_verbose && _nbTimeStep%_freqSave ==0)
				cout<<"_khi= "<<_khi<<", _kappa= "<< _kappa << ", _ksi= "<<_ksi <<", ami= "<<ami<<endl;

			//On remplit les valeurs propres
			vp_dist[0]=uj_n+ami;
			vp_dist[1]=uj_n-ami;
			vp_dist[2]=uj_n;

			_maxvploc=fabs(uj_n)+ami;
			if(_maxvploc>_maxvp)
				_maxvp=_maxvploc;

			/******** Construction de la matrice A^+ *********/
			_AroePlus[0*_nVar+0]=0;
			_AroePlus[0*_nVar+1]=0;
			for(int i=0;i<_Ndim;i++)
				_AroePlus[0*_nVar+2+i]=0;
			_AroePlus[0*_nVar+2+_Ndim]=0;
			_AroePlus[1*_nVar+0]=0;
			_AroePlus[1*_nVar+1]=0;
			for(int i=0;i<_Ndim;i++)
				_AroePlus[1*_nVar+2+i]=0;
			_AroePlus[1*_nVar+2+_Ndim]=0;
			for(int i=0;i<_Ndim;i++)
			{
				_AroePlus[(2+i)*_nVar+0]=(_khi+_kappa*ecini)*_vec_normal[i];
				_AroePlus[(2+i)*_nVar+1]=_ksi*_vec_normal[i];
				for(int j=0;j<_Ndim;j++)
					_AroePlus[(2+i)*_nVar+2+j]=-_kappa*_vec_normal[i]*_Ui[2+j];
				_AroePlus[(2+i)*_nVar+2+i]+=0;
				_AroePlus[(2+i)*_nVar+2+_Ndim]=_kappa*_vec_normal[i];
			}
			_AroePlus[(2+_Ndim)*_nVar+0]=(_khi+_kappa*ecini)*ui_n;
			_AroePlus[(2+_Ndim)*_nVar+1]=_ksi*ui_n;
			for(int i=0;i<_Ndim;i++)
				_AroePlus[(2+_Ndim)*_nVar+2+i]=-_kappa*uj_n*_Ui[2+i];
			_AroePlus[(2+_Ndim)*_nVar+2+_Ndim]=_kappa*ui_n;

			/******** Construction de la matrice A^- *********/
			_AroeMinus[0*_nVar+0]=0;
			_AroeMinus[0*_nVar+1]=0;
			for(int i=0;i<_Ndim;i++)
				_AroeMinus[0*_nVar+2+i]=_vec_normal[i];
			_AroeMinus[0*_nVar+2+_Ndim]=0;
			_AroeMinus[1*_nVar+0]=-uj_n*cmj;
			_AroeMinus[1*_nVar+1]=uj_n;
			for(int i=0;i<_Ndim;i++)
				_AroeMinus[1*_nVar+2+i]=cmj*_vec_normal[i];
			_AroeMinus[1*_nVar+2+_Ndim]=0;
			for(int i=0;i<_Ndim;i++)
			{
				_AroeMinus[(2+i)*_nVar+0]=-uj_n*_Uj[2+i];
				_AroeMinus[(2+i)*_nVar+1]=0;
				for(int j=0;j<_Ndim;j++)
					_AroeMinus[(2+i)*_nVar+2+j]=_Uj[2+i]*_vec_normal[j];
				_AroeMinus[(2+i)*_nVar+2+i]+=uj_n;
				_AroeMinus[(2+i)*_nVar+2+_Ndim]=0;
			}
			_AroeMinus[(2+_Ndim)*_nVar+0]=-Hm*uj_n;
			_AroeMinus[(2+_Ndim)*_nVar+1]=0;
			for(int i=0;i<_Ndim;i++)
				_AroeMinus[(2+_Ndim)*_nVar+2+i]=Hm*_vec_normal[i];
			_AroeMinus[(2+_Ndim)*_nVar+2+_Ndim]=uj_n;
		}
	}
	/*
	 ******* Construction de la matrice de Roe ********
	_Aroe[0*_nVar+0]=0;
	_Aroe[0*_nVar+1]=0;
	for(int i=0;i<_Ndim;i++)
		_Aroe[0*_nVar+2+i]=_vec_normal[i];
	_Aroe[0*_nVar+2+_Ndim]=0;
	_Aroe[1*_nVar+0]=-ui_n*cmi;
	_Aroe[1*_nVar+1]=ui_n;
	for(int i=0;i<_Ndim;i++)
		_Aroe[1*_nVar+2+i]=cmi*_vec_normal[i];
	_Aroe[1*_nVar+2+_Ndim]=0;
	for(int i=0;i<_Ndim;i++)
	{
		_Aroe[(2+i)*_nVar+0]=(_khi+_kappa*ecinj)*_vec_normal[i]-ui_n*_Ui[2+i];
		_Aroe[(2+i)*_nVar+1]=_ksi*_vec_normal[i];
		for(int j=0;j<_Ndim;j++)
			_Aroe[(2+i)*_nVar+2+j]=_Ui[2+i]*_vec_normal[j]-_kappa*_vec_normal[i]*_Uj[2+j];
		_Aroe[(2+i)*_nVar+2+i]+=ui_n;
		_Aroe[(2+i)*_nVar+2+_Ndim]=_kappa*_vec_normal[i];
	}
	_Aroe[(2+_Ndim)*_nVar+0]=(_khi+_kappa*ecinj-Hm)*ui_n;
	_Aroe[(2+_Ndim)*_nVar+1]=_ksi*ui_n;
	for(int i=0;i<_Ndim;i++)
		_Aroe[(2+_Ndim)*_nVar+2+i]=Hm*_vec_normal[i]-_kappa*uj_n*_Ui[2+i];
	_Aroe[(2+_Ndim)*_nVar+2+_Ndim]=(_kappa+1)*ui_n;
	 */

}

void DriftModel::applyVFRoeLowMachCorrections()
{
	if(_nonLinearFormulation!=VFRoe)
		throw CdmathException("DriftModel::applyVFRoeLowMachCorrections: applyVFRoeLowMachCorrections method should be called only for VFRoe formulation");
	else//_nonLinearFormulation==VFRoe
	{
		if(_spaceScheme==lowMach){
			double u_2=0;
			for(int i=0;i<_Ndim;i++)
				u_2 += _Uroe[2+i]*_Uroe[2+i];

			double 	c = _maxvploc;//mixture sound speed
			double M=sqrt(u_2)/c;//Mach number
			_Vij[1]=M*_Vij[1]+(1-M)*(_Vi[1]+_Vj[1])/2;
			Prim2Cons(_Vij,0,_Uij,0);
		}
		else if(_spaceScheme==pressureCorrection)
		{
			double norm_uij=0, uij_n=0, ui_n=0, uj_n=0;
			for(int i=0;i<_Ndim;i++)
			{
				norm_uij += _Uroe[2+i]*_Uroe[2+i];
				uij_n += _Uroe[2+i]*_vec_normal[i];
				ui_n += _Vi[2+i]*_vec_normal[i];
				uj_n += _Vj[2+i]*_vec_normal[i];
			}
			norm_uij=sqrt(norm_uij);
			_Vij[1]=(_Vi[1]+_Vj[1])/2 + uij_n/norm_uij*(_Vj[1]-_Vi[1])/4 - _Uroe[0]*norm_uij*(uj_n-ui_n)/4;
		}
		else if(_spaceScheme==staggered)
		{
			double uij_n=0;
			for(int i=0;i<_Ndim;i++)
				uij_n += _Uroe[1+i]*_vec_normal[i];

			if(uij_n>=0){
				_Vij[0]=_Vi[0];
				_Vij[1]=_Vj[1];
				for(int i=0;i<_Ndim;i++)
					_Vij[2+i]=_Vi[2+i];
				_Vij[_nVar-1]=_Vi[_nVar-1];
			}
			else{
				_Vij[0]=_Vj[0];
				_Vij[1]=_Vi[1];
				for(int i=0;i<_Ndim;i++)
					_Vij[2+i]=_Vj[2+i];
				_Vij[_nVar-1]=_Vj[_nVar-1];
			}
			Prim2Cons(_Vij,0,_Uij,0);
		}
	}
}
void DriftModel::testConservation()
{
	double SUM, DELTA, x;
	int I,compponentl;
	for(int l=0; l<_nVar; l++)
	{
		{
			if(l == 0)
				cout << "Masse totale (kg): ";
			else if(l == 1)
				cout << "Masse partielle 1 (kg): ";
			else
			{
				if(l == _nVar-1)
					cout << "Energie totale (J): ";
				else
					cout << "Quantite de mouvement direction "<<l-1<<" (kg.m.s^-1): ";
			}
		}
		SUM = 0;
		DELTA = 0;
		for(int I=0; I<_Nmailles; I++)
		{
			compponentl=I*_nVar+l;
			VecGetValues(_old, 1, &compponentl, &x);//on recupere la valeur du champ
			SUM += x;
			VecGetValues(_next, 1, &compponentl, &x);//on recupere la variation du champ
			DELTA += x;
		}
		{
			if(fabs(SUM)>_precision)
				cout << SUM << ", variation relative: " << fabs(DELTA /SUM)  << endl;
			else
				cout << " a une somme nulle,  variation absolue: " << fabs(DELTA) << endl;
		}
	}
}

void DriftModel::save(){
	string prim(_path+"/DriftModelPrim_");
	string cons(_path+"/DriftModelCons_");
	prim+=_fileName;
	cons+=_fileName;

	PetscInt Ii;
	for (long i = 0; i < _Nmailles; i++){
		// j = 0 : concentration, j=1 : pressure; j = _nVar - 1: temperature; j = 2,..,_nVar-2: velocity
		for (int j = 0; j < _nVar; j++){
			Ii = i*_nVar +j;
			VecGetValues(_primitives,1,&Ii,&_VV(i,j));
		}
	}
	if(_saveConservativeField){
		double tmp;
		for (long i = 0; i < _Nmailles; i++){
			// j = 0 : total density; j = 1 : vapour density; j = _nVar - 1 : energy j = 2,..,_nVar-2: momentum
			for (int j = 0; j < _nVar; j++){
				Ii = i*_nVar +j;
				VecGetValues(_courant,1,&Ii,&tmp);
				_UU(i,j)=tmp;///_porosityField(i);
			}
		}
		_UU.setTime(_time,_nbTimeStep);
	}
	_VV.setTime(_time,_nbTimeStep);
	// create mesh and component info
	if (_nbTimeStep ==0){
		string prim_suppress ="rm -rf "+prim+"_*";
		string cons_suppress ="rm -rf "+cons+"_*";
		system(prim_suppress.c_str());//Nettoyage des précédents calculs identiques
		system(cons_suppress.c_str());//Nettoyage des précédents calculs identiques

		if(_saveConservativeField){
			_UU.setInfoOnComponent(0,"Total Density");// (kg/m^3)

			_UU.setInfoOnComponent(1,"Partial Density");// (kg/m^3)
			_UU.setInfoOnComponent(2,"Momentum_x");// (kg/m^2/s)
			if (_Ndim>1)
				_UU.setInfoOnComponent(3,"Momentum_y");// (kg/m^2/s)
			if (_Ndim>2)
				_UU.setInfoOnComponent(4,"Momentum_z");// (kg/m^2/s)

			_UU.setInfoOnComponent(_nVar-1,"Energy (J/m^3)");
			switch(_saveFormat)
			{
			case VTK :
				_UU.writeVTK(cons);
				break;
			case MED :
				_UU.writeMED(cons);
				break;
			case CSV :
				_UU.writeCSV(cons);
				break;
			}
		}

		_VV.setInfoOnComponent(0,"Concentration");
		_VV.setInfoOnComponent(1,"Pressure (Pa)");
		_VV.setInfoOnComponent(2,"Velocity_x (m/s)");
		if (_Ndim>1)
			_VV.setInfoOnComponent(3,"Velocity_y (m/s)");
		if (_Ndim>2)
			_VV.setInfoOnComponent(4,"Velocity_z (m/s)");
		_VV.setInfoOnComponent(_nVar-1,"Temperature (K)");
		switch(_saveFormat)
		{
		case VTK :
			_VV.writeVTK(prim);
			break;
		case MED :
			_VV.writeMED(prim);
			break;
		case CSV :
			_VV.writeCSV(prim);
			break;
		}
	}
	// do not create mesh
	else{
		switch(_saveFormat)
		{
		case VTK :
			_VV.writeVTK(prim,false);
			break;
		case MED :
			_VV.writeMED(prim,false);
			break;
		case CSV :
			_VV.writeCSV(prim);
			break;
		}
		if(_saveConservativeField){
			switch(_saveFormat)
			{
			case VTK :
				_UU.writeVTK(cons,false);
				break;
			case MED :
				_UU.writeMED(cons,false);
				break;
			case CSV :
				_UU.writeCSV(cons);
				break;
			}
		}
	}
	if(_saveVelocity){
		for (long i = 0; i < _Nmailles; i++){
			// j = 0 : concentration, j=1 : pressure; j = _nVar - 1: temperature; j = 2,..,_nVar-2: velocity
			for (int j = 0; j < _Ndim; j++)//On récupère les composantes de vitesse
			{
				int Ii = i*_nVar +2+j;
				VecGetValues(_primitives,1,&Ii,&_Vitesse(i,j));
			}
			for (int j = _Ndim; j < 3; j++)//On met à zero les composantes de vitesse si la dimension est <3
				_Vitesse(i,j)=0;
		}
		_Vitesse.setTime(_time,_nbTimeStep);
		if (_nbTimeStep ==0){
			_Vitesse.setInfoOnComponent(0,"Velocity_x (m/s)");
			_Vitesse.setInfoOnComponent(1,"Velocity_y (m/s)");
			_Vitesse.setInfoOnComponent(2,"Velocity_z (m/s)");
			switch(_saveFormat)
			{
			case VTK :
				_Vitesse.writeVTK(prim+"_Velocity");
				break;
			case MED :
				_Vitesse.writeMED(prim+"_Velocity");
				break;
			case CSV :
				_Vitesse.writeCSV(prim+"_Velocity");
				break;
			}
		}
		else{
			switch(_saveFormat)
			{
			case VTK :
				_Vitesse.writeVTK(prim+"_Velocity",false);
				break;
			case MED :
				_Vitesse.writeMED(prim+"_Velocity",false);
				break;
			case CSV :
				_Vitesse.writeCSV(prim+"_Velocity");
				break;
			}
		}
	}
	if(_saveVoidFraction)
	{
		double p,Tm,cv,rhom,rho_v,alpha_v;
		int Ii;
		for (long i = 0; i < _Nmailles; i++){
			Ii = i*_nVar;
			VecGetValues(_courant,1,&Ii,&rhom);
			Ii = i*_nVar;
			VecGetValues(_primitives,1,&Ii,&cv);
			Ii = i*_nVar +1;
			VecGetValues(_primitives,1,&Ii,&p);
			Ii = i*_nVar +_nVar-1;
			VecGetValues(_primitives,1,&Ii,&Tm);

			rho_v=_fluides[0]->getDensity(p,Tm);
			alpha_v=cv*rhom/rho_v;
			_VoidFraction(i)=alpha_v;
		}
		_VoidFraction.setTime(_time,_nbTimeStep);
		if (_nbTimeStep ==0){
			switch(_saveFormat)
			{
			case VTK :
				_VoidFraction.writeVTK(prim+"_VoidFraction");
				break;
			case MED :
				_VoidFraction.writeMED(prim+"_VoidFraction");
				break;
			case CSV :
				_VoidFraction.writeCSV(prim+"_VoidFraction");
				break;
			}
		}
		else{
			switch(_saveFormat)
			{
			case VTK :
				_VoidFraction.writeVTK(prim+"_VoidFraction",false);
				break;
			case MED :
				_VoidFraction.writeMED(prim+"_VoidFraction",false);
				break;
			case CSV :
				_VoidFraction.writeCSV(prim+"_VoidFraction");
				break;
			}
		}
	}
	if(_saveEnthalpy)
	{
		double p,Tm,cv,rhom,rho_v,rho_l,m_v,m_l;
		int Ii;
		for (long i = 0; i < _Nmailles; i++){
			Ii = i*_nVar;
			VecGetValues(_courant,1,&Ii,&rhom);
			Ii = i*_nVar;
			VecGetValues(_primitives,1,&Ii,&cv);
			Ii = i*_nVar +1;
			VecGetValues(_primitives,1,&Ii,&p);
			Ii = i*_nVar +_nVar-1;
			VecGetValues(_primitives,1,&Ii,&Tm);

			double rho_v=_fluides[0]->getDensity(p,Tm);
			double rho_l=_fluides[1]->getDensity(p,Tm);
			double m_v=cv*rhom;
			double m_l=(1-cv)*rhom;
			double h_v=_fluides[0]->getEnthalpy(Tm,rho_v);
			double h_l=_fluides[1]->getEnthalpy(Tm,rho_l);
			_Enthalpy(i)=(m_v*h_v+m_l*h_l)/rhom;
		}
		_Enthalpy.setTime(_time,_nbTimeStep);
		if (_nbTimeStep ==0){
			switch(_saveFormat)
			{
			case VTK :
				_Enthalpy.writeVTK(prim+"_Enthalpy");
				break;
			case MED :
				_Enthalpy.writeMED(prim+"_Enthalpy");
				break;
			case CSV :
				_Enthalpy.writeCSV(prim+"_Enthalpy");
				break;
			}
		}
		else{
			switch(_saveFormat)
			{
			case VTK :
				_Enthalpy.writeVTK(prim+"_Enthalpy",false);
				break;
			case MED :
				_Enthalpy.writeMED(prim+"_Enthalpy",false);
				break;
			case CSV :
				_Enthalpy.writeCSV(prim+"_Enthalpy");
				break;
			}
		}
	}
}