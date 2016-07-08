//============================================================================
// Name        : ProblemCoreFlows
// Author      : M. Ndjinga
// Version     :
// Copyright   : CEA Saclay 2014
// Description : Generic class for thermal hydraulics problems
//============================================================================

/*! \class ProblemFluid ProblemFluid.hxx "ProblemFluid.hxx"
 *  \brief Factorises the methods that are common to the non scalar models (fluid models)
 *  \details Common functions to fluid models
 */
#ifndef PROBLEMFLUID_HXX_
#define PROBLEMFLUID_HXX_

#include "Fluide.h"
#include "ProblemCoreFlows.hxx"
#include "utilitaire_algebre.h"

using namespace std;

//! enumeration NonLinearFormulation
/*! the formulation used to compute the non viscous fluxes */
enum NonLinearFormulation
{
	Roe,/**< Ph. Roe non linear formulation is used */
	VFRoe,/**< Masella, Faille and Gallouet non linear formulation is used */
	VFFC,/**< Ghidaglia, Kumbaro and Le Coq non linear formulation is used */
	reducedRoe,/**< compacted formulation of Roe scheme without computation of the fluxes */
};

class ProblemFluid: public ProblemCoreFlows
{

public :
	/**\fn
	 * \brief constructeur de la classe ProblemFluid
	 */
	ProblemFluid(void);

	//Gestion du calcul (interface ICoCo)

	/** \fn initialize
	 * \brief Alocates memory and checks that the mesh, the boundary and the intitial data are set
	 * \Details It is a pure virtual function overloaded y each model.
	 * @param  void
	 *  */
	virtual void initialize();

	/** \fn terminate
	 * \brief empties the memory
	 * @param void
	 *  */
	virtual void terminate();

	/** \fn initTimeStep
	 * \brief Sets a new time step dt to be solved later
	 *  @param  dt is  the value of the time step
	 *  \return false if dt <0 et True otherwise
	 *  			   */
	bool initTimeStep(double dt);

	/** \fn computeTimeStep
	 * \brief Proposes a value for the next time step to be solved using mesh data and cfl coefficient
	 *  \return  double dt the proposed time step
	 *  \return  bool stop, true if the calculation should not be continued (stationary state, maximum time or time step numer reached)
	 *  */
	double computeTimeStep(bool & stop);

	/** \fn abortTimeStep
	 * \brief Reset the time step dt to 0
	 *  */
	void abortTimeStep();

	/** \fn iterateTimeStep
	 * \brief
	 * @param
	 * @return
	 * */
	bool iterateTimeStep(bool &ok);

	/** \fn save
	 * \brief saves the current results in MED or VTK files
	 * \details It is a pure virtual function overloaded in each model
	 * @param  void
	 */
	virtual void save()=0;

	/** \fn validateTimeStep
	 * \brief Validates the solution computed y solveTimeStep
	 * \details updates the currens time t=t+dt, save unknown fields, resets the time step dt to 0, tests the stationnarity.
	 * c It is a pure virtual function overloaded in each model
	 * @param  void
	 *  */
	virtual void validateTimeStep();

	/** \fn setOutletBoundaryCondition
	 * \brief adds a new boundary condition of type Outlet
	 * \details
	 * \param [in] string : the name of the boundary
	 * \param [in] double : the value of the pressure at the boundary
	 * \param [out] void
	 *  */
	void setOutletBoundaryCondition(string groupName,double Pressure){
		_limitField[groupName]=LimitField(Outlet,Pressure,vector<double>(_nbPhases,0),vector<double>(_nbPhases,0),vector<double>(_nbPhases,0),-1,-1,-1,-1);
	};

	/** \fn setViscosity
	 * \brief sets the vector of viscosity coefficients
	 * @param viscosite is a vector of size equal to the number of phases and containing the viscosity of each phase
	 * @return throws an exception if the input vector size is not equal to the number of phases
	 * 	 * */
	void setViscosity(vector<double> viscosite){
		if(_nbPhases!= viscosite.size())
			throw CdmathException("ProblemFluid::setViscosity: incorrect vector size vs number of phases");
		for(int i=0;i<_nbPhases;i++)
			_fluides[i]->setViscosity(viscosite[i]);
	};

	/** \fn setConductivity
	 * \brief sets the vector of conductivity coefficients
	 * @param conductivite is a vector of size equal to the number of phases and containing the conductivity of each phase
	 * @return throws an exception if the input vector size is not equal to the number of phases
	 * */
	void setConductivity(vector<double> conductivite){
		if(_nbPhases!= conductivite.size())
			throw CdmathException("ProblemFluid::setConductivity: incorrect vector size vs number of phases");
		for(int i=0;i<_nbPhases;i++)
			_fluides[i]->setConductivity(conductivite[i]);
	};

	/** \fn setGravity
	 * \brief sets the gravity force in the model
	 * @param gravite is a vector of size equal to the space dimension
	 * 				 * */
	void setGravity(vector<double> gravite){
		_gravity3d = gravite;
	};

	/** \fn setDragCoeffs
	 * \brief Sets the drag coefficients
	 * @param dragCoeffs is a  vector of size equal to the number of phases and containing the value of the friction coefficient of each phase
	 * @return throws an exception if the input vector size is not equal to the numer of phases
	 * */
	void setDragCoeffs(vector<double> dragCoeffs){
		if(_nbPhases!= dragCoeffs.size())
			throw CdmathException("ProblemFluid::setDragCoeffs: incorrect vector size vs number of phases");
		for(int i=0;i<_nbPhases;i++)
			_fluides[i]->setDragCoeffs(dragCoeffs[i]);
	};

	/** \fn getNumberOfPhases
	 * \brief The numer of phase (one or two) depending on the model considered
	 * @param void
	 * @return the numer of phases considered in the model
	 * */
	int getNumberOfPhases(){
		return _nbPhases;
	};

	/** \fn computeNewtonVariation
	 * \brief Calcule les itération de Newton
	 * @param void
	 * */
	void computeNewtonVariation();

	/** \fn testConservation
	 * \brief Teste et affiche la conservation de masse et de la quantité de mouvement
	 * \Details la fonction est virtuelle pure, on la surcharge dans chacun des modèles
	 * @param void
	 * */
	virtual void testConservation()=0;

	/** \fn saveVelocity
	 * \brief saves the velocity field in a separate file so that paraview can display streamlines
	 * @param void
	 * */
	void saveVelocity(bool save_v=true){
		_saveVelocity=save_v;
	}

	/** \fn saveConservativeField
	 * \brief saves the conservative fields (density, momentum etc...)
	 * @param void
	 * */
	void saveConservativeField(bool save=true){
		_saveConservativeField=save;
	}
	/** \fn setEntropicCorrection
	 * \brief include an entropy correction to avoid non entropic solutions
	 * @param boolean that is true if entropy correction should be applied
	 * @param void
	 * */
	void setEntropicCorrection(bool entropyCorr){
		_entropicCorrection=entropyCorr;
	}

	/** \fn entropicShift
	 * \brief computes the eigenvalue jumps for the entropy correction
	 * @param normal vector n to the interface between the two cells _l and _r
	 * @param void
	 * */
	virtual void entropicShift(double* n)=0;

	// Petsc resolution

	/** \fn setLinearSolver
	 * \brief sets the linear solver and preconditioner
	 * \details virtuelle function overloaded by intanciable classes
	 * @param kspType linear solver type (GMRES or BICGSTAB)
	 * @param pcType preconditioner (ILU,LU or NONE)
	 * @param scaling performs a bancing of the system matrix before calling th preconditioner
	 */
	void setLinearSolver(linearSolver kspType, preconditioner pcType, bool scaling=false)
	{
		ProblemCoreFlows::setLinearSolver(kspType, pcType);
		_isScaling= scaling;
	};

	/** \fn setLatentHeat
	 * \brief Sets the value of the latent heat
	 * @param double L, the value of the latent heat
	 * */
	void setLatentHeat(double L){
		_latentHeat=L;
	}

	/** \fn getLatentHeat
	 * \brief returns the value of the latent heat
	 * @param double L, the value of the latent heat
	 * */
	double getLatentHeat(){
		return _latentHeat;
	}

	/** \fn setSatTemp
	 * \brief sets the saturation temperature
	 * @param  Tsat double corresponds to saturation temperature
	 * */
	void setSatTemp(double Tsat){
		_Tsat=Tsat;
	}

	/** \fn setSatTemp
	 * \brief sets the saturation temperature
	 * @param  Tsat double corresponds to saturation temperature
	 * */
	double getSatTemp(){
		return _Tsat;
	}

	/** \fn setSatPressure
	 * \brief sets the saturation pressure
	 * @param  Psat double corresponds to saturation pressure
	 * */
	void setSatPressure(double Psat, double dHsatl_over_dp=0.05){
		_Psat=Psat;
		_dHsatl_over_dp=dHsatl_over_dp;
	}

	/** \fn setPorosityField
	 * \brief set the porosity field;
	 * @param [in] Field porosity field (field on CELLS)
	 * */
	void setPorosityField(Field Porosity){
		_porosityField=Porosity;
		_porosityFieldSet=true;
	}

	/** \fn getPorosityField
	 * \brief returns the porosity field;
	 * @param
	 * */
	Field getPorosityField(){
		return _porosityField;
	}

	/** \fn setPorosityFieldFile
	 * \brief set the porosity field
	 * \details
	 * \param [in] string fileName (including file path)
	 * \param [in] string fieldName
	 * \param [out] void
	 *  */
	void setPorosityField(string fileName, string fieldName){
		_porosityField=Field(fileName, CELLS,fieldName);
		_porosityFieldSet=true;
	}

	/** \fn setPressureLossField
	 * \brief set the pressure loss coefficients field;
	 * @param [in] Field pressure loss field (field on FACES)
	 * */
	void setPressureLossField(Field PressureLoss){
		_pressureLossField=PressureLoss;
		_pressureLossFieldSet=true;
	}
	/** \fn setPressureLossField
	 * \brief set the pressure loss coefficient field
	 * \details localised friction force
	 * \param [in] string fileName (including file path)
	 * \param [in] string fieldName
	 * \param [out] void
	 *  */
	void setPressureLossField(string fileName, string fieldName){
		_pressureLossField=Field(fileName, FACES,fieldName);
		_pressureLossFieldSet=true;
	}

	/** \fn setSectionField
	 * \brief set the cross section field;
	 * @param [in] Field cross section field (field on CELLS)
	 * */
	void setSectionField(Field sectionField){
		_sectionField=sectionField;
		_sectionFieldSet=true;
	}
	/** \fn setSectionField
	 * \brief set the cross section field
	 * \details for variable cross section pipe network
	 * \param [in] string fileName (including file path)
	 * \param [in] string fieldName
	 * \param [out] void
	 *  */
	void setSectionField(string fileName, string fieldName){
		_sectionField=Field(fileName, CELLS,fieldName);
		_sectionFieldSet=true;
	}

	/** \fn setNonLinearFormulation
	 * \brief sets the formulation used for the computation of non viscous fluxes
	 * \details Roe, VFRoe, VFFC
	 * \param [in] enum NonLinearFormulation
	 * \param [out] void
	 *  */
	void setNonLinearFormulation(NonLinearFormulation nonLinearFormulation){
		_nonLinearFormulation=nonLinearFormulation;
	}

	/** \fn getNonLinearFormulation
	 * \brief returns the formulation used for the computation of non viscous fluxes
	 * \details Roe, VFRoe, VFFC
	 * \param [in] void
	 * \param [out] enum NonLinearFormulation
	 *  */
	NonLinearFormulation getNonLinearFormulation(){
		return _nonLinearFormulation;
	}

	//données initiales
	/*
	virtual vector<string> getInputFieldsNames()=0 ;//Renvoie les noms des champs dont le problème a besoin (données initiales)
	virtual  Field& getInputFieldTemplate(const string& name)=0;//Renvoie le format de champs attendu (maillage, composantes etc)
	virtual void setInputField(const string& name, const Field& afield)=0;//enregistre les valeurs d'une donnée initiale
	virtual vector<string> getOutputFieldsNames()=0 ;//liste tous les champs que peut fournir le code pour le postraitement
	virtual Field& getOutputField(const string& nameField )=0;//Renvoie un champs pour le postraitement
	 */

protected :
	/** Number of phases in the fluid */
	int _nbPhases;
	/** Field of conservative variables (the primitive variables are defined in the mother class ProblemCoreFlows */
	Field  _UU;
	/** Field of interfacial states of the VFRoe scheme **/
	Field _UUstar, _VVstar;
	/** the formulation used to compute the non viscous fluxes **/
	NonLinearFormulation _nonLinearFormulation;

	/** boolean used to specify that an entropic correction should be used */
	bool _entropicCorrection;
	/** Vector containing the eigenvalue jumps for the entropic correction **/
	vector<double> _entropicShift;

	/** Fluid equation of state*/
	vector<	Fluide* > _fluides;
	//!Viscosity coefficients 
	vector<double> _viscosite;
	//!Conductivity coefficients 
	vector<double> _conductivite;

	// Source terms 
	vector<double> _gravite, _gravity3d, _dragCoeffs;
	double _latentHeat, _Tsat,_Psat,_dHsatl_over_dp;
	Field _porosityField, _pressureLossField, _dp_over_dt, _sectionField;
	bool _porosityFieldSet, _pressureLossFieldSet, _sectionFieldSet;
	double _porosityi, _porosityj;//porosity of the left and right states around an interface
	// User options
	bool _isScaling;
	bool _saveVelocity;//In order to display streamlines with paraview
	bool _saveConservativeField;//Save conservative fields such as density momentum...
	bool _saveInterfacialField;//Save interfacial fields of the VFRoe scheme

	// Variables du schema numerique 
	Vec _courant, _next, _bScaling,_old, _primitives, _Uext,_Uextdiff ,_vecScaling,_invVecScaling, _Vext;
	//courant state vector, vector computed at next time step, second member of the equation
	PetscScalar *_AroePlus, *_AroeMinus,*_Jcb,*_JcbDiff, *_a, *_blockDiag,  *_invBlockDiag,*_Diffusion, *_Gravity;
	PetscScalar *_Aroe, *_absAroe, *_signAroe, *_invAroe;
	PetscScalar *_phi, *_Ui, *_Uj,  *_Vi, *_Vj,  *_Si, *_Sj, * _pressureLossVector, * _porosityGradientSourceVector, *_externalStates;
	double *_Uroe, *_Udiff, *_temp, *_l, *_r,  *_vec_normal;
	double * _Uij, *_Vij;//Conservative and primitive interfacial states of the VFRoe scheme
	int *_idm, *_idn;
	double _inv_dxi,_inv_dxj;//diametre des cellules i et j autour d'une face
	double _err_press_max,_part_imag_max,_minm1,_minm2;
	int _nbMaillesNeg, _nbVpCplx;
	bool _isBoundary;// la face courante est elle une face de bord ?
	double _maxvploc;

	/** \fn convectionState
	 * \brief calcule l'etat de Roe entre deux cellules voisinnes
	 * \Details c'ets une fonction virtuelle, on la surcharge dans chacun des modèles
	 * @param i,j : entiers correspondant aux numéros des cellules à gauche et à droite de l'interface
	 * @param IsBord : est un booléen qui nous dit si la cellule voisine est sur le bord ou pas
	 * */
	virtual void convectionState(const long &i, const long &j, const bool &IsBord)=0;

	/** \fn convectionMatrices
	 * \brief calcule la matrice de convection de l'etat interfacial entre deux cellules voisinnes
	 * \Details convectionMatrices est une fonction virtuelle pure, on la surcharge dans chacun des modèles
	 * */
	virtual void convectionMatrices()=0;

	/** \fn diffusionStateAndMatrices
	 * \brief calcule la matrice de diffusion de l'etat interface pour la diffusion
	 * \Details est une fonction virtuelle pure, on la surcharge dans chacun eds modèles
	 * @param i,j : entier correspondent aux indices des cellules  à gauche et droite respectivement
	 * @param IsBord: bollean telling if (i,j) is a boundary face
	 * @return
	 * */
	virtual void diffusionStateAndMatrices(const long &i,const long &j, const bool &IsBord)=0;

	/** \fn addSourceTermToSecondMember
	 * \brief Adds the contribution of source terms to the right hand side of the system: gravity,
	 *  phase change, heat power and friction
	 * @param i,j : left and right cell number
	 * @param nbNeighboursi, integer giving the number of neighbours of cell i
	 * @param nbNeighboursj, integer giving the number of neighbours of cell j
	 * @param boolean isBoundary is true for a boundary face (i,j) and false otherwise
	 * @param double mesureFace the lenght or area of the face
	 * */
	void addSourceTermToSecondMember(const int i, int nbNeighboursi,const int j, int nbNeighboursj,bool isBoundary, int ij, double mesureFace);

	/** \fn sourceVector
	 * \brief Computes the source term (at the exclusion of pressure loss terms)
	 * \Details pure virtual function, overloaded by each model
	 * @param Si output vector containing the source  term
	 * @param Ui, Vi input conservative and primitive vectors
	 * @param i the cell number. Used to read the power field
	 * @return
	 * */
	virtual void sourceVector(PetscScalar * Si,PetscScalar * Ui,PetscScalar * Vi, int i)=0;

	/** \fn convectionFlux
	 * \brief Computes the convection flux F(U) projected on a vector n
	 * @param U is the conservative variable vector 
	 * @param V is the extended primitive variable vector 
	 * @param normal is a unit vector giving the direction where the convection flux matrix F(U) is projected
	 * @param porosity is the ration of the volume occupied by the fluid in the cell (default value is 1)
	 * @return The convection flux projected in the direction given by the normal vector: F(U)*normal */
	virtual Vector convectionFlux(Vector U,Vector V, Vector normale, double porosity)=0;

	/** \fn pressureLossVector
	 * \brief Computes the contribution of pressure loss terms in the source term
	 * \Details pure virtual function, overloaded by each model
	 * @param pressureLoss output vector containing the pressure loss contributions
	 * @param K, input pressure loss coefficient
	 * @param Ui input primitive vectors
	 * @param Vi input conservative vectors
	 * @param Uj input primitive vectors
	 * @param Vj input conservative vectors
	 * @return
	 * */
	virtual void pressureLossVector(PetscScalar * pressureLoss, double K, PetscScalar * Ui, PetscScalar * Vi, PetscScalar * Uj, PetscScalar * Vj)=0;

	/** \fn porosityGradientSourceVector
	 * \brief Computes the contribution of the porosity variation in the source term
	 * \Details pure virtual function, overloaded by each model
	 * @param porosityGradientVector output vector containing the porosity variation contributions to the source term
	 * @param Ui input primitive vectors celli
	 * @param Vi input conservative vectors celli
	 * @param porosityi input porosity value celli
	 * @param deltaxi input diameter celli
	 * @param Uj input primitive vectors cellj
	 * @param Vj input conservative vectors cellj
	 * @param porosityj input porosity value cellj
	 * @param deltaxj input diameter cellj
	 * @return
	 * */
	virtual void porosityGradientSourceVector()=0;

	//matrice de gravite

	/** \fn jacobian
	 * \brief Calcule la jacobienne de la ConditionLimite convection
	 * \Details est une fonction virtuelle pure, qu'on surcharge dans chacun des modèles
	 * @param j entier , l'indice de la cellule sur le bord
	 * @param nameOfGroup  : chaine de caractères, correspond au type de la condition limite
	 * */
	virtual void jacobian(const int &j, string nameOfGroup,double * normale)=0;

	/** \fn jacobianDiff
	 * \brief Calcule la jacobienne de la CL de diffusion
	 * \Details est une fonction virtuelle pure, qu'on surcharge dans chacun des modèles
	 * @param j entier , l'indice de la cellule sur le bord
	 * @param nameOfGroup  : chaine de caractères, correspond au type de la condition limite
	 * */
	virtual void jacobianDiff(const int &j, string nameOfGroup)=0;

	/** \fn setBoundaryState
	 * \brief Calcule l'etat fictif a la frontiere
	 * \Details est une fonction virtuelle pure, qu'on surcharge dans chacun des modèles
	 * @param j entier , l'indice de la cellule sur le bord
	 * @param nameOfGroup  : chaine de caractères, correspond au type de la condition limite
	 * @param normale est un vecteur de double correspond au vecteur normal à l'interface
	 * @return
	 * */
	virtual void setBoundaryState(string nameOfGroup, const int &j,double *normale)=0;

	/** \fn addDiffusionToSecondMember
	 * \brief Compute the contribution of the diffusion operator to the right hand side of the system
	 * \Details this function is pure virtual, and overloaded in each physical model class
	 * @param i left cell number
	 * @param j right cell number
	 * @param boolean isBoundary is true for a boundary face (i,j) and false otherwise
	 * */
	virtual void addDiffusionToSecondMember(const int &i,const int &j,bool isBoundary)=0;

	//!Computes the interfacial flux for the VFFC formulation of the staggered upwinding
	virtual Vector staggeredVFFCFlux()=0;
	//!Compute the corrected interfacial state for lowMach, pressureCorrection and staggered versions of the VFRoe formulation
	virtual void applyVFRoeLowMachCorrections()=0;

	//remplit les vecteurs de scaling
	/** \fn computeScaling
	 * \brief Special preconditioner based on a matrix scaling strategy
	 * \Details pure  virtual function overloaded in every model class
	 * @param offset double , correspond à la plus grande valeur propre
	 * @return
	 * */
	virtual void computeScaling(double offset) =0;

	// Fonctions utilisant la loi d'etat 

	/** \fn consToPrim
	 * \brief calcule le vecteur primitif à partir du vecteur conservatif
	 * \Details est une fonction virtuelle pure, on la surcharge dans chacun des modèles
	 * @param Ucons : vecteur conservatif
	 * @pram Vprim : vecteur primitif
	 * @param porosity est le coefficient de porisité en case de problème avec porosité
	 * */
	virtual void consToPrim(const double *Ucons, double* Vprim,double porosity=1) = 0;

	/** \fn Prim2Cons
	 * \brief calcule le vecteur conservatif à partir du vecteur primitif
	 * \Details est une fonction virtuelle pure, on la surcharge dans chacun des modèles
	 * @param U : vecteur conservatif
	 * @pram V : vecteur primitif
	 * @param i,j : les indices des cellules voisines pour les quelles on veut calculer
	 * le vecteur conservatif
	 * 	 */
	virtual void Prim2Cons(const double *V, const int &i, double *U, const int &j)=0;

	/** \fn getRacines
	 * \brief Computes the roots of a polynomial
	 * @param polynome is a vector containing the coefficients of the polynom
	 * @return vector containing the roots (complex numbers)
	 * */
	vector< complex<double> > getRacines(vector< double > polynome);

	// Some supplement functions ---------------------------------------------------------------------------------------------

	/** \fn addConvectionToSecondMember
	 * \brief Adds the contribution of the convection to the system right hand side for a face (i,j) inside the domain
	 * @param i left cell number
	 * @param j right cell number
	 * @param isBord is a boolean that is true if the interface (i,j) is a boundary interface
	 * */
	virtual void addConvectionToSecondMember(const int &i,const int &j,bool isBord);

	/** \fn updatePrimitives
	 * \brief met à jour le vecteur primitif à partir du champ conservatif
	 * @param void
	 * */
	void updatePrimitives();
	/** \fn AbsMatriceRoe
	 * \brief Computes the absolute value of the Roe matrix
	 * @param valeurs_propres_dist is the vector of distinct eigenvalues of the Roe matrix
	 * */
	void AbsMatriceRoe(vector< complex<double> > valeurs_propres_dist);

	/** \fn SigneMatriceRoe
	 * \brief Computes the sign of the Roe matrix
	 * @param valeurs_propres_dist is the vector of distinct eigenvalues of the Roe matrix
	 * */
	void SigneMatriceRoe(vector< complex<double> > valeurs_propres_dist);

	/** \fn InvMatriceRoe
	 * \brief Computes the inverse of the Roe matrix
	 * @param valeurs_propres_dist is the vector of distinct eigenvalues of the Roe matrix
	 * */
	void InvMatriceRoe(vector< complex<double> > valeurs_propres_dist);

};

#endif /* PROBLEMFLUID_HXX_ */