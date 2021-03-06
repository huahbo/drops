#=============================================================
#    DROPS parameter file for    TestPoissonPar
#    Testroutines for solving the poisson-equation -nu*u=f
#    with solution u = xyz (1-x) (1-y) (1-z)
#=============================================================

# Coefficients for Poisson
Poisson{
  nu = 1    # coeff of the difffusion
}

# refine strategy
Refining {
  BasicRefX   = 4       # number of basic refines in x direction
  BasicRefY   = 4      # number of basic refines in y direction
  BasicRefZ   = 4       # number of basic refines in z direction
  dx          = 1.0     # dimension of the brick in x direction  Wird zun�chst ignoriert, so dass ich die L�sung kenne!
  dy          = 1.0     # dimension of the brick in y direction  Wird zun�chst ignoriert, so dass ich die L�sung kenne!
  dz          = 1.0     # dimension of the brick in z direction  Wird zun�chst ignoriert, so dass ich die L�sung kenne!
  RefAll      = __REFINE__       # regular refinement or number of adaptive steps
  MarkDrop    = 0       # mark droplet once if non adaptive, if 0 then mark not else marke once. If adaptive mark tetras around drop
  MarkCorner  = 0       # mark corner
  Adpativ     = 0      # to be adaptive or not
}

# loadbalancing strategy
LoadBalancing{
 RefineStrategy =  1  # 0 no loadbalancing, 1 adaptive repart, 2 PartKWay
 TransferUnknowns= 1  # no unknowns are transfered while loadbalancing, 1 unknowns are transfered
}

# choose one solver of the pool of solvers
Solver {
  Solver      = __SOLVER__      # solver of Poisson problem: 0 CG, 1 PCG, 2 PGMRes, 3 PGCR, 4 QMR, 5 PQMR, 6 BiCGStab
  Precond     = __PRECOND__     # preconditioner for solver (if preconditioned): 0 Dummy, 1 Jac0, 2 SSOR0, 3 BiCGStab (for QMR only Jac0 or Dummy)
  Relax       = 1      # relaxiation parameter for the preconditioner
  PCTol       = 0.001  # tolerance of BiCGStab as preconditioner
  PCIter      = 200    # number of maximal iterations for BiCGStab as preconditioner
  Iteration   = 1000   # number of maximal iterations for solver
  Tol         = 1e-10  # tolerace for the residuum
  Restart     = 50    # dimension of Krylov subspace if solver is GMRES or truncation-parameter, if solver is GCR
  UseMGS      = 0      # use modified Gramm-Schmidt for GMRES to compute Krylov subspace basis (slower, but more accurate)
  Relative    = 0      # use relative measurement for resid
  Accur       = 1      # use accur variant of solver (not: CG, QMR) (always: BiCGStab)
  Modified    = 1      # modified variante for better scalability
  PreCondMeth = 1      # left or right preconditioning (for GMRES): 0 left, 1 right
}

# miscellaneous
Misc{
  PrintGEO      = 0  # print multigrid as geomview file
  PrintMG       = 0  # print multigrid as ascii file
  PrintSize     = 1  # display distribution of elements
  CheckMG       = 0  # check the parallel multigrid
  PrintUnknowns = 1  # display number of unknowns
}

#ensight
Ensight{
 ensight   = 0              # create ensight files
 EnsCase   = Poisson        # Case name
 EnsDir    = ensight_par    # directory of ensight files
 GeomName  = Wuerfel        # description of geometry
 VarName   = Temperatur     # description of variable name
}
