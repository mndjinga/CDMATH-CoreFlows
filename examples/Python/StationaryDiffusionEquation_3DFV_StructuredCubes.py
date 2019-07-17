#!/usr/bin/env python
# -*-coding:utf-8 -*
#===============================================================================================================================
# Name        : Résolution VF de l'équation de Poisson 3D -\triangle T = f sur uncube avec conditions aux limites de Dirichlet T=0
# Author      : Michaël Ndjinga
# Copyright   : CEA Saclay 2019
# Description : Utilisation de la méthode des volumes finis avec champs T et f discrétisés aux cellules d'un maillage quelconque
#				Création et sauvegarde du champ résultant ainsi que du champ second membre en utilisant CDMATH
#               Comparaison de la solution numérique avec la solution exacte T=sin(pi*x)*sin(pi*y)*sin(pi*z)
#================================================================================================================================

import CoreFlows as cf
import cdmath as cm
from math import sin, pi

def StationaryDiffusionEquation_3DFV_StructuredCubes():
	spaceDim = 3;
	# Prepare for the mesh
	print("Building mesh " );
	xinf = 0 ;
	xsup=1.0;
	yinf=0.0;
	ysup=1.0;
	zinf = 0.0;
	zsup = 1.0;
	nx = 20;
	ny = 20; 
	nz = 20; 
	M=cm.Mesh(xinf,xsup,nx,yinf,ysup,ny,zinf,zsup,nz)#Regular cube mesh
	# set the limit field for each boundary
	eps=1e-6;
	M.setGroupAtPlan(xsup,0,eps,"Bord1")
	M.setGroupAtPlan(xinf,0,eps,"Bord2")
	M.setGroupAtPlan(ysup,1,eps,"Bord3")
	M.setGroupAtPlan(yinf,1,eps,"Bord4")
	M.setGroupAtPlan(zsup,2,eps,"Bord5")
	M.setGroupAtPlan(zinf,2,eps,"Bord6")
	
	print "Built a regular 3D cube mesh with ", nx,"x" ,ny,"x" ,nz, " cells"

	FEComputation=False
	myProblem = cf.StationaryDiffusionEquation(spaceDim,FEComputation);
	myProblem.setMesh(M);

	# set the limit value for each boundary
	T1=0;
	T2=0;
	T3=0;
	T4=0;
	T5=0;
	T6=0;
	
	myProblem.setDirichletBoundaryCondition("Bord1",T1)
	myProblem.setDirichletBoundaryCondition("Bord2",T2)
	myProblem.setDirichletBoundaryCondition("Bord3",T3)
	myProblem.setDirichletBoundaryCondition("Bord4",T4)
	myProblem.setDirichletBoundaryCondition("Bord5",T5)
	myProblem.setDirichletBoundaryCondition("Bord6",T6)

	#Set the right hand side function
	my_RHSfield = cm.Field("RHS_field", cm.CELLS, M, 1)
	for i in range(M.getNumberOfCells()):
		Ci= M.getCell(i)
		x = Ci.x()
		y = Ci.y()
		z = Ci.z()

		my_RHSfield[i]=2*pi*pi*sin(pi*x)*sin(pi*y)*sin(pi*z)#mettre la fonction definie au second membre de l'edp
	
	myProblem.setHeatPowerField(my_RHSfield)
	myProblem.setLinearSolver(cf.GMRES,cf.ILU);

	# name of result file
	fileName = "StationaryDiffusion_3DFV_StructuredCubes";

	# computation parameters
	myProblem.setFileName(fileName);

	# Run the computation
	myProblem.initialize();
	print("Running python "+ fileName );

	ok = myProblem.solveStationaryProblem();
	if (not ok):
		print( "Python simulation of " + fileName + "  failed ! " );
		pass
	else:
		print( "Python simulation of " + fileName + " is successful !" );
		####################### Postprocessing #########################
		my_ResultField = myProblem.getOutputTemperatureField()
		#The following formulas use the fact that the exact solution is equal the right hand side divided by 3*pi*pi
		max_abs_sol_exacte=max(my_RHSfield.max(),-my_RHSfield.min())/(3*pi*pi)
		max_sol_num=my_ResultField.max()
		min_sol_num=my_ResultField.min()
		erreur_abs=0
		for i in range(M.getNumberOfCells()) :
			if  erreur_abs < abs(my_RHSfield[i]/(3*pi*pi) - my_ResultField[i]) :
				erreur_abs = abs(my_RHSfield[i]/(3*pi*pi) - my_ResultField[i])
		
		print("Absolute error = max(| exact solution - numerical solution |) = ",erreur_abs )
		print("Relative error = max(| exact solution - numerical solution |)/max(| exact solution |) = ",erreur_abs/max_abs_sol_exacte)
		print ("Maximum numerical solution = ", max_sol_num, " Minimum numerical solution = ", min_sol_num)
		
		assert erreur_abs/max_abs_sol_exacte <1.
        pass

	print( "------------ !!! End of calculation !!! -----------" );

	myProblem.terminate();
	return ok

if __name__ == """__main__""":
	StationaryDiffusionEquation_3DFV_StructuredCubes()
