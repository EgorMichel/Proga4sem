#include <dolfin.h>
#include <gmsh.h>
#include "HyperElasticity.h"

using namespace dolfin;

// Sub domain for clamp at left end
class Left : public SubDomain
{
  bool inside(const Array<double>& x, bool on_boundary) const
  {
    return (std::abs(x[0]) < DOLFIN_EPS) && on_boundary;
  }
};

// Sub domain for rotation at right end
class Right : public SubDomain
{
  bool inside(const Array<double>& x, bool on_boundary) const
  {
    return (std::abs(x[0] - 1.0) < DOLFIN_EPS) && on_boundary;
  }
};

// Dirichlet boundary condition for clamp at left end
class Clamp : public Expression
{
public:

  Clamp() : Expression(3) {}

  void eval(Array<double>& values, const Array<double>& x) const
  {
    values[0] = 0.0;
    values[1] = 0.0;
    values[2] = 0.0;
  }

};

// Dirichlet boundary condition for rotation at right end
class Rotation : public Expression
{
public:

  Rotation() : Expression(3) {}

  void eval(Array<double>& values, const Array<double>& x) const
  {
    const double scale = 0.5;

    // Center of rotation
    const double y0 = 0.5;
    const double z0 = 0.5;

    // Large angle of rotation (60 degrees)
    double theta = 1.04719755;

    // New coordinates
    double y = y0 + (x[1] - y0)*cos(theta) - (x[2] - z0)*sin(theta);
    double z = z0 + (x[1] - y0)*sin(theta) + (x[2] - z0)*cos(theta);

    // Rotate at right end
    values[0] = 0.0;
    values[1] = scale*(y - x[1]);
    values[2] = scale*(z - x[2]);
  }
};

int main()
{

  // Create mesh and define function space
//  auto mesh = dolfin::Mesh("TRUE_PIKACHU.xml");
  auto mesh = std::make_shared<UnitCubeMesh>(1, 1, 1);
  auto V = std::make_shared<HyperElasticity::FunctionSpace>(mesh);

  // Define Dirichlet boundaries
  auto left = std::make_shared<Left>();
  auto right = std::make_shared<Right>();

  // Define Dirichlet boundary functions
  auto c = std::make_shared<Clamp>();
  auto r = std::make_shared<Rotation>();

  // Create Dirichlet boundary conditions
  DirichletBC bcl(V, c, left);
  DirichletBC bcr(V, r, right);
  std::vector<const DirichletBC*> bcs = {{&bcl, &bcr}};

  // Define source and boundary traction functions
  auto B = std::make_shared<Constant>(0.0, -0.5, 0.0);
  auto T = std::make_shared<Constant>(0.1,  0.0, 0.0);

  // Define solution function
  auto u = std::make_shared<Function>(V);

  // Set material parameters
  const double E  = 10.0;
  const double nu = 0.3;
  auto mu = std::make_shared<Constant>(E/(2*(1 + nu)));
  auto lambda = std::make_shared<Constant>(E*nu/((1 + nu)*(1 - 2*nu)));

  // Create (linear) form defining (nonlinear) variational problem
  HyperElasticity::ResidualForm F(V);
  F.mu = mu; F.lmbda = lambda; F.u = u;
  F.B = B; F.T = T;

  // Create Jacobian dF = F' (for use in nonlinear solver).
  HyperElasticity::JacobianForm J(V, V);
  J.mu = mu; J.lmbda = lambda; J.u = u;

  // Solve nonlinear variational problem F(u; v) = 0
  solve(F == 0, *u, bcs, J);

  // Save solution in VTK format
  File file("displacement.pvd");
  file << *u;

  return 0;
}
